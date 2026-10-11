/**
 * @file orb_baremetal_selftest.c
 * @brief Zynq PS-to-PL bit-exact regression and performance benchmark.
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
    kWidth = 96,
    kHeight = 80,
    kPixels = kWidth * kHeight,
    kBudget = 40,
    kPointRegions = 4,
    kPixelRegions = 4,
    kResizeSourceWidth = 67,
    kResizeSourceHeight = 53,
    kResizeDestinationWidth = 55,
    kResizeDestinationHeight = 41,
    kCacheLineBytes = 32,
    kResizeSourcePixels = kResizeSourceWidth * kResizeSourceHeight,
    kResizeDestinationPixels = kResizeDestinationWidth * kResizeDestinationHeight,
    kResizeSourceStorageBytes =
        ((kResizeSourcePixels + kCacheLineBytes - 1) / kCacheLineBytes) * kCacheLineBytes,
    kResizeDestinationStorageBytes =
        ((kResizeDestinationPixels + kCacheLineBytes - 1) / kCacheLineBytes) * kCacheLineBytes,
    kAlgorithmVersion = 0x00010000,
    kCapabilityDualThreshold = 1,
    kMaxGridTopK = 1,
    kDefaultBenchmarkIterations = 100,
    kMaximumBenchmarkIterations = 1000,
    kFabricClockHz = 100000000,
    kMailboxAddress = 0x01000000,
    kMailboxCommandMagic = 0x4f524243,
    kMailboxResultMagic = 0x4f524252,
    kMailboxSchemaVersion = 1,
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

/**
 * @brief Fixed 256-byte command/result mailbox read directly by XSCT.
 *
 * Every field is one 32-bit word so the layout is independent of compiler padding.
 * The first two words are written by XSCT before the CPU is continued. The PS owns
 * all remaining words and flushes them to DDR whenever it publishes progress.
 */
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
    uint32_t reserved[25];
} OrbBenchmarkMailbox;

/** @brief Accumulated configuration and accelerator wait timing. */
typedef struct {
    uint64_t config_total_ticks;
    uint64_t wait_total_ticks;
    uint64_t wait_min_ticks;
    uint64_t wait_max_ticks;
} OrbPerfStats;

typedef char OrbPointSizeMustBe16[(sizeof(OrbPoint) == 16) ? 1 : -1];
typedef char OrbFeatureSizeMustBe64[(sizeof(OrbFeature) == 64) ? 1 : -1];
typedef char OrbMailboxSizeMustBe256[(sizeof(OrbBenchmarkMailbox) == 256) ? 1 : -1];
typedef char OrbMailboxMustFitDdr[
    ((kMailboxAddress >= XPAR_PS7_DDR_0_S_AXI_BASEADDR) &&
     (kMailboxAddress + sizeof(OrbBenchmarkMailbox) - 1 <= XPAR_PS7_DDR_0_S_AXI_HIGHADDR))
        ? 1
        : -1];

static uint8_t g_image[kPixels] __attribute__((aligned(64)));
static uint16_t g_pixel_work[kPixelRegions * kPixels] __attribute__((aligned(64)));
static OrbPoint g_point_work[kPointRegions * kPixels] __attribute__((aligned(64)));
static OrbFeature g_features[kBudget + 1] __attribute__((aligned(64)));
static uint8_t g_resize_source[kResizeSourceStorageBytes] __attribute__((aligned(64)));
static uint8_t g_resize_destination[kResizeDestinationStorageBytes] __attribute__((aligned(64)));

extern char _end;

/**
 * @brief Return the fixed DDR command/result mailbox.
 * @return Volatile pointer to the 256-byte mailbox at address 0x01000000.
 */
static volatile OrbBenchmarkMailbox* benchmark_mailbox(void) {
    return (volatile OrbBenchmarkMailbox*)(UINTPTR)kMailboxAddress;
}

/** @brief Flush the complete mailbox so XSCT observes the latest PS writes. */
static void publish_mailbox(void) {
    Xil_DCacheFlushRange((INTPTR)kMailboxAddress, sizeof(OrbBenchmarkMailbox));
}

/**
 * @brief Read and validate the XSCT benchmark request.
 * @return Requested iteration count, or the safe default when no valid command exists.
 */
static uint32_t read_requested_iterations(void) {
    volatile OrbBenchmarkMailbox* const mailbox = benchmark_mailbox();
    Xil_DCacheInvalidateRange((INTPTR)kMailboxAddress, sizeof(OrbBenchmarkMailbox));
    if (mailbox->command_magic == kMailboxCommandMagic &&
        mailbox->requested_iterations >= 1 &&
        mailbox->requested_iterations <= kMaximumBenchmarkIterations) {
        return mailbox->requested_iterations;
    }
    return kDefaultBenchmarkIterations;
}

/**
 * @brief Initialize all PS-owned mailbox words and publish RUNNING status.
 * @param iterations Validated number of benchmark iterations.
 */
static void initialize_mailbox(uint32_t iterations) {
    volatile OrbBenchmarkMailbox* const mailbox = benchmark_mailbox();
    volatile uint32_t* const words = (volatile uint32_t*)mailbox;
    size_t index;
    for (index = 2; index < sizeof(OrbBenchmarkMailbox) / sizeof(uint32_t); ++index) {
        words[index] = 0;
    }
    mailbox->result_magic = kMailboxResultMagic;
    mailbox->schema_version = kMailboxSchemaVersion;
    mailbox->status = kMailboxRunning;
    mailbox->requested_iterations = iterations;
    mailbox->timer_hz = (uint32_t)COUNTS_PER_SECOND;
    mailbox->fabric_clock_hz = kFabricClockHz;
    publish_mailbox();
}

/**
 * @brief Publish a terminal benchmark failure.
 * @param error Stable phase-specific error code.
 * @param detail Accelerator status or low digest word associated with the failure.
 * @param iteration One-based failing iteration, or zero for initialization failures.
 * @return Nonzero value suitable for returning from main().
 */
static int publish_failure(uint32_t error, uint32_t detail, uint32_t iteration) {
    volatile OrbBenchmarkMailbox* const mailbox = benchmark_mailbox();
    mailbox->error_code = error;
    mailbox->failure_detail = detail;
    mailbox->failing_iteration = iteration;
    mailbox->status = kMailboxFail;
    publish_mailbox();
    return (int)error;
}

/**
 * @brief Convert global-timer ticks to rounded microseconds.
 * @param ticks Number of ARM global-timer ticks.
 * @return Rounded elapsed time in microseconds.
 */
static uint64_t ticks_to_microseconds(uint64_t ticks) {
    return (ticks * UINT64_C(1000000) + (uint64_t)COUNTS_PER_SECOND / 2) /
           (uint64_t)COUNTS_PER_SECOND;
}

/**
 * @brief Convert wall-clock ticks to estimated 100 MHz PL cycles.
 * @param ticks Number of ARM global-timer ticks.
 * @return Estimated PL cycles; this includes AXI and PS polling latency.
 */
static uint64_t ticks_to_estimated_pl_cycles(uint64_t ticks) {
    return (ticks * (uint64_t)kFabricClockHz + (uint64_t)COUNTS_PER_SECOND / 2) /
           (uint64_t)COUNTS_PER_SECOND;
}

/**
 * @brief Convert average microseconds per transaction to milli-FPS.
 * @param average_us Average transaction duration in microseconds.
 * @return Frames per second multiplied by 1000.
 */
static uint32_t microseconds_to_milli_fps(uint32_t average_us) {
    return average_us == 0 ? 0 : (uint32_t)(UINT64_C(1000000000) / average_us);
}

/** @brief Reset one timing accumulator before the benchmark loop. */
static void initialize_perf_stats(OrbPerfStats* stats) {
    stats->config_total_ticks = 0;
    stats->wait_total_ticks = 0;
    stats->wait_min_ticks = UINT64_MAX;
    stats->wait_max_ticks = 0;
}

/**
 * @brief Add one measured transaction to a timing accumulator.
 * @param stats Destination accumulator.
 * @param config_ticks PS register programming and start overhead.
 * @param wait_ticks Accelerator completion wait time.
 */
static void record_perf(OrbPerfStats* stats, uint64_t config_ticks, uint64_t wait_ticks) {
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

/** @brief Append one 32-bit word in little-endian byte order. */
static uint64_t hash_u32(uint64_t hash, uint32_t value) {
    int byte_index;
    for (byte_index = 0; byte_index < 4; ++byte_index) {
        hash = hash_byte(hash, (uint8_t)(value >> (8 * byte_index)));
    }
    return hash;
}

/** @brief Clear every PL output and scratch allocation before the first transaction. */
static void clear_output_buffers(void) {
    size_t index;
    for (index = 0; index < kPixelRegions * kPixels; ++index) g_pixel_work[index] = 0;
    for (index = 0; index < kPointRegions * kPixels; ++index) {
        g_point_work[index].x = 0;
        g_point_work[index].y = 0;
        g_point_work[index].score = 0;
        g_point_work[index].tile = 0;
    }
    for (index = 0; index < kBudget + 1; ++index) {
        uint32_t* const words = (uint32_t*)&g_features[index];
        size_t word_index;
        for (word_index = 0; word_index < sizeof(OrbFeature) / sizeof(uint32_t); ++word_index) {
            words[word_index] = 0;
        }
    }
}

/** @brief Generate the deterministic image used by RTL co-simulation. */
static void generate_input_image(void) {
    int x;
    int y;
    for (y = 0; y < kHeight; ++y) {
        for (x = 0; x < kWidth; ++x) {
            const int checker = (((x / 5) + (y / 7)) & 1) ? 109 : 0;
            g_image[y * kWidth + x] =
                (uint8_t)((x * 29 + y * 47 + x * y + checker + 31) & 255);
        }
    }
}

/** @brief Generate and clear the deterministic resize transaction buffers. */
static void prepare_resize_buffers(void) {
    int x;
    int y;
    size_t index;
    for (index = 0; index < sizeof(g_resize_source); ++index) g_resize_source[index] = 0;
    for (y = 0; y < kResizeSourceHeight; ++y) {
        for (x = 0; x < kResizeSourceWidth; ++x) {
            g_resize_source[y * kResizeSourceWidth + x] =
                (uint8_t)((x * 37 + y * 53 + (x * y) * 3 + 17) & 255);
        }
    }
    for (index = 0; index < sizeof(g_resize_destination); ++index) {
        g_resize_destination[index] = 0;
    }
}

/**
 * @brief Wait for one level transaction with a bounded timeout.
 * @param accelerator Initialized level accelerator instance.
 * @param timeout_seconds Maximum wait time in seconds.
 * @return Zero on completion or -1 on timeout.
 */
static int wait_for_level(XOrb_level_hls* accelerator, unsigned timeout_seconds) {
    XTime start;
    XTime now;
    XTime_GetTime(&start);
    do {
        if (XOrb_level_hls_IsDone(accelerator)) return 0;
        XTime_GetTime(&now);
    } while ((now - start) < ((XTime)timeout_seconds * (XTime)COUNTS_PER_SECOND));
    return -1;
}

/**
 * @brief Wait for one resize transaction with a bounded timeout.
 * @param accelerator Initialized resize accelerator instance.
 * @param timeout_seconds Maximum wait time in seconds.
 * @return Zero on completion or -1 on timeout.
 */
static int wait_for_resize(XOrb_resize_hls* accelerator, unsigned timeout_seconds) {
    XTime start;
    XTime now;
    XTime_GetTime(&start);
    do {
        if (XOrb_resize_hls_IsDone(accelerator)) return 0;
        XTime_GetTime(&now);
    } while ((now - start) < ((XTime)timeout_seconds * (XTime)COUNTS_PER_SECOND));
    return -1;
}

/**
 * @brief Hash every externally visible level result in frozen RTL-test order.
 * @param metadata Feature trailer containing valid record counts.
 * @return Deterministic 64-bit FNV-1a digest.
 */
static uint64_t digest_outputs(const OrbFeature* metadata) {
    const uint16_t* const score = g_pixel_work;
    const uint16_t* const gaussian = g_pixel_work + 2 * kPixels;
    const OrbPoint* const candidates = g_point_work;
    const OrbPoint* const selected = g_point_work + kPixels;
    uint64_t hash = UINT64_C(14695981039346656037);
    int index;
    int descriptor_word;

    for (index = 0; index < kPixels; ++index) hash = hash_byte(hash, (uint8_t)score[index]);
    for (index = 0; index < kPixels; ++index) hash = hash_byte(hash, (uint8_t)gaussian[index]);
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
        const OrbFeature* const feature = &g_features[index];
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
 * @brief Execute and verify one resize transaction while collecting timing.
 * @param accelerator Initialized resize accelerator.
 * @param stats Resize timing accumulator.
 * @param digest Output digest observed for this iteration.
 * @param verification_ticks Accumulator for cache-invalidate and digest time.
 * @param detail Failure detail returned to the mailbox.
 * @return Zero on success, or phase-local error 2 through 4.
 */
static int run_resize_once(XOrb_resize_hls* accelerator, OrbPerfStats* stats,
                           uint64_t* digest, uint64_t* verification_ticks,
                           uint32_t* detail) {
    static const uint64_t expected_digest = UINT64_C(541945487680902820);
    XTime config_start;
    XTime wait_start;
    XTime wait_end;
    XTime verify_start;
    XTime verify_end;
    uint64_t hash = UINT64_C(14695981039346656037);
    size_t index;
    int status;

    XTime_GetTime(&config_start);
    XOrb_resize_hls_Set_source(accelerator, (u32)(UINTPTR)g_resize_source);
    XOrb_resize_hls_Set_destination(accelerator, (u32)(UINTPTR)g_resize_destination);
    XOrb_resize_hls_Set_source_width(accelerator, kResizeSourceWidth);
    XOrb_resize_hls_Set_source_height(accelerator, kResizeSourceHeight);
    XOrb_resize_hls_Set_destination_width(accelerator, kResizeDestinationWidth);
    XOrb_resize_hls_Set_destination_height(accelerator, kResizeDestinationHeight);
    XOrb_resize_hls_Start(accelerator);
    XTime_GetTime(&wait_start);
    status = wait_for_resize(accelerator, 30);
    XTime_GetTime(&wait_end);
    record_perf(stats, wait_start - config_start, wait_end - wait_start);
    if (status != 0) return 2;

    status = (int)XOrb_resize_hls_Get_return(accelerator);
    if (status != 0) {
        *detail = (uint32_t)status;
        return 3;
    }

    XTime_GetTime(&verify_start);
    Xil_DCacheInvalidateRange((INTPTR)g_resize_destination, sizeof(g_resize_destination));
    for (index = 0; index < kResizeDestinationPixels; ++index) {
        hash = hash_byte(hash, g_resize_destination[index]);
    }
    XTime_GetTime(&verify_end);
    *verification_ticks += verify_end - verify_start;
    *digest = hash;
    if (hash != expected_digest) {
        *detail = (uint32_t)hash;
        return 4;
    }
    return 0;
}

/**
 * @brief Execute and verify one ORB level transaction while collecting timing.
 * @param accelerator Initialized level accelerator.
 * @param stats Level timing accumulator.
 * @param digest Output digest observed for this iteration.
 * @param verification_ticks Accumulator for cache-invalidate and digest time.
 * @param detail Failure detail returned to the mailbox.
 * @return Zero on success, or phase-local error 2 through 4.
 */
static int run_level_once(XOrb_level_hls* accelerator, OrbPerfStats* stats,
                          uint64_t* digest, uint64_t* verification_ticks,
                          uint32_t* detail) {
    static const uint64_t expected_digest = UINT64_C(7505441316282489474);
    const OrbFeature* const metadata = &g_features[kBudget];
    XTime config_start;
    XTime wait_start;
    XTime wait_end;
    XTime verify_start;
    XTime verify_end;
    uint64_t hash;
    int status;

    XTime_GetTime(&config_start);
    XOrb_level_hls_Set_image_r(accelerator, (u32)(UINTPTR)g_image);
    XOrb_level_hls_Set_pixel_work(accelerator, (u32)(UINTPTR)g_pixel_work);
    XOrb_level_hls_Set_gaussian_work(accelerator, (u32)(UINTPTR)(g_pixel_work + 2 * kPixels));
    XOrb_level_hls_Set_point_work(accelerator, (u32)(UINTPTR)g_point_work);
    XOrb_level_hls_Set_features(accelerator, (u32)(UINTPTR)g_features);
    XOrb_level_hls_Set_width(accelerator, kWidth);
    XOrb_level_hls_Set_height(accelerator, kHeight);
    XOrb_level_hls_Set_budget(accelerator, kBudget);
    XOrb_level_hls_Set_high_threshold(accelerator, 20);
    XOrb_level_hls_Set_low_threshold(accelerator, 7);
    XOrb_level_hls_Set_dual_threshold(accelerator, 1);
    XOrb_level_hls_Set_level(accelerator, 2);
    XOrb_level_hls_Set_pixel_capacity(accelerator, kPixels);
    XOrb_level_hls_Set_candidate_capacity(accelerator, kPixels);
    XOrb_level_hls_Set_selected_capacity(accelerator, kPixels);
    XOrb_level_hls_Set_cell_capacity(accelerator, kPixels);
    XOrb_level_hls_Set_feature_capacity(accelerator, kBudget);
    XOrb_level_hls_Start(accelerator);
    XTime_GetTime(&wait_start);
    status = wait_for_level(accelerator, 30);
    XTime_GetTime(&wait_end);
    record_perf(stats, wait_start - config_start, wait_end - wait_start);
    if (status != 0) return 2;

    status = (int)XOrb_level_hls_Get_return(accelerator);
    if (status != 0) {
        *detail = (uint32_t)status;
        return 3;
    }

    XTime_GetTime(&verify_start);
    Xil_DCacheInvalidateRange((INTPTR)g_pixel_work, sizeof(g_pixel_work));
    Xil_DCacheInvalidateRange((INTPTR)g_point_work, sizeof(g_point_work));
    Xil_DCacheInvalidateRange((INTPTR)g_features, sizeof(g_features));
    if (metadata->x < 0 || metadata->x > kPixels ||
        metadata->y < 0 || metadata->y > kBudget) {
        *detail = ((uint32_t)metadata->x << 16) | ((uint32_t)metadata->y & 0xffffu);
        XTime_GetTime(&verify_end);
        *verification_ticks += verify_end - verify_start;
        return 4;
    }
    hash = digest_outputs(metadata);
    XTime_GetTime(&verify_end);
    *verification_ticks += verify_end - verify_start;
    *digest = hash;
    if (metadata->x != 262 || metadata->y != 40 ||
        metadata->angle_q16 != kAlgorithmVersion ||
        (metadata->descriptor[0] & kCapabilityDualThreshold) == 0 ||
        metadata->descriptor[1] != kMaxGridTopK || hash != expected_digest) {
        *detail = (uint32_t)hash;
        return 4;
    }
    return 0;
}

/**
 * @brief Store final counts, digests, timing and throughput in the fixed mailbox.
 * @param iterations Number of successfully verified iterations.
 * @param resize_stats Resize timing accumulator.
 * @param level_stats Level timing accumulator.
 * @param resize_digest Final resize digest.
 * @param level_digest Final level digest.
 * @param verification_ticks Total cache-invalidate and digest time.
 * @param wall_ticks Total end-to-end benchmark-loop time.
 */
static void finalize_mailbox(uint32_t iterations, const OrbPerfStats* resize_stats,
                             const OrbPerfStats* level_stats, uint64_t resize_digest,
                             uint64_t level_digest, uint64_t verification_ticks,
                             uint64_t wall_ticks) {
    volatile OrbBenchmarkMailbox* const mailbox = benchmark_mailbox();
    const OrbFeature* const metadata = &g_features[kBudget];
    const uint64_t resize_config_avg_ticks = resize_stats->config_total_ticks / iterations;
    const uint64_t resize_wait_avg_ticks = resize_stats->wait_total_ticks / iterations;
    const uint64_t level_config_avg_ticks = level_stats->config_total_ticks / iterations;
    const uint64_t level_wait_avg_ticks = level_stats->wait_total_ticks / iterations;
    const uint64_t pipeline_avg_ticks =
        (resize_stats->config_total_ticks + resize_stats->wait_total_ticks +
         level_stats->config_total_ticks + level_stats->wait_total_ticks) /
        iterations;
    const uint64_t total_elapsed_us = ticks_to_microseconds(wall_ticks);

    mailbox->candidates = (uint32_t)metadata->x;
    mailbox->selected = (uint32_t)metadata->y;
    mailbox->algorithm_version = metadata->angle_q16;
    mailbox->capabilities = metadata->descriptor[0];
    mailbox->max_grid_top_k = metadata->descriptor[1];
    mailbox->resize_digest_hi = (uint32_t)(resize_digest >> 32);
    mailbox->resize_digest_lo = (uint32_t)resize_digest;
    mailbox->level_digest_hi = (uint32_t)(level_digest >> 32);
    mailbox->level_digest_lo = (uint32_t)level_digest;

    mailbox->resize_config_avg_us = (uint32_t)ticks_to_microseconds(resize_config_avg_ticks);
    mailbox->resize_wait_avg_us = (uint32_t)ticks_to_microseconds(resize_wait_avg_ticks);
    mailbox->resize_wait_min_us = (uint32_t)ticks_to_microseconds(resize_stats->wait_min_ticks);
    mailbox->resize_wait_max_us = (uint32_t)ticks_to_microseconds(resize_stats->wait_max_ticks);
    mailbox->resize_est_cycles_avg =
        (uint32_t)ticks_to_estimated_pl_cycles(resize_wait_avg_ticks);
    mailbox->resize_fps_milli = microseconds_to_milli_fps(
        mailbox->resize_config_avg_us + mailbox->resize_wait_avg_us);

    mailbox->level_config_avg_us = (uint32_t)ticks_to_microseconds(level_config_avg_ticks);
    mailbox->level_wait_avg_us = (uint32_t)ticks_to_microseconds(level_wait_avg_ticks);
    mailbox->level_wait_min_us = (uint32_t)ticks_to_microseconds(level_stats->wait_min_ticks);
    mailbox->level_wait_max_us = (uint32_t)ticks_to_microseconds(level_stats->wait_max_ticks);
    mailbox->level_est_cycles_avg =
        (uint32_t)ticks_to_estimated_pl_cycles(level_wait_avg_ticks);
    mailbox->level_fps_milli = microseconds_to_milli_fps(
        mailbox->level_config_avg_us + mailbox->level_wait_avg_us);

    mailbox->pipeline_avg_us = (uint32_t)ticks_to_microseconds(pipeline_avg_ticks);
    mailbox->pipeline_fps_milli = microseconds_to_milli_fps(mailbox->pipeline_avg_us);
    mailbox->verification_avg_us =
        (uint32_t)ticks_to_microseconds(verification_ticks / iterations);
    mailbox->wall_avg_us = (uint32_t)ticks_to_microseconds(wall_ticks / iterations);
    mailbox->wall_fps_milli = microseconds_to_milli_fps(mailbox->wall_avg_us);
    mailbox->total_elapsed_us_hi = (uint32_t)(total_elapsed_us >> 32);
    mailbox->total_elapsed_us_lo = (uint32_t)total_elapsed_us;
    mailbox->status = kMailboxPass;
    publish_mailbox();
}

/**
 * @brief Run the bit-exact multi-frame PS/PL regression and performance benchmark.
 * @return Zero when every frame is bit exact, otherwise a stable mailbox error code.
 */
int main(void) {
    volatile OrbBenchmarkMailbox* const mailbox = benchmark_mailbox();
    const OrbFeature* const metadata = &g_features[kBudget];
    XOrb_resize_hls resize_accelerator;
    XOrb_level_hls level_accelerator;
    OrbPerfStats resize_stats;
    OrbPerfStats level_stats;
    uint64_t resize_digest = 0;
    uint64_t level_digest = 0;
    uint64_t verification_ticks = 0;
    uint64_t wall_ticks = 0;
    uint32_t iterations;
    uint32_t iteration;
    uint32_t detail = 0;
    int status;

    if ((UINTPTR)&_end >= (UINTPTR)kMailboxAddress) {
        xil_printf("ORB_BENCHMARK_FAIL mailbox_overlap end=%08x\r\n", (u32)(UINTPTR)&_end);
        return kErrorMailboxOverlap;
    }

    iterations = read_requested_iterations();
    initialize_mailbox(iterations);
    xil_printf("ORB PS/PL benchmark start iterations=%d mailbox=%08x\r\n",
               iterations, kMailboxAddress);

    prepare_resize_buffers();
    generate_input_image();
    clear_output_buffers();
    Xil_DCacheFlushRange((INTPTR)g_resize_source, sizeof(g_resize_source));
    Xil_DCacheFlushRange((INTPTR)g_resize_destination, sizeof(g_resize_destination));
    Xil_DCacheFlushRange((INTPTR)g_image, sizeof(g_image));
    Xil_DCacheFlushRange((INTPTR)g_pixel_work, sizeof(g_pixel_work));
    Xil_DCacheFlushRange((INTPTR)g_point_work, sizeof(g_point_work));
    Xil_DCacheFlushRange((INTPTR)g_features, sizeof(g_features));

    status = XOrb_resize_hls_Initialize(&resize_accelerator, XPAR_ORB_RESIZE_DEVICE_ID);
    if (status != XST_SUCCESS) {
        xil_printf("ORB_RESIZE_FAIL init=%d\r\n", status);
        return publish_failure(kErrorResizeInitialize, (uint32_t)status, 0);
    }
    status = XOrb_level_hls_Initialize(&level_accelerator, XPAR_ORB_LEVEL_DEVICE_ID);
    if (status != XST_SUCCESS) {
        xil_printf("ORB_SELFTEST_FAIL init=%d\r\n", status);
        return publish_failure(kErrorLevelInitialize, (uint32_t)status, 0);
    }

    initialize_perf_stats(&resize_stats);
    initialize_perf_stats(&level_stats);
    for (iteration = 1; iteration <= iterations; ++iteration) {
        XTime iteration_start;
        XTime iteration_end;
        XTime_GetTime(&iteration_start);

        detail = 0;
        status = run_resize_once(&resize_accelerator, &resize_stats, &resize_digest,
                                 &verification_ticks, &detail);
        if (status != 0) {
            mailbox->resize_digest_hi = (uint32_t)(resize_digest >> 32);
            mailbox->resize_digest_lo = (uint32_t)resize_digest;
            xil_printf("ORB_RESIZE_FAIL iteration=%d code=%d detail=%08x\r\n",
                       iteration, status, detail);
            return publish_failure(100u + (uint32_t)status, detail, iteration);
        }

        detail = 0;
        status = run_level_once(&level_accelerator, &level_stats, &level_digest,
                                &verification_ticks, &detail);
        if (status != 0) {
            mailbox->resize_digest_hi = (uint32_t)(resize_digest >> 32);
            mailbox->resize_digest_lo = (uint32_t)resize_digest;
            mailbox->level_digest_hi = (uint32_t)(level_digest >> 32);
            mailbox->level_digest_lo = (uint32_t)level_digest;
            mailbox->candidates = (uint32_t)metadata->x;
            mailbox->selected = (uint32_t)metadata->y;
            xil_printf("ORB_SELFTEST_FAIL iteration=%d code=%d candidates=%d selected=%d detail=%08x\r\n",
                       iteration, status, metadata->x, metadata->y, detail);
            return publish_failure(200u + (uint32_t)status, detail, iteration);
        }

        XTime_GetTime(&iteration_end);
        wall_ticks += iteration_end - iteration_start;
        mailbox->completed_iterations = iteration;
        if ((iteration % 10u) == 0u || iteration == iterations) publish_mailbox();
    }

    finalize_mailbox(iterations, &resize_stats, &level_stats, resize_digest,
                     level_digest, verification_ticks, wall_ticks);
    xil_printf("ORB_RESIZE_PASS pixels=2255 digest=%08x%08x\r\n",
               (u32)(resize_digest >> 32), (u32)resize_digest);
    xil_printf("ORB_SELFTEST_PASS candidates=262 selected=40 version=1.0 dual_threshold_supported=1 max_K=1 digest=%08x%08x\r\n",
               (u32)(level_digest >> 32), (u32)level_digest);
    xil_printf("ORB_BENCHMARK_PASS iterations=%d resize_us=%d level_us=%d pipeline_fps_milli=%d\r\n",
               iterations,
               mailbox->resize_config_avg_us + mailbox->resize_wait_avg_us,
               mailbox->level_config_avg_us + mailbox->level_wait_avg_us,
               mailbox->pipeline_fps_milli);
    return 0;
}
