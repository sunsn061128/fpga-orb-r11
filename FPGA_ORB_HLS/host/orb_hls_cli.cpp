#include "orb_hls.hpp"
#include "orb_packed_image.hpp"
#include "golden.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using orb_golden::Config;
using orb_golden::Feature;
using orb_golden::Image;
using orb_golden::Layer;
using orb_golden::Point;
using orb_golden::Result;

/** Per-call execution times for the host C model, expressed in milliseconds. */
struct RunTiming {
    std::array<double, 8> resize_ms{};
    std::array<double, 8> level_ms{};
    double frame_ms = 0.0;
};

/**
 * @brief Convert a steady-clock interval to milliseconds.
 * @param begin Interval start time.
 * @param end Interval end time.
 * @return Elapsed interval in milliseconds.
 */
double elapsed_ms(std::chrono::steady_clock::time_point begin,
                  std::chrono::steady_clock::time_point end) {
    return std::chrono::duration<double, std::milli>(end - begin).count();
}

/**
 * @brief Append one byte to a 64-bit FNV-1a digest.
 * @param hash Current digest value.
 * @param value Byte to append.
 * @return Updated digest value.
 */
uint64_t hash_byte(uint64_t hash, uint8_t value) {
    hash ^= value;
    return hash * UINT64_C(1099511628211);
}

/**
 * @brief Append one 32-bit value in little-endian order.
 * @param hash Current digest value.
 * @param value Value to append.
 * @return Updated digest value.
 */
uint64_t hash_u32(uint64_t hash, uint32_t value) {
    for (int byte = 0; byte < 4; ++byte) {
        hash = hash_byte(hash, uint8_t(value >> (8 * byte)));
    }
    return hash;
}

/**
 * @brief Hash the row-major pixels of one pyramid image.
 * @param image Image whose complete active pixel range is hashed.
 * @return Deterministic 64-bit FNV-1a digest.
 */
uint64_t digest_image(const Image& image) {
    uint64_t hash = UINT64_C(14695981039346656037);
    for (uint8_t pixel : image.pixels) hash = hash_byte(hash, pixel);
    return hash;
}

/**
 * @brief Hash an ordered FAST/NMS candidate list in the PL record layout.
 * @param points Candidate points in tile traversal order.
 * @return FNV-1a digest over x, y, score, and tile fields.
 */
uint64_t digest_candidates(const std::vector<Point>& points) {
    uint64_t hash = UINT64_C(14695981039346656037);
    for (const Point& point : points) {
        hash = hash_u32(hash, uint32_t(point.x));
        hash = hash_u32(hash, uint32_t(point.y));
        hash = hash_u32(hash, uint32_t(point.score));
        hash = hash_u32(hash, uint32_t(point.tile));
    }
    return hash;
}

/**
 * @brief Hash one level in the externally visible order used by the PS checker.
 * @param layer Complete reference level result.
 * @return Digest covering score, Gaussian, metadata, points, and descriptors.
 */
uint64_t digest_level(const Layer& layer) {
    uint64_t hash = UINT64_C(14695981039346656037);
    for (uint8_t value : layer.score.pixels) hash = hash_byte(hash, value);
    for (uint8_t value : layer.blurred.pixels) hash = hash_byte(hash, value);
    hash = hash_u32(hash, uint32_t(layer.candidates.size()));
    hash = hash_u32(hash, uint32_t(layer.selected.size()));
    hash = hash_u32(hash, uint32_t(layer.cell_side));
    hash = hash_u32(hash, uint32_t(layer.cell_cols));
    hash = hash_u32(hash, uint32_t(layer.cell_rows));
    hash = hash_u32(hash, uint32_t(layer.empty_cells));
    hash = hash_u32(hash, uint32_t(layer.fallback_cells));
    hash = hash_u32(hash, orb_hls::kAlgorithmVersion);
    hash = hash_u32(hash, orb_hls::kCapabilityDualThreshold);
    hash = hash_u32(hash, uint32_t(orb_hls::kMaxGridTopK));
    for (const Point& point : layer.candidates) {
        hash = hash_u32(hash, uint32_t(point.x));
        hash = hash_u32(hash, uint32_t(point.y));
        hash = hash_u32(hash, uint32_t(point.score));
        hash = hash_u32(hash, uint32_t(point.tile));
    }
    for (size_t index = 0; index < layer.selected.size(); ++index) {
        const Point& point = layer.selected[index];
        const Feature& feature = layer.features[index];
        hash = hash_u32(hash, uint32_t(point.x));
        hash = hash_u32(hash, uint32_t(point.y));
        hash = hash_u32(hash, uint32_t(point.score));
        hash = hash_u32(hash, uint32_t(point.tile));
        hash = hash_u32(hash, uint32_t(feature.point.x));
        hash = hash_u32(hash, uint32_t(feature.point.y));
        hash = hash_u32(hash, uint32_t(feature.point.score));
        hash = hash_u32(hash, uint32_t(feature.level));
        hash = hash_u32(hash, uint32_t(feature.bin));
        hash = hash_u32(hash, uint32_t(feature.m10));
        hash = hash_u32(hash, uint32_t(feature.m01));
        hash = hash_u32(hash, feature.angle_q16);
        for (int word = 0; word < 8; ++word) {
            uint32_t descriptor_word = 0;
            for (int byte = 0; byte < 4; ++byte) {
                descriptor_word |= uint32_t(feature.descriptor[size_t(word * 4 + byte)]) << (8 * byte);
            }
            hash = hash_u32(hash, descriptor_word);
        }
    }
    return hash;
}

/**
 * @brief Print per-level image and algorithm digests for the physical-board checker.
 * @param result Complete HLS-model result.
 */
void print_reference_digests(const Result& result) {
    std::cout << "HLS_REFERENCE_DIGESTS" << std::hex << std::setfill('0');
    for (size_t level = 0; level < result.layers.size(); ++level) {
        const Layer& layer = result.layers[level];
        std::cout << " L" << std::dec << level << std::hex
                  << "_image=" << std::setw(16) << digest_image(layer.image)
                  << " L" << std::dec << level << std::hex
                  << "_level=" << std::setw(16) << digest_level(layer)
                  << " L" << std::dec << level << std::hex
                  << "_fast=" << std::setw(16) << digest_candidates(layer.candidates)
                  << " L" << std::dec << level
                  << "_candidates=" << layer.candidates.size()
                  << " L" << level << "_selected=" << layer.selected.size();
    }
    std::cout << std::dec << std::setfill(' ') << '\n';
}

/**
 * @brief Enforce a host-side precondition.
 * @param condition Condition that must evaluate true.
 * @param message Diagnostic text used when the condition fails.
 * @throws std::runtime_error when condition is false.
 */
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

/**
 * @brief Run the multi-level PS/PL software model and reconstruct the reference result object.
 * @param input Source grayscale image.
 * @param config Validated hardware-profile configuration.
 * @param timing Optional destination for per-kernel host C-model timings.
 * @return Complete per-level traces and cross-level ordered features.
 * @throws std::runtime_error for unsupported profiles or kernel status failures.
 */
Result run_hls(const Image& input, const Config& config, RunTiming* timing = nullptr) {
    const auto frame_begin = std::chrono::steady_clock::now();
    require(config.levels >= 1 && config.levels <= 8, "levels must be 1..8");
    require(config.features > 0 && config.features <= orb_hls::kMaxFeatures, "features out of range");
    require(config.scale_num == 6 && config.scale_den == 5, "hardware profile currently supports scale 6/5");
    require(config.fixed_fast && config.grid && config.blur, "hardware profile requires fixed FAST, grid, and Gaussian");
    require(config.angle_bins == 32, "hardware profile requires A32 orientation");
    require(config.grid_top_k >= 1, "grid top-K must be positive");
    require(config.grid_top_k <= orb_hls::kMaxGridTopK,
            "requested grid top-K exceeds hardware capability max_K=1");
    require(config.low_threshold <= config.threshold, "threshold ordering");

    Result result;
    std::vector<Feature> natural;
    const std::vector<int> budgets = orb_golden::budgets(config);
    uint64_t scale_q24 = uint64_t(1) << 24;
    Image current = input;

    for (int level = 0; level < config.levels; ++level) {
        const int width = int(orb_golden::round_even(int64_t(input.width) * (int64_t(1) << 24), int64_t(scale_q24)));
        const int height = int(orb_golden::round_even(int64_t(input.height) * (int64_t(1) << 24), int64_t(scale_q24)));
        if (level != 0) {
            Image resized(width, height);
            const std::vector<uint64_t> packed_current =
                orb_hls::pack_image(current.pixels.data(), current.pixels.size());
            std::vector<uint64_t> packed_resized(
                orb_hls::packed_word_count(resized.pixels.size()));
            const auto resize_begin = std::chrono::steady_clock::now();
            const int status = orb_resize_hls(packed_current.data(), packed_resized.data(),
                                              current.width, current.height, width, height);
            const auto resize_end = std::chrono::steady_clock::now();
            orb_hls::unpack_image(
                packed_resized.data(), resized.pixels.data(), resized.pixels.size());
            if (timing != nullptr) {
                timing->resize_ms[size_t(level)] = elapsed_ms(resize_begin, resize_end);
            }
            require(status == orb_hls::kOk, "orb_resize_hls failed with status " + std::to_string(status));
            current = std::move(resized);
        }

        const size_t pixels = size_t(width) * size_t(height);
        std::vector<uint16_t> pixel_work(4u * pixels);
        std::vector<orb_hls::Point> point_work(4u * pixels);
        uint16_t* const score = pixel_work.data();
        uint16_t* const gaussian = pixel_work.data() + 2u * pixels;
        orb_hls::Point* const candidates = point_work.data();
        orb_hls::Point* const selected = point_work.data() + pixels;
        const int feature_capacity = budgets[size_t(level)];
        std::vector<orb_hls::Feature> features(size_t(feature_capacity) + 1u);
        const auto level_begin = std::chrono::steady_clock::now();
        const int status = orb_level_hls(
            current.pixels.data(), pixel_work.data(), gaussian, point_work.data(), features.data(),
            width, height, budgets[size_t(level)],
            config.threshold, config.low_threshold, config.dual_threshold ? 1 : 0, level,
            int(pixels), int(pixels), int(pixels), int(pixels), feature_capacity);
        const auto level_end = std::chrono::steady_clock::now();
        if (timing != nullptr) {
            timing->level_ms[size_t(level)] = elapsed_ms(level_begin, level_end);
        }
        require(status == orb_hls::kOk, "orb_level_hls failed at level " + std::to_string(level) +
                                            " with status " + std::to_string(status));
        const auto& metadata = features[size_t(feature_capacity)];
        const orb_hls::LevelMeta meta = {
            metadata.x, metadata.y, metadata.score, metadata.level,
            metadata.bin, metadata.m10, metadata.m01, metadata.angle_q16,
            metadata.descriptor0, int32_t(metadata.descriptor1)};
        require(meta.algorithm_version == orb_hls::kAlgorithmVersion,
                "accelerator algorithm version mismatch");
        require((meta.capability_flags & orb_hls::kCapabilityDualThreshold) != 0,
                "accelerator does not report dual-threshold support");
        require(config.grid_top_k <= meta.max_grid_top_k,
                "requested grid top-K exceeds accelerator capability");

        Layer layer;
        layer.image = current;
        layer.score = Image(width, height);
        layer.blurred = Image(width, height);
        for (size_t index = 0; index < pixels; ++index) {
            layer.score.pixels[index] = uint8_t(score[index]);
            layer.blurred.pixels[index] = uint8_t(gaussian[index]);
        }
        layer.scale_q24 = scale_q24;
        layer.budget = budgets[size_t(level)];
        layer.cell_side = meta.cell_side;
        layer.cell_cols = meta.cell_cols;
        layer.cell_rows = meta.cell_rows;
        layer.empty_cells = meta.empty_cells;
        layer.fallback_cells = meta.fallback_cells;
        layer.candidates.reserve(size_t(meta.candidate_count));
        for (int i = 0; i < meta.candidate_count; ++i) {
            const auto p = candidates[size_t(i)];
            layer.candidates.push_back({p.x, p.y, p.score, p.tile});
        }
        layer.selected.reserve(size_t(meta.selected_count));
        layer.features.reserve(size_t(meta.selected_count));
        for (int i = 0; i < meta.selected_count; ++i) {
            const auto selected_point = selected[size_t(i)];
            layer.selected.push_back({selected_point.x, selected_point.y, selected_point.score, selected_point.tile});
            const auto& source = features[size_t(i)];
            Feature feature;
            feature.point = {source.x, source.y, source.score, selected_point.tile};
            feature.level = source.level;
            feature.bin = source.bin;
            feature.m10 = source.m10;
            feature.m01 = source.m01;
            feature.angle_q16 = source.angle_q16;
            const uint32_t descriptor_words[8] = {
                source.descriptor0, source.descriptor1, source.descriptor2, source.descriptor3,
                source.descriptor4, source.descriptor5, source.descriptor6, source.descriptor7};
            for (int word = 0; word < 8; ++word) {
                for (int byte = 0; byte < 4; ++byte) {
                    feature.descriptor[size_t(word * 4 + byte)] =
                        uint8_t(descriptor_words[word] >> (8 * byte));
                }
            }
            layer.features.push_back(feature);
            natural.push_back(feature);
        }
        result.layers.push_back(std::move(layer));
        scale_q24 = uint64_t(orb_golden::round_even(int64_t(scale_q24 * config.scale_num), config.scale_den));
    }

    result.ordered.resize(natural.size());
    size_t left = 0;
    size_t right = natural.size();
    for (const Feature& feature : natural) {
        const uint64_t scaled_x = uint64_t(feature.point.x) * result.layers[size_t(feature.level)].scale_q24;
        const bool overlap = int64_t(scaled_x) >= int64_t(config.overlap_left) * (int64_t(1) << 24) &&
                             int64_t(scaled_x) <= int64_t(config.overlap_right) * (int64_t(1) << 24);
        if (overlap) result.ordered[--right] = feature;
        else result.ordered[left++] = feature;
    }
    result.mono_left = int(left);
    if (timing != nullptr) {
        timing->frame_ms = elapsed_ms(frame_begin, std::chrono::steady_clock::now());
    }
    return result;
}

/**
 * @brief Emit averaged host C-model timings in a machine-readable single line.
 * @param accumulated Sum of all measured iteration timings.
 * @param iterations Number of measured iterations in the sum.
 * @param levels Number of active pyramid levels.
 * @param width Source image width in pixels.
 * @param height Source image height in pixels.
 */
void print_benchmark(const RunTiming& accumulated, int iterations, int levels, int width, int height) {
    const double divisor = double(iterations);
    const double frame_ms = accumulated.frame_ms / divisor;
    std::cout << std::fixed << std::setprecision(3)
              << "HLS_HOST_BENCHMARK width=" << width
              << " height=" << height
              << " levels=" << levels
              << " iterations=" << iterations
              << " avg_frame_ms=" << frame_ms
              << " avg_fps=" << (frame_ms > 0.0 ? 1000.0 / frame_ms : 0.0);
    for (int level = 0; level < levels; ++level) {
        std::cout << " L" << level << "_resize_ms=" << accumulated.resize_ms[size_t(level)] / divisor
                  << " L" << level << "_orb_ms=" << accumulated.level_ms[size_t(level)] / divisor;
    }
    std::cout << " scope=host_c_model\n";
}

} // namespace

/**
 * @brief Parse the golden-compatible CLI, execute the HLS model, and serialize outputs.
 * @param argc Number of command-line arguments.
 * @param argv Command-line argument array containing unique option/value pairs.
 * @return 0 on success or 2 after reporting an invalid input or processing failure.
 */
int main(int argc, char** argv) try {
    std::map<std::string, std::string> arguments;
    for (int i = 1; i < argc; i += 2) {
        require(i + 1 < argc, "expected --option value pairs");
        require(arguments.emplace(argv[i], argv[i + 1]).second, "duplicate option");
    }
    const std::string allowed =
        " --input --output --trace --levels --features --scale-num --scale-den --threshold --low-threshold "
        "--fixed-fast --grid --blur --angle-bins --timestamp --role --overlap-left --overlap-right "
        "--dual-threshold --grid-top-k --benchmark-iterations --benchmark-warmup --print-digests ";
    for (const auto& argument : arguments) {
        require(allowed.find(" " + argument.first + " ") != std::string::npos, "unknown option " + argument.first);
    }
    require(arguments.count("--input") && arguments.count("--output"), "--input and --output are required");
    const auto integer = [&](const char* name, int fallback) {
        return arguments.count(name) ? std::stoi(arguments.at(name)) : fallback;
    };
    Config config;
    config.levels = integer("--levels", config.levels);
    config.features = integer("--features", config.features);
    config.scale_num = uint32_t(integer("--scale-num", int(config.scale_num)));
    config.scale_den = uint32_t(integer("--scale-den", int(config.scale_den)));
    config.threshold = integer("--threshold", config.threshold);
    config.low_threshold = integer("--low-threshold", config.low_threshold);
    config.fixed_fast = integer("--fixed-fast", 1) != 0;
    config.grid = integer("--grid", 1) != 0;
    config.blur = integer("--blur", 1) != 0;
    config.angle_bins = integer("--angle-bins", 32);
    config.dual_threshold = integer("--dual-threshold", config.dual_threshold ? 1 : 0) != 0;
    config.grid_top_k = integer("--grid-top-k", 1);
    config.overlap_left = integer("--overlap-left", 0);
    config.overlap_right = integer("--overlap-right", 1000);
    const int role = integer("--role", 0);
    require(role == 0 || role == 1, "role must be 0 or 1");
    const uint64_t timestamp = arguments.count("--timestamp") ? std::stoull(arguments.at("--timestamp")) : 0;
    const bool benchmark_requested = arguments.count("--benchmark-iterations") != 0;
    const bool print_digests = integer("--print-digests", 0) != 0;
    const int benchmark_iterations = integer("--benchmark-iterations", 1);
    const int benchmark_warmup = integer("--benchmark-warmup", 0);
    require(benchmark_iterations >= 1, "benchmark iterations must be positive");
    require(benchmark_warmup >= 0, "benchmark warmup must be non-negative");

    const Image input = orb_golden::read_pgm(arguments.at("--input"));
    for (int iteration = 0; iteration < benchmark_warmup; ++iteration) {
        (void)run_hls(input, config);
    }
    RunTiming accumulated;
    Result result;
    for (int iteration = 0; iteration < benchmark_iterations; ++iteration) {
        RunTiming timing;
        result = run_hls(input, config, &timing);
        accumulated.frame_ms += timing.frame_ms;
        for (int level = 0; level < config.levels; ++level) {
            accumulated.resize_ms[size_t(level)] += timing.resize_ms[size_t(level)];
            accumulated.level_ms[size_t(level)] += timing.level_ms[size_t(level)];
        }
    }
    orb_golden::write_packet(arguments.at("--output"), result, config, input.width, input.height, timestamp, role);
    if (arguments.count("--trace")) orb_golden::write_trace(arguments.at("--trace"), result, config);
    if (benchmark_requested) {
        print_benchmark(accumulated, benchmark_iterations, config.levels, input.width, input.height);
    }
    if (print_digests) print_reference_digests(result);
    std::cout << "HLS_MODEL_PASS points=" << result.ordered.size()
              << " levels=" << config.levels
              << " algorithm_version=1.0 dual_threshold_supported=1 max_K="
              << orb_hls::kMaxGridTopK << '\n';
    return 0;
} catch (const std::exception& error) {
    std::cerr << "HLS_MODEL_FAILED " << error.what() << '\n';
    return 2;
}
