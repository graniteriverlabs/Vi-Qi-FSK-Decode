/**
 * @file fsk_online_stream.h
 * @brief CM4-side online period-only stream interface.
 *
 * The online stream module no longer sends full decoder records.
 * It only collects uint16_t period values and writes them as one block
 * per process_block() call.
 */

#ifndef INC_FSK_ONLINE_STREAM_H_
#define INC_FSK_ONLINE_STREAM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "fsk_config.h"
#include "fsk_debug.h"

#if (!OFFLINE_CAPTURE_MODE) && \
    (FSK_DEBUG_LIVE_PERIOD_FAST_STREAM  || FSK_DEBUG_LIVE_PERIOD_BLOCK_STREAM)

/**
 * @brief Reset online period stream state and counters.
 */
void fsk_online_stream_init(void);

#endif

#if (!OFFLINE_CAPTURE_MODE) && FSK_DEBUG_LIVE_PERIOD_FAST_STREAM


/**
 * @brief Process one DMA half/full block using period-only fast path.
 *
 * This function computes only:
 *
 *     period = curr_timestamp - previous_timestamp
 *
 * It does not execute:
 * - moving average
 * - delayed difference
 * - transition detection
 * - bit decode
 * - process_frame()
 *
 * It writes exactly one block to the CM4->CM7 shared ring.
 *
 * @param[in] start        Start index inside hrtim_capture_buf[].
 * @param[in] sample_count Number of raw HRTIM timestamps to process.
 *                         Normally FSK_DMA_HALF_SAMPLES.
 */
void fsk_online_period_fast_block_process(uint32_t start, uint32_t sample_count);

/**
 * @brief Number of period blocks successfully written to shared ring.
 */
extern volatile uint32_t g_fsk_period_fast_write_ok_count;

/**
 * @brief Number of period blocks dropped because shared ring was full.
 */
extern volatile uint32_t g_fsk_period_fast_drop_count;

/**
 * @brief Last period block size in bytes.
 */
extern volatile uint32_t g_fsk_period_fast_last_bytes;

/**
 * @brief Maximum DWT cycles used by one fast period block process call.
 */
extern volatile uint32_t g_fsk_period_fast_max_cycles;

#endif

#if (!OFFLINE_CAPTURE_MODE) && FSK_DEBUG_LIVE_PERIOD_BLOCK_STREAM
/**
 * @brief Start collecting one period block.
 *
 * Call this once at the beginning of process_block().
 *
 * @param[in] expected_periods Number of period samples expected in this block.
 */
void fsk_online_period_block_begin(uint32_t expected_periods);

/**
 * @brief Add one period value to the current online period block.
 *
 * This function only stores the value into a local RAM buffer.
 * It does not write to the shared ring per sample.
 *
 * @param[in] period Computed period value.
 */
void fsk_online_period_block_add(uint32_t period);

/**
 * @brief Flush the collected period block to the shared ring.
 *
 * Call this once at the end of process_block().
 * The write is non-blocking. If the shared ring is full, the whole block
 * is dropped and counted.
 */
void fsk_online_period_block_end(void);

/**
 * @brief Number of period blocks successfully written into shared ring.
 */
extern volatile uint32_t g_fsk_period_block_write_ok_count;

/**
 * @brief Number of period blocks dropped because shared ring was full.
 */
extern volatile uint32_t g_fsk_period_block_drop_count;

/**
 * @brief Number of local collection errors.
 *
 * This increments if process_block() produced more periods than the local
 * period block buffer can hold, or if expected count mismatches actual count.
 */
extern volatile uint32_t g_fsk_period_block_local_error_count;

/**
 * @brief Number of bytes in the most recent attempted block write.
 */
extern volatile uint32_t g_fsk_period_block_last_bytes;

/**
 * @brief Maximum DWT cycle count measured from begin() to end().
 */
extern volatile uint32_t g_fsk_period_block_max_cycles;

#endif	 /* (!OFFLINE_CAPTURE_MODE) && FSK_DEBUG_LIVE_PERIOD_BLOCK_STREAM */

#ifdef __cplusplus
}
#endif

#endif /* INC_FSK_ONLINE_STREAM_H_ */
