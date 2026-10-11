#include "golden.hpp"
#include "orb_hls.hpp"
#include "orb_packed_image.hpp"

#include <cstdint>
#include <iostream>
#include <vector>

namespace {

/**
 * @brief Build a deterministic textured image that exercises tile boundaries.
 * @param width Requested image width.
 * @param height Requested image height.
 * @return Row-major reference image with gradients and high-frequency detail.
 */
orb_golden::Image make_test_image(int width, int height) {
    orb_golden::Image image(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const uint32_t checker = ((x / 5 + y / 7) & 1) ? 91u : 13u;
            image.at(x, y) = uint8_t((x * 17 + y * 29 + checker + (x * y) % 67) & 255);
        }
    }
    return image;
}

/**
 * @brief Compare one hardware candidate against the CPU v1.0 reference record.
 * @param actual Candidate emitted by orb_fast_stream_hls.
 * @param expected Candidate emitted by the golden tiled FAST/NMS implementation.
 * @return True when all externally visible fields are identical.
 */
bool same_point(const orb_hls::Point& actual, const orb_golden::Point& expected) {
    return actual.x == expected.x && actual.y == expected.y &&
           actual.score == expected.score && actual.tile == expected.tile;
}

/**
 * @brief Compute an FNV-1a digest over one ordered hardware candidate list.
 * @param points Candidate storage.
 * @param count Number of valid records.
 * @return Digest of x, y, score, and tile in little-endian field order.
 */
uint64_t digest_points(const std::vector<orb_hls::Point>& points, int count) {
    uint64_t hash = UINT64_C(14695981039346656037);
    for (int index = 0; index < count; ++index) {
        const orb_hls::Point& point = points[size_t(index)];
        const uint32_t fields[4] = {
            uint32_t(point.x), uint32_t(point.y),
            uint32_t(point.score), uint32_t(point.tile)};
        for (int field = 0; field < 4; ++field) {
            for (int byte = 0; byte < 4; ++byte) {
                hash ^= uint8_t(fields[field] >> (byte * 8));
                hash *= UINT64_C(1099511628211);
            }
        }
    }
    return hash;
}

} // namespace

/**
 * @brief Verify fused FAST/NMS output against the independent v1.0 CPU model.
 * @param argc Optional PGM argument count.
 * @param argv Optional PGM path used for a full-frame host regression.
 * @return Zero when high-only and dual-threshold candidate lists are bit exact.
 */
int main(int argc, char** argv) {
    if (argc > 2) return 1;
#ifdef ORB_FAST_HLS_CSIM
    const orb_golden::Image image = make_test_image(160, 120);
#else
    const orb_golden::Image image = argc == 2
        ? orb_golden::read_pgm(argv[1])
        : make_test_image(160, 120);
#endif
    const int candidate_capacity = image.width * image.height;
    const std::vector<uint64_t> packed =
        orb_hls::pack_image(image.pixels.data(), image.pixels.size());
    std::vector<orb_hls::Point> actual(
        static_cast<std::size_t>(candidate_capacity));

    for (int dual_threshold = 0; dual_threshold <= 1; ++dual_threshold) {
        orb_golden::Config config;
        config.threshold = 20;
        config.low_threshold = 7;
        config.dual_threshold = dual_threshold != 0;
#ifndef ORB_FAST_HLS_CSIM
        const std::vector<orb_golden::Point> expected =
            orb_golden::tiled_candidates(image, config);
#endif
        const int count = orb_fast_stream_hls(
            packed.data(), actual.data(), image.width, image.height,
            config.threshold, config.low_threshold, dual_threshold,
            candidate_capacity);
#ifdef ORB_FAST_HLS_CSIM
        static const int expected_count[2] = {786, 797};
        static const uint64_t expected_digest[2] = {
            UINT64_C(0x93f75503782ac41c), UINT64_C(0x40ffc0a5b2a23f7d)};
        const uint64_t actual_digest = digest_points(actual, count);
        if (count != expected_count[dual_threshold] ||
            actual_digest != expected_digest[dual_threshold]) {
            std::cerr << "TB_FAST_STREAM_FAIL mode=" << dual_threshold
                      << " actual_count=" << count
                      << " expected_count=" << expected_count[dual_threshold]
                      << " actual_digest=" << std::hex << actual_digest
                      << " expected_digest=" << expected_digest[dual_threshold]
                      << std::dec << '\n';
            return 1;
        }
#else
        if (count != int(expected.size())) {
            std::cerr << "TB_FAST_STREAM_FAIL mode=" << dual_threshold
                      << " actual_count=" << count
                      << " expected_count=" << expected.size() << '\n';
            return 1;
        }
        for (int index = 0; index < count; ++index) {
            if (!same_point(actual[size_t(index)], expected[size_t(index)])) {
                const orb_hls::Point& a = actual[size_t(index)];
                const orb_golden::Point& e = expected[size_t(index)];
                std::cerr << "TB_FAST_STREAM_FAIL mode=" << dual_threshold
                          << " index=" << index
                          << " actual=" << a.x << ',' << a.y << ',' << a.score << ',' << a.tile
                          << " expected=" << e.x << ',' << e.y << ',' << e.score << ',' << e.tile
                          << '\n';
                return 1;
            }
        }
#endif
        std::cout << "TB_FAST_STREAM_MODE mode=" << dual_threshold
                  << " count=" << count
                  << " digest=" << std::hex << digest_points(actual, count)
                  << std::dec << '\n';
    }

    std::cout << "TB_FAST_STREAM_PASS width=" << image.width
              << " height=" << image.height << '\n';
    return 0;
}
