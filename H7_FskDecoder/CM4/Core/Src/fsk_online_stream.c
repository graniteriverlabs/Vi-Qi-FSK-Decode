/*
 * @file fsk_online_stream.c
 * @brief CM4-side online streaming implementation for FSK decoder.
 *
 * This module reduces high-rate internal decoder activity into a manageable
 * binary event stream suitable for CM4 -> CM7 transfer.
 *  Created on: Mar 10, 2026
 *      Author: kanna
 */


#include "fsk_online_stream.h"

#include "stm32h7xx_hal.h"

/**
 * @brief Runtime configuration for online streaming.
 *
 * Default behavior:
 * - enabled
 * - event mode
 * - downsample divisor = 32 (used only in downsample mode)
 */
volatile fsk_online_stream_cfg_t g_fsk_online_stream_cfg =
{
    .enable         = 1U,
    .mode           = FSK_STREAM_MODE_DOWNSAMPLE,
    .downsample_div = 1U
};

/**
 * @brief Internal sample counter used for downsample mode.
 */
static uint16_t s_downsample_counter = 0U;

/**
 * @brief Number of bytes produced since last CM7 notification.
 *
 * The producer should not notify CM7 for every record. This accumulator lets
 * us notify only after a meaningful amount of fresh data has been generated.
 */
static uint32_t s_notify_accum = 0U;

/**
 * @brief Clamp unsigned 32-bit value into uint16 range.
 *
 * @param[in] value Input value.
 *
 * @return Clamped 16-bit value.
 */
static uint16_t fsk_stream_clamp_u16(uint32_t value)
{
    return (value > 0xFFFFU) ? 0xFFFFU : (uint16_t)value;
}

/**
 * @brief Clamp signed 32-bit value into int16 range.
 *
 * @param[in] value Input value.
 *
 * @return Clamped 16-bit signed value.
 */
static int16_t fsk_stream_clamp_s16(int32_t value)
{
    if (value > 32767)
    {
        return 32767;
    }

    if (value < -32768)
    {
        return -32768;
    }

    return (int16_t)value;
}

/**
 * @brief Notify CM7 that a chunk of stream data is available.
 *
 * This function intentionally performs only a low-rate notification. The heavy
 * transport happens through shared memory, not through the semaphore itself.
 *
 * @note Replace HSEM ID with your actual project-defined channel if needed.
 */
static void fsk_stream_notify_cm7(void)
{
    /**
     * If your project already has an HSEM channel definition, use that macro
     * here instead of hardcoded 0.
     */
    HAL_HSEM_Release(0U, 0U);
}

void fsk_online_stream_init(void)
{
    s_downsample_counter = 0U;
    s_notify_accum       = 0U;
}

void fsk_online_stream_maybe_log(uint32_t period,
                                 uint32_t ma,
                                 int32_t  delta,
                                 uint8_t  toggled,
                                 uint8_t  valid_transition,
                                 int8_t   sign,
                                 uint8_t  decoder_state)
{
    ipc_stream_record_t rec;
    uint8_t should_emit = 0U;

    if (g_fsk_online_stream_cfg.enable == 0U)
    {
        return;
    }

    switch ((fsk_stream_mode_t)g_fsk_online_stream_cfg.mode)
    {
        case FSK_STREAM_MODE_EVENTS:
        {
            /**
             * Event mode is meant for continuous online visibility with low
             * bandwidth. Only interesting moments are emitted.
             */
            if ((valid_transition != 0U) || (toggled != 0U))
            {
                should_emit = 1U;
            }
            break;
        }

        case FSK_STREAM_MODE_DOWNSAMPLE:
        {
            /**
             * Downsample mode is useful when you want a reduced trend view of
             * signal behavior rather than only discrete events.
             */
            s_downsample_counter++;

            if (s_downsample_counter >= g_fsk_online_stream_cfg.downsample_div)
            {
                s_downsample_counter = 0U;
                should_emit = 1U;
            }
            break;
        }

        case FSK_STREAM_MODE_OFF:
        default:
        {
            should_emit = 0U;
            break;
        }
    }

    if (should_emit == 0U)
    {
        return;
    }

    rec.period = fsk_stream_clamp_u16(period);
    rec.ma     = fsk_stream_clamp_u16(ma);
    rec.delta  = fsk_stream_clamp_s16(delta);
    rec.flags  = 0U;
    rec.state  = decoder_state;

    if (toggled != 0U)
    {
        rec.flags |= IPC_STREAM_FLAG_TOGGLED;
    }

    if (valid_transition != 0U)
    {
        rec.flags |= IPC_STREAM_FLAG_VALID_TRANSITION;
    }

    if (sign > 0)
    {
        rec.flags |= IPC_STREAM_FLAG_SIGN_POSITIVE;
    }

    if (ipc_stream_write((const uint8_t *)&rec, sizeof(rec)) == 0)
    {
        s_notify_accum += sizeof(rec);

        if (s_notify_accum >= IPC_STREAM_NOTIFY_CHUNK)
        {
            s_notify_accum = 0U;
            fsk_stream_notify_cm7();
        }
    }
}
