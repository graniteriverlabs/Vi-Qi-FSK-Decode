/*
 * fsk_config.h
 *
 *  Created on: Feb 24, 2026
 *      Author: kanna
 */

#ifndef INC_FSK_CONFIG_H_
#define INC_FSK_CONFIG_H_

#include "fsk_debug.h"

#define OFFLINE_CAPTURE_MODE   0   // 1 = 64k snapshot mode		0 = continuous circular mode

#if OFFLINE_CAPTURE_MODE

//#define CAPTURE_SAMPLES  16384   // 64 KB snapshot
#if FSK_DEBUG_OFFLINE_PERIOD_ONLY
/*hrtim_capture_buf[] = uint32_t = 4 bytes/sample
debug_buf[]         = period = 2 bytes/sample
Total               = 6 bytes/sample

6*10240 = 60kB
*/
#define CAPTURE_SAMPLES  10240   // 60 KB snapshot
#else
/*hrtim_capture_buf[] = uint32_t = 4 bytes/sample
debug_buf[]         = ipc_stream_record_t = 8 bytes/sample
Total               = 12 bytes/sample

12*5120 = 60kB
*/
#define CAPTURE_SAMPLES  5120   // 20 KB snapshot
#endif

#else

#define CAPTURE_SAMPLES  2048    // circular buffer size
//#define CAPTURE_SAMPLES  512    // circular buffer size

#endif

/**
 * @brief Size of one period-only stream record in bytes.
 *
 * Current live/offline debug stream format is:
 *     uint16_t period
 */
#define FSK_STREAM_RECORD_SIZE_BYTES         2U

/**
 * @brief Number of bytes in one live period chunk.
 */
#define FSK_PERIOD_CHUNK_BYTES               \
    (FSK_PERIOD_CHUNK_PERIODS * FSK_STREAM_RECORD_SIZE_BYTES)

/**
 * @brief Notify CM7 after each successful live period chunk write.
 *
 * 1U is recommended. The notify is low-rate because each notification
 * corresponds to one 1024-byte block, not one sample.
 */
#define FSK_PERIOD_CHUNK_NOTIFY_CM7          1U

/**
 * @brief CAPTURE_SAMPLES must be even because DMA is processed in two halves.
 *
 * The firmware processes:
 * - first half  : index 0 to CAPTURE_SAMPLES/2 - 1
 * - second half : index CAPTURE_SAMPLES/2 to CAPTURE_SAMPLES - 1
 */
#if ((CAPTURE_SAMPLES % 2U) != 0U)
#error "CAPTURE_SAMPLES must be even because DMA half/full processing is used."
#endif

/**
 * @brief Number of raw HRTIM capture samples processed per DMA interrupt.
 *
 * This is derived from CAPTURE_SAMPLES. If CAPTURE_SAMPLES is changed later,
 * this value updates automatically.
 */
#define FSK_DMA_HALF_SAMPLES            (CAPTURE_SAMPLES / 2U)

/**
 * @brief Number of uint16_t period records produced per online period block.
 *
 * One DMA half/full interrupt produces exactly one period block.
 */
#define FSK_PERIOD_BLOCK_PERIODS        (FSK_DMA_HALF_SAMPLES)

/**
 * @brief Number of bytes written to the shared ring per DMA half/full event
 *        in online period-only block streaming mode.
 *
 * Example:
 * - CAPTURE_SAMPLES = 2048
 * - FSK_DMA_HALF_SAMPLES = 1024
 * - FSK_PERIOD_BLOCK_BYTES = 1024 * 2 = 2048 bytes
 */
#define FSK_PERIOD_BLOCK_BYTES             (FSK_PERIOD_BLOCK_PERIODS * FSK_STREAM_RECORD_SIZE_BYTES)


#define HRTIM_TIMER_MAX    			65535u   // must match PERxR
#define DELTA_MIN		   			12		// 30ns/2.5ns = 12
#define DELTA_MAX		   			120		// 360ns/2.5ns = 120

#define BIT1_TARGET_512      		256U
#define BIT0_TARGET_512      		512U
#define IDLE_TARGET_512    			1024U

#define BIT1_TARGET_128   			64U
#define BIT0_TARGET_128   			128U
#define IDLE_TARGET_128    			512U


#define TOL_PERCENT      			20

#define WITHIN(val, target) \
    (((val) > ((target) - ((target) * TOL_PERCENT / 100))) && \
     ((val) < ((target) + ((target) * TOL_PERCENT / 100))))






/************* FSK RESPONSE      **************/

#define FSK_DEFAULT_POWER_MODE      FSK_POWER_MODE_MPP

/**
 * @brief Number of bits in an 8-bit response pattern.
 */
#define FSK_RESPONSE_PATTERN_BITS    8U

/**
 * @brief Minimum number of MPP preamble ONE bits before a data packet.
 *
 * Valid MPP data packet start examples:
 *   1110...
 *   11110...
 *   111110...
 *
 * In all three examples, the final ZERO is the actual start bit of the
 * first 11-bit asynchronous data byte.
 */
#define FSK_MPP_PREAMBLE_MIN_BITS       3U

/**
 * @brief Maximum number of MPP preamble ONE bits before a data packet.
 */
#define FSK_MPP_PREAMBLE_MAX_BITS       5U


/**
 * @brief Number of bits in one asynchronous data-byte frame.
 */
#define FSK_DATA_FRAME_BITS          11U

/**
 * @brief Index of the stop bit inside one 11-bit data-byte frame.
 */
#define FSK_DATA_STOP_BIT_INDEX      10U

#define MPP_ACK 					0x88U
#define ACK 						0xFFU
#define NAK 						0x00U
#define ND 							0xAAU
#define ATN 						0xCCU

#endif /* INC_FSK_CONFIG_H_ */
