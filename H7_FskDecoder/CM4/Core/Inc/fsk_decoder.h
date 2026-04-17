/*
 * fsk_decoder.h
 *
 *  Created on: Feb 24, 2026
 *      Author: kanna
 */

#ifndef INC_FSK_DECODER_H_
#define INC_FSK_DECODER_H_


#include "stdio.h"
#include <fsk_config.h>

/* Enums */
typedef enum
{
    RX_IDLE,
    RX_COLLECT_BITS,
} rx_state_t;

typedef enum
{
    SYMBOL_NONE = 0,
    SYMBOL_HALF,	// ~256
    SYMBOL_FULL,	// ~512
    SYMBOL_IDLE,
    SYMBOL_INVALID
} symbol_t;

typedef enum
{
    FSK_LIMIT_512 = 0,
    FSK_LIMIT_128 = 1
} fsk_limit_mode_t;

/**
 * @brief High-level FSK frame format currently expected by the decoder.
 *
 * Response patterns are 8-bit packets without start/parity/stop.
 * Data bytes are 11-bit asynchronous frames:
 * start + 8 data bits + parity + stop.
 */
typedef enum
{
    FSK_FRAME_MODE_RESPONSE = 0U,
    FSK_FRAME_MODE_DATA     = 1U
} fsk_frame_mode_t;

/******************************************************************************/
/* FSK PROTOCOL MODE TYPES                                                     */
/******************************************************************************/

/**
 * @brief Active wireless-power protocol mode for FSK decoder behavior.
 *
 * BPP:
 *   No FSK communication is expected.
 *
 * EPP:
 *   FSK is used, but data packets do not contain MPP preamble bits.
 *
 * MPP:
 *   FSK is used, and data packets contain 3..5 leading ONE preamble bits
 *   before the first ZERO start bit.
 */
typedef enum
{
    FSK_POWER_MODE_BPP = 0U,
    FSK_POWER_MODE_EPP = 1U,
    FSK_POWER_MODE_MPP = 2U
} fsk_power_mode_t;

/**
 * @brief Preamble validation result for MPP data packets.
 */
typedef enum
{
    FSK_PREAMBLE_ERROR_NONE = 0U,
    FSK_PREAMBLE_ERROR_TOO_SHORT,
    FSK_PREAMBLE_ERROR_TOO_LONG,
    FSK_PREAMBLE_ERROR_NO_START_BIT
} fsk_preamble_error_t;


/**
 * @brief Set active FSK protocol mode.
 *
 * Call this when the application knows whether the current session is
 * BPP, EPP, or MPP.
 *
 * @param[in] mode Active power mode.
 */
void fsk_decoder_set_power_mode(fsk_power_mode_t mode);

/**
 * @brief Get active FSK protocol mode.
 *
 * @return Current decoder power mode.
 */
fsk_power_mode_t fsk_decoder_get_power_mode(void);

/**
 * @brief Number of MPP preamble validation errors.
 */
extern volatile uint32_t g_fsk_preamble_error_count;

/**
 * @brief Last detected MPP preamble length in bits.
 */
extern volatile uint8_t g_fsk_last_preamble_bits;

/**
 * @brief Last MPP preamble validation error.
 *
 * Value is from @ref fsk_preamble_error_t.
 */
extern volatile uint8_t g_fsk_last_preamble_error;

/**
 * @brief Number of data frame errors after protocol-specific preprocessing.
 */
extern volatile uint32_t g_fsk_data_frame_error_count;

#ifndef CAPTURE_SAMPLES
#error "CAPTURE_SAMPLES must be defined before including fsk_capture.h"
#endif

extern volatile uint32_t hrtim_capture_buf[CAPTURE_SAMPLES];

extern volatile uint8_t debugFlag;
extern volatile uint8_t printdata;







/* Function Declarations*/
void process_block(uint32_t start, uint32_t length);
void post_processing(void);
#if OFFLINE_CAPTURE_MODE
void ethernet_dump_capture(void);
extern volatile uint32_t debug_index;
void uart_dump_capture(void);
#endif
void fsk_decoder_reset_state(void);
#endif /* INC_FSK_DECODER_H_ */
