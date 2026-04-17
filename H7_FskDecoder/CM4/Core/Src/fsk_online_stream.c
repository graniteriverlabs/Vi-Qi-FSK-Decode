/**
 * @file fsk_online_stream.c
 * @brief CM4-side online period-only block stream implementation.
 *
 * This module replaces the old full-record online stream.
 *
 * Old behavior:
 * - Pack period + MA + delta + flags + state.
 * - Potentially write stream records from the hot per-sample path.
 *
 * New behavior:
 * - Collect only uint16_t period values.
 * - Write one complete period block at the end of process_block().
 * - Never block waiting for shared-ring space.
 */

#include "fsk_online_stream.h"

#if (!OFFLINE_CAPTURE_MODE) && \
    (FSK_DEBUG_LIVE_PERIOD_FAST_STREAM  || FSK_DEBUG_LIVE_PERIOD_BLOCK_STREAM)

#include "main.h"
#include "ipc_stream_shared.h"
#include <fsk_decoder.h>


/* -------------------------------------------------------------------------- */
/* Fast period-only block stream state                                         */
/* -------------------------------------------------------------------------- */

#if FSK_DEBUG_LIVE_PERIOD_FAST_STREAM

/**
 * @brief Local period block buffer.
 *
 * Size is derived from CAPTURE_SAMPLES.
 */
__attribute__((aligned(32)))
static uint16_t s_fsk_period_fast_block[FSK_PERIOD_BLOCK_PERIODS];

/**
 * @brief Previous HRTIM timestamp used for fast period-only calculation.
 *
 * This must be visible to fsk_online_stream_init(), because init resets it
 * before each new capture.
 */
static uint32_t s_fsk_period_fast_prev_timestamp = 0U;

/**
 * @brief Successful period block writes.
 */
volatile uint32_t g_fsk_period_fast_write_ok_count = 0U;

/**
 * @brief Period blocks dropped because shared ring was full.
 */
volatile uint32_t g_fsk_period_fast_drop_count = 0U;

/**
 * @brief Last attempted period block size in bytes.
 */
volatile uint32_t g_fsk_period_fast_last_bytes = 0U;

/**
 * @brief Maximum DWT cycles used by one fast block call.
 */
volatile uint32_t g_fsk_period_fast_max_cycles = 0U;

#endif /* FSK_DEBUG_LIVE_PERIOD_FAST_STREAM */

/* -------------------------------------------------------------------------- */
/* Optional period block stream state                                          */
/* -------------------------------------------------------------------------- */

#if FSK_DEBUG_LIVE_PERIOD_BLOCK_STREAM

__attribute__((aligned(32)))
static uint16_t s_fsk_period_block[FSK_PERIOD_BLOCK_PERIODS];

static uint32_t s_fsk_period_block_index = 0U;
static uint32_t s_fsk_period_block_expected = 0U;
static uint32_t s_fsk_period_block_start_cycles = 0U;

volatile uint32_t g_fsk_period_block_write_ok_count = 0U;
volatile uint32_t g_fsk_period_block_drop_count = 0U;
volatile uint32_t g_fsk_period_block_local_error_count = 0U;
volatile uint32_t g_fsk_period_block_last_bytes = 0U;
volatile uint32_t g_fsk_period_block_max_cycles = 0U;

#endif /* FSK_DEBUG_LIVE_PERIOD_BLOCK_STREAM */

/* -------------------------------------------------------------------------- */
/* Common online stream init                                                   */
/* -------------------------------------------------------------------------- */

void fsk_online_stream_init(void)
{
#if FSK_DEBUG_LIVE_PERIOD_FAST_STREAM
    /**
     * Reset previous timestamp so each new externally triggered capture starts
     * with a known period reference.
     */
    s_fsk_period_fast_prev_timestamp = 0U;

    g_fsk_period_fast_write_ok_count = 0U;
    g_fsk_period_fast_drop_count = 0U;
    g_fsk_period_fast_last_bytes = 0U;
    g_fsk_period_fast_max_cycles = 0U;
#endif

#if FSK_DEBUG_LIVE_PERIOD_BLOCK_STREAM
    s_fsk_period_block_index = 0U;
    s_fsk_period_block_expected = 0U;
    s_fsk_period_block_start_cycles = 0U;

    g_fsk_period_block_write_ok_count = 0U;
    g_fsk_period_block_drop_count = 0U;
    g_fsk_period_block_local_error_count = 0U;
    g_fsk_period_block_last_bytes = 0U;
    g_fsk_period_block_max_cycles = 0U;
#endif
}

/* -------------------------------------------------------------------------- */
/* Fast period-only block stream                                               */
/* -------------------------------------------------------------------------- */

#if (!OFFLINE_CAPTURE_MODE) && FSK_DEBUG_LIVE_PERIOD_FAST_STREAM

void fsk_online_period_fast_block_process(uint32_t start, uint32_t sample_count)
{
	uint32_t start_cycles;
	uint32_t byte_count;

	/**
	 * Defensive clamp.
	 *
	 * Normally sample_count must be FSK_DMA_HALF_SAMPLES.
	 */
	if (sample_count > FSK_PERIOD_BLOCK_PERIODS)
	{
		sample_count = FSK_PERIOD_BLOCK_PERIODS;
	}

	start_cycles = DWT->CYCCNT;

	for (uint32_t i = 0U; i < sample_count; i++)
	{
		uint32_t curr = hrtim_capture_buf[start + i];
		uint32_t period;

		if (curr >= s_fsk_period_fast_prev_timestamp)
		{
			period = curr - s_fsk_period_fast_prev_timestamp;
		}
		else
		{
			period = (HRTIM_TIMER_MAX - s_fsk_period_fast_prev_timestamp) + curr + 1U;
		}

		s_fsk_period_fast_prev_timestamp = curr;

		/**
		 * Period stream record is uint16_t.
		 *
		 * Saturate instead of wrapping if a large gap occurs.
		 */
		s_fsk_period_fast_block[i] =
			(period > 0xFFFFU) ? 0xFFFFU : (uint16_t)period;
	}

	byte_count = sample_count * sizeof(uint16_t);
	g_fsk_period_fast_last_bytes = byte_count;

	/**
	 * Single non-blocking shared-ring write.
	 *
	 * This is the important rule:
	 * one DMA half/full event -> one ipc_stream_write().
	 */
	if (ipc_stream_write((const uint8_t *)s_fsk_period_fast_block, byte_count) == 0)
	{
		g_fsk_period_fast_write_ok_count++;

#if FSK_PERIOD_CHUNK_NOTIFY_CM7
		HAL_HSEM_Release(0U, 0U);
#endif
	}
	else
	{
		/**
		 * Do not wait/retry. Dropping debug data is safer than delaying DMA.
		 */
		g_fsk_period_fast_drop_count++;
	}

	{
		uint32_t elapsed_cycles = DWT->CYCCNT - start_cycles;

		if (elapsed_cycles > g_fsk_period_fast_max_cycles)
		{
			g_fsk_period_fast_max_cycles = elapsed_cycles;
		}
	}
}

#endif /* FSK_DEBUG_LIVE_PERIOD_FAST_STREAM */

/* -------------------------------------------------------------------------- */
/* Optional period block stream                                                */
/* -------------------------------------------------------------------------- */


#if (!OFFLINE_CAPTURE_MODE) && FSK_DEBUG_LIVE_PERIOD_BLOCK_STREAM

void fsk_online_period_block_begin(uint32_t expected_periods)
{
    /**
     * Start one period block for one process_block() call.
     *
     * process_block() is expected to process exactly one DMA half-buffer,
     * so expected_periods should normally be FSK_DMA_HALF_SAMPLES.
     */
    s_fsk_period_block_index = 0U;
    s_fsk_period_block_start_cycles = DWT->CYCCNT;

    if (expected_periods > FSK_PERIOD_BLOCK_PERIODS)
    {
        /**
         * Clamp to local buffer size. Count the mismatch but do not stop
         * capture, because this is debug transport only.
         */
        s_fsk_period_block_expected = FSK_PERIOD_BLOCK_PERIODS;
        g_fsk_period_block_local_error_count++;
    }
    else
    {
        s_fsk_period_block_expected = expected_periods;
    }
}

void fsk_online_period_block_add(uint32_t period)
{
    if (s_fsk_period_block_index >= FSK_PERIOD_BLOCK_PERIODS)
    {
        /**
         * Local block overflow. Do not write past the buffer.
         */
        g_fsk_period_block_local_error_count++;
        return;
    }

    /**
     * Period-only debug record.
     *
     * If period ever exceeds uint16_t, saturate instead of wrapping.
     */
    s_fsk_period_block[s_fsk_period_block_index++] =
        (period > 0xFFFFU) ? 0xFFFFU : (uint16_t)period;
}

void fsk_online_period_block_end(void)
{
    uint32_t byte_count;
    uint32_t elapsed_cycles;

    if (s_fsk_period_block_index == 0U)
    {
        return;
    }

    /**
     * Count mismatch between expected and actual records.
     *
     * This should not happen during normal DMA half/full processing.
     */
    if (s_fsk_period_block_index != s_fsk_period_block_expected)
    {
        g_fsk_period_block_local_error_count++;
    }

    byte_count = s_fsk_period_block_index * sizeof(uint16_t);
    g_fsk_period_block_last_bytes = byte_count;

    /**
     * One non-blocking shared-ring write per process_block().
     *
     * This is the key design rule:
     * - no per-sample ipc_stream_write()
     * - no retry loop
     * - no HAL_Delay()
     */
    if (ipc_stream_write((const uint8_t *)s_fsk_period_block, byte_count) == 0)
    {
        g_fsk_period_block_write_ok_count++;

#if FSK_PERIOD_CHUNK_NOTIFY_CM7
        HAL_HSEM_Release(0U, 0U);
#endif
    }
    else
    {
        /**
         * If the shared ring is full, drop this debug block.
         *
         * Capture/decode must not be delayed by debug streaming.
         */
        g_fsk_period_block_drop_count++;
    }

    elapsed_cycles = DWT->CYCCNT - s_fsk_period_block_start_cycles;
    if (elapsed_cycles > g_fsk_period_block_max_cycles)
    {
        g_fsk_period_block_max_cycles = elapsed_cycles;
    }

    /**
     * Prepare for the next block.
     */
    s_fsk_period_block_index = 0U;
}

#endif /* period block stream */
#endif
