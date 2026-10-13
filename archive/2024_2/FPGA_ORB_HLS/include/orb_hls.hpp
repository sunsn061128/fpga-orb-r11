#pragma once

#include <cstdint>

/** @brief Maximum element depth presented to each image-sized AXI pointer. */
#ifdef ORB_HLS_COSIM_SMALL_DEPTH
#define ORB_HLS_IMAGE_INTERFACE_DEPTH 7680
#define ORB_HLS_PACKED_IMAGE_INTERFACE_DEPTH 960
#else
#define ORB_HLS_IMAGE_INTERFACE_DEPTH 2073600
#define ORB_HLS_PACKED_IMAGE_INTERFACE_DEPTH 259200
#endif

/** @brief Maximum element depth presented to the packed feature AXI pointer. */
#ifdef ORB_HLS_COSIM_SMALL_DEPTH
#define ORB_HLS_FEATURE_INTERFACE_DEPTH 41
#else
#define ORB_HLS_FEATURE_INTERFACE_DEPTH 10001
#endif

namespace orb_hls {

/** Maximum supported input or pyramid-layer width in pixels. */
constexpr int kMaxWidth = 1920;
/** Maximum supported input or pyramid-layer height in pixels. */
constexpr int kMaxHeight = 1080;
/** Maximum requested feature budget across one frame. */
constexpr int kMaxFeatures = 10000;
/** Packed algorithm contract version: upper 16 bits major, lower 16 bits minor. */
constexpr uint32_t kAlgorithmVersion = UINT32_C(0x00010000);
/** Capability bit indicating support for per-grid-cell high/low FAST fallback. */
constexpr uint32_t kCapabilityDualThreshold = UINT32_C(1) << 0;
/** Maximum supported number of selected points per grid cell. */
constexpr int kMaxGridTopK = 1;

/** Status values returned through the AXI-Lite function return register. */
enum Status {
    kOk = 0,
    kInvalidArgument = 1,
    kCandidateOverflow = 2,
    kCellOverflow = 3,
    kSelectedOverflow = 4,
};

/**
 * @brief FAST/NMS point stored in the effective coordinate system.
 *
 * The effective coordinate system removes the 16-pixel ORB descriptor border.
 * The structure packs to 128 bits on the HLS AXI master interface.
 */
struct Point {
    int32_t x;
    int32_t y;
    int32_t score;
    int32_t tile;
};

/**
 * @brief One selected ORB feature produced by a pyramid-level invocation.
 *
 * Coordinates are layer coordinates including the 16-pixel border. Angles use
 * unsigned Q16.16 degrees. The structure is exactly 64 bytes and packs to a
 * 512-bit AXI memory record. The caller allocates one extra record at index
 * feature_capacity; that final record carries LevelMeta fields in x through m01.
 */
struct Feature {
    int32_t x;
    int32_t y;
    int32_t score;
    int32_t level;
    int32_t bin;
    int32_t m10;
    int32_t m01;
    uint32_t angle_q16;
    uint32_t descriptor0;
    uint32_t descriptor1;
    uint32_t descriptor2;
    uint32_t descriptor3;
    uint32_t descriptor4;
    uint32_t descriptor5;
    uint32_t descriptor6;
    uint32_t descriptor7;
};

static_assert(sizeof(Feature) == 64, "Feature must remain one 512-bit AXI record");

/** @brief Counts, grid geometry, and hardware capabilities returned for one level. */
struct LevelMeta {
    int32_t candidate_count;
    int32_t selected_count;
    int32_t cell_side;
    int32_t cell_cols;
    int32_t cell_rows;
    int32_t empty_cells;
    int32_t fallback_cells;
    uint32_t algorithm_version;
    uint32_t capability_flags;
    int32_t max_grid_top_k;
};

} // namespace orb_hls

/**
 * @brief Resize one grayscale pyramid layer with the reference integer bilinear contract.
 *
 * @param source Row-major source pixels packed eight little-endian bytes per word.
 * @param destination Row-major destination pixels packed eight little-endian bytes per word.
 * @param source_width Source width in pixels, in the range 1..kMaxWidth.
 * @param source_height Source height in pixels, in the range 1..kMaxHeight.
 * @param destination_width Destination width in pixels, in the range 1..kMaxWidth.
 * @param destination_height Destination height in pixels, in the range 1..kMaxHeight.
 * @return An orb_hls::Status value.
 *
 * @pre Source and destination addresses are 8-byte aligned and contain at least
 *      ceil(width*height/8) 64-bit words.
 * @note The top uses two 64-bit AXI4 memory masters and one AXI4-Lite control interface.
 */
extern "C" int orb_resize_hls(
    const uint64_t* source,
    uint64_t* destination,
    int source_width,
    int source_height,
    int destination_width,
    int destination_height);

/**
 * @brief Detect bit-exact FAST/NMS candidates without storing a full score image.
 *
 * @param image Row-major grayscale pixels packed eight little-endian bytes per word.
 * @param candidates Output candidate records in v1.0 tile and raster order.
 * @param width Layer width in pixels, in the range 1..kMaxWidth.
 * @param height Layer height in pixels, in the range 1..kMaxHeight.
 * @param high_threshold Inclusive FAST high threshold in the range 1..255.
 * @param low_threshold Inclusive fallback threshold, not above high_threshold.
 * @param dual_threshold Set to 1 to emit candidates down to low_threshold or 0
 *        to emit high-threshold candidates only.
 * @param candidate_capacity Number of Point records available at candidates.
 * @return Candidate count on success, -1 for invalid arguments, or -2 when the
 *         detected candidate count exceeds candidate_capacity.
 *
 * @pre image is 8-byte aligned and contains ceil(width*height/8) words.
 * @note FAST scoring and tile-local 3x3 NMS are fused through line buffers; no
 *       external score-image workspace is read or written.
 */
extern "C" int orb_fast_stream_hls(
    const uint64_t* image,
    orb_hls::Point* candidates,
    int width,
    int height,
    int high_threshold,
    int low_threshold,
    int dual_threshold,
    int candidate_capacity);

/**
 * @brief Execute the bit-exact ORB front end for one already-sized pyramid layer.
 *
 * @param image Row-major raw grayscale layer, width*height bytes.
 * @param pixel_work Four contiguous 16-bit regions of pixel_capacity elements.
 *        The accelerator stores FAST scores in region 0 and cell flags in
 *        region 3; regions 1 and 2 remain reserved by the shared workspace.
 * @param gaussian_work Row-major 16-bit Gaussian output with pixel_capacity
 *        elements. It may alias pixel_work region 2 because the interfaces use
 *        independent AXI masters and the two ranges do not overlap logically.
 * @param point_work Four contiguous Point regions sized candidate_capacity,
 *        selected_capacity, cell_capacity, and cell_capacity for candidates,
 *        selected points, high winners, and low winners respectively.
 * @param features Selected 64-byte records followed by one metadata trailer at
 *        features[feature_capacity]. The trailer maps x=candidate_count,
 *        y=selected_count, score=cell_side, level=cell_cols, bin=cell_rows,
 *        m10=empty_cells, m01=fallback_cells, angle_q16=algorithm_version,
 *        descriptor0=capability_flags, and descriptor1=max_grid_top_k.
 * @param width Layer width in pixels, in the range 1..kMaxWidth.
 * @param height Layer height in pixels, in the range 1..kMaxHeight.
 * @param budget Maximum number of selected features for this level.
 * @param high_threshold Inclusive FAST high threshold in the range 1..255.
 * @param low_threshold Inclusive T2 fallback threshold, not above high_threshold.
 * @param dual_threshold Set to 1 for T2 cell fallback or 0 for high threshold only.
 * @param level Zero-based pyramid level written into each feature record.
 * @param pixel_capacity Elements available in each pixel_work region.
 * @param candidate_capacity Point elements available in the candidate region.
 * @param selected_capacity Point elements available in the selected region.
 * @param cell_capacity Point elements available in each cell-winner region.
 * @param feature_capacity Maximum number of ordinary Feature records; the
 *        features buffer must contain feature_capacity+1 elements.
 * @return An orb_hls::Status value; overflow statuses leave the call incomplete.
 *
 * @note The top is transaction based (ap_ctrl_hs). Inputs must remain valid until
 *       completion; external memory must not be modified concurrently by software.
 */
extern "C" int orb_level_hls(
    const uint8_t* image,
    uint16_t* pixel_work,
    uint16_t* gaussian_work,
    orb_hls::Point* point_work,
    orb_hls::Feature* features,
    int width,
    int height,
    int budget,
    int high_threshold,
    int low_threshold,
    int dual_threshold,
    int level,
    int pixel_capacity,
    int candidate_capacity,
    int selected_capacity,
    int cell_capacity,
    int feature_capacity);
