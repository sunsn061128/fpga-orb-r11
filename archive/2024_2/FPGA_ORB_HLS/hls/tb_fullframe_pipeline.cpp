#include "orb_hls.hpp"
#include "orb_packed_image.hpp"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>

namespace {

constexpr int kLevels = 8;
constexpr int kMaximumPixels = 960 * 720;
constexpr int kCandidateCapacity = kMaximumPixels;
constexpr int kSelectedCapacity = 1000;
constexpr int kCellCapacity = 2048;

const int kWidths[kLevels] = {960, 800, 667, 556, 463, 386, 322, 268};
const int kHeights[kLevels] = {720, 600, 500, 417, 347, 289, 241, 201};
const int kBudgets[kLevels] = {217, 181, 151, 126, 105, 87, 73, 60};
const int kExpectedCandidates[kLevels] = {3873, 2595, 1765, 1128, 776, 492, 296, 153};
const int kExpectedSelected[kLevels] = {64, 45, 44, 38, 29, 24, 19, 17};
const uint64_t kExpectedImageDigests[kLevels] = {
    UINT64_C(0xa71a34a433733ba4), UINT64_C(0xee8d0f6e9245bcd0),
    UINT64_C(0x40b97eb940e3e7d1), UINT64_C(0xa274001b8653ee49),
    UINT64_C(0x2b3a5f5496d20e47), UINT64_C(0x96c38611b18a7266),
    UINT64_C(0x41b52a82b7356594), UINT64_C(0x248e5fc98bc6ba72),
};
const uint64_t kExpectedLevelDigests[kLevels] = {
    UINT64_C(0xbd5edebf6facc24c), UINT64_C(0xd2dffa4ce2dd9573),
    UINT64_C(0x96d9a8b4270222e0), UINT64_C(0x674589769a093721),
    UINT64_C(0x73468edd91c566c3), UINT64_C(0x3000603e34597da1),
    UINT64_C(0xa397d47b3233ca46), UINT64_C(0xdeed28aaa79d5f29),
};

/**
 * @brief Append one byte to a 64-bit FNV-1a digest.
 * @param hash Digest state to update.
 * @param value Byte value to append.
 */
void hash_byte(uint64_t& hash, uint8_t value) {
    hash ^= value;
    hash *= UINT64_C(1099511628211);
}

/**
 * @brief Append one 32-bit value in explicit little-endian order.
 * @param hash Digest state to update.
 * @param value Value whose four bytes are appended.
 */
void hash_u32(uint64_t& hash, uint32_t value) {
    for (int byte = 0; byte < 4; ++byte) hash_byte(hash, uint8_t(value >> (8 * byte)));
}

/**
 * @brief Compute the board-check digest of an active image prefix.
 * @param image Row-major image storage.
 * @param pixels Number of active pixels.
 * @return Deterministic 64-bit FNV-1a digest.
 */
uint64_t digest_image(const std::vector<uint8_t>& image, size_t pixels) {
    uint64_t hash = UINT64_C(14695981039346656037);
    for (size_t index = 0; index < pixels; ++index) hash_byte(hash, image[index]);
    return hash;
}

/**
 * @brief Digest every externally visible result of one HLS level transaction.
 * @param pixel_work Four-region pixel workspace containing the score image.
 * @param gaussian Gaussian-filtered image storage.
 * @param point_work Packed candidate, selected, and grid-winner workspace.
 * @param features Selected feature records followed by the metadata trailer.
 * @param pixels Number of active image pixels.
 * @param feature_capacity Index of the metadata trailer.
 * @return Deterministic digest matching the PS and CPU-reference checkers.
 */
uint64_t digest_level(const std::vector<uint16_t>& pixel_work,
                      const uint16_t* gaussian,
                      const std::vector<orb_hls::Point>& point_work,
                      const std::vector<orb_hls::Feature>& features,
                      size_t pixels, int feature_capacity) {
    const orb_hls::Feature& metadata = features[size_t(feature_capacity)];
    const orb_hls::Point* const candidates = point_work.data();
    const orb_hls::Point* const selected = candidates + kCandidateCapacity;
    uint64_t hash = UINT64_C(14695981039346656037);
    for (size_t index = 0; index < pixels; ++index) hash_byte(hash, uint8_t(pixel_work[index]));
    for (size_t index = 0; index < pixels; ++index) hash_byte(hash, uint8_t(gaussian[index]));
    hash_u32(hash, uint32_t(metadata.x));
    hash_u32(hash, uint32_t(metadata.y));
    hash_u32(hash, uint32_t(metadata.score));
    hash_u32(hash, uint32_t(metadata.level));
    hash_u32(hash, uint32_t(metadata.bin));
    hash_u32(hash, uint32_t(metadata.m10));
    hash_u32(hash, uint32_t(metadata.m01));
    hash_u32(hash, metadata.angle_q16);
    hash_u32(hash, metadata.descriptor0);
    hash_u32(hash, metadata.descriptor1);
    for (int index = 0; index < metadata.x; ++index) {
        const orb_hls::Point& point = candidates[index];
        hash_u32(hash, uint32_t(point.x));
        hash_u32(hash, uint32_t(point.y));
        hash_u32(hash, uint32_t(point.score));
        hash_u32(hash, uint32_t(point.tile));
    }
    for (int index = 0; index < metadata.y; ++index) {
        const orb_hls::Point& point = selected[index];
        const orb_hls::Feature& feature = features[size_t(index)];
        hash_u32(hash, uint32_t(point.x));
        hash_u32(hash, uint32_t(point.y));
        hash_u32(hash, uint32_t(point.score));
        hash_u32(hash, uint32_t(point.tile));
        hash_u32(hash, uint32_t(feature.x));
        hash_u32(hash, uint32_t(feature.y));
        hash_u32(hash, uint32_t(feature.score));
        hash_u32(hash, uint32_t(feature.level));
        hash_u32(hash, uint32_t(feature.bin));
        hash_u32(hash, uint32_t(feature.m10));
        hash_u32(hash, uint32_t(feature.m01));
        hash_u32(hash, feature.angle_q16);
        hash_u32(hash, feature.descriptor0);
        hash_u32(hash, feature.descriptor1);
        hash_u32(hash, feature.descriptor2);
        hash_u32(hash, feature.descriptor3);
        hash_u32(hash, feature.descriptor4);
        hash_u32(hash, feature.descriptor5);
        hash_u32(hash, feature.descriptor6);
        hash_u32(hash, feature.descriptor7);
    }
    return hash;
}

} // namespace

/**
 * @brief Verify the complete 960x720, eight-level resize and ORB pipeline.
 * @param argc Argument count; the normal PGM and an optional fallback-grid PGM are accepted.
 * @param argv Argument array containing the normal and optional fallback-grid PGM paths.
 * @return Zero when all requested counts and digests are bit exact, otherwise one.
 */
int main(int argc, char** argv) {
    if (argc != 2 && argc != 3) return 1;
    std::ifstream input(argv[1], std::ios::binary);
    if (!input) return 1;
    input.seekg(15);

    std::vector<uint8_t> current(kMaximumPixels);
    input.read(reinterpret_cast<char*>(current.data()), kMaximumPixels);
    if (!input) return 1;
    std::vector<uint8_t> next(kMaximumPixels);
    std::vector<uint16_t> pixel_work(size_t(4) * kMaximumPixels);
    uint16_t* const gaussian = pixel_work.data() + size_t(2) * kMaximumPixels;
    std::vector<orb_hls::Point> point_work(
        size_t(kCandidateCapacity + kSelectedCapacity + 2 * kCellCapacity));
    std::vector<orb_hls::Feature> features(size_t(kSelectedCapacity + 1));

    for (int level = 0; level < kLevels; ++level) {
        const size_t pixels = size_t(kWidths[level]) * size_t(kHeights[level]);
        if (level != 0) {
            const size_t next_pixels = size_t(kWidths[level]) * size_t(kHeights[level]);
            const size_t current_pixels =
                size_t(kWidths[level - 1]) * size_t(kHeights[level - 1]);
            const std::vector<uint64_t> packed_current =
                orb_hls::pack_image(current.data(), current_pixels);
            std::vector<uint64_t> packed_next(orb_hls::packed_word_count(next_pixels));
            const int resize_status = orb_resize_hls(
                packed_current.data(), packed_next.data(), kWidths[level - 1], kHeights[level - 1],
                kWidths[level], kHeights[level]);
            if (resize_status != orb_hls::kOk) return 1;
            orb_hls::unpack_image(packed_next.data(), next.data(), next_pixels);
            current.swap(next);
        }
        const uint64_t image_digest = digest_image(current, pixels);
        if (image_digest != kExpectedImageDigests[level]) {
            std::cerr << "TB_FULLFRAME_PIPELINE_FAIL image level=" << level << '\n';
            return 1;
        }

        const int status = orb_level_hls(
            current.data(), pixel_work.data(), gaussian, point_work.data(), features.data(),
            kWidths[level], kHeights[level], kBudgets[level], 20, 7, 1, level,
            kMaximumPixels, kCandidateCapacity, kSelectedCapacity, kCellCapacity,
            kBudgets[level]);
        if (status != orb_hls::kOk) {
            std::cerr << "TB_FULLFRAME_PIPELINE_FAIL status=" << status
                      << " level=" << level << '\n';
            return 1;
        }
        const orb_hls::Feature& metadata = features[size_t(kBudgets[level])];
        const uint64_t level_digest = digest_level(
            pixel_work, gaussian, point_work, features, pixels, kBudgets[level]);
        if (metadata.x != kExpectedCandidates[level] ||
            metadata.y != kExpectedSelected[level] ||
            level_digest != kExpectedLevelDigests[level]) {
            std::cerr << "TB_FULLFRAME_PIPELINE_FAIL level=" << level
                      << " candidates=" << metadata.x
                      << " selected=" << metadata.y
                      << " digest=0x" << std::hex << level_digest << std::dec << '\n';
            return 1;
        }
        std::cout << "level=" << level << " candidates=" << metadata.x
                  << " selected=" << metadata.y << " PASS\n";
    }
    if (argc == 3) {
        constexpr int fallback_budget = 1086;
        constexpr int fallback_candidates = 14170;
        constexpr int fallback_selected = 849;
        constexpr uint64_t fallback_digest = UINT64_C(0x00c5234be3a1e5d0);
        std::ifstream fallback_input(argv[2], std::ios::binary);
        if (!fallback_input) return 1;
        fallback_input.seekg(15);
        fallback_input.read(reinterpret_cast<char*>(current.data()), kMaximumPixels);
        if (!fallback_input) return 1;
        std::vector<orb_hls::Feature> fallback_features(size_t(fallback_budget + 1));
        const int status = orb_level_hls(
            current.data(), pixel_work.data(), gaussian, point_work.data(), fallback_features.data(),
            kWidths[0], kHeights[0], fallback_budget, 20, 7, 1, 0,
            kMaximumPixels, kCandidateCapacity, kSelectedCapacity, kCellCapacity,
            fallback_budget);
        if (status != orb_hls::kOk) {
            std::cerr << "TB_FULLFRAME_PIPELINE_FAIL fallback_status=" << status << '\n';
            return 1;
        }
        const orb_hls::Feature& metadata = fallback_features[size_t(fallback_budget)];
        const uint64_t observed = digest_level(
            pixel_work, gaussian, point_work, fallback_features,
            size_t(kMaximumPixels), fallback_budget);
        if (metadata.x != fallback_candidates || metadata.y != fallback_selected ||
            observed != fallback_digest) {
            std::cerr << "TB_FULLFRAME_PIPELINE_FAIL fallback candidates=" << metadata.x
                      << " selected=" << metadata.y << " digest=0x"
                      << std::hex << observed << std::dec << '\n';
            return 1;
        }
        std::cout << "fallback_grid cells=1064 candidates=" << metadata.x
                  << " selected=" << metadata.y << " PASS\n";
    }
    std::cout << "TB_FULLFRAME_PIPELINE_PASS levels=8 width=960 height=720\n";
    return 0;
}
