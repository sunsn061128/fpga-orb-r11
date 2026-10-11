#include "orb_hls.hpp"
#include "constants.hpp"

#include <cstdint>

namespace {

using orb_hls::Feature;
using orb_hls::Point;

// A FAST tile is at most 69 pixels wide or high, plus a three-pixel halo on
// each side. Keeping this bound explicit lets HLS implement the NMS line cache
// as fixed-size on-chip memories rather than an external-memory scratch buffer.
constexpr int kMaxNmsTileExtent = 75;
// The 960x720 normal profile uses at most 204 grid cells per level. A 256-cell
// packed cache accelerates that target while the external-memory path retains
// the full generic capacity contract for initialization and stress profiles.
constexpr int kOnChipGridCells = 256;

/**
 * @brief Pack one nonnegative effective-coordinate point into 41 bits.
 * @param point Point whose coordinates and tile index obey the top-level bounds.
 * @return Compact x, y, score, and tile representation for the grid cache.
 */
uint64_t pack_grid_point(const Point& point) {
    return (uint64_t(uint32_t(point.x) & 0x7ffu)) |
           (uint64_t(uint32_t(point.y) & 0x7ffu) << 11) |
           (uint64_t(uint32_t(point.score) & 0xffu) << 22) |
           (uint64_t(uint32_t(point.tile) & 0x7ffu) << 30);
}

/**
 * @brief Restore one point from the compact on-chip grid representation.
 * @param packed Compact value produced by pack_grid_point.
 * @return Point with the original effective coordinates, score, and tile.
 */
Point unpack_grid_point(uint64_t packed) {
    Point point;
    point.x = int32_t(packed & 0x7ffu);
    point.y = int32_t((packed >> 11) & 0x7ffu);
    point.score = int32_t((packed >> 22) & 0xffu);
    point.tile = int32_t((packed >> 30) & 0x7ffu);
    return point;
}

/**
 * @brief Divide a signed integer using round-to-nearest, ties-to-even.
 * @param value Signed numerator.
 * @param divisor Strictly positive denominator.
 * @return Rounded signed quotient.
 */
int64_t round_even_hw(int64_t value, int64_t divisor) {
    const bool negative = value < 0;
    const uint64_t magnitude = negative ? uint64_t(-(value + 1)) + 1u : uint64_t(value);
    const uint64_t d = uint64_t(divisor);
    uint64_t quotient = magnitude / d;
    const uint64_t remainder = magnitude % d;
    if (remainder > d - remainder || (remainder == d - remainder && (quotient & 1u))) {
        ++quotient;
    }
    return negative ? -int64_t(quotient) : int64_t(quotient);
}

/**
 * @brief Compute the floor of the unsigned integer square root.
 * @param value Unsigned radicand.
 * @return Largest integer whose square does not exceed value.
 */
uint64_t isqrt_hw(uint64_t value) {
    uint64_t result = 0;
    uint64_t bit = uint64_t(1) << 62;
    while (bit > value) bit >>= 2;
    while (bit != 0) {
        if (value >= result + bit) {
            value -= result + bit;
            result = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
        bit >>= 2;
    }
    return result;
}

/**
 * @brief Map an out-of-range coordinate with OpenCV BORDER_REFLECT_101 semantics.
 * @param coordinate Coordinate that may lie outside the image interval.
 * @param size Positive image-axis extent.
 * @return Reflected coordinate in the range 0..size-1.
 */
int reflect101_hw(int coordinate, int size) {
    if (size == 1) return 0;
    int value = coordinate;
    for (int iteration = 0; iteration < 4; ++iteration) {
        if (value < 0) value = -value;
        else if (value >= size) value = 2 * size - value - 2;
    }
    return value;
}

struct Axis {
    int a;
    int b;
    int weight;
};

/**
 * @brief Build the two source indices and Q0.11 interpolation weight for one output coordinate.
 * @param target Destination-axis coordinate.
 * @param source_size Source-axis extent.
 * @param destination_size Destination-axis extent.
 * @return Replicated-edge source indices and a weight in the range 0..2048.
 */
Axis resize_axis(int target, int source_size, int destination_size) {
    const int64_t numerator = int64_t(2 * target + 1) * source_size - destination_size;
    const int64_t denominator = int64_t(2) * destination_size;
    int64_t a = numerator / denominator;
    int64_t remainder = numerator % denominator;
    if (remainder < 0) {
        --a;
        remainder += denominator;
    }
    if (a < 0) return {0, 0, 0};
    if (a >= source_size - 1) return {source_size - 1, source_size - 1, 0};
    return {int(a), int(a + 1), int(round_even_hw(remainder * 2048, denominator))};
}

/**
 * @brief Compare two effective-coordinate points in raster order.
 * @param a Left comparison operand.
 * @param b Right comparison operand.
 * @return True when a precedes b by y and then x.
 */
bool raster_before(const Point& a, const Point& b) {
    return a.y < b.y || (a.y == b.y && a.x < b.x);
}

/**
 * @brief Compare points by descending score with raster-order tie breaking.
 * @param a Left comparison operand.
 * @param b Right comparison operand.
 * @return True when a must be emitted before b.
 */
bool stronger(const Point& a, const Point& b) {
    return a.score > b.score || (a.score == b.score && raster_before(a, b));
}

/**
 * @brief Perform a deterministic signed arithmetic right shift.
 * @param value Signed value to shift.
 * @param amount Shift amount in the range 0..31.
 * @return value divided by 2^amount with negative results rounded toward minus infinity.
 */
int64_t signed_shift(int64_t value, int amount) {
    if (amount == 0) return value;
    const int64_t divisor = int64_t(1) << amount;
    return value >= 0 ? value / divisor : -((-value + divisor - 1) / divisor);
}

/**
 * @brief Convert IC moments to an unsigned Q16.16 orientation in degrees.
 * @param m10 Horizontal intensity moment.
 * @param m01 Vertical intensity moment.
 * @return Angle in [0,360) degrees encoded as unsigned Q16.16.
 */
uint32_t angle_q16_hw(int32_t m10, int32_t m01) {
    if (m10 == 0 && m01 == 0) return 0;
    if (m01 == 0) return m10 < 0 ? 180u * 65536u : 0u;
    if (m10 == 0) return m01 < 0 ? 270u * 65536u : 90u * 65536u;
    int64_t x = int64_t(m10) * (int64_t(1) << 20);
    int64_t y = int64_t(m01) * (int64_t(1) << 20);
    int64_t z = 0;
    if (x < 0) {
        x = -x;
        y = -y;
        z = int64_t(180) << 24;
    }
    for (int k = 0; k < 32 && y != 0; ++k) {
        const int64_t old_x = x;
        if (y > 0) {
            x += signed_shift(y, k);
            y -= signed_shift(old_x, k);
            z += orb_golden::cordic_degrees_q24[k];
        } else {
            x -= signed_shift(y, k);
            y += signed_shift(old_x, k);
            z -= orb_golden::cordic_degrees_q24[k];
        }
    }
    const int64_t full = int64_t(360) << 24;
    z = (z % full + full) % full;
    return uint32_t(round_even_hw(z, 256) % (int64_t(360) << 16));
}

/**
 * @brief Quantize a Q16.16 angle to one of 32 circular orientation bins.
 * @param angle Angle in unsigned Q16.16 degrees.
 * @return Bin index in the range 0..31.
 */
int quantize32(uint32_t angle) {
    const uint64_t full = uint64_t(360) << 16;
    return int((uint64_t(angle) * 32u + full / 2u) / full % 32u);
}

/**
 * @brief Compute ORB intensity-centroid moments over the radius-15 circular patch.
 * @param image Row-major raw layer pixels.
 * @param width Layer stride in pixels.
 * @param x Patch-center x coordinate, at least 15 pixels from either edge.
 * @param y Patch-center y coordinate, at least 15 pixels from either edge.
 * @param m10 Output horizontal moment.
 * @param m01 Output vertical moment.
 */
void compute_moments(const uint8_t* image, int width, int x, int y, int32_t& m10, int32_t& m01) {
    m10 = 0;
    m01 = 0;
    for (int u = -15; u <= 15; ++u) m10 += u * int(image[y * width + x + u]);
    for (int v = 1; v <= 15; ++v) {
        int vertical_sum = 0;
        for (int u = -orb_golden::umax[v]; u <= orb_golden::umax[v]; ++u) {
            const int plus = image[(y + v) * width + x + u];
            const int minus = image[(y - v) * width + x + u];
            vertical_sum += plus - minus;
            m10 += u * (plus + minus);
        }
        m01 += v * vertical_sum;
    }
}

/**
 * @brief Sample one rotated BRIEF pattern coordinate from the blurred layer.
 * @param gaussian Row-major 16-bit work region containing 8-bit Gaussian values.
 * @param width Layer stride in pixels.
 * @param x0 Descriptor-center x coordinate.
 * @param y0 Descriptor-center y coordinate.
 * @param pattern_index Index of an x,y pair in the ORB pattern table.
 * @param cosine Cosine of the quantized angle in signed Q1.30.
 * @param sine Sine of the quantized angle in signed Q1.30.
 * @return Sampled grayscale value.
 */
uint8_t rotated_sample(const uint16_t* gaussian, int width, int x0, int y0, int pattern_index,
                       int64_t cosine, int64_t sine) {
#pragma HLS INLINE
    const int x = orb_golden::pattern[pattern_index];
    const int y = orb_golden::pattern[pattern_index + 1];
    const int dx = int(round_even_hw(x * cosine - y * sine, int64_t(1) << 30));
    const int dy = int(round_even_hw(x * sine + y * cosine, int64_t(1) << 30));
    return uint8_t(gaussian[(y0 + dy) * width + x0 + dx]);
}

/**
 * @brief Compute the bit-exact FAST response from the 16 radius-three ring samples.
 * @param center Center-pixel intensity.
 * @param ring Clockwise ring samples in the same order as the reference implementation.
 * @return Reference response before the public score's minus-one adjustment.
 *
 * The fixed loop bounds let HLS build the comparison network without issuing any
 * external-memory reads. Keeping the original min/max order preserves exact output.
 */
int fast_response(uint8_t center, const uint8_t ring[16]) {
#pragma HLS INLINE
#pragma HLS ARRAY_PARTITION variable=ring complete dim=1
    int difference[25];
#pragma HLS ARRAY_PARTITION variable=difference complete dim=1
    for (int i = 0; i < 16; ++i) {
#pragma HLS UNROLL
        difference[i] = int(ring[i]) - int(center);
    }
    for (int i = 16; i < 25; ++i) {
#pragma HLS UNROLL
        difference[i] = difference[i - 16];
    }

    int response_by_start[16];
#pragma HLS ARRAY_PARTITION variable=response_by_start complete dim=1
    for (int start = 0; start < 16; ++start) {
#pragma HLS UNROLL
        int low = 255;
        int high = -255;
        for (int offset = 0; offset < 9; ++offset) {
#pragma HLS UNROLL
            const int value = difference[start + offset];
            if (value < low) low = value;
            if (value > high) high = value;
        }
        response_by_start[start] = low > -high ? low : -high;
    }

    int best = 0;
    for (int start = 0; start < 16; ++start) {
#pragma HLS UNROLL
        if (response_by_start[start] > best) best = response_by_start[start];
    }
    return best;
}

/**
 * @brief Apply the exact seven-tap horizontal Gaussian kernel.
 * @param samples Seven reflected 8-bit source samples ordered left to right.
 * @return Unnormalized 16-bit horizontal intermediate used by the reference.
 */
uint16_t gaussian_horizontal_value(const uint8_t samples[7]) {
#pragma HLS INLINE
#pragma HLS ARRAY_PARTITION variable=samples complete dim=1
    static const uint8_t kernel[7] = {18, 34, 48, 56, 48, 34, 18};
    uint32_t sum = 0;
    for (int tap = 0; tap < 7; ++tap) {
#pragma HLS UNROLL
        sum += uint32_t(kernel[tap]) * samples[tap];
    }
    return uint16_t(sum);
}

/**
 * @brief Apply the exact seven-tap vertical Gaussian kernel and normalization.
 * @param samples Seven 16-bit horizontal intermediates ordered top to bottom.
 * @return Final blurred 8-bit value stored in a 16-bit work element.
 */
uint16_t gaussian_vertical_value(const uint16_t samples[7]) {
#pragma HLS INLINE
#pragma HLS ARRAY_PARTITION variable=samples complete dim=1
    static const uint8_t kernel[7] = {18, 34, 48, 56, 48, 34, 18};
    uint32_t sum = 0;
    for (int tap = 0; tap < 7; ++tap) {
#pragma HLS UNROLL
        sum += uint32_t(kernel[tap]) * samples[tap];
    }
    return uint16_t((sum + 32768u) >> 16);
}

} // namespace

/** @copydoc orb_resize_hls */
extern "C" int orb_resize_hls(const uint64_t* source, uint64_t* destination,
                              int source_width, int source_height,
                              int destination_width, int destination_height) {
#ifdef ORB_HLS_COSIM_SMALL_DEPTH
#pragma HLS INTERFACE m_axi port=source offset=slave bundle=gmem0 depth=960
#pragma HLS INTERFACE m_axi port=destination offset=slave bundle=gmem1 depth=960
#else
#pragma HLS INTERFACE m_axi port=source offset=slave bundle=gmem0 depth=259200
#pragma HLS INTERFACE m_axi port=destination offset=slave bundle=gmem1 depth=259200
#endif
#pragma HLS INTERFACE s_axilite port=source bundle=control
#pragma HLS INTERFACE s_axilite port=destination bundle=control
#pragma HLS INTERFACE s_axilite port=source_width bundle=control
#pragma HLS INTERFACE s_axilite port=source_height bundle=control
#pragma HLS INTERFACE s_axilite port=destination_width bundle=control
#pragma HLS INTERFACE s_axilite port=destination_height bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control
    if (source_width <= 0 || source_height <= 0 || destination_width <= 0 || destination_height <= 0 ||
        source_width > orb_hls::kMaxWidth || source_height > orb_hls::kMaxHeight ||
        destination_width > orb_hls::kMaxWidth || destination_height > orb_hls::kMaxHeight) {
        return orb_hls::kInvalidArgument;
    }
    Axis horizontal_axes[orb_hls::kMaxWidth];
    Axis vertical_axes[orb_hls::kMaxHeight];
    uint64_t source_rows[2][(orb_hls::kMaxWidth + 14) / 8];
#pragma HLS ARRAY_PARTITION variable=source_rows complete dim=1
#pragma HLS RESOURCE variable=horizontal_axes core=RAM_2P_BRAM
#pragma HLS RESOURCE variable=vertical_axes core=RAM_2P_BRAM
#pragma HLS RESOURCE variable=source_rows core=RAM_2P_BRAM

    // Axis division depends only on the coordinate and is therefore computed once
    // per transaction instead of once per destination pixel row.
ResizeHorizontalAxis:
    for (int x = 0; x < destination_width; ++x) {
        horizontal_axes[x] = resize_axis(x, source_width, destination_width);
    }
ResizeVerticalAxis:
    for (int y = 0; y < destination_height; ++y) {
        vertical_axes[y] = resize_axis(y, source_height, destination_height);
    }

    int cached_row0 = -1;
    int cached_row1 = -1;
    uint8_t output_pack[8] = {};
#pragma HLS ARRAY_PARTITION variable=output_pack complete dim=1
    ResizeRows:
    for (int y = 0; y < destination_height; ++y) {
        const Axis vertical = vertical_axes[y];
        int bank_a = cached_row0 == vertical.a ? 0 : (cached_row1 == vertical.a ? 1 : -1);
        int bank_b = cached_row0 == vertical.b ? 0 : (cached_row1 == vertical.b ? 1 : -1);

        // Preserve a cached row needed by the other vertical tap. Otherwise,
        // replace either bank and stream the missing source row exactly once.
        if (bank_a < 0) {
            bank_a = bank_b == 0 ? 1 : 0;
            const int row_base = vertical.a * source_width;
            const int first_word = row_base >> 3;
            const int final_word = (row_base + source_width + 7) >> 3;
        LoadResizeRowA:
            for (int word_index = first_word; word_index < final_word; ++word_index) {
#pragma HLS PIPELINE II=1
                source_rows[bank_a][word_index - first_word] = source[word_index];
            }
            if (bank_a == 0) cached_row0 = vertical.a;
            else cached_row1 = vertical.a;
        }
        if (vertical.b == vertical.a) {
            bank_b = bank_a;
        } else {
            bank_b = cached_row0 == vertical.b ? 0 : (cached_row1 == vertical.b ? 1 : -1);
        }
        if (bank_b < 0) {
            bank_b = bank_a == 0 ? 1 : 0;
            const int row_base = vertical.b * source_width;
            const int first_word = row_base >> 3;
            const int final_word = (row_base + source_width + 7) >> 3;
        LoadResizeRowB:
            for (int word_index = first_word; word_index < final_word; ++word_index) {
#pragma HLS PIPELINE II=1
                source_rows[bank_b][word_index - first_word] = source[word_index];
            }
            if (bank_b == 0) cached_row0 = vertical.b;
            else cached_row1 = vertical.b;
        }
    InterpolateResizeRow:
        for (int x = 0; x < destination_width; ++x) {
#pragma HLS PIPELINE II=1
            const Axis horizontal = horizontal_axes[x];
            const int row_a_base = vertical.a * source_width;
            const int row_b_base = vertical.b * source_width;
            const int row_a_first_word = row_a_base >> 3;
            const int row_b_first_word = row_b_base >> 3;
            const int row_a_global_a = row_a_base + horizontal.a;
            const int row_a_global_b = row_a_base + horizontal.b;
            const int row_b_global_a = row_b_base + horizontal.a;
            const int row_b_global_b = row_b_base + horizontal.b;
            const uint64_t bank0_word_a = source_rows[0][
                ((bank_a == 0 ? row_a_global_a : row_b_global_a) >> 3) -
                (bank_a == 0 ? row_a_first_word : row_b_first_word)];
            const uint64_t bank0_word_b = source_rows[0][
                ((bank_a == 0 ? row_a_global_b : row_b_global_b) >> 3) -
                (bank_a == 0 ? row_a_first_word : row_b_first_word)];
            const uint64_t bank1_word_a = source_rows[1][
                ((bank_b == 1 ? row_b_global_a : row_a_global_a) >> 3) -
                (bank_b == 1 ? row_b_first_word : row_a_first_word)];
            const uint64_t bank1_word_b = source_rows[1][
                ((bank_b == 1 ? row_b_global_b : row_a_global_b) >> 3) -
                (bank_b == 1 ? row_b_first_word : row_a_first_word)];
            const uint8_t bank0_a = uint8_t(
                bank0_word_a >> (((bank_a == 0 ? row_a_global_a : row_b_global_a) & 7) * 8));
            const uint8_t bank0_b = uint8_t(
                bank0_word_b >> (((bank_a == 0 ? row_a_global_b : row_b_global_b) & 7) * 8));
            const uint8_t bank1_a = uint8_t(
                bank1_word_a >> (((bank_b == 1 ? row_b_global_a : row_a_global_a) & 7) * 8));
            const uint8_t bank1_b = uint8_t(
                bank1_word_b >> (((bank_b == 1 ? row_b_global_b : row_a_global_b) & 7) * 8));
            const uint8_t row_a0 = bank_a == 0 ? bank0_a : bank1_a;
            const uint8_t row_a1 = bank_a == 0 ? bank0_b : bank1_b;
            const uint8_t row_b0 = bank_b == 0 ? bank0_a : bank1_a;
            const uint8_t row_b1 = bank_b == 0 ? bank0_b : bank1_b;
            const int64_t row0 = int64_t(row_a0) * (2048 - horizontal.weight) +
                                 int64_t(row_a1) * horizontal.weight;
            const int64_t row1 = int64_t(row_b0) * (2048 - horizontal.weight) +
                                 int64_t(row_b1) * horizontal.weight;
            const int64_t value = ((row0 >> 4) * (2048 - vertical.weight) >> 16) +
                                  ((row1 >> 4) * vertical.weight >> 16);
            const int destination_index = y * destination_width + x;
            const int destination_lane = destination_index & 7;
            output_pack[destination_lane] = uint8_t((value + 2) >> 2);
            if (destination_lane == 7 ||
                destination_index + 1 == destination_width * destination_height) {
                uint64_t packed = 0;
                for (int lane = 0; lane < 8; ++lane) {
#pragma HLS UNROLL
                    packed |= uint64_t(output_pack[lane]) << (lane * 8);
                }
                destination[destination_index >> 3] = packed;
            }
        }
    }
    return orb_hls::kOk;
}

/** @copydoc orb_fast_stream_hls */
extern "C" int orb_fast_stream_hls(
    const uint64_t* image, Point* candidates, int width, int height,
    int high_threshold, int low_threshold, int dual_threshold,
    int candidate_capacity) {
#ifdef ORB_HLS_COSIM_SMALL_DEPTH
#pragma HLS INTERFACE m_axi port=image offset=slave bundle=gmem0 depth=960
#pragma HLS INTERFACE m_axi port=candidates offset=slave bundle=gmem1 depth=4096
#else
#pragma HLS INTERFACE m_axi port=image offset=slave bundle=gmem0 depth=259200
#pragma HLS INTERFACE m_axi port=candidates offset=slave bundle=gmem1 depth=691200
#endif
#pragma HLS DATA_PACK variable=candidates
#pragma HLS INTERFACE s_axilite port=image bundle=control
#pragma HLS INTERFACE s_axilite port=candidates bundle=control
#pragma HLS INTERFACE s_axilite port=width bundle=control
#pragma HLS INTERFACE s_axilite port=height bundle=control
#pragma HLS INTERFACE s_axilite port=high_threshold bundle=control
#pragma HLS INTERFACE s_axilite port=low_threshold bundle=control
#pragma HLS INTERFACE s_axilite port=dual_threshold bundle=control
#pragma HLS INTERFACE s_axilite port=candidate_capacity bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control
    if (width <= 0 || height <= 0 || width > orb_hls::kMaxWidth ||
        height > orb_hls::kMaxHeight || high_threshold < 1 ||
        high_threshold > 255 || low_threshold < 1 ||
        low_threshold > high_threshold ||
        (dual_threshold != 0 && dual_threshold != 1) ||
        candidate_capacity < 0) {
        return -1;
    }

    const int effective_width = width - 32;
    const int effective_height = height - 32;
    if (effective_width <= 6 || effective_height <= 6) return 0;

    int tile_cols = effective_width / 35;
    int tile_rows = effective_height / 35;
    if (tile_cols < 1) tile_cols = 1;
    if (tile_rows < 1) tile_rows = 1;
    const int tile_width = (effective_width + tile_cols - 1) / tile_cols;
    const int tile_height = (effective_height + tile_rows - 1) / tile_rows;
    const int threshold = dual_threshold ? low_threshold : high_threshold;
    int candidate_count = 0;

    // The largest v1.0 tile has at most 43 score columns at the supported
    // resolution. Eighty entries include its FAST halo and retain margin for
    // the small-image extension without allocating a full image line.
    constexpr int kFastRawTileExtent = 80;
    uint8_t fast_lines[7][kFastRawTileExtent];
    uint8_t nms_lines[2][kFastRawTileExtent];
    uint64_t packed_row[(kFastRawTileExtent + 14) / 8];
#pragma HLS ARRAY_PARTITION variable=fast_lines complete dim=1
#pragma HLS ARRAY_PARTITION variable=nms_lines complete dim=1
#pragma HLS RESOURCE variable=fast_lines core=RAM_S2P_BRAM
#pragma HLS RESOURCE variable=nms_lines core=RAM_S2P_BRAM
#pragma HLS RESOURCE variable=packed_row core=RAM_2P_BRAM
#pragma HLS DEPENDENCE variable=fast_lines inter false
#pragma HLS DEPENDENCE variable=nms_lines inter false

FastTileRows:
    for (int tile_y = 0; tile_y < tile_rows; ++tile_y) {
    FastTileColumns:
        for (int tile_x = 0; tile_x < tile_cols; ++tile_x) {
            const int y0 = 16 + tile_y * tile_height;
            const int x0 = 16 + tile_x * tile_width;
            int y1 = y0 + tile_height + 6;
            int x1 = x0 + tile_width + 6;
            if (y1 > height - 16) y1 = height - 16;
            if (x1 > width - 16) x1 = width - 16;
            if (y0 >= height - 19 || x0 >= width - 22) continue;
            const int score_width = x1 - x0;
            const int score_height = y1 - y0;
            const int raw_width = score_width + 4;
            const int raw_height = score_height + 4;
            if (raw_width > kFastRawTileExtent) return -1;

        FastRawRows:
            for (int raw_y = 0; raw_y < raw_height; ++raw_y) {
                const int source_y = y0 - 3 + raw_y;
                const int row_base = source_y * width + x0 - 3;
                const int first_word = row_base >> 3;
                const int final_word = (row_base + raw_width + 7) >> 3;
            LoadFastPackedRow:
                for (int word = first_word; word < final_word; ++word) {
#pragma HLS PIPELINE II=1
                    packed_row[word - first_word] = image[word];
                }

                uint8_t fast_window[7][7];
                uint8_t nms_window[3][3];
#pragma HLS ARRAY_PARTITION variable=fast_window complete dim=1
#pragma HLS ARRAY_PARTITION variable=fast_window complete dim=2
#pragma HLS ARRAY_PARTITION variable=nms_window complete dim=1
#pragma HLS ARRAY_PARTITION variable=nms_window complete dim=2
            ClearFastWindowRows:
                for (int row = 0; row < 7; ++row) {
#pragma HLS UNROLL
                ClearFastWindowColumns:
                    for (int column = 0; column < 7; ++column) {
#pragma HLS UNROLL
                        fast_window[row][column] = 0;
                    }
                }
            ClearNmsWindowRows:
                for (int row = 0; row < 3; ++row) {
#pragma HLS UNROLL
                ClearNmsWindowColumns:
                    for (int column = 0; column < 3; ++column) {
#pragma HLS UNROLL
                        nms_window[row][column] = 0;
                    }
                }
            FastRawColumns:
                for (int raw_x = 0; raw_x < raw_width; ++raw_x) {
#pragma HLS LOOP_FLATTEN off
#pragma HLS PIPELINE II=1
                    const int source_index = row_base + raw_x;
                    const uint64_t packed = packed_row[(source_index >> 3) - first_word];
                    const uint8_t source_pixel = uint8_t(packed >> ((source_index & 7) * 8));
                    uint8_t fast_column[7];
#pragma HLS ARRAY_PARTITION variable=fast_column complete dim=1
                    for (int row = 0; row < 6; ++row) {
#pragma HLS UNROLL
                        fast_column[row] = fast_lines[row + 1][raw_x];
                    }
                    fast_column[6] = source_pixel;
                    for (int row = 0; row < 6; ++row) {
#pragma HLS UNROLL
                        fast_lines[row][raw_x] = fast_column[row];
                    }
                    fast_lines[6][raw_x] = source_pixel;
                    for (int row = 0; row < 7; ++row) {
#pragma HLS UNROLL
                        for (int column = 0; column < 6; ++column) {
#pragma HLS UNROLL
                            fast_window[row][column] = fast_window[row][column + 1];
                        }
                        fast_window[row][6] = fast_column[row];
                    }

                    if (raw_y < 6 || raw_x < 6) continue;
                    uint8_t ring[16];
#pragma HLS ARRAY_PARTITION variable=ring complete dim=1
                    ring[0] = fast_window[6][3];
                    ring[1] = fast_window[6][4];
                    ring[2] = fast_window[5][5];
                    ring[3] = fast_window[4][6];
                    ring[4] = fast_window[3][6];
                    ring[5] = fast_window[2][6];
                    ring[6] = fast_window[1][5];
                    ring[7] = fast_window[0][4];
                    ring[8] = fast_window[0][3];
                    ring[9] = fast_window[0][2];
                    ring[10] = fast_window[1][1];
                    ring[11] = fast_window[2][0];
                    ring[12] = fast_window[3][0];
                    ring[13] = fast_window[4][0];
                    ring[14] = fast_window[5][1];
                    ring[15] = fast_window[6][2];
                    const int best = fast_response(fast_window[3][3], ring);
                    const uint8_t score = uint8_t(best > 0 ? best - 1 : 0);
                    const int score_x = raw_x - 6;
                    const int score_y = raw_y - 6;
                    const uint8_t previous2 = nms_lines[0][score_x];
                    const uint8_t previous1 = nms_lines[1][score_x];
                    nms_lines[0][score_x] = previous1;
                    nms_lines[1][score_x] = score;
                    for (int row = 0; row < 3; ++row) {
#pragma HLS UNROLL
                        nms_window[row][0] = nms_window[row][1];
                        nms_window[row][1] = nms_window[row][2];
                    }
                    nms_window[0][2] = previous2;
                    nms_window[1][2] = previous1;
                    nms_window[2][2] = score;
                    if (score_y < 4 || score_x < 4) continue;

                    const int local_y = score_y - 1;
                    const int local_x = score_x - 1;
                    const int response = nms_window[1][1];
                    if (response < threshold) continue;
                    bool keep = true;
                FastNmsRows:
                    for (int offset_y = -1; offset_y <= 1; ++offset_y) {
#pragma HLS UNROLL
                    FastNmsColumns:
                        for (int offset_x = -1; offset_x <= 1; ++offset_x) {
#pragma HLS UNROLL
                            if (offset_x == 0 && offset_y == 0) continue;
                            const int neighbour_x = local_x + offset_x;
                            const int neighbour_y = local_y + offset_y;
                            int neighbour = 0;
                            if (neighbour_x >= 3 && neighbour_x < score_width - 3 &&
                                neighbour_y >= 3 && neighbour_y < score_height - 3) {
                                neighbour = nms_window[offset_y + 1][offset_x + 1];
                            }
                            if (neighbour >= response) keep = false;
                        }
                    }
                    if (keep) {
                        if (candidate_count < candidate_capacity) {
                            const Point point = {
                                x0 + local_x - 16, y0 + local_y - 16,
                                response, tile_y * tile_cols + tile_x};
                            candidates[candidate_count] = point;
                        }
                        ++candidate_count;
                    }
                }
            }
        }
    }
    return candidate_count <= candidate_capacity ? candidate_count : -2;
}

/** @copydoc orb_level_hls */
extern "C" int orb_level_hls(
    const uint8_t* image, uint16_t* pixel_work, uint16_t* gaussian_work,
    Point* point_work, Feature* features,
    int width, int height, int budget, int high_threshold, int low_threshold,
    int dual_threshold, int level, int pixel_capacity, int candidate_capacity, int selected_capacity,
    int cell_capacity, int feature_capacity) {
#ifdef ORB_HLS_COSIM_SMALL_DEPTH
#pragma HLS INTERFACE m_axi port=image offset=slave bundle=gmem0 depth=7680
#pragma HLS INTERFACE m_axi port=pixel_work offset=slave bundle=gmem1 depth=30720
#pragma HLS INTERFACE m_axi port=point_work offset=slave bundle=gmem2 depth=30720
#pragma HLS INTERFACE m_axi port=gaussian_work offset=slave bundle=gmem4 depth=7680
#pragma HLS INTERFACE m_axi port=features offset=slave bundle=gmem3 depth=41
#else
#pragma HLS INTERFACE m_axi port=image offset=slave bundle=gmem0 depth=2073600
#pragma HLS INTERFACE m_axi port=pixel_work offset=slave bundle=gmem1 depth=8294400
#pragma HLS INTERFACE m_axi port=point_work offset=slave bundle=gmem2 depth=8294400
#pragma HLS INTERFACE m_axi port=gaussian_work offset=slave bundle=gmem4 depth=2073600
#pragma HLS INTERFACE m_axi port=features offset=slave bundle=gmem3 depth=10001
#endif
#pragma HLS DATA_PACK variable=point_work
#pragma HLS DATA_PACK variable=features
#pragma HLS INTERFACE s_axilite port=image bundle=control
#pragma HLS INTERFACE s_axilite port=pixel_work bundle=control
#pragma HLS INTERFACE s_axilite port=gaussian_work bundle=control
#pragma HLS INTERFACE s_axilite port=point_work bundle=control
#pragma HLS INTERFACE s_axilite port=features bundle=control
#pragma HLS INTERFACE s_axilite port=width bundle=control
#pragma HLS INTERFACE s_axilite port=height bundle=control
#pragma HLS INTERFACE s_axilite port=budget bundle=control
#pragma HLS INTERFACE s_axilite port=high_threshold bundle=control
#pragma HLS INTERFACE s_axilite port=low_threshold bundle=control
#pragma HLS INTERFACE s_axilite port=dual_threshold bundle=control
#pragma HLS INTERFACE s_axilite port=level bundle=control
#pragma HLS INTERFACE s_axilite port=pixel_capacity bundle=control
#pragma HLS INTERFACE s_axilite port=candidate_capacity bundle=control
#pragma HLS INTERFACE s_axilite port=selected_capacity bundle=control
#pragma HLS INTERFACE s_axilite port=cell_capacity bundle=control
#pragma HLS INTERFACE s_axilite port=feature_capacity bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control
    if (width <= 0 || height <= 0 || width > orb_hls::kMaxWidth || height > orb_hls::kMaxHeight ||
        budget <= 0 || budget > orb_hls::kMaxFeatures || high_threshold < 1 || high_threshold > 255 ||
        low_threshold < 1 || low_threshold > high_threshold || (dual_threshold != 0 && dual_threshold != 1) ||
        pixel_capacity < width * height) {
        return orb_hls::kInvalidArgument;
    }

    uint16_t* const score = pixel_work;
    uint16_t* const gaussian = gaussian_work;
    uint16_t* const cell_flags = pixel_work + 3 * pixel_capacity;
    Point* const candidates = point_work;
    Point* const selected = point_work + candidate_capacity;
    Point* const cell_best_high = selected + selected_capacity;
    Point* const cell_best_low = cell_best_high + cell_capacity;

    // Keep the seven FAST rows in independent banks. This layout preserves the
    // reference algorithm's row/column access pattern and avoids a custom
    // packed-memory read/modify/write protocol.
    uint8_t fast_lines[7][orb_hls::kMaxWidth];
#pragma HLS ARRAY_PARTITION variable=fast_lines complete dim=1
#pragma HLS RESOURCE variable=fast_lines core=RAM_S2P_BRAM
#pragma HLS DEPENDENCE variable=fast_lines inter false

    uint16_t gaussian_lines[7][orb_hls::kMaxWidth];
#pragma HLS ARRAY_PARTITION variable=gaussian_lines complete dim=1
#pragma HLS RESOURCE variable=gaussian_lines core=RAM_S2P_BRAM
#pragma HLS DEPENDENCE variable=gaussian_lines inter false

    // Stream FAST and Gaussian through one shared image read. The six reflected
    // horizontal halo samples service Gaussian, while in-range samples also
    // update the FAST line/window state. Both pixel-rate paths therefore share
    // one width+6-cycle row scan.
    for (int input_y = 0; input_y < height; ++input_y) {
        uint8_t fast_window[7][7];
#pragma HLS ARRAY_PARTITION variable=fast_window complete dim=1
#pragma HLS ARRAY_PARTITION variable=fast_window complete dim=2
        for (int row = 0; row < 7; ++row) {
#pragma HLS UNROLL
            for (int column = 0; column < 7; ++column) {
#pragma HLS UNROLL
                fast_window[row][column] = 0;
            }
        }
        uint8_t horizontal_window[7] = {};
#pragma HLS ARRAY_PARTITION variable=horizontal_window complete dim=1
        for (int stream_x = -3; stream_x < width + 3; ++stream_x) {
#pragma HLS LOOP_FLATTEN off
#pragma HLS PIPELINE II=1
            const int source_x = reflect101_hw(stream_x, width);
            const uint8_t source_pixel = image[input_y * width + source_x];

            for (int tap = 0; tap < 6; ++tap) {
#pragma HLS UNROLL
                horizontal_window[tap] = horizontal_window[tap + 1];
            }
            horizontal_window[6] = source_pixel;

            if (stream_x >= 0 && stream_x < width) {
                uint8_t fast_column[7];
#pragma HLS ARRAY_PARTITION variable=fast_column complete dim=1
                for (int row = 0; row < 6; ++row) {
#pragma HLS UNROLL
                    fast_column[row] = fast_lines[row + 1][stream_x];
                }
                fast_column[6] = source_pixel;
                for (int row = 0; row < 6; ++row) {
#pragma HLS UNROLL
                    fast_lines[row][stream_x] = fast_column[row];
                }
                fast_lines[6][stream_x] = fast_column[6];

                for (int row = 0; row < 7; ++row) {
#pragma HLS UNROLL
                    for (int column = 0; column < 6; ++column) {
#pragma HLS UNROLL
                        fast_window[row][column] = fast_window[row][column + 1];
                    }
                    fast_window[row][6] = fast_column[row];
                }

                if (input_y >= 3 && stream_x >= 3) {
                    int best = 0;
                    if (input_y >= 6 && stream_x >= 6) {
                        uint8_t ring[16];
#pragma HLS ARRAY_PARTITION variable=ring complete dim=1
                        ring[0] = fast_window[6][3];
                        ring[1] = fast_window[6][4];
                        ring[2] = fast_window[5][5];
                        ring[3] = fast_window[4][6];
                        ring[4] = fast_window[3][6];
                        ring[5] = fast_window[2][6];
                        ring[6] = fast_window[1][5];
                        ring[7] = fast_window[0][4];
                        ring[8] = fast_window[0][3];
                        ring[9] = fast_window[0][2];
                        ring[10] = fast_window[1][1];
                        ring[11] = fast_window[2][0];
                        ring[12] = fast_window[3][0];
                        ring[13] = fast_window[4][0];
                        ring[14] = fast_window[5][1];
                        ring[15] = fast_window[6][2];
                        best = fast_response(fast_window[3][3], ring);
                    }
                    const int output_y = input_y - 3;
                    const int output_x = stream_x - 3;
                    score[output_y * width + output_x] = uint16_t(best > 0 ? best - 1 : 0);
                }
            }

            if (stream_x >= 3) {
                const int output_x = stream_x - 3;
                uint16_t gaussian_column[7];
#pragma HLS ARRAY_PARTITION variable=gaussian_column complete dim=1
                for (int row = 0; row < 6; ++row) {
#pragma HLS UNROLL
                    gaussian_column[row] = gaussian_lines[row + 1][output_x];
                }
                gaussian_column[6] = gaussian_horizontal_value(horizontal_window);
                for (int row = 0; row < 6; ++row) {
#pragma HLS UNROLL
                    gaussian_lines[row][output_x] = gaussian_column[row];
                }
                gaussian_lines[6][output_x] = gaussian_column[6];

                if (height >= 7 && input_y >= 3) {
                    uint16_t vertical_window[7];
#pragma HLS ARRAY_PARTITION variable=vertical_window complete dim=1
                    if (input_y == 3) {
                        vertical_window[0] = gaussian_column[6];
                        vertical_window[1] = gaussian_column[5];
                        vertical_window[2] = gaussian_column[4];
                        vertical_window[3] = gaussian_column[3];
                        vertical_window[4] = gaussian_column[4];
                        vertical_window[5] = gaussian_column[5];
                        vertical_window[6] = gaussian_column[6];
                    } else if (input_y == 4) {
                        vertical_window[0] = gaussian_column[4];
                        vertical_window[1] = gaussian_column[3];
                        vertical_window[2] = gaussian_column[2];
                        vertical_window[3] = gaussian_column[3];
                        vertical_window[4] = gaussian_column[4];
                        vertical_window[5] = gaussian_column[5];
                        vertical_window[6] = gaussian_column[6];
                    } else if (input_y == 5) {
                        vertical_window[0] = gaussian_column[2];
                        vertical_window[1] = gaussian_column[1];
                        vertical_window[2] = gaussian_column[2];
                        vertical_window[3] = gaussian_column[3];
                        vertical_window[4] = gaussian_column[4];
                        vertical_window[5] = gaussian_column[5];
                        vertical_window[6] = gaussian_column[6];
                    } else {
                        for (int tap = 0; tap < 7; ++tap) {
#pragma HLS UNROLL
                            vertical_window[tap] = gaussian_column[tap];
                        }
                    }
                    const int output_y = input_y - 3;
                    gaussian[output_y * width + output_x] = gaussian_vertical_value(vertical_window);
                }
            }
        }
    }

    int fast_right_start = width - 3;
    if (fast_right_start < 0) fast_right_start = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = fast_right_start; x < width; ++x) {
#pragma HLS LOOP_FLATTEN off
#pragma HLS PIPELINE II=1
            score[y * width + x] = 0;
        }
    }
    int fast_bottom_start = height - 3;
    if (fast_bottom_start < 0) fast_bottom_start = 0;
    for (int y = fast_bottom_start; y < height; ++y) {
        for (int x = 0; x < fast_right_start; ++x) {
#pragma HLS LOOP_FLATTEN off
#pragma HLS PIPELINE II=1
            score[y * width + x] = 0;
        }
    }

    const int effective_width = width - 32;
    const int effective_height = height - 32;
    int candidate_count = 0;
    if (effective_width > 6 && effective_height > 6) {
        uint16_t nms_lines[2][kMaxNmsTileExtent];
#pragma HLS ARRAY_PARTITION variable=nms_lines complete dim=1
#pragma HLS RESOURCE variable=nms_lines core=RAM_S2P_BRAM
#pragma HLS DEPENDENCE variable=nms_lines inter false
        int tile_cols = effective_width / 35;
        int tile_rows = effective_height / 35;
        if (tile_cols < 1) tile_cols = 1;
        if (tile_rows < 1) tile_rows = 1;
        const int tile_width = (effective_width + tile_cols - 1) / tile_cols;
        const int tile_height = (effective_height + tile_rows - 1) / tile_rows;
        const int threshold = dual_threshold ? low_threshold : high_threshold;
        for (int tile_y = 0; tile_y < tile_rows; ++tile_y) {
            for (int tile_x = 0; tile_x < tile_cols; ++tile_x) {
                const int y0 = 16 + tile_y * tile_height;
                const int x0 = 16 + tile_x * tile_width;
                int y1 = y0 + tile_height + 6;
                int x1 = x0 + tile_width + 6;
                if (y1 > height - 16) y1 = height - 16;
                if (x1 > width - 16) x1 = width - 16;
                if (y0 >= height - 19 || x0 >= width - 22) continue;
                const int tile_storage_width = x1 - x0;
                const int tile_storage_height = y1 - y0;

                // Stream one score per cycle through two cached rows and a
                // fully partitioned 3x3 window. The input coordinate is one
                // row and column ahead of the NMS center, so all eight
                // neighbours are available without random DDR reads.
                NmsStreamRows:
                for (int input_y = 0; input_y < tile_storage_height - 2; ++input_y) {
                    uint16_t nms_window[3][3] = {};
#pragma HLS ARRAY_PARTITION variable=nms_window complete dim=1
#pragma HLS ARRAY_PARTITION variable=nms_window complete dim=2
                    NmsStreamColumns:
                    for (int input_x = 0; input_x < tile_storage_width - 2; ++input_x) {
#pragma HLS LOOP_FLATTEN off
#pragma HLS PIPELINE II=1
                        const uint16_t previous2 = nms_lines[0][input_x];
                        const uint16_t previous1 = nms_lines[1][input_x];
                        const uint16_t current =
                            score[(y0 + input_y) * width + x0 + input_x];
                        nms_lines[0][input_x] = previous1;
                        nms_lines[1][input_x] = current;
                        for (int row = 0; row < 3; ++row) {
#pragma HLS UNROLL
                            nms_window[row][0] = nms_window[row][1];
                            nms_window[row][1] = nms_window[row][2];
                        }
                        nms_window[0][2] = previous2;
                        nms_window[1][2] = previous1;
                        nms_window[2][2] = current;

                        if (input_y < 4 || input_x < 4) continue;
                        const int local_y = input_y - 1;
                        const int local_x = input_x - 1;
                        const int response = nms_window[1][1];
                        if (response < threshold) continue;
                        bool keep = true;
                        NmsNeighbourRows:
                        for (int j = -1; j <= 1; ++j) {
#pragma HLS UNROLL
                            NmsNeighbourColumns:
                            for (int i = -1; i <= 1; ++i) {
#pragma HLS UNROLL
                                if (i == 0 && j == 0) continue;
                                const int nx = local_x + i;
                                const int ny = local_y + j;
                                int neighbour = 0;
                                if (nx >= 3 && nx < tile_storage_width - 3 &&
                                    ny >= 3 && ny < tile_storage_height - 3) {
                                    neighbour = nms_window[j + 1][i + 1];
                                }
                                if (neighbour >= response) keep = false;
                            }
                        }
                        if (keep) {
                            // Defer the overflow return until the pipeline has
                            // drained. An in-loop return becomes a loop-exit
                            // dependency in Vivado HLS 2018.3 and forces II=2.
                            if (candidate_count < candidate_capacity) {
                                Point point = {x0 + local_x - 16, y0 + local_y - 16,
                                               response, tile_y * tile_cols + tile_x};
                                candidates[candidate_count] = point;
                            }
                            ++candidate_count;
                        }
                    }
                }
            }
        }
    }
    if (candidate_count > candidate_capacity) return orb_hls::kCandidateOverflow;

    int cell_side = 0;
    int cell_cols = 0;
    int cell_rows = 0;
    int empty_cells = 0;
    int fallback_cells = 0;
    int selected_count = 0;
    if (effective_width > 0 && effective_height > 0) {
        cell_side = int(isqrt_hw(uint64_t(effective_width) * uint64_t(effective_height) / uint64_t(budget)));
        if (cell_side < 1) cell_side = 1;
        cell_cols = effective_width / cell_side;
        cell_rows = effective_height / cell_side;
        if (cell_cols < 1) cell_cols = 1;
        if (cell_rows < 1) cell_rows = 1;
        const int cell_count = cell_cols * cell_rows;
        if (cell_count > cell_capacity) return orb_hls::kCellOverflow;
        if (cell_count <= kOnChipGridCells) {
            uint8_t local_flags[kOnChipGridCells];
            uint64_t local_best_high[kOnChipGridCells];
            uint64_t local_best_low[kOnChipGridCells];
#pragma HLS RESOURCE variable=local_flags core=RAM_2P_BRAM
#pragma HLS RESOURCE variable=local_best_high core=RAM_2P_BRAM
#pragma HLS RESOURCE variable=local_best_low core=RAM_2P_BRAM
        ClearLocalGrid:
            for (int cell = 0; cell < cell_count; ++cell) {
#pragma HLS PIPELINE II=1
                local_flags[cell] = 0;
            }
        SelectLocalGrid:
            for (int index = 0; index < candidate_count; ++index) {
                const Point point = candidates[index];
                int cell_x = point.x / cell_side;
                int cell_y = point.y / cell_side;
                if (cell_x >= cell_cols) cell_x = cell_cols - 1;
                if (cell_y >= cell_rows) cell_y = cell_rows - 1;
                const int cell = cell_y * cell_cols + cell_x;
                uint8_t flags = local_flags[cell];
                if (point.score >= high_threshold) {
                    if ((flags & 1u) == 0 ||
                        stronger(point, unpack_grid_point(local_best_high[cell]))) {
                        local_best_high[cell] = pack_grid_point(point);
                    }
                    flags |= 1u;
                } else if (dual_threshold && point.score >= low_threshold) {
                    if ((flags & 2u) == 0 ||
                        stronger(point, unpack_grid_point(local_best_low[cell]))) {
                        local_best_low[cell] = pack_grid_point(point);
                    }
                    flags |= 2u;
                }
                local_flags[cell] = flags;
            }
        EmitLocalGrid:
            for (int cell = 0; cell < cell_count; ++cell) {
                Point point;
                const uint8_t flags = local_flags[cell];
                if (flags & 1u) {
                    point = unpack_grid_point(local_best_high[cell]);
                } else if (dual_threshold && (flags & 2u)) {
                    point = unpack_grid_point(local_best_low[cell]);
                    ++fallback_cells;
                } else {
                    ++empty_cells;
                    continue;
                }
                if (selected_count >= selected_capacity) return orb_hls::kSelectedOverflow;
                selected[selected_count++] = point;
            }
        } else {
        ClearExternalGrid:
            for (int cell = 0; cell < cell_count; ++cell) cell_flags[cell] = 0;
        SelectExternalGrid:
            for (int index = 0; index < candidate_count; ++index) {
                const Point point = candidates[index];
                int cell_x = point.x / cell_side;
                int cell_y = point.y / cell_side;
                if (cell_x >= cell_cols) cell_x = cell_cols - 1;
                if (cell_y >= cell_rows) cell_y = cell_rows - 1;
                const int cell = cell_y * cell_cols + cell_x;
                if (point.score >= high_threshold) {
                    if ((cell_flags[cell] & 1u) == 0 || stronger(point, cell_best_high[cell])) {
                        cell_best_high[cell] = point;
                    }
                    cell_flags[cell] |= 1u;
                } else if (dual_threshold && point.score >= low_threshold) {
                    if ((cell_flags[cell] & 2u) == 0 || stronger(point, cell_best_low[cell])) {
                        cell_best_low[cell] = point;
                    }
                    cell_flags[cell] |= 2u;
                }
            }
        EmitExternalGrid:
            for (int cell = 0; cell < cell_count; ++cell) {
                Point point;
                if (cell_flags[cell] & 1u) {
                    point = cell_best_high[cell];
                } else if (dual_threshold && (cell_flags[cell] & 2u)) {
                    point = cell_best_low[cell];
                    ++fallback_cells;
                } else {
                    ++empty_cells;
                    continue;
                }
                if (selected_count >= selected_capacity) return orb_hls::kSelectedOverflow;
                selected[selected_count++] = point;
            }
        }
        if (selected_count > budget) {
            for (int i = 1; i < selected_count; ++i) {
                const Point key = selected[i];
                int j = i;
                while (j > 0 && stronger(key, selected[j - 1])) {
                    selected[j] = selected[j - 1];
                    --j;
                }
                selected[j] = key;
            }
            selected_count = budget;
        }
    }

    if (height >= 7) {
        static const uint8_t bottom_indices[3][7] = {
            {1, 2, 3, 4, 5, 6, 5},
            {2, 3, 4, 5, 6, 5, 4},
            {3, 4, 5, 6, 5, 4, 3}};
#pragma HLS ARRAY_PARTITION variable=bottom_indices complete dim=2
        for (int bottom = 0; bottom < 3; ++bottom) {
            const int output_y = height - 3 + bottom;
            for (int x = 0; x < width; ++x) {
#pragma HLS LOOP_FLATTEN off
#pragma HLS PIPELINE II=1
                uint16_t cached_lines[7];
#pragma HLS ARRAY_PARTITION variable=cached_lines complete dim=1
                uint16_t vertical_window[7];
#pragma HLS ARRAY_PARTITION variable=vertical_window complete dim=1
                for (int tap = 0; tap < 7; ++tap) {
#pragma HLS UNROLL
                    cached_lines[tap] = gaussian_lines[tap][x];
                }
                for (int tap = 0; tap < 7; ++tap) {
#pragma HLS UNROLL
                    vertical_window[tap] = cached_lines[bottom_indices[bottom][tap]];
                }
                gaussian[output_y * width + x] = gaussian_vertical_value(vertical_window);
            }
        }
    } else {
        for (int output_y = 0; output_y < height; ++output_y) {
            for (int x = 0; x < width; ++x) {
                uint16_t vertical_window[7];
#pragma HLS ARRAY_PARTITION variable=vertical_window complete dim=1
                const int base = 7 - height;
                for (int tap = 0; tap < 7; ++tap) {
                    const int source_y = reflect101_hw(output_y + tap - 3, height);
                    vertical_window[tap] = gaussian_lines[base + source_y][x];
                }
                gaussian[output_y * width + x] = gaussian_vertical_value(vertical_window);
            }
        }
    }

    if (selected_count > feature_capacity) return orb_hls::kSelectedOverflow;
    for (int index = 0; index < selected_count; ++index) {
        const Point effective = selected[index];
        Feature feature = {};
        feature.x = effective.x + 16;
        feature.y = effective.y + 16;
        feature.score = effective.score;
        feature.level = level;
        compute_moments(image, width, feature.x, feature.y, feature.m10, feature.m01);
        const uint32_t precise_angle = angle_q16_hw(feature.m10, feature.m01);
        feature.bin = quantize32(precise_angle);
        feature.angle_q16 = uint32_t(uint64_t(feature.bin) * (uint64_t(360) << 16) / 32u);
        const int64_t cosine = orb_golden::trig32[feature.bin][0];
        const int64_t sine = orb_golden::trig32[feature.bin][1];
        uint32_t descriptor[8] = {};
        for (int bit = 0; bit < 256; ++bit) {
#pragma HLS PIPELINE II=2
            const uint8_t first = rotated_sample(gaussian, width, feature.x, feature.y, bit * 4, cosine, sine);
            const uint8_t second = rotated_sample(gaussian, width, feature.x, feature.y, bit * 4 + 2, cosine, sine);
            if (first < second) descriptor[bit / 32] |= uint32_t(1u << (bit % 32));
        }
        feature.descriptor0 = descriptor[0];
        feature.descriptor1 = descriptor[1];
        feature.descriptor2 = descriptor[2];
        feature.descriptor3 = descriptor[3];
        feature.descriptor4 = descriptor[4];
        feature.descriptor5 = descriptor[5];
        feature.descriptor6 = descriptor[6];
        feature.descriptor7 = descriptor[7];
        features[index] = feature;
    }

    Feature metadata = {};
    metadata.x = candidate_count;
    metadata.y = selected_count;
    metadata.score = cell_side;
    metadata.level = cell_cols;
    metadata.bin = cell_rows;
    metadata.m10 = empty_cells;
    metadata.m01 = fallback_cells;
    metadata.angle_q16 = orb_hls::kAlgorithmVersion;
    metadata.descriptor0 = orb_hls::kCapabilityDualThreshold;
    metadata.descriptor1 = uint32_t(orb_hls::kMaxGridTopK);
    features[feature_capacity] = metadata;
    return orb_hls::kOk;
}
