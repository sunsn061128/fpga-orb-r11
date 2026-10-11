/**
 * @file orb_fast_benchmark.c
 * @brief Bit-exact and performance benchmark for resize plus streaming FAST.
 */

#include "xorb_fast_stream_hls.h"
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
    kPgmHeaderBytes = 15,
    kMaximumBenchmarkIterations = 1000,
    kFabricClockHz = 100000000,
    kMailboxAddress = 0x01000000,
    kInputPgmAddress = 0x02000000,
    kPyramidBuffer0Address = 0x02400000,
    kPyramidBuffer1Address = 0x02600000,
    kAlignedInputAddress = 0x02800000,
    kCandidateAddress = 0x04000000,
    kMailboxCommandMagic = 0x4f524643,
    kMailboxResultMagic = 0x4f524652,
    kMailboxSchemaVersion = 1,
};

enum OrbFastMailboxStatus {
    kMailboxEmpty = 0,
    kMailboxRunning = 1,
    kMailboxPass = 2,
    kMailboxFail = 3,
};

enum OrbFastError {
    kErrorNone = 0,
    kErrorMailboxOverlap = 1,
    kErrorInputPgm = 10,
    kErrorResizeInitialize = 101,
    kErrorFastInitialize = 102,
    kErrorResizeTimeout = 201,
    kErrorFastTimeout = 202,
    kErrorResizeAccelerator = 203,
    kErrorFastAccelerator = 204,
    kErrorImageDigest = 205,
    kErrorCandidateCount = 206,
    kErrorCandidateDigest = 207,
};

/** @brief Candidate layout shared with the 128-bit FAST AXI master. */
typedef struct {
    int32_t x;
    int32_t y;
    int32_t score;
    int32_t tile;
} OrbPoint;

/** @brief Fixed 512-byte result block read directly through JTAG by XSCT. */
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
    uint32_t fast_digest_hi;
    uint32_t fast_digest_lo;
    uint32_t image_digest_hi;
    uint32_t image_digest_lo;
    uint32_t fast_config_avg_us;
    uint32_t fast_wait_avg_us;
    uint32_t fast_wait_min_us;
    uint32_t fast_wait_max_us;
    uint32_t fast_fps_milli;
    uint32_t resize_config_avg_us;
    uint32_t resize_wait_avg_us;
    uint32_t resize_wait_min_us;
    uint32_t resize_wait_max_us;
    uint32_t resize_fps_milli;
    uint32_t pipeline_avg_us;
    uint32_t pipeline_fps_milli;
    uint32_t verification_avg_us;
    uint32_t wall_avg_us;
    uint32_t wall_fps_milli;
    uint32_t total_elapsed_us_hi;
    uint32_t total_elapsed_us_lo;
    uint32_t failure_detail;
    uint32_t reserved[95];
} OrbFastMailbox;

/** @brief Per-frame accumulated accelerator configuration and wait timing. */
typedef struct {
    uint64_t config_total_ticks;
    uint64_t wait_total_ticks;
    uint64_t wait_min_ticks;
    uint64_t wait_max_ticks;
} OrbPerfStats;

typedef char OrbPointSizeMustBe16[(sizeof(OrbPoint) == 16) ? 1 : -1];
typedef char OrbMailboxSizeMustBe512[(sizeof(OrbFastMailbox) == 512) ? 1 : -1];
typedef char OrbWorkspaceMustFitDdr[
    ((kCandidateAddress + kCandidateCapacity * sizeof(OrbPoint) - 1) <=
      XPAR_PS7_DDR_0_S_AXI_HIGHADDR)
        ? 1
        : -1];

static const int kWidths[kLevels] = {960, 800, 667, 556, 463, 386, 322, 268};
static const int kHeights[kLevels] = {720, 600, 500, 417, 347, 289, 241, 201};
static const int kExpectedCandidates[kLevels] = {3873, 2595, 1765, 1128, 776, 492, 296, 153};
static const uint64_t kExpectedImageDigests[kLevels] = {
    UINT64_C(0xa71a34a433733ba4), UINT64_C(0xee8d0f6e9245bcd0),
    UINT64_C(0x40b97eb940e3e7d1), UINT64_C(0xa274001b8653ee49),
    UINT64_C(0x2b3a5f5496d20e47), UINT64_C(0x96c38611b18a7266),
    UINT64_C(0x41b52a82b7356594), UINT64_C(0x248e5fc98bc6ba72),
};
static const uint64_t kExpectedFastDigests[kLevels] = {
    UINT64_C(0xd17e55a15fa9919a), UINT64_C(0x601c79093cd214e4),
    UINT64_C(0x13a06740fd796735), UINT64_C(0xb133dc5fd8ff66a8),
    UINT64_C(0x38ee90a529fc9edd), UINT64_C(0x0de84359615e9e39),
    UINT64_C(0xa1012516a2e0a347), UINT64_C(0x351b80fcb07d44af),
};

extern char _end;

/** @brief Return the fixed DDR command/result mailbox. */
static volatile OrbFastMailbox* benchmark_mailbox(void) {
    return (volatile OrbFastMailbox*)(UINTPTR)kMailboxAddress;
}

/** @brief Return the aligned level-zero input image. */
static const uint8_t* input_pixels(void) {
    return (const uint8_t*)(UINTPTR)kAlignedInputAddress;
}

/** @brief Return one of the two resize ping-pong buffers. */
static uint8_t* pyramid_buffer(int index) {
    return (uint8_t*)(UINTPTR)(index == 0 ? kPyramidBuffer0Address : kPyramidBuffer1Address);
}

/** @brief Return the maximum-capacity candidate output buffer. */
static OrbPoint* candidate_buffer(void) {
    return (OrbPoint*)(UINTPTR)kCandidateAddress;
}

/** @brief Flush the result mailbox so XSCT observes the latest PS writes. */
static void publish_mailbox(void) {
    Xil_DCacheFlushRange((INTPTR)kMailboxAddress, sizeof(OrbFastMailbox));
}

/** @brief Return the requested iteration count after bounds checking. */
static uint32_t read_requested_iterations(void) {
    volatile OrbFastMailbox* const mailbox = benchmark_mailbox();
    Xil_DCacheInvalidateRange((INTPTR)kMailboxAddress, sizeof(OrbFastMailbox));
    if (mailbox->command_magic == kMailboxCommandMagic &&
        mailbox->requested_iterations >= 1 &&
        mailbox->requested_iterations <= kMaximumBenchmarkIterations) {
        return mailbox->requested_iterations;
    }
    return 1;
}

/** @brief Clear the mailbox and publish the RUNNING state. */
static void initialize_mailbox(uint32_t iterations) {
    volatile OrbFastMailbox* const mailbox = benchmark_mailbox();
    volatile uint32_t* const words = (volatile uint32_t*)mailbox;
    size_t index;
    for (index = 0; index < sizeof(*mailbox) / sizeof(uint32_t); ++index) words[index] = 0;
    mailbox->requested_iterations = iterations;
    mailbox->result_magic = kMailboxResultMagic;
    mailbox->schema_version = kMailboxSchemaVersion;
    mailbox->status = kMailboxRunning;
    mailbox->timer_hz = (uint32_t)COUNTS_PER_SECOND;
    mailbox->fabric_clock_hz = kFabricClockHz;
    publish_mailbox();
}

/** @brief Publish a terminal failure and return its stable error code. */
static int publish_failure(uint32_t error, uint32_t detail, uint32_t iteration) {
    volatile OrbFastMailbox* const mailbox = benchmark_mailbox();
    mailbox->error_code = error;
    mailbox->failure_detail = detail;
    mailbox->failing_iteration = iteration;
    mailbox->status = kMailboxFail;
    publish_mailbox();
    return (int)error;
}

/** @brief Convert global-timer ticks to rounded integer microseconds. */
static uint64_t ticks_to_microseconds(uint64_t ticks) {
    return (ticks * UINT64_C(1000000) + COUNTS_PER_SECOND / 2) / COUNTS_PER_SECOND;
}

/** @brief Convert microseconds per frame to frames per second times 1000. */
static uint32_t microseconds_to_milli_fps(uint32_t average_us) {
    if (average_us == 0) return 0;
    return (uint32_t)((UINT64_C(1000000000) + average_us / 2) / average_us);
}

/** @brief Reset a frame-level timing accumulator. */
static void initialize_perf_stats(OrbPerfStats* stats) {
    stats->config_total_ticks = 0;
    stats->wait_total_ticks = 0;
    stats->wait_min_ticks = UINT64_MAX;
    stats->wait_max_ticks = 0;
}

/** @brief Accumulate one frame's configuration and accelerator wait times. */
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

/** @brief Append one 32-bit value in little-endian order. */
static uint64_t hash_u32(uint64_t hash, uint32_t value) {
    int byte;
    for (byte = 0; byte < 4; ++byte) hash = hash_byte(hash, (uint8_t)(value >> (8 * byte)));
    return hash;
}

/** @brief Append one 64-bit value in little-endian word order. */
static uint64_t hash_u64(uint64_t hash, uint64_t value) {
    hash = hash_u32(hash, (uint32_t)value);
    return hash_u32(hash, (uint32_t)(value >> 32));
}

/** @brief Hash all active row-major pixels in one pyramid level. */
static uint64_t digest_image(const uint8_t* image, int pixels) {
    uint64_t hash = UINT64_C(14695981039346656037);
    int index;
    for (index = 0; index < pixels; ++index) hash = hash_byte(hash, image[index]);
    return hash;
}

/** @brief Hash the ordered 128-bit candidate records emitted by the FAST core. */
static uint64_t digest_candidates(const OrbPoint* points, int count) {
    uint64_t hash = UINT64_C(14695981039346656037);
    int index;
    for (index = 0; index < count; ++index) {
        hash = hash_u32(hash, (uint32_t)points[index].x);
        hash = hash_u32(hash, (uint32_t)points[index].y);
        hash = hash_u32(hash, (uint32_t)points[index].score);
        hash = hash_u32(hash, (uint32_t)points[index].tile);
    }
    return hash;
}

/**
 * @brief Validate the downloaded PGM and copy its pixels to an aligned buffer.
 * @return Zero when the fixed 960x720 binary-PGM header and payload are valid.
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
 * @brief Poll FAST and an optional concurrent resize until both complete.
 * @param fast_accelerator Active streaming-FAST accelerator.
 * @param resize_accelerator Active resize accelerator when resize_active is nonzero.
 * @param resize_active Nonzero when the next pyramid level is being generated.
 * @param timeout_seconds Shared transaction timeout.
 * @param fast_done Completion timestamp for FAST.
 * @param resize_done Completion timestamp for resize when active.
 * @return Timeout mask: bit zero denotes FAST and bit one denotes resize.
 */
static int wait_for_accelerators(XOrb_fast_stream_hls* fast_accelerator,
                                 XOrb_resize_hls* resize_accelerator,
                                 int resize_active,
                                 unsigned timeout_seconds,
                                 XTime* fast_done,
                                 XTime* resize_done) {
    XTime start;
    XTime now;
    int fast_complete = 0;
    int resize_complete = resize_active ? 0 : 1;
    XTime_GetTime(&start);
    do {
        if (!fast_complete && XOrb_fast_stream_hls_IsDone(fast_accelerator)) {
            XTime_GetTime(fast_done);
            fast_complete = 1;
        }
        if (!resize_complete && XOrb_resize_hls_IsDone(resize_accelerator)) {
            XTime_GetTime(resize_done);
            resize_complete = 1;
        }
        if (fast_complete && resize_complete) return 0;
        XTime_GetTime(&now);
    } while ((now - start) < ((XTime)timeout_seconds * (XTime)COUNTS_PER_SECOND));
    return (fast_complete ? 0 : 1) | (resize_complete ? 0 : 2);
}

/**
 * @brief Execute and verify one complete eight-level resize plus FAST frame.
 * @return Stable OrbFastError code, or kErrorNone on success.
 */
static int run_frame(XOrb_resize_hls* resize_accelerator,
                     XOrb_fast_stream_hls* fast_accelerator,
                     OrbPerfStats* resize_stats,
                     OrbPerfStats* fast_stats,
                     uint64_t* image_digest,
                     uint64_t* fast_digest,
                     uint64_t* verification_ticks,
                     uint64_t* pipeline_ticks,
                     uint32_t* total_candidates,
                     uint64_t fast_level_ticks[kLevels],
                     uint64_t resize_level_ticks[kLevels],
                     uint32_t level_counts[kLevels],
                     uint64_t level_digests[kLevels],
                     uint32_t* detail) {
    const uint8_t* current_image = input_pixels();
    OrbPoint* const candidates = candidate_buffer();
    uint64_t image_aggregate = UINT64_C(14695981039346656037);
    uint64_t fast_aggregate = UINT64_C(14695981039346656037);
    uint64_t fast_config_ticks = 0;
    uint64_t fast_wait_ticks = 0;
    uint64_t resize_config_ticks = 0;
    uint64_t resize_wait_ticks = 0;
    uint64_t pipeline_frame_ticks = 0;
    uint32_t candidate_sum = 0;
    int level;

    for (level = 0; level < kLevels; ++level) {
        const int width = kWidths[level];
        const int height = kHeights[level];
        const int active_pixels = width * height;
        const int resize_active = level + 1 < kLevels;
        uint8_t* const next_image = resize_active ? pyramid_buffer(level & 1) : (uint8_t*)0;
        XTime verify_start;
        XTime verify_end;
        XTime config_start;
        XTime config_end;
        XTime stage_start;
        XTime fast_wait_start;
        XTime fast_done;
        XTime resize_wait_start = 0;
        XTime resize_done = 0;
        uint64_t observed_image_digest;
        uint64_t observed_fast_digest;
        int candidate_count;
        int status;

        XTime_GetTime(&verify_start);
        Xil_DCacheInvalidateRange((INTPTR)current_image, (u32)active_pixels);
        observed_image_digest = digest_image(current_image, active_pixels);
        XTime_GetTime(&verify_end);
        *verification_ticks += verify_end - verify_start;
        if (observed_image_digest != kExpectedImageDigests[level]) {
            *detail = ((uint32_t)level << 24) | ((uint32_t)observed_image_digest & 0x00ffffffu);
            return kErrorImageDigest;
        }
        image_aggregate = hash_u64(image_aggregate, observed_image_digest);

        XTime_GetTime(&config_start);
        XOrb_fast_stream_hls_Set_image_r(fast_accelerator, (u32)(UINTPTR)current_image);
        XOrb_fast_stream_hls_Set_candidates(fast_accelerator, (u32)(UINTPTR)candidates);
        XOrb_fast_stream_hls_Set_width(fast_accelerator, width);
        XOrb_fast_stream_hls_Set_height(fast_accelerator, height);
        XOrb_fast_stream_hls_Set_high_threshold(fast_accelerator, 20);
        XOrb_fast_stream_hls_Set_low_threshold(fast_accelerator, 7);
        XOrb_fast_stream_hls_Set_dual_threshold(fast_accelerator, 1);
        XOrb_fast_stream_hls_Set_candidate_capacity(fast_accelerator, kCandidateCapacity);
        XTime_GetTime(&config_end);
        fast_config_ticks += config_end - config_start;

        if (resize_active) {
            XTime_GetTime(&config_start);
            XOrb_resize_hls_Set_source(resize_accelerator, (u32)(UINTPTR)current_image);
            XOrb_resize_hls_Set_destination(resize_accelerator, (u32)(UINTPTR)next_image);
            XOrb_resize_hls_Set_source_width(resize_accelerator, width);
            XOrb_resize_hls_Set_source_height(resize_accelerator, height);
            XOrb_resize_hls_Set_destination_width(resize_accelerator, kWidths[level + 1]);
            XOrb_resize_hls_Set_destination_height(resize_accelerator, kHeights[level + 1]);
            XTime_GetTime(&config_end);
            resize_config_ticks += config_end - config_start;
        }

        XTime_GetTime(&stage_start);
        XTime_GetTime(&fast_wait_start);
        XOrb_fast_stream_hls_Start(fast_accelerator);
        if (resize_active) {
            XTime_GetTime(&resize_wait_start);
            XOrb_resize_hls_Start(resize_accelerator);
        }
        status = wait_for_accelerators(
            fast_accelerator, resize_accelerator, resize_active, 30,
            &fast_done, &resize_done);
        if (status & 1) {
            *detail = (uint32_t)level;
            return kErrorFastTimeout;
        }
        if (status & 2) {
            *detail = (uint32_t)(level + 1);
            return kErrorResizeTimeout;
        }
        fast_wait_ticks += fast_done - fast_wait_start;
        fast_level_ticks[level] += fast_done - fast_wait_start;
        if (resize_active) {
            resize_wait_ticks += resize_done - resize_wait_start;
            resize_level_ticks[level + 1] += resize_done - resize_wait_start;
        }
        pipeline_frame_ticks +=
            ((resize_active && resize_done > fast_done) ? resize_done : fast_done) - stage_start;

        candidate_count = (int32_t)XOrb_fast_stream_hls_Get_return(fast_accelerator);
        if (candidate_count < 0 || candidate_count > kCandidateCapacity) {
            *detail = ((uint32_t)level << 16) | ((uint32_t)candidate_count & 0xffffu);
            return kErrorFastAccelerator;
        }
        if (resize_active) {
            status = (int32_t)XOrb_resize_hls_Get_return(resize_accelerator);
            if (status != 0) {
                *detail = ((uint32_t)(level + 1) << 16) | ((uint32_t)status & 0xffffu);
                return kErrorResizeAccelerator;
            }
        }

        XTime_GetTime(&verify_start);
        Xil_DCacheInvalidateRange((INTPTR)candidates, (u32)candidate_count * sizeof(OrbPoint));
        observed_fast_digest = digest_candidates(candidates, candidate_count);
        XTime_GetTime(&verify_end);
        *verification_ticks += verify_end - verify_start;
        level_counts[level] = (uint32_t)candidate_count;
        level_digests[level] = observed_fast_digest;
        if (candidate_count != kExpectedCandidates[level]) {
            *detail = ((uint32_t)level << 24) | ((uint32_t)candidate_count & 0x00ffffffu);
            return kErrorCandidateCount;
        }
        if (observed_fast_digest != kExpectedFastDigests[level]) {
            *detail = ((uint32_t)level << 24) | ((uint32_t)observed_fast_digest & 0x00ffffffu);
            return kErrorCandidateDigest;
        }
        candidate_sum += (uint32_t)candidate_count;
        fast_aggregate = hash_u64(fast_aggregate, observed_fast_digest);
        current_image = next_image;
    }

    record_frame_perf(fast_stats, fast_config_ticks, fast_wait_ticks);
    record_frame_perf(resize_stats, resize_config_ticks, resize_wait_ticks);
    *pipeline_ticks += pipeline_frame_ticks;
    *image_digest = image_aggregate;
    *fast_digest = fast_aggregate;
    *total_candidates = candidate_sum;
    return kErrorNone;
}

/** @brief Publish averaged performance and the last verified frame summary. */
static void finalize_mailbox(uint32_t iterations,
                             const OrbPerfStats* resize_stats,
                             const OrbPerfStats* fast_stats,
                             uint64_t image_digest,
                             uint64_t fast_digest,
                             uint64_t verification_ticks,
                             uint64_t pipeline_ticks,
                             uint64_t wall_ticks,
                             uint32_t total_candidates,
                             const uint64_t fast_level_ticks[kLevels],
                             const uint64_t resize_level_ticks[kLevels],
                             const uint32_t level_counts[kLevels],
                             const uint64_t level_digests[kLevels]) {
    volatile OrbFastMailbox* const mailbox = benchmark_mailbox();
    const uint64_t fast_config_avg = fast_stats->config_total_ticks / iterations;
    const uint64_t fast_wait_avg = fast_stats->wait_total_ticks / iterations;
    const uint64_t resize_config_avg = resize_stats->config_total_ticks / iterations;
    const uint64_t resize_wait_avg = resize_stats->wait_total_ticks / iterations;
    const uint64_t pipeline_avg = pipeline_ticks / iterations;
    const uint64_t total_us = ticks_to_microseconds(wall_ticks);
    int level;

    mailbox->candidates = total_candidates;
    mailbox->fast_digest_hi = (uint32_t)(fast_digest >> 32);
    mailbox->fast_digest_lo = (uint32_t)fast_digest;
    mailbox->image_digest_hi = (uint32_t)(image_digest >> 32);
    mailbox->image_digest_lo = (uint32_t)image_digest;
    mailbox->fast_config_avg_us = (uint32_t)ticks_to_microseconds(fast_config_avg);
    mailbox->fast_wait_avg_us = (uint32_t)ticks_to_microseconds(fast_wait_avg);
    mailbox->fast_wait_min_us = (uint32_t)ticks_to_microseconds(fast_stats->wait_min_ticks);
    mailbox->fast_wait_max_us = (uint32_t)ticks_to_microseconds(fast_stats->wait_max_ticks);
    mailbox->fast_fps_milli = microseconds_to_milli_fps(
        mailbox->fast_config_avg_us + mailbox->fast_wait_avg_us);
    mailbox->resize_config_avg_us = (uint32_t)ticks_to_microseconds(resize_config_avg);
    mailbox->resize_wait_avg_us = (uint32_t)ticks_to_microseconds(resize_wait_avg);
    mailbox->resize_wait_min_us = (uint32_t)ticks_to_microseconds(resize_stats->wait_min_ticks);
    mailbox->resize_wait_max_us = (uint32_t)ticks_to_microseconds(resize_stats->wait_max_ticks);
    mailbox->resize_fps_milli = microseconds_to_milli_fps(
        mailbox->resize_config_avg_us + mailbox->resize_wait_avg_us);
    mailbox->pipeline_avg_us = (uint32_t)ticks_to_microseconds(pipeline_avg);
    mailbox->pipeline_fps_milli = microseconds_to_milli_fps(mailbox->pipeline_avg_us);
    mailbox->verification_avg_us =
        (uint32_t)ticks_to_microseconds(verification_ticks / iterations);
    mailbox->wall_avg_us = (uint32_t)ticks_to_microseconds(wall_ticks / iterations);
    mailbox->wall_fps_milli = microseconds_to_milli_fps(mailbox->wall_avg_us);
    mailbox->total_elapsed_us_hi = (uint32_t)(total_us >> 32);
    mailbox->total_elapsed_us_lo = (uint32_t)total_us;
    for (level = 0; level < kLevels; ++level) {
        mailbox->reserved[level] = level_counts[level];
        mailbox->reserved[8 + 2 * level] = (uint32_t)(level_digests[level] >> 32);
        mailbox->reserved[9 + 2 * level] = (uint32_t)level_digests[level];
        mailbox->reserved[24 + level] =
            (uint32_t)ticks_to_microseconds(fast_level_ticks[level] / iterations);
        mailbox->reserved[32 + level] = level == 0 ? 0u :
            (uint32_t)ticks_to_microseconds(resize_level_ticks[level] / iterations);
    }
    mailbox->status = kMailboxPass;
    publish_mailbox();
}

/** @brief Run continuous bit-exact frames and publish averaged PL performance. */
int main(void) {
    volatile OrbFastMailbox* const mailbox = benchmark_mailbox();
    XOrb_resize_hls resize_accelerator;
    XOrb_fast_stream_hls fast_accelerator;
    OrbPerfStats resize_stats;
    OrbPerfStats fast_stats;
    uint64_t image_digest = 0;
    uint64_t fast_digest = 0;
    uint64_t verification_ticks = 0;
    uint64_t pipeline_ticks = 0;
    uint64_t wall_ticks = 0;
    uint64_t fast_level_ticks[kLevels] = {0};
    uint64_t resize_level_ticks[kLevels] = {0};
    uint32_t level_counts[kLevels] = {0};
    uint64_t level_digests[kLevels] = {0};
    uint32_t total_candidates = 0;
    uint32_t iterations;
    uint32_t iteration;
    uint32_t detail = 0;
    int status;

    if ((UINTPTR)&_end >= (UINTPTR)kMailboxAddress) {
        return publish_failure(kErrorMailboxOverlap, (uint32_t)(UINTPTR)&_end, 0);
    }
    iterations = read_requested_iterations();
    initialize_mailbox(iterations);
    if (validate_input_pgm() != 0) return publish_failure(kErrorInputPgm, kInputPgmAddress, 0);

    status = XOrb_resize_hls_Initialize(&resize_accelerator, XPAR_ORB_RESIZE_DEVICE_ID);
    if (status != XST_SUCCESS) return publish_failure(kErrorResizeInitialize, (uint32_t)status, 0);
    status = XOrb_fast_stream_hls_Initialize(
        &fast_accelerator, XPAR_ORB_FAST_STREAM_DEVICE_ID);
    if (status != XST_SUCCESS) return publish_failure(kErrorFastInitialize, (uint32_t)status, 0);

    initialize_perf_stats(&resize_stats);
    initialize_perf_stats(&fast_stats);
    for (iteration = 1; iteration <= iterations; ++iteration) {
        XTime frame_start;
        XTime frame_end;
        XTime_GetTime(&frame_start);
        detail = 0;
        status = run_frame(
            &resize_accelerator, &fast_accelerator, &resize_stats, &fast_stats,
            &image_digest, &fast_digest, &verification_ticks, &pipeline_ticks,
            &total_candidates, fast_level_ticks, resize_level_ticks,
            level_counts, level_digests, &detail);
        XTime_GetTime(&frame_end);
        wall_ticks += frame_end - frame_start;
        if (status != kErrorNone) return publish_failure((uint32_t)status, detail, iteration);
        mailbox->completed_iterations = iteration;
        if ((iteration % 10u) == 0u || iteration == iterations) publish_mailbox();
    }

    finalize_mailbox(
        iterations, &resize_stats, &fast_stats, image_digest, fast_digest,
        verification_ticks, pipeline_ticks, wall_ticks, total_candidates,
        fast_level_ticks, resize_level_ticks, level_counts, level_digests);
    xil_printf("ORB_FAST_PASS iterations=%d candidates=%d pipeline_us=%d\r\n",
               iterations, total_candidates, mailbox->pipeline_avg_us);
    return 0;
}
