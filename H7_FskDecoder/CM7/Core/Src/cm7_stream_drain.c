/*
 * cm7_stream_drain.c
 *
 *  Created on: Mar 11, 2026
 *      Author: kanna
 */


#include "cm7_stream_drain.h"

#include "ipc_stream_shared.h"

/**
 * @file cm7_stream_drain.c
 * @brief CM7-side stream draining implementation.
 *
 * The job of this module is deliberately simple:
 * - read from shared ring
 * - fill a local packet-sized buffer
 * - return payload length
 *
 * The networking stack can then transmit that payload over UDP.
 */

void cm7_stream_drain_init(void)
{
    /**
     * No internal state required at present.
     * Function kept for symmetry and future extension.
     */
}

uint32_t cm7_stream_drain_fill_packet(uint8_t *out_buf, uint32_t out_len)
{
    if ((out_buf == NULL) || (out_len == 0U))
    {
        return 0U;
    }

    if (out_len > CM7_UDP_STAGING_SIZE)
    {
        out_len = CM7_UDP_STAGING_SIZE;
    }

    /**
     * Read as much as possible up to the packet limit.
     *
     * In most designs, the caller will repeatedly call this until it returns 0,
     * sending each non-zero chunk as one UDP packet.
     */
    return ipc_stream_read(out_buf, out_len);
}
