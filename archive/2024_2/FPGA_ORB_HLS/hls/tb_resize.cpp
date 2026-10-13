#include "orb_hls.hpp"
#include "orb_packed_image.hpp"

#include <cstdint>
#include <iostream>
#include <vector>

namespace {

/**
 * @brief Compute a 64-bit FNV-1a digest over a byte range.
 * @param data First byte of the range.
 * @param size Number of bytes to include.
 * @return Deterministic digest used by the standalone RTL testbench.
 */
uint64_t fnv1a(const uint8_t* data, size_t size) {
    uint64_t hash = UINT64_C(14695981039346656037);
    for (size_t index = 0; index < size; ++index) {
        hash ^= data[index];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

} // namespace

/**
 * @brief Verify the resize HLS top against a frozen golden-output digest.
 * @return 0 when the output digest matches, otherwise 1.
 */
int main() {
    constexpr int source_width = 67;
    constexpr int source_height = 53;
    constexpr int destination_width = 55;
    constexpr int destination_height = 41;
    constexpr size_t interface_depth = ORB_HLS_IMAGE_INTERFACE_DEPTH;
    constexpr uint64_t expected_digest = UINT64_C(541945487680902820);
    std::vector<uint8_t> source(interface_depth);
    for (int y = 0; y < source_height; ++y) {
        for (int x = 0; x < source_width; ++x) {
            source[size_t(y) * size_t(source_width) + size_t(x)] =
                uint8_t((x * 37 + y * 53 + (x * y) * 3 + 17) & 255);
        }
    }
    std::vector<uint8_t> actual(interface_depth);
    const std::vector<uint64_t> packed_source =
        orb_hls::pack_image(source.data(), source.size());
    std::vector<uint64_t> packed_actual(ORB_HLS_PACKED_IMAGE_INTERFACE_DEPTH);
    const int status = orb_resize_hls(
        packed_source.data(), packed_actual.data(), source_width, source_height,
        destination_width, destination_height);
    if (status != orb_hls::kOk) {
        std::cerr << "TB_RESIZE_FAIL status=" << status << '\n';
        return 1;
    }
    const size_t valid_pixels = size_t(destination_width) * size_t(destination_height);
    orb_hls::unpack_image(packed_actual.data(), actual.data(), valid_pixels);
    const uint64_t actual_digest = fnv1a(actual.data(), valid_pixels);
    if (actual_digest != expected_digest) {
        std::cerr << "TB_RESIZE_FAIL expected_digest=" << expected_digest
                  << " actual_digest=" << actual_digest << '\n';
        return 1;
    }
    std::cout << "TB_RESIZE_PASS pixels=" << valid_pixels
              << " fnv1a=" << actual_digest << '\n';
    return 0;
}
