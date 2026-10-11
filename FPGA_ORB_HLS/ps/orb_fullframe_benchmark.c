/**
 * @file orb_fullframe_benchmark.c
 * @brief 960x720, eight-level Zynq PS/PL bit-exact benchmark.
 */

#include "xorb_level_hls.h"
#include "xorb_resize_hls.h"
#include "xil_cache.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "xtime_l.h"

#include <stddef.h>
#include <stdint.h>

enum {
    kLevels = 8,
    kInputWidth = 960,
    kInputHeight = 720,
    kMaximumPixels = kInputWidth * kInputHeight,
    kCandidateCapacity = kMaximumPixels,
    kSelectedCapacity = 1000,
    kCellCapacity = 2048,
    kPixelRegions = 4,
    kPointWorkspaceCount = kCandidateCapacity + kSelectedCapacity + 2 * kCellCapacity,
    kPgmHeaderBytes = 15,
    kAlgorithmVersion = 0x00010000,
    kCapabilityDualThreshold = 1,
    kMaxGridTopK = 1,
    kMaximumBenchmarkIterations = 1000,
    kFabricClockHz = 100000000,
    kMailboxAddress = 0x01000000,
    kInputPgmAddress = 0x02000000,
    kPyramidBuffer0Address = 0x02400000,
    kPyramidBuffer1Address = 0x02600000,
    kAlignedInputAddress = 0x02800000,
    kPixelWorkspaceAddress = 0x03000000,
    kPointWorkspaceAddress = 0x04000000,
    kFeatureWorkspaceAddress = 0x05000000,
    kMailboxCommandMagic = 0x4f524243,
    kMailboxResultMagic = 0x4f524252,
    kMailboxSchemaVersion = 4,
};

enum OrbMailboxStatus {
    kMailboxEmpty = 0,
    kMailboxRunning = 1,
    kMailboxPass = 2,
    kMailboxFail = 3,
};

enum OrbMailboxError {
    kErrorNone = 0,
    kErrorMailboxOverlap = 1,
    kErrorInputPgm = 10,
    kErrorResizeInitialize = 101,
    kErrorResizeTimeout = 102,
    kErrorResizeAccelerator = 103,
    kErrorResizeDigest = 104,
    kErrorLevelInitialize = 201,
    kErrorLevelTimeout = 202,
    kErrorLevelAccelerator = 203,
    kErrorLevelDigest = 204,
};

/** @brief Point layout shared with the 128-bit HLS AXI master. */
typedef struct {
    int32_t x;
    int32_t y;
    int32_t score;
    int32_t tile;
} OrbPoint;

/** @brief Feature layout shared with the 512-bit HLS AXI master. */
typedef struct {
    int32_t x;
    int32_t y;
    int32_t score;
    int32_t level;
    int32_t bin;
    int32_t m10;
    int32_t m01;
    uint32_t angle_q16;
    uint32_t descriptor[8];
} OrbFeature;

/** @brief Fixed 512-byte command/result mailbox read directly by XSCT. */
typedef struct {
    uint32_t command_magic;
    uint32_t requested_iterations;
    uint32_t result_magic;
    uint32_t schema_version;
    uint32_t status;
    uint32_t error_code;
    uint32_t completed_iterations;
    uint32_t failing_iteration;
    uint32_t timer_hz;
    uint32_t fabric_clock_hz;
    uint32_t candidates;
    uint32_t selected;
    uint32_t algorithm_version;
    uint32_t capabilities;
    uint32_t max_grid_top_k;
    uint32_t resize_digest_hi;
    uint32_t resize_digest_lo;
    uint32_t level_digest_hi;
    uint32_t level_digest_lo;
    uint32_t resize_config_avg_us;
    uint32_t resize_wait_avg_us;
    uint32_t resize_wait_min_us;
    uint32_t resize_wait_max_us;
    uint32_t resize_est_cycles_avg;
    uint32_t resize_fps_milli;
    uint32_t level_config_avg_us;
    uint32_t level_wait_avg_us;
    uint32_t level_wait_min_us;
    uint32_t level_wait_max_us;
    uint32_t level_est_cycles_avg;
    uint32_t level_fps_milli;
    uint32_t pipeline_avg_us;
    uint32_t pipeline_fps_milli;
    uint32_t verification_avg_us;
    uint32_t wall_avg_us;
    uint32_t wall_fps_milli;
    uint32_t total_elapsed_us_hi;
    uint32_t total_elapsed_us_lo;
    uint32_t failure_detail;
    uint32_t reserved[89];
} OrbBenchmarkMailbox;

/** @brief Per-frame accumulated configuration and accelerator wait timing. */
typedef struct {
    uint64_t config_total_ticks;
    uint64_t wait_total_ticks;
    uint64_t wait_min_ticks;
    uint64_t wait_max_ticks;
} OrbPerfStats;

typedef char OrbPointSizeMustBe16[(sizeof(OrbPoint) == 16) ? 1 : -1];
typedef char OrbFeatureSizeMustBe64[(sizeof(OrbFeature) == 64) ? 1 : -1];
typedef char OrbMailboxSizeMustBe512[(sizeof(OrbBenchmarkMailbox) == 512) ? 1 : -1];
typedef char OrbWorkspaceMustFitDdr[
    ((kFeatureWorkspaceAddress + (kSelectedCapacity + 1) * sizeof(OrbFeature) - 1) <=
     XPAR_PS7_DDR_0_S_AXI_HIGHADDR)
        ? 1
        : -1];

static const int kWidths[kLevels] = {960, 800, 667, 556, 463, 386, 322, 268};
static const int kHeights[kLevels] = {720, 600, 500, 417, 347, 289, 241, 201};
static const int kBudgets[kLevels] = {217, 181, 151, 126, 105, 87, 73, 60};
static const int kExpectedCandidates[kLevels] = {3873, 2595, 1765, 1128, 776, 492, 296, 153};
static const int kExpectedSelected[kLevels] = {64, 45, 44, 38, 29, 24, 19, 17};
static const uint64_t kExpectedImageDigests[kLevels] = {
    UINT64_C(0xa71a34a433733ba4), UINT64_C(0xee8d0f6e9245bcd0),
    UINT64_C(0x40b97eb940e3e7d1), UINT64_C(0xa274001b8653ee49),
    UINT64_C(0x2b3a5f5496d20e47), UINT64_C(0x96c38611b18a7266),
    UINT64_C(0x41b52a82b7356594), UINT64_C(0x248e5fc98bc6ba72),
};
static const uint64_t kExpectedLevelDigests[kLevels] = {
    UINT64_C(0xbd5edebf6facc24c), UINT64_C(0xd2dffa4ce2dd9573),
    UINT64_C(0x96d9a8b4270222e0), UINT64_C(0x674589769a093721),
    UINT64_C(0x73468edd91c566c3), UINT64_C(0x3000603e34597da1),
    UINT64_C(0xa397d47b3233ca46), UINT64_C(0xdeed28aaa79d5f29),
};

extern char _end;

/** @brief Return the fixed DDR command/result mailbox. */
static volatile OrbBenchmarkMailbox* benchmark_mailbox(void) {
    return (volatile OrbBenchmarkMailbox*)(UINTPTR)kMailboxAddress;
}

/** @brief Return the 8-byte-aligned input image consumed by both PL accelerators. */
static const uint8_t* input_pixels(void) {
    return (const uint8_t*)(UINTPTR)kAlignedInputAddress;
}

/** @brief Return one of the two ping-pong pyramid buffers. */
static uint8_t* pyramid_buffer(int index) {
    return (uint8_t*)(UINTPTR)(index == 0 ? kPyramidBuffer0Address : kPyramidBuffer1Address);
}

/** @brief Return the shared four-region 16-bit pixel workspace. */
static uint16_t* pixel_workspace(void) {
    return (uint16_t*)(UINTPTR)kPixelWorkspaceAddress;
}

/** @brief Return the packed candidate, selected, and cell-winner workspace. */
static OrbPoint* point_workspace(void) {
    return (OrbPoint*)(UINTPTR)kPointWorkspaceAddress;
}

/** @brief Return the selected-feature and metadata-trailer workspace. */
static OrbFeature* feature_workspace(void) {
    return (OrbFeature*)(UINTPTR)kFeatureWorkspaceAddress;
}

/** @brief Flush the complete mailbox so XSCT observes the latest PS writes. */
static void publish_mailbox(void) {
    Xil_DCacheFlushRange((INTPTR)kMailboxAddress, sizeof(OrbBenchmarkMailbox));
}

/** @brief Return the validated requested iteration count. */
static uint32_t read_requested_iterations(void) {
    volatile OrbBenchmarkMailbox* const mailbox = benchmark_mailbox();
    Xil_DCacheInvalidateRange((INTPTR)kMailboxAddress, sizeof(OrbBenchmarkMailbox));
    if (mailbox->command_magic == kMailboxCommandMagic &&
        mailbox->requested_iterations >= 1 &&
        mailbox->requested_iterations <= kMaximumBenchmarkIterations) {
        return mailbox->requested_iterations;
    }
    return 1;
}

/** @brief Clear and publish the RUNNING mailbox state. */
static void initialize_mailbox(uint32_t iterations) {
    volatile OrbBenchmarkMailbox* const mailbox = benchmark_mailbox();
    volatile uint32_t* const words = (volatile uint32_t*)mailbox;
    size_t index;
    for (index = 0; index < sizeof(*mailbox) / sizeof(uint32_t); ++index) words[index] = 0;
    mailbox->result_magic = kMailboxResultMagic;
    mailbox->schema_version = kMailboxSchemaVersion;
    mailbox->status = kMailboxRunning;
    mailbox->requested_iterations = iterations;
    mailbox->timer_hz = (uint32_t)COUNTS_PER_SECOND;
    mailbox->fabric_clock_hz = kFabricClockHz;
    publish_mailbox();
}

/** @brief Publish a terminal failure and return its stable error code. */
static int publish_failure(uint32_t error, uint32_t detail, uint32_t iteration) {
    volatile OrbBenchmarkMailbox* const mailbox = benchmark_mailbox();
    mailbox->error_code = error;
    mailbox->failure_detail = detail;
    mailbox->failing_iteration = iteration;
    mailbox->status = kMailboxFail;
    publish_mailbox();
    return (int)error;
}

/** @brief Convert ARM global-timer ticks to integer microseconds. */
static uint64_t ticks_to_microseconds(uint64_t ticks) {
    return (ticks * UINT64_C(1000000) + COUNTS_PER_SECOND / 2) / COUNTS_PER_SECOND;
}

/** @brief Convert observed wall-clock ticks to estimated 100 MHz PL cycles. */
static uint64_t ticks_to_estimated_pl_cycles(uint64_t ticks) {
    return (ticks * kFabricClockHz + COUNTS_PER_SECOND / 2) / COUNTS_PER_SECOND;
}

/** @brief Convert integer microseconds per frame to FPS multiplied by 1000. */
static uint32_t microseconds_to_milli_fps(uint32_t average_us) {
    if (average_us == 0) return 0;
    return (uint32_t)((UINT64_C(1000000000) + average_us / 2) / average_us);
}

/** @brief Initialize one frame-level timing accumulator. */
static void initialize_perf_stats(OrbPerfStats* stats) {
    stats->config_total_ticks = 0;
    stats->wait_total_ticks = 0;
    stats->wait_min_ticks = UINT64_MAX;
    stats->wait_max_ticks = 0;
}

/** @brief Add one complete frame's configuration and wait intervals. */
static void record_frame_perf(OrbPerfStats* stats, uint64_t config_ticks, uint64_t wait_ticks) {
    stats->config_total_ticks += config_ticks;
    stats->wait_total_ticks += wait_ticks;
    if (wait_ticks < stats->wait_min_ticks) stats->wait_min_ticks = wait_ticks;
    if (wait_ticks > stats->wait_max_ticks) stats->wait_max_ticks = wait_ticks;
}

/** @brief Append one byte to a 64-bit FNV-1a digest. */
static uint64_t hash_byte(uint64_t hash, uint8_t value) {
    hash ^= value;
    return hash * UINT64_C(1099511628211);
}

/** @brief Append one 32-bit word in little-endian order. */
static uint64_t hash_u32(uint64_t hash, uint32_t value) {
    int byte;
    for (byte = 0; byte < 4; ++byte) hash = hash_byte(hash, (uint8_t)(value >> (8 * byte)));
    return hash;
}

/** @brief Append one 64-bit word as two little-endian 32-bit words. */
static uint64_t hash_u64(uint64_t hash, uint64_t value) {
    hash = hash_u32(hash, (uint32_t)value);
    return hash_u32(hash, (uint32_t)(value >> 32));
}

/** @brief Hash a complete active row-major image. */
static uint64_t digest_image(const uint8_t* image, int pixels) {
    uint64_t hash = UINT64_C(14695981039346656037);
    int index;
    for (index = 0; index < pixels; ++index) hash = hash_byte(hash, image[index]);
    return hash;
}

/**
 * @brief Validate the downloaded PGM and stage its pixels at an aligned address.
 * @return Zero when the header is valid and the aligned input is ready.
 *
 * The 64-bit resize master requires an 8-byte-aligned base address, whereas the
 * 15-byte PGM header leaves the downloaded payload unaligned. This one-time copy
 * occurs before benchmark timing and is not part of per-frame processing.
 */
static int validate_input_pgm(void) {
    static const uint8_t expected[kPgmHeaderBytes] = {
        'P', '5', '\n', '9', '6', '0', ' ', '7', '2', '0', '\n', '2', '5', '5', '\n'};
    const uint8_t* const source = (const uint8_t*)(UINTPTR)kInputPgmAddress;
    uint8_t* const aligned = (uint8_t*)(UINTPTR)kAlignedInputAddress;
    int index;
    Xil_DCacheInvalidateRange((INTPTR)kInputPgmAddress, kPgmHeaderBytes + kMaximumPixels);
    for (index = 0; index < kPgmHeaderBytes; ++index) {
        if (source[index] != expected[index]) return -1;
    }
    for (index = 0; index < kMaximumPixels; ++index) {
        aligned[index] = source[kPgmHeaderBytes + index];
    }
    Xil_DCacheFlushRange((INTPTR)kAlignedInputAddress, kMaximumPixels);
    return 0;
}

/**
 * @brief Poll the level core and an optional concurrent resize transaction.
 * @param level_accelerator Active ORB level accelerator.
 * @param resize_accelerator Resize accelerator, active when resize_active is nonzero.
 * @param resize_active Nonzero when a next-level resize was started.
 * @param timeout_seconds Shared bounded timeout in seconds.
 * @param level_done_ticks Completion timestamp of the ORB level core.
 * @param resize_done_ticks Completion timestamp of the resize core when active.
 * @return Zero after completion, or a timeout mask: bit 0 level and bit 1 resize.
 */
static int wait_for_accelerators(XOrb_level_hls* level_accelerator,
                                 XOrb_resize_hls* resize_accelerator,
                                 int resize_active,
                                 unsigned timeout_seconds,
                                 XTime* level_done_ticks,
                                 XTime* resize_done_ticks) {
    XTime start;
    XTime now;
    int level_done = 0;
    int resize_done = resize_active ? 0 : 1;
    XTime_GetTime(&start);
    do {
        if (!level_done && XOrb_level_hls_IsDone(level_accelerator)) {
            XTime_GetTime(level_done_ticks);
            level_done = 1;
        }
        if (!resize_done && XOrb_resize_hls_IsDone(resize_accelerator)) {
            XTime_GetTime(resize_done_ticks);
            resize_done = 1;
        }
        if (level_done && resize_done) return 0;
        XTime_GetTime(&now);
    } while ((now - start) < ((XTime)timeout_seconds * (XTime)COUNTS_PER_SECOND));
    return (level_done ? 0 : 1) | (resize_done ? 0 : 2);
}

/**
 * @brief Poll one isolated resize transaction with a bounded timeout.
 * @param resize_accelerator Active resize accelerator.
 * @param timeout_seconds Maximum polling time in seconds.
 * @param done_ticks Completion timestamp captured immediately after ap_done.
 * @return Zero after completion, or nonzero after timeout.
 */
static int wait_for_resize(XOrb_resize_hls* resize_accelerator,
                           unsigned timeout_seconds,
                           XTime* done_ticks) {
    XTime start;
    XTime now;
    XTime_GetTime(&start);
    do {
        if (XOrb_resize_hls_IsDone(resize_accelerator)) {
            XTime_GetTime(done_ticks);
            return 0;
        }
        XTime_GetTime(&now);
    } while ((now - start) < ((XTime)timeout_seconds * (XTime)COUNTS_PER_SECOND));
    return -1;
}

/** @brief Hash one level's externally visible result in the CPU-reference order. */
static uint64_t digest_level_outputs(int pixels, const OrbFeature* metadata) {
    const uint16_t* const score = pixel_workspace();
    const uint16_t* const gaussian = pixel_workspace() + 2 * kMaximumPixels;
    const OrbPoint* const candidates = point_workspace();
    const OrbPoint* const selected = point_workspace() + kCandidateCapacity;
    const OrbFeature* const features = feature_workspace();
    uint64_t hash = UINT64_C(14695981039346656037);
    int index;
    int descriptor_word;

    for (index = 0; index < pixels; ++index) hash = hash_byte(hash, (uint8_t)score[index]);
    for (index = 0; index < pixels; ++index) hash = hash_byte(hash, (uint8_t)gaussian[index]);
    hash = hash_u32(hash, (uint32_t)metadata->x);
    hash = hash_u32(hash, (uint32_t)metadata->y);
    hash = hash_u32(hash, (uint32_t)metadata->score);
    hash = hash_u32(hash, (uint32_t)metadata->level);
    hash = hash_u32(hash, (uint32_t)metadata->bin);
    hash = hash_u32(hash, (uint32_t)metadata->m10);
    hash = hash_u32(hash, (uint32_t)metadata->m01);
    hash = hash_u32(hash, metadata->angle_q16);
    hash = hash_u32(hash, metadata->descriptor[0]);
    hash = hash_u32(hash, metadata->descriptor[1]);
    for (index = 0; index < metadata->x; ++index) {
        hash = hash_u32(hash, (uint32_t)candidates[index].x);
        hash = hash_u32(hash, (uint32_t)candidates[index].y);
        hash = hash_u32(hash, (uint32_t)candidates[index].score);
        hash = hash_u32(hash, (uint32_t)candidates[index].tile);
    }
    for (index = 0; index < metadata->y; ++index) {
        const OrbPoint* const point = &selected[index];
        const OrbFeature* const feature = &features[index];
        hash = hash_u32(hash, (uint32_t)point->x);
        hash = hash_u32(hash, (uint32_t)point->y);
        hash = hash_u32(hash, (uint32_t)point->score);
        hash = hash_u32(hash, (uint32_t)point->tile);
        hash = hash_u32(hash, (uint32_t)feature->x);
        hash = hash_u32(hash, (uint32_t)feature->y);
        hash = hash_u32(hash, (uint32_t)feature->score);
        hash = hash_u32(hash, (uint32_t)feature->level);
        hash = hash_u32(hash, (uint32_t)feature->bin);
        hash = hash_u32(hash, (uint32_t)feature->m10);
        hash = hash_u32(hash, (uint32_t)feature->m01);
        hash = hash_u32(hash, feature->angle_q16);
        for (descriptor_word = 0; descriptor_word < 8; ++descriptor_word) {
            hash = hash_u32(hash, feature->descriptor[descriptor_word]);
        }
    }
    return hash;
}

/**
 * @brief Execute and verify one complete 960x720, eight-level frame.
 * @return A stable OrbMailboxError code, or kErrorNone on success.
 */
static int run_full_frame_once(XOrb_resize_hls* resize_accelerator,
                               XOrb_level_hls* level_accelerator,
                               OrbPerfStats* resize_stats,
                               OrbPerfStats* level_stats,
                               uint64_t* image_digest,
                               uint64_t* level_digest,
                               uint64_t* verification_ticks,
                               uint64_t* pipeline_ticks,
                               uint32_t* total_candidates,
                               uint32_t* total_selected,
                               uint64_t resize_level_ticks[kLevels],
                               uint64_t orb_level_ticks[kLevels],
                               uint32_t* detail) {
    const uint8_t* current_image = input_pixels();
    uint16_t* const pixels = pixel_workspace();
    OrbPoint* const points = point_workspace();
    OrbFeature* const features = feature_workspace();
    uint64_t resize_config_ticks = 0;
    uint64_t resize_wait_ticks = 0;
    uint64_t level_config_ticks = 0;
    uint64_t level_wait_ticks = 0;
    uint64_t pipeline_frame_ticks = 0;
    uint64_t image_aggregate = UINT64_C(14695981039346656037);
    uint64_t level_aggregate = UINT64_C(14695981039346656037);
    uint32_t candidates_sum = 0;
    uint32_t selected_sum = 0;
    int level;

    for (level = 0; level < kLevels; ++level) {
        const int width = kWidths[level];
        const int height = kHeights[level];
        const int active_pixels = width * height;
        const int resize_active = level + 1 < kLevels;
        uint8_t* const next_image = resize_active ? pyramid_buffer(level & 1) : (uint8_t*)0;
        uint64_t observed_image_digest;
        uint64_t observed_level_digest;
        XTime stage_start;
        XTime stage_end;
        XTime level_config_start;
        XTime level_config_end;
        XTime level_wait_start;
        XTime level_wait_end;
        XTime resize_config_start = 0;
        XTime resize_config_end = 0;
        XTime resize_wait_start = 0;
        XTime resize_wait_end = 0;
        XTime verify_start;
        XTime verify_end;
        const OrbFeature* metadata;
        int status;

        XTime_GetTime(&verify_start);
        Xil_DCacheInvalidateRange((INTPTR)current_image, (u32)active_pixels);
        observed_image_digest = digest_image(current_image, active_pixels);
        XTime_GetTime(&verify_end);
        *verification_ticks += verify_end - verify_start;
        if (observed_image_digest != kExpectedImageDigests[level]) {
            *detail = ((uint32_t)level << 24) | ((uint32_t)observed_image_digest & 0x00ffffffu);
            return kErrorResizeDigest;
        }
        image_aggregate = hash_u64(image_aggregate, observed_image_digest);

        // Configure both independent PL cores before starting them back to
        // back. The resize writes the next ping-pong image while ORB only
        // reads the current image, so the transactions are data independent.
        XTime_GetTime(&stage_start);
        XTime_GetTime(&level_config_start);
        XOrb_level_hls_Set_image_r(level_accelerator, (u32)(UINTPTR)current_image);
        XOrb_level_hls_Set_pixel_work(level_accelerator, (u32)(UINTPTR)pixels);
        XOrb_level_hls_Set_gaussian_work(
            level_accelerator, (u32)(UINTPTR)(pixels + 2 * kMaximumPixels));
        XOrb_level_hls_Set_point_work(level_accelerator, (u32)(UINTPTR)points);
        XOrb_level_hls_Set_features(level_accelerator, (u32)(UINTPTR)features);
        XOrb_level_hls_Set_width(level_accelerator, width);
        XOrb_level_hls_Set_height(level_accelerator, height);
        XOrb_level_hls_Set_budget(level_accelerator, kBudgets[level]);
        XOrb_level_hls_Set_high_threshold(level_accelerator, 20);
        XOrb_level_hls_Set_low_threshold(level_accelerator, 7);
        XOrb_level_hls_Set_dual_threshold(level_accelerator, 1);
        XOrb_level_hls_Set_level(level_accelerator, level);
        XOrb_level_hls_Set_pixel_capacity(level_accelerator, kMaximumPixels);
        XOrb_level_hls_Set_candidate_capacity(level_accelerator, kCandidateCapacity);
        XOrb_level_hls_Set_selected_capacity(level_accelerator, kSelectedCapacity);
        XOrb_level_hls_Set_cell_capacity(level_accelerator, kCellCapacity);
        XOrb_level_hls_Set_feature_capacity(level_accelerator, kBudgets[level]);
        XTime_GetTime(&level_config_end);
        level_config_ticks += level_config_end - level_config_start;

        if (resize_active) {
            XTime_GetTime(&resize_config_start);
            XOrb_resize_hls_Set_source(resize_accelerator, (u32)(UINTPTR)current_image);
            XOrb_resize_hls_Set_destination(resize_accelerator, (u32)(UINTPTR)next_image);
            XOrb_resize_hls_Set_source_width(resize_accelerator, width);
            XOrb_resize_hls_Set_source_height(resize_accelerator, height);
            XOrb_resize_hls_Set_destination_width(resize_accelerator, kWidths[level + 1]);
            XOrb_resize_hls_Set_destination_height(resize_accelerator, kHeights[level + 1]);
            XTime_GetTime(&resize_config_end);
            resize_config_ticks += resize_config_end - resize_config_start;
        }

        XOrb_level_hls_Start(level_accelerator);
        XTime_GetTime(&level_wait_start);
        if (resize_active) {
            XOrb_resize_hls_Start(resize_accelerator);
            XTime_GetTime(&resize_wait_start);
        }
        status = wait_for_accelerators(
            level_accelerator, resize_accelerator, resize_active, 120,
            &level_wait_end, &resize_wait_end);
        if (status & 1) {
            *detail = (uint32_t)level;
            return kErrorLevelTimeout;
        }
        if (status & 2) {
            *detail = (uint32_t)(level + 1);
            return kErrorResizeTimeout;
        }
        level_wait_ticks += level_wait_end - level_wait_start;
        orb_level_ticks[level] += level_wait_end - level_wait_start;
        stage_end = level_wait_end;
        if (resize_active) {
            resize_wait_ticks += resize_wait_end - resize_wait_start;
            resize_level_ticks[level + 1] += resize_wait_end - resize_wait_start;
            if (resize_wait_end > stage_end) stage_end = resize_wait_end;
        }
        pipeline_frame_ticks += stage_end - stage_start;

        status = (int)XOrb_level_hls_Get_return(level_accelerator);
        if (status != 0) {
            *detail = ((uint32_t)level << 16) | ((uint32_t)status & 0xffffu);
            return kErrorLevelAccelerator;
        }
        if (resize_active) {
            status = (int)XOrb_resize_hls_Get_return(resize_accelerator);
            if (status != 0) {
                *detail = ((uint32_t)(level + 1) << 16) | ((uint32_t)status & 0xffffu);
                return kErrorResizeAccelerator;
            }
        }

        XTime_GetTime(&verify_start);
        Xil_DCacheInvalidateRange(
            (INTPTR)features, (u32)((kBudgets[level] + 1) * sizeof(OrbFeature)));
        metadata = &features[kBudgets[level]];
        if (metadata->x < 0 || metadata->x > kCandidateCapacity ||
            metadata->y < 0 || metadata->y > kBudgets[level]) {
            XTime_GetTime(&verify_end);
            *verification_ticks += verify_end - verify_start;
            *detail = ((uint32_t)level << 24) |
                      (((uint32_t)metadata->x & 0xfffu) << 12) |
                      ((uint32_t)metadata->y & 0xfffu);
            return kErrorLevelDigest;
        }
        Xil_DCacheInvalidateRange((INTPTR)pixels, (u32)(active_pixels * sizeof(uint16_t)));
        Xil_DCacheInvalidateRange(
            (INTPTR)(pixels + 2 * kMaximumPixels), (u32)(active_pixels * sizeof(uint16_t)));
        Xil_DCacheInvalidateRange((INTPTR)points, (u32)(metadata->x * sizeof(OrbPoint)));
        Xil_DCacheInvalidateRange(
            (INTPTR)(points + kCandidateCapacity), (u32)(metadata->y * sizeof(OrbPoint)));
        observed_level_digest = digest_level_outputs(active_pixels, metadata);
        XTime_GetTime(&verify_end);
        *verification_ticks += verify_end - verify_start;
        if (metadata->x != kExpectedCandidates[level] ||
            metadata->y != kExpectedSelected[level] ||
            metadata->angle_q16 != kAlgorithmVersion ||
            (metadata->descriptor[0] & kCapabilityDualThreshold) == 0 ||
            metadata->descriptor[1] != kMaxGridTopK ||
            observed_level_digest != kExpectedLevelDigests[level]) {
            *detail = ((uint32_t)level << 24) | ((uint32_t)observed_level_digest & 0x00ffffffu);
            return kErrorLevelDigest;
        }
        level_aggregate = hash_u64(level_aggregate, observed_level_digest);
        candidates_sum += (uint32_t)metadata->x;
        selected_sum += (uint32_t)metadata->y;
        if (resize_active) current_image = next_image;
    }

    record_frame_perf(resize_stats, resize_config_ticks, resize_wait_ticks);
    record_frame_perf(level_stats, level_config_ticks, level_wait_ticks);
    *pipeline_ticks += pipeline_frame_ticks;
    *image_digest = image_aggregate;
    *level_digest = level_aggregate;
    *total_candidates = candidates_sum;
    *total_selected = selected_sum;
    return kErrorNone;
}

/**
 * @brief Profile the level and resize accelerators without mutual DDR contention.
 *
 * The level core runs first for each pyramid level. The resize core then creates
 * the next level while the level core is idle. Results are not CPU-verified here
 * because the preceding concurrent benchmark already verifies the same inputs and
 * accelerator configuration byte-for-byte.
 *
 * @param resize_accelerator Initialized resize accelerator.
 * @param level_accelerator Initialized ORB level accelerator.
 * @param isolated_resize_ticks Per-level resize durations; index zero remains zero.
 * @param isolated_orb_ticks Per-level ORB durations.
 * @param detail Stable failing-level detail written on error.
 * @return A stable OrbMailboxError code, or kErrorNone on success.
 */
static int run_isolated_profile_once(XOrb_resize_hls* resize_accelerator,
                                     XOrb_level_hls* level_accelerator,
                                     uint64_t isolated_resize_ticks[kLevels],
                                     uint64_t isolated_orb_ticks[kLevels],
                                     uint32_t* detail) {
    const uint8_t* current_image = input_pixels();
    uint16_t* const pixels = pixel_workspace();
    OrbPoint* const points = point_workspace();
    OrbFeature* const features = feature_workspace();
    int level;

    for (level = 0; level < kLevels; ++level) {
        const int width = kWidths[level];
        const int height = kHeights[level];
        const int resize_active = level + 1 < kLevels;
        uint8_t* const next_image = resize_active ? pyramid_buffer(level & 1) : (uint8_t*)0;
        XTime start;
        XTime end;
        int status;

        XOrb_level_hls_Set_image_r(level_accelerator, (u32)(UINTPTR)current_image);
        XOrb_level_hls_Set_pixel_work(level_accelerator, (u32)(UINTPTR)pixels);
        XOrb_level_hls_Set_gaussian_work(
            level_accelerator, (u32)(UINTPTR)(pixels + 2 * kMaximumPixels));
        XOrb_level_hls_Set_point_work(level_accelerator, (u32)(UINTPTR)points);
        XOrb_level_hls_Set_features(level_accelerator, (u32)(UINTPTR)features);
        XOrb_level_hls_Set_width(level_accelerator, width);
        XOrb_level_hls_Set_height(level_accelerator, height);
        XOrb_level_hls_Set_budget(level_accelerator, kBudgets[level]);
        XOrb_level_hls_Set_high_threshold(level_accelerator, 20);
        XOrb_level_hls_Set_low_threshold(level_accelerator, 7);
        XOrb_level_hls_Set_dual_threshold(level_accelerator, 1);
        XOrb_level_hls_Set_level(level_accelerator, level);
        XOrb_level_hls_Set_pixel_capacity(level_accelerator, kMaximumPixels);
        XOrb_level_hls_Set_candidate_capacity(level_accelerator, kCandidateCapacity);
        XOrb_level_hls_Set_selected_capacity(level_accelerator, kSelectedCapacity);
        XOrb_level_hls_Set_cell_capacity(level_accelerator, kCellCapacity);
        XOrb_level_hls_Set_feature_capacity(level_accelerator, kBudgets[level]);

        XOrb_level_hls_Start(level_accelerator);
        XTime_GetTime(&start);
        status = wait_for_accelerators(
            level_accelerator, resize_accelerator, 0, 120, &end, &end);
        if (status != 0) {
            *detail = (uint32_t)level;
            return kErrorLevelTimeout;
        }
        isolated_orb_ticks[level] = end - start;
        status = (int)XOrb_level_hls_Get_return(level_accelerator);
        if (status != 0) {
            *detail = ((uint32_t)level << 16) | ((uint32_t)status & 0xffffu);
            return kErrorLevelAccelerator;
        }

        if (resize_active) {
            XOrb_resize_hls_Set_source(resize_accelerator, (u32)(UINTPTR)current_image);
            XOrb_resize_hls_Set_destination(resize_accelerator, (u32)(UINTPTR)next_image);
            XOrb_resize_hls_Set_source_width(resize_accelerator, width);
            XOrb_resize_hls_Set_source_height(resize_accelerator, height);
            XOrb_resize_hls_Set_destination_width(resize_accelerator, kWidths[level + 1]);
            XOrb_resize_hls_Set_destination_height(resize_accelerator, kHeights[level + 1]);

            XOrb_resize_hls_Start(resize_accelerator);
            XTime_GetTime(&start);
            if (wait_for_resize(resize_accelerator, 120, &end) != 0) {
                *detail = (uint32_t)(level + 1);
                return kErrorResizeTimeout;
            }
            isolated_resize_ticks[level + 1] = end - start;
            status = (int)XOrb_resize_hls_Get_return(resize_accelerator);
            if (status != 0) {
                *detail = ((uint32_t)(level + 1) << 16) | ((uint32_t)status & 0xffffu);
                return kErrorResizeAccelerator;
            }
            current_image = next_image;
        }
    }
    return kErrorNone;
}

/**
 * @brief Measure four cumulative ORB execution cut points on every pyramid level.
 *
 * Runtime capacity checks provide deterministic early exits in the existing PL
 * bitstream: zero candidate capacity exits after NMS, zero selected capacity
 * exits at the first grid emission, and zero feature capacity exits immediately
 * before direction/descriptor generation. The fourth run is the complete path.
 * Subtracting adjacent cumulative times gives a coarse phase breakdown without
 * rebuilding or modifying the hardware core.
 *
 * @param resize_accelerator Initialized resize accelerator used to build levels.
 * @param level_accelerator Initialized ORB level accelerator under test.
 * @param stage_ticks Four cumulative time arrays indexed by stage then level.
 * @param detail Stable failing-stage/level detail written on error.
 * @return A stable OrbMailboxError code, or kErrorNone on success.
 */
static int run_cumulative_stage_profile_once(
    XOrb_resize_hls* resize_accelerator,
    XOrb_level_hls* level_accelerator,
    uint64_t stage_ticks[4][kLevels],
    uint32_t* detail) {
    const uint8_t* current_image = input_pixels();
    uint16_t* const pixels = pixel_workspace();
    OrbPoint* const points = point_workspace();
    OrbFeature* const features = feature_workspace();
    int level;

    for (level = 0; level < kLevels; ++level) {
        const int width = kWidths[level];
        const int height = kHeights[level];
        const int resize_active = level + 1 < kLevels;
        uint8_t* const next_image = resize_active ? pyramid_buffer(level & 1) : (uint8_t*)0;
        int stage;

        for (stage = 0; stage < 4; ++stage) {
            XTime start;
            XTime end;
            int status;

            XOrb_level_hls_Set_image_r(level_accelerator, (u32)(UINTPTR)current_image);
            XOrb_level_hls_Set_pixel_work(level_accelerator, (u32)(UINTPTR)pixels);
            XOrb_level_hls_Set_gaussian_work(
                level_accelerator, (u32)(UINTPTR)(pixels + 2 * kMaximumPixels));
            XOrb_level_hls_Set_point_work(level_accelerator, (u32)(UINTPTR)points);
            XOrb_level_hls_Set_features(level_accelerator, (u32)(UINTPTR)features);
            XOrb_level_hls_Set_width(level_accelerator, width);
            XOrb_level_hls_Set_height(level_accelerator, height);
            XOrb_level_hls_Set_budget(level_accelerator, kBudgets[level]);
            XOrb_level_hls_Set_high_threshold(level_accelerator, 20);
            XOrb_level_hls_Set_low_threshold(level_accelerator, 7);
            XOrb_level_hls_Set_dual_threshold(level_accelerator, 1);
            XOrb_level_hls_Set_level(level_accelerator, level);
            XOrb_level_hls_Set_pixel_capacity(level_accelerator, kMaximumPixels);
            XOrb_level_hls_Set_candidate_capacity(
                level_accelerator, stage == 0 ? 0 : kCandidateCapacity);
            XOrb_level_hls_Set_selected_capacity(
                level_accelerator, stage == 1 ? 0 : kSelectedCapacity);
            XOrb_level_hls_Set_cell_capacity(level_accelerator, kCellCapacity);
            XOrb_level_hls_Set_feature_capacity(
                level_accelerator, stage == 2 ? 0 : kBudgets[level]);

            XOrb_level_hls_Start(level_accelerator);
            XTime_GetTime(&start);
            status = wait_for_accelerators(
                level_accelerator, resize_accelerator, 0, 120, &end, &end);
            if (status != 0) {
                *detail = ((uint32_t)stage << 16) | (uint32_t)level;
                return kErrorLevelTimeout;
            }
            stage_ticks[stage][level] = end - start;
            status = (int)XOrb_level_hls_Get_return(level_accelerator);
            if ((stage == 0 && status != 2) ||
                ((stage == 1 || stage == 2) && status != 4) ||
                (stage == 3 && status != 0)) {
                *detail = ((uint32_t)stage << 24) |
                          ((uint32_t)level << 16) |
                          ((uint32_t)status & 0xffffu);
                return kErrorLevelAccelerator;
            }
        }

        if (resize_active) {
            XTime start;
            XTime end;
            int status;
            XOrb_resize_hls_Set_source(resize_accelerator, (u32)(UINTPTR)current_image);
            XOrb_resize_hls_Set_destination(resize_accelerator, (u32)(UINTPTR)next_image);
            XOrb_resize_hls_Set_source_width(resize_accelerator, width);
            XOrb_resize_hls_Set_source_height(resize_accelerator, height);
            XOrb_resize_hls_Set_destination_width(resize_accelerator, kWidths[level + 1]);
            XOrb_resize_hls_Set_destination_height(resize_accelerator, kHeights[level + 1]);
            XOrb_resize_hls_Start(resize_accelerator);
            XTime_GetTime(&start);
            if (wait_for_resize(resize_accelerator, 120, &end) != 0) {
                *detail = (uint32_t)(level + 1);
                return kErrorResizeTimeout;
            }
            status = (int)XOrb_resize_hls_Get_return(resize_accelerator);
            if (status != 0) {
                *detail = ((uint32_t)(level + 1) << 16) | ((uint32_t)status & 0xffffu);
                return kErrorResizeAccelerator;
            }
            current_image = next_image;
        }
    }
    return kErrorNone;
}

/** @brief Store final counts, digests, timing, and throughput in the DDR mailbox. */
static void finalize_mailbox(uint32_t iterations,
                             const OrbPerfStats* resize_stats,
                             const OrbPerfStats* level_stats,
                             uint64_t image_digest,
                             uint64_t level_digest,
                             uint64_t verification_ticks,
                             uint64_t pipeline_ticks,
                             uint64_t wall_ticks,
                             uint32_t candidates,
                             uint32_t selected,
                             const uint64_t resize_level_ticks[kLevels],
                             const uint64_t orb_level_ticks[kLevels],
                             const uint64_t isolated_resize_ticks[kLevels],
                             const uint64_t isolated_orb_ticks[kLevels],
                             const uint64_t stage_ticks[4][kLevels]) {
    volatile OrbBenchmarkMailbox* const mailbox = benchmark_mailbox();
    const uint64_t resize_config_avg = resize_stats->config_total_ticks / iterations;
    const uint64_t resize_wait_avg = resize_stats->wait_total_ticks / iterations;
    const uint64_t level_config_avg = level_stats->config_total_ticks / iterations;
    const uint64_t level_wait_avg = level_stats->wait_total_ticks / iterations;
    const uint64_t pipeline_avg = pipeline_ticks / iterations;
    const uint64_t total_us = ticks_to_microseconds(wall_ticks);

    mailbox->candidates = candidates;
    mailbox->selected = selected;
    mailbox->algorithm_version = kAlgorithmVersion;
    mailbox->capabilities = kCapabilityDualThreshold;
    mailbox->max_grid_top_k = kMaxGridTopK;
    mailbox->resize_digest_hi = (uint32_t)(image_digest >> 32);
    mailbox->resize_digest_lo = (uint32_t)image_digest;
    mailbox->level_digest_hi = (uint32_t)(level_digest >> 32);
    mailbox->level_digest_lo = (uint32_t)level_digest;
    mailbox->resize_config_avg_us = (uint32_t)ticks_to_microseconds(resize_config_avg);
    mailbox->resize_wait_avg_us = (uint32_t)ticks_to_microseconds(resize_wait_avg);
    mailbox->resize_wait_min_us = (uint32_t)ticks_to_microseconds(resize_stats->wait_min_ticks);
    mailbox->resize_wait_max_us = (uint32_t)ticks_to_microseconds(resize_stats->wait_max_ticks);
    mailbox->resize_est_cycles_avg = (uint32_t)ticks_to_estimated_pl_cycles(resize_wait_avg);
    mailbox->resize_fps_milli = microseconds_to_milli_fps(
        mailbox->resize_config_avg_us + mailbox->resize_wait_avg_us);
    mailbox->level_config_avg_us = (uint32_t)ticks_to_microseconds(level_config_avg);
    mailbox->level_wait_avg_us = (uint32_t)ticks_to_microseconds(level_wait_avg);
    mailbox->level_wait_min_us = (uint32_t)ticks_to_microseconds(level_stats->wait_min_ticks);
    mailbox->level_wait_max_us = (uint32_t)ticks_to_microseconds(level_stats->wait_max_ticks);
    mailbox->level_est_cycles_avg = (uint32_t)ticks_to_estimated_pl_cycles(level_wait_avg);
    mailbox->level_fps_milli = microseconds_to_milli_fps(
        mailbox->level_config_avg_us + mailbox->level_wait_avg_us);
    mailbox->pipeline_avg_us = (uint32_t)ticks_to_microseconds(pipeline_avg);
    mailbox->pipeline_fps_milli = microseconds_to_milli_fps(mailbox->pipeline_avg_us);
    mailbox->verification_avg_us =
        (uint32_t)ticks_to_microseconds(verification_ticks / iterations);
    mailbox->wall_avg_us = (uint32_t)ticks_to_microseconds(wall_ticks / iterations);
    mailbox->wall_fps_milli = microseconds_to_milli_fps(mailbox->wall_avg_us);
    mailbox->total_elapsed_us_hi = (uint32_t)(total_us >> 32);
    mailbox->total_elapsed_us_lo = (uint32_t)total_us;
    {
        int level;
        uint64_t isolated_resize_total = 0;
        uint64_t isolated_orb_total = 0;
        for (level = 1; level < kLevels; ++level) {
            mailbox->reserved[level - 1] = (uint32_t)ticks_to_microseconds(
                resize_level_ticks[level] / iterations);
            mailbox->reserved[14 + level] = (uint32_t)ticks_to_microseconds(
                isolated_resize_ticks[level]);
            isolated_resize_total += isolated_resize_ticks[level];
        }
        for (level = 0; level < kLevels; ++level) {
            mailbox->reserved[7 + level] = (uint32_t)ticks_to_microseconds(
                orb_level_ticks[level] / iterations);
            mailbox->reserved[22 + level] = (uint32_t)ticks_to_microseconds(
                isolated_orb_ticks[level]);
            isolated_orb_total += isolated_orb_ticks[level];
        }
        mailbox->reserved[30] = (uint32_t)ticks_to_microseconds(isolated_resize_total);
        mailbox->reserved[31] = (uint32_t)ticks_to_microseconds(isolated_orb_total);
        mailbox->reserved[32] = (uint32_t)ticks_to_microseconds(
            isolated_resize_total + isolated_orb_total);
        {
            int stage;
            for (stage = 0; stage < 4; ++stage) {
                uint64_t stage_total = 0;
                for (level = 0; level < kLevels; ++level) {
                    mailbox->reserved[33 + stage * kLevels + level] =
                        (uint32_t)ticks_to_microseconds(stage_ticks[stage][level]);
                    stage_total += stage_ticks[stage][level];
                }
                mailbox->reserved[65 + stage] =
                    (uint32_t)ticks_to_microseconds(stage_total);
            }
        }
    }
    mailbox->status = kMailboxPass;
    publish_mailbox();
}

/** @brief Run the full-frame bit-exact regression and average-performance benchmark. */
int main(void) {
    volatile OrbBenchmarkMailbox* const mailbox = benchmark_mailbox();
    XOrb_resize_hls resize_accelerator;
    XOrb_level_hls level_accelerator;
    OrbPerfStats resize_stats;
    OrbPerfStats level_stats;
    uint64_t image_digest = 0;
    uint64_t level_digest = 0;
    uint64_t verification_ticks = 0;
    uint64_t pipeline_ticks = 0;
    uint64_t wall_ticks = 0;
    uint32_t candidates = 0;
    uint32_t selected = 0;
    uint32_t iterations;
    uint32_t iteration;
    uint32_t detail = 0;
    uint64_t resize_level_ticks[kLevels] = {0};
    uint64_t orb_level_ticks[kLevels] = {0};
    uint64_t isolated_resize_ticks[kLevels] = {0};
    uint64_t isolated_orb_ticks[kLevels] = {0};
    uint64_t stage_ticks[4][kLevels] = {{0}};
    int status;

    if ((UINTPTR)&_end >= (UINTPTR)kMailboxAddress) {
        return publish_failure(kErrorMailboxOverlap, (uint32_t)(UINTPTR)&_end, 0);
    }
    iterations = read_requested_iterations();
    initialize_mailbox(iterations);
    if (validate_input_pgm() != 0) return publish_failure(kErrorInputPgm, kInputPgmAddress, 0);

    status = XOrb_resize_hls_Initialize(&resize_accelerator, XPAR_ORB_RESIZE_DEVICE_ID);
    if (status != XST_SUCCESS) {
        return publish_failure(kErrorResizeInitialize, (uint32_t)status, 0);
    }
    status = XOrb_level_hls_Initialize(&level_accelerator, XPAR_ORB_LEVEL_DEVICE_ID);
    if (status != XST_SUCCESS) {
        return publish_failure(kErrorLevelInitialize, (uint32_t)status, 0);
    }

    initialize_perf_stats(&resize_stats);
    initialize_perf_stats(&level_stats);
    for (iteration = 1; iteration <= iterations; ++iteration) {
        XTime frame_start;
        XTime frame_end;
        XTime_GetTime(&frame_start);
        detail = 0;
        status = run_full_frame_once(
            &resize_accelerator, &level_accelerator, &resize_stats, &level_stats,
            &image_digest, &level_digest, &verification_ticks, &pipeline_ticks,
            &candidates, &selected, resize_level_ticks, orb_level_ticks, &detail);
        XTime_GetTime(&frame_end);
        wall_ticks += frame_end - frame_start;
        if (status != kErrorNone) {
            mailbox->resize_digest_hi = (uint32_t)(image_digest >> 32);
            mailbox->resize_digest_lo = (uint32_t)image_digest;
            mailbox->level_digest_hi = (uint32_t)(level_digest >> 32);
            mailbox->level_digest_lo = (uint32_t)level_digest;
            return publish_failure((uint32_t)status, detail, iteration);
        }
        mailbox->completed_iterations = iteration;
        if ((iteration % 10u) == 0u || iteration == iterations) publish_mailbox();
    }

    detail = 0;
    status = run_isolated_profile_once(
        &resize_accelerator, &level_accelerator,
        isolated_resize_ticks, isolated_orb_ticks, &detail);
    if (status != kErrorNone) {
        return publish_failure((uint32_t)status, detail, iterations);
    }

    detail = 0;
    status = run_cumulative_stage_profile_once(
        &resize_accelerator, &level_accelerator, stage_ticks, &detail);
    if (status != kErrorNone) {
        return publish_failure((uint32_t)status, detail, iterations);
    }

    finalize_mailbox(iterations, &resize_stats, &level_stats,
                     image_digest, level_digest, verification_ticks, pipeline_ticks, wall_ticks,
                     candidates, selected, resize_level_ticks, orb_level_ticks,
                     isolated_resize_ticks, isolated_orb_ticks, stage_ticks);
    xil_printf("ORB_FULLFRAME_PASS iterations=%d candidates=%d selected=%d pipeline_us=%d\r\n",
               iterations, candidates, selected, mailbox->pipeline_avg_us);
    return 0;
}
