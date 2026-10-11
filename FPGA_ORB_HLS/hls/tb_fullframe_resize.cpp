#include "orb_hls.hpp"
#include "orb_packed_image.hpp"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>

namespace {

/** @brief Compute the 64-bit FNV-1a digest used by the board checker. */
uint64_t digest(const std::vector<uint8_t>& pixels) {
    uint64_t hash = UINT64_C(14695981039346656037);
    for (uint8_t pixel : pixels) {
        hash ^= pixel;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

} // namespace

/**
 * @brief Verify all eight 960x720 pyramid images against the frozen CPU trace.
 * @param argc Argument count; one input PGM path is required.
 * @param argv Argument array containing the input PGM path.
 * @return Zero when every level is bit exact, otherwise one.
 */
int main(int argc, char** argv) {
    static const int widths[8] = {960, 800, 667, 556, 463, 386, 322, 268};
    static const int heights[8] = {720, 600, 500, 417, 347, 289, 241, 201};
    static const uint64_t expected[8] = {
        UINT64_C(0xa71a34a433733ba4), UINT64_C(0xee8d0f6e9245bcd0),
        UINT64_C(0x40b97eb940e3e7d1), UINT64_C(0xa274001b8653ee49),
        UINT64_C(0x2b3a5f5496d20e47), UINT64_C(0x96c38611b18a7266),
        UINT64_C(0x41b52a82b7356594), UINT64_C(0x248e5fc98bc6ba72),
    };
    if (argc != 2) return 1;
    std::ifstream input(argv[1], std::ios::binary);
    if (!input) return 1;
    input.seekg(15);
    std::vector<uint8_t> current(size_t(widths[0]) * size_t(heights[0]));
    input.read(reinterpret_cast<char*>(current.data()), std::streamsize(current.size()));
    if (!input || digest(current) != expected[0]) return 1;
    for (int level = 1; level < 8; ++level) {
        std::vector<uint8_t> next(size_t(widths[level]) * size_t(heights[level]));
        const std::vector<uint64_t> packed_current =
            orb_hls::pack_image(current.data(), current.size());
        std::vector<uint64_t> packed_next(orb_hls::packed_word_count(next.size()));
        const int status = orb_resize_hls(
            packed_current.data(), packed_next.data(), widths[level - 1], heights[level - 1],
            widths[level], heights[level]);
        orb_hls::unpack_image(packed_next.data(), next.data(), next.size());
        if (status != orb_hls::kOk || digest(next) != expected[level]) {
            std::cerr << "TB_FULLFRAME_RESIZE_FAIL level=" << level << '\n';
            return 1;
        }
        current.swap(next);
    }
    std::cout << "TB_FULLFRAME_RESIZE_PASS levels=8 width=960 height=720\n";
    return 0;
}
