#include "orb_hls.hpp"

#include <cstdint>
#include <iostream>
#include <vector>

namespace {

/**
 * @brief Add one byte to a 64-bit FNV-1a digest.
 * @param hash Digest state to update.
 * @param value Byte value to append.
 */
void hash_byte(uint64_t& hash, uint8_t value) {
    hash ^= value;
    hash *= UINT64_C(1099511628211);
}

/**
 * @brief Add one 32-bit value to a digest in explicit little-endian order.
 * @param hash Digest state to update.
 * @param value Value whose four bytes are appended.
 */
void hash_u32(uint64_t& hash, uint32_t value) {
    for (int byte = 0; byte < 4; ++byte) hash_byte(hash, uint8_t(value >> (8 * byte)));
}

/**
 * @brief Digest every externally visible result of one HLS level transaction.
 * @param score FAST score image.
 * @param gaussian Gaussian-filtered image.
 * @param candidates Candidate records; metadata.x selects the valid prefix.
 * @param selected Selected records; metadata.y selects the valid prefix.
 * @param features Feature records followed by the metadata trailer.
 * @param metadata Metadata trailer stored at features[feature_capacity].
 * @param valid_pixels Number of valid score and Gaussian pixels.
 * @return Deterministic 64-bit FNV-1a digest.
 */
uint64_t digest_outputs(const uint16_t* score, const uint16_t* gaussian,
                        const orb_hls::Point* candidates,
                        const orb_hls::Point* selected,
                        const orb_hls::Feature* features,
                        const orb_hls::Feature& metadata, size_t valid_pixels) {
    uint64_t hash = UINT64_C(14695981039346656037);
    for (size_t index = 0; index < valid_pixels; ++index) hash_byte(hash, uint8_t(score[index]));
    for (size_t index = 0; index < valid_pixels; ++index) hash_byte(hash, uint8_t(gaussian[index]));
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
        const orb_hls::Point& point = candidates[size_t(index)];
        hash_u32(hash, uint32_t(point.x));
        hash_u32(hash, uint32_t(point.y));
        hash_u32(hash, uint32_t(point.score));
        hash_u32(hash, uint32_t(point.tile));
    }
    for (int index = 0; index < metadata.y; ++index) {
        const orb_hls::Point& point = selected[size_t(index)];
        hash_u32(hash, uint32_t(point.x));
        hash_u32(hash, uint32_t(point.y));
        hash_u32(hash, uint32_t(point.score));
        hash_u32(hash, uint32_t(point.tile));
        const orb_hls::Feature& feature = features[size_t(index)];
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
 * @brief Verify the complete one-level T2 HLS top against a frozen golden-output digest.
 * @return 0 when counts and the complete output digest match; otherwise 1.
 */
int main() {
    constexpr int width = 96;
    constexpr int height = 80;
    constexpr int budget = 40;
    constexpr int level_index = 2;
    constexpr size_t image_interface_depth = ORB_HLS_IMAGE_INTERFACE_DEPTH;
    constexpr size_t feature_interface_depth = ORB_HLS_FEATURE_INTERFACE_DEPTH;
    constexpr int expected_candidates = 262;
    constexpr int expected_selected = 40;
    constexpr uint64_t expected_digest = UINT64_C(7505441316282489474);
    std::vector<uint8_t> image(image_interface_depth);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int checker = ((x / 5 + y / 7) & 1) ? 109 : 0;
            image[size_t(y) * size_t(width) + size_t(x)] =
                uint8_t((x * 29 + y * 47 + x * y + checker + 31) & 255);
        }
    }

    const size_t pixels = size_t(width) * size_t(height);
    std::vector<uint16_t> pixel_work(4u * image_interface_depth);
    std::vector<orb_hls::Point> point_work(4u * image_interface_depth);
    uint16_t* const score = pixel_work.data();
    uint16_t* const gaussian = pixel_work.data() + 2u * pixels;
    orb_hls::Point* const candidates = point_work.data();
    orb_hls::Point* const selected = point_work.data() + pixels;
    std::vector<orb_hls::Feature> features(feature_interface_depth);
    const int status = orb_level_hls(
        image.data(), pixel_work.data(), gaussian, point_work.data(), features.data(),
        width, height, budget, 20, 7, 1, level_index,
        int(pixels), int(pixels), int(pixels), int(pixels), budget);
    if (status != orb_hls::kOk) {
        std::cerr << "TB_LEVEL_FAIL status=" << status << '\n';
        return 1;
    }

    const orb_hls::Feature& metadata = features[size_t(budget)];
    if (metadata.x != expected_candidates || metadata.y != expected_selected ||
        metadata.angle_q16 != orb_hls::kAlgorithmVersion ||
        (metadata.descriptor0 & orb_hls::kCapabilityDualThreshold) == 0 ||
        metadata.descriptor1 != uint32_t(orb_hls::kMaxGridTopK)) {
        std::cerr << "TB_LEVEL_FAIL candidates=" << metadata.x
                  << " selected=" << metadata.y
                  << " algorithm_version=" << metadata.angle_q16
                  << " capabilities=" << metadata.descriptor0
                  << " max_K=" << metadata.descriptor1 << '\n';
        return 1;
    }
    const uint64_t actual_digest =
        digest_outputs(score, gaussian, candidates, selected, features.data(), metadata, pixels);
    if (actual_digest != expected_digest) {
        std::cerr << "TB_LEVEL_FAIL expected_digest=" << expected_digest
                  << " actual_digest=" << actual_digest << '\n';
        return 1;
    }
    std::cout << "TB_LEVEL_PASS candidates=" << metadata.x
              << " selected=" << metadata.y
              << " algorithm_version=1.0 dual_threshold_supported=1 max_K="
              << metadata.descriptor1 << " fnv1a=" << actual_digest << '\n';
    return 0;
}
