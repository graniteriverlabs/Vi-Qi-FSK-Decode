/*
 * fsk_online_stream.h
 *
 *  Created on: Mar 10, 2026
 *      Author: kanna
 */

#ifndef INC_FSK_ONLINE_STREAM_H_
#define INC_FSK_ONLINE_STREAM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "ipc_stream_shared.h"

/**
 * @file fsk_online_stream.h
 * @brief CM4-side online debug streaming API for FSK decoder.
 *
 * This module converts selected decoder observations into compact
 * @ref ipc_stream_record_t records and writes them into the shared ring buffer.
 *
 * The goal is to support online debugging without flooding the transport with
 * full-rate raw sample data.
 */

/**
 * @brief Online streaming modes.
 */
typedef enum
{
    FSK_STREAM_MODE_OFF        = 0, /**< No online streaming */
    FSK_STREAM_MODE_EVENTS     = 1, /**< Stream only interesting events */
    FSK_STREAM_MODE_DOWNSAMPLE = 2  /**< Stream one out of every N samples */
} fsk_stream_mode_t;

/**
 * @brief Runtime configuration for online streaming.
 */
typedef struct
{
    volatile uint8_t  enable;          /**< Master enable switch */
    volatile uint8_t  mode;            /**< One of @ref fsk_stream_mode_t */
    volatile uint16_t downsample_div;  /**< N for 1-of-N streaming */
} fsk_online_stream_cfg_t;

/**
 * @brief Global runtime configuration object.
 */
extern volatile fsk_online_stream_cfg_t g_fsk_online_stream_cfg;

/**
 * @brief Initialize CM4-side online streaming state.
 */
void fsk_online_stream_init(void);

/**
 * @brief Attempt to log one decoder observation.
 *
 * This function should be called from the CM4 decoder path after values such as
 * period, moving average, delayed-difference, sign, and state are known.
 *
 * @param[in] period            Scaled period value.
 * @param[in] ma                Moving average value.
 * @param[in] delta             Signed delayed-difference value.
 * @param[in] toggled           Non-zero if toggle event occurred.
 * @param[in] valid_transition  Non-zero if transition is considered valid.
 * @param[in] sign              Sign of delta (-1, 0, +1).
 * @param[in] decoder_state     Current decoder state.
 *
 * @note In event mode, only interesting events are emitted.
 * @note In downsample mode, records are emitted at 1/N rate.
 */
void fsk_online_stream_maybe_log(uint32_t period,
                                 uint32_t ma,
                                 int32_t  delta,
                                 uint8_t  toggled,
                                 uint8_t  valid_transition,
                                 int8_t   sign,
                                 uint8_t  decoder_state);

#ifdef __cplusplus
}
#endif


#endif /* INC_FSK_ONLINE_STREAM_H_ */
