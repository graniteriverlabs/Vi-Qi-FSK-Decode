/*
 * cm7_stream_drain.h
 *
 *  Created on: Mar 11, 2026
 *      Author: kanna
 */

#ifndef INC_CM7_STREAM_DRAIN_H_
#define INC_CM7_STREAM_DRAIN_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @file cm7_stream_drain.h
 * @brief CM7-side helpers to drain CM4 stream ring and prepare UDP payloads.
 *
 * This module pulls bytes from the shared ring into a local UDP staging buffer.
 * The caller is responsible for actually sending the payload using the network
 * stack already present on CM7.
 */

/**
 * @brief Size of the UDP payload staging buffer.
 *
 * Chosen below standard Ethernet MTU-safe UDP payload.
 */
#define CM7_UDP_STAGING_SIZE   1400U

/**
 * @brief Initialize CM7-side drain state.
 */
void cm7_stream_drain_init(void);

/**
 * @brief Drain available bytes into a local UDP staging buffer.
 *
 * @param[out] out_buf Output buffer.
 * @param[in]  out_len Size of output buffer.
 *
 * @return Number of bytes placed into @p out_buf.
 *
 * @note The caller can send the returned chunk as one UDP payload.
 */
uint32_t cm7_stream_drain_fill_packet(uint8_t *out_buf, uint32_t out_len);

#ifdef __cplusplus
}
#endif

#endif /* INC_CM7_STREAM_DRAIN_H_ */
