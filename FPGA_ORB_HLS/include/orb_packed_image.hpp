#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace orb_hls {

/**
 * @brief Return the number of 64-bit words required for a packed byte image.
 * @param pixel_count Number of row-major 8-bit pixels.
 * @return Ceiling of pixel_count divided by eight.
 */
inline std::size_t packed_word_count(std::size_t pixel_count) {
    return (pixel_count + 7u) / 8u;
}

/**
 * @brief Pack row-major pixels into little-endian 64-bit AXI words.
 * @param pixels First source pixel.
 * @param pixel_count Number of source pixels.
 * @return Packed words with unused final lanes cleared to zero.
 */
inline std::vector<uint64_t> pack_image(const uint8_t* pixels, std::size_t pixel_count) {
    std::vector<uint64_t> words(packed_word_count(pixel_count), UINT64_C(0));
    for (std::size_t index = 0; index < pixel_count; ++index) {
        words[index >> 3] |= uint64_t(pixels[index]) << ((index & 7u) * 8u);
    }
    return words;
}

/**
 * @brief Unpack little-endian 64-bit AXI words into row-major pixels.
 * @param words First packed source word.
 * @param pixels First destination pixel.
 * @param pixel_count Number of pixels to unpack.
 */
inline void unpack_image(const uint64_t* words, uint8_t* pixels, std::size_t pixel_count) {
    for (std::size_t index = 0; index < pixel_count; ++index) {
        pixels[index] = uint8_t(words[index >> 3] >> ((index & 7u) * 8u));
    }
}

} // namespace orb_hls
