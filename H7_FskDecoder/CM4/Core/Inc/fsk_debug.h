/**
 * @file fsk_debug.h
 * @brief Central debug configuration for FSK capture/decode.
 *
 * This file collects debug-only build macros so that fsk_config.h
 * remains focused on protocol/timing configuration.
 */

#ifndef INC_FSK_DEBUG_H_
#define INC_FSK_DEBUG_H_

/**
 * @brief Enable external-trigger start for online debug mode.
 *
 * 0: Online capture starts automatically during initialization.
 * 1: Online capture waits for external trigger before starting.
 */
#define FSK_EXT_TRIGGER_DEBUG           0U

/**
 * @brief Enable synthetic stream generator instead of real decoder stream.
 *
 * 0: Stream real FSK decoder records.
 * 1: Stream test pattern records.
 */
#define FSK_STREAM_TEST_MODE            0U

/**
 * @brief Offline debug output format selector.
 *
 * 0: Existing full debug record:
 *    period + delta + ma + flags + state.
 *
 * 1: Period-only debug record:
 *    uint16_t period only.
 *
 * Period-only mode allows much deeper offline captures because each
 * processed sample uses only 2 bytes instead of 8 bytes.
 */
#define FSK_DEBUG_OFFLINE_PERIOD_ONLY   1U

/**
 * @brief Enable online live period-only chunk streaming.
 *
 * 0U: Online mode runs normal process_block() decode path.
 * 1U: Online mode streams only uint16_t period chunks.
 *
 * This is intended for long packet debug. It avoids MA/delta/state/full-record
 * streaming overhead on CM4.
 */
#define FSK_DEBUG_LIVE_PERIOD_BLOCK_STREAM   0U


/**
 * @brief Enable online fast period-only streaming.
 *
 * This mode:
 * - computes only period = curr - prev
 * - writes one uint16_t period block per DMA half/full event
 * - does not call process_block()
 * - does not run MA/delta/decode
 */
#define FSK_DEBUG_LIVE_PERIOD_FAST_STREAM      0U

#if (FSK_DEBUG_LIVE_PERIOD_BLOCK_STREAM && FSK_DEBUG_LIVE_PERIOD_FAST_STREAM)
#error "Enable only one: FSK_DEBUG_LIVE_PERIOD_BLOCK_STREAM or FSK_DEBUG_LIVE_PERIOD_FAST_STREAM."
#endif

/**
 * @brief Enable GPIO timing probe around processing blocks.
 *
 * 0: No timing GPIO pulse.
 * 1: Toggle/set debug GPIO around process_block().
 */
#define FSK_DEBUG_PROCESS_TIMING_GPIO   1U

/**
 * @brief Enable decoder transition GPIO toggling.
 *
 * This is useful only when probing decoder events on a scope.
 * Keep disabled during performance-sensitive tests.
 */
#define FSK_DEBUG_TOGGLE_ON_TRANSITION  1U

#endif /* INC_FSK_DEBUG_H_ */
