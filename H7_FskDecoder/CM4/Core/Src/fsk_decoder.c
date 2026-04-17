/*
 * fsk_decoder.c
 *
 *  Created on: Feb 24, 2026
 *      Author: kanna
 */

#include "main.h"
#include <fsk_decoder.h>
#include <fsk_config.h>
#include <fsk_debug.h>
#include <fsk_online_stream.h>
#include <ipc_stream_shared.h>

#if OFFLINE_CAPTURE_MODE

__attribute__((aligned(4)))
volatile uint32_t hrtim_capture_buf[CAPTURE_SAMPLES];



typedef struct __attribute__((packed))
{
    uint32_t period;
    uint32_t ma;
    uint16_t delta;
    uint8_t  toggled;
    uint8_t  reserved; /**< Pad to 12 bytes */
} debug_sample_t;
//__attribute__((section(".FskRemoteSection"), aligned(32)))
#if FSK_DEBUG_OFFLINE_PERIOD_ONLY

__attribute__((aligned(32)))
uint16_t debug_period_buf[CAPTURE_SAMPLES];

#else

__attribute__((aligned(32)))
ipc_stream_record_t debug_buf[CAPTURE_SAMPLES];

#endif
volatile uint32_t debug_index =0;

#else

__attribute__((aligned(4)))
volatile uint32_t hrtim_capture_buf[CAPTURE_SAMPLES];

#endif

/* 16 Moving Avg variables */
#define MA_WINDOW 16

static uint32_t ma_buffer[MA_WINDOW];
static uint32_t ma_sum = 0;
static uint8_t  ma_index = 0;
static uint8_t  ma_filled = 0;

/* Delayed Diff Variables*/
#define DELAY 24

static uint32_t ma_delay_buffer[DELAY];
static uint8_t  delay_index = 0;
static uint8_t  delay_filled = 0;

/* Decoder Logic Variables*/
static int8_t active = 0;		// 0 = waiting for first edge
static int8_t last_sign = 0;   // +1 or -1
static uint16_t period_counter = 0;

typedef struct
{
    rx_state_t	state;

    uint8_t 	half_pending;
    uint16_t    bit_count;
    uint8_t     bit_stream[256];
} decoder_t;
static decoder_t decoder;

/* Function Declarations BEGIN */
static inline uint32_t moving_average_16(uint32_t new_value);
static inline int32_t delayed_diff(uint32_t new_ma);
static inline symbol_t classify_symbol(uint32_t count);
static inline uint16_t fsk_decoder_get_idle_target(void);
static inline void push_bit(uint8_t bit);
static uint8_t calculate_parity(uint8_t byte);
static void process_frame(void);
static void fsk_decoder_handle_symbol(symbol_t symbol);
static void fsk_decoder_handle_idle_end(void);
static void fsk_decoder_reset_collection_state(void);
static uint8_t fsk_decoder_try_complete_tail_on_idle(void);
static uint8_t fsk_decoder_is_frame_ready(void);
static uint8_t fsk_decoder_prepare_data_bits_for_active_mode(void);
static uint8_t fsk_decoder_strip_mpp_preamble(void);
static void fsk_decoder_report_mpp_preamble_error(void);
static uint8_t fsk_decoder_validate_and_dispatch_data_frames(void);
static uint8_t fsk_decoder_find_mpp_start_index(uint16_t *start_index);
/* Function Declarations END*/


static uint32_t prev_timestamp = 0;
static fsk_limit_mode_t fsk_limit_mode = FSK_LIMIT_512;

/******************************************************************************/
/* FSK PROTOCOL MODE STATE                                                     */
/******************************************************************************/

/**
 * @brief Active protocol mode used by the FSK decoder.
 *
 * This is intentionally separate from fsk_frame_mode.
 *
 * fsk_frame_mode tells whether the currently collected bits are:
 * - response pattern
 * - data packet
 *
 * fsk_power_mode tells how a data packet should be interpreted:
 * - BPP: no FSK expected
 * - EPP: no preamble
 * - MPP: data packet has 3..5 preamble ONE bits
 */
static fsk_power_mode_t s_fsk_power_mode = FSK_DEFAULT_POWER_MODE;

/**
 * @brief Number of MPP preamble errors detected.
 */
volatile uint32_t g_fsk_preamble_error_count = 0U;

/**
 * @brief Last detected MPP preamble length.
 */
volatile uint8_t g_fsk_last_preamble_bits = 0U;

/**
 * @brief Last MPP preamble error reason.
 */
volatile uint8_t g_fsk_last_preamble_error = FSK_PREAMBLE_ERROR_NONE;

/**
 * @brief Number of data frame errors after protocol-specific preprocessing.
 */
volatile uint32_t g_fsk_data_frame_error_count = 0U;

void process_block(uint32_t start, uint32_t length)
{
#if (!OFFLINE_CAPTURE_MODE) && FSK_DEBUG_LIVE_PERIOD_BLOCK_STREAM
    /*
     * Start one online period block for this process_block() call.
     *
     * process_block() is expected to be called once per DMA half/full block.
     * Therefore this should result in one shared-ring write at the end.
     */
    fsk_online_period_block_begin(length);
#endif

    for (uint32_t i = 0; i < length; i++)
    {
#if OFFLINE_CAPTURE_MODE
        if (debug_index >= CAPTURE_SAMPLES)
        {
        	__BKPT(0);
            return;
        }
#endif
        uint32_t curr = hrtim_capture_buf[start + i];
        uint32_t period;

        /**
         * @brief Indicates whether the current iteration produced a
         *        decoder toggle event.
         *
         * This flag is exported only for online streaming visibility.
         * It is reset at the beginning of every processed sample.
         */
        uint8_t toggled = 0U;

        /**
         * @brief Indicates whether the current delayed-difference value
         *        lies inside the valid transition threshold window.
         *
         * This is used by the online stream module in event mode.
         */
        uint8_t valid_transition = 0U;

        if (curr >= prev_timestamp)
            period = curr - prev_timestamp;
        else
            period = (HRTIM_TIMER_MAX - prev_timestamp) + curr + 1U;

//        period = period / 32;
        prev_timestamp = curr;

#if (!OFFLINE_CAPTURE_MODE) && FSK_DEBUG_LIVE_PERIOD_BLOCK_STREAM
        /*
         * Store period into the current block.
         * This does not write to the shared ring per sample.
         */
        fsk_online_period_block_add(period);
#endif

        uint32_t ma = moving_average_16(period);
        int32_t diff = delayed_diff(ma);
        int32_t abs_diff = (diff >=0) ? diff : -diff;



        int8_t sign = 0;

        if (diff > 0)	sign = 1;
        else if (diff < 0)	sign = -1;

        /* Valid transition condition */
        if ((abs_diff > DELTA_MIN) && (abs_diff < DELTA_MAX))
        {
            /**
             * @brief Mark this sample as a valid transition candidate.
             *
             * The decoder already uses this threshold band for signal
             * interpretation. We reuse the same information for online
             * debug streaming.
             */
            valid_transition = 1U;

            if (!active)
            {
                // First transition detected
                active = 1;
                last_sign = sign;
                period_counter = 0;
#if FSK_DEBUG_TOGGLE_ON_TRANSITION
                HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_14);
#endif
            }
            else
            {
                // Already counting — check if opposite sign
                if (sign == -last_sign)
                {
                    // Opposite edge detected
                    /**
                     * @brief Mark that the current sample produced a
                     *        decoder toggle event.
                     *
                     * This is useful in online debug mode because it lets
                     * the host visualize when the decoder recognized an
                     * opposite-sign transition.
                     */
                    toggled = 1U;

                	symbol_t symbol = classify_symbol(period_counter);

					period_counter = 0;
					last_sign = sign;

					/**
					 * @brief Feed the classified symbol into the decoder state machine.
					 *
					 * This function handles response-pattern decoding and data-byte decoding
					 * using the current fsk_frame_mode.
					 */
					fsk_decoder_handle_symbol(symbol);

#if FSK_DEBUG_TOGGLE_ON_TRANSITION
					HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_14);
#endif
                }
            }
        }

        /* If active, keep counting */
        if (active)
        {
        	uint16_t idle_target = fsk_decoder_get_idle_target();
        	if (period_counter < idle_target)
        	{
        		period_counter++;
        	}
        	else
        	{
        	    /**
        	     * @brief No further transition arrived before the idle timeout.
        	     *
        	     * This handles the case where the final response/data bit ends without
        	     * another opposite-sign transition, so SYMBOL_IDLE is never reached
        	     * through normal symbol classification.
        	     */
        	    fsk_decoder_handle_idle_end();
        	}
        }

#if OFFLINE_CAPTURE_MODE

#if FSK_DEBUG_OFFLINE_PERIOD_ONLY

        debug_period_buf[debug_index] =
            (period > 0xFFFFU) ? 0xFFFFU : (uint16_t)period;

        debug_index++;

#else
        debug_buf[debug_index].period = (period > 0xFFFFU) ? 0xFFFFU : (uint16_t)period;
        debug_buf[debug_index].ma     = (ma > 0xFFFFU) ? 0xFFFFU : (uint16_t)ma;

        debug_buf[debug_index].delta = (int16_t)abs_diff;

        debug_buf[debug_index].flags = 0U;
        if (toggled)
        {
            debug_buf[debug_index].flags |= IPC_STREAM_FLAG_TOGGLED;
        }

        if (valid_transition)
        {
            debug_buf[debug_index].flags |= IPC_STREAM_FLAG_VALID_TRANSITION;
        }

        if (sign > 0)
        {
            debug_buf[debug_index].flags |= IPC_STREAM_FLAG_SIGN_POSITIVE;
        }
        debug_buf[debug_index].state = (uint8_t)decoder.state;;

        debug_index++;
#endif

#else

#if FSK_STREAM_TEST_MODE
        fsk_stream_test_emit();
#else

        /**
         * Online full-record streaming is intentionally removed.
         *
         * If FSK_DEBUG_LIVE_PERIOD_FAST_STREAM is enabled, this function is not
         * called in online mode. main.c calls
         * fsk_online_period_fast_block_process() instead.
         *
         * If normal online decode mode is enabled, this function performs
         * decoding only and produces no full-record stream.
         */
        (void)toggled;
        (void)valid_transition;

#endif

#endif
    }
#if (!OFFLINE_CAPTURE_MODE) && FSK_DEBUG_LIVE_PERIOD_BLOCK_STREAM
    /*
     * One process_block() call -> one shared-ring write.
     */
    fsk_online_period_block_end();
#endif
}

#if OFFLINE_CAPTURE_MODE

void ethernet_dump_capture(void)
{
    uint32_t notify_accum = 0U;

    for (uint32_t i = 0; i < debug_index; i++)
    {
#if FSK_DEBUG_OFFLINE_PERIOD_ONLY
        while (ipc_stream_write((const uint8_t *)&debug_period_buf[i], sizeof(debug_period_buf[i])) != 0)
#else
        while (ipc_stream_write((const uint8_t *)&debug_buf[i], sizeof(debug_buf[i])) != 0)
#endif
        {
            HAL_Delay(1);
        }

#if FSK_DEBUG_OFFLINE_PERIOD_ONLY
        notify_accum += sizeof(debug_period_buf[i]);
#else
        notify_accum += sizeof(debug_buf[i]);
#endif

        if (notify_accum >= IPC_STREAM_NOTIFY_CHUNK)
        {
            notify_accum = 0U;
            HAL_HSEM_Release(0U, 0U);
        }
    }

    if (notify_accum != 0U)
    {
        HAL_HSEM_Release(0U, 0U);
    }
}

#if 0
void uart_dump_capture(void)
{
    char line[64];

    /* Start marker */
    const char *start = "START\r\n";
//    HAL_UART_Transmit(&huart1,
//                      (uint8_t *)start,
//                      strlen(start),
//                      HAL_MAX_DELAY);
    printf("%s \n", start);

    for (uint32_t i = 0; i < CAPTURE_SAMPLES; i++)
    {
    	printf("%lu\t%lu\t%lu\t%u\r\n",
    	       (unsigned long)debug_buf[i].period,
    	       (unsigned long)debug_buf[i].ma,
    	       (unsigned long)debug_buf[i].delta,
    	       debug_buf[i].flags);
    }

    /* End marker */
    const char *end = "END\r\n";
//    HAL_UART_Transmit(&huart1,
//                      (uint8_t *)end,
//                      strlen(end),
//                      HAL_MAX_DELAY);
    printf("%s \n", end);
}
#endif
#endif

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
#if OFFLINE_CAPTURE_MODE || FSK_EXT_TRIGGER_DEBUG
    if (GPIO_Pin == FSK_TRIG_Pin)
    {
        if (!capture_active)
        {
#if OFFLINE_CAPTURE_MODE
            debug_index = 0;
#endif
            start_capture();
        }
    }
#endif
}


static inline uint32_t moving_average_16(uint32_t new_value)
{
    if (ma_filled < MA_WINDOW)
    {
        ma_buffer[ma_index++] = new_value;
        ma_sum += new_value;
        ma_filled++;

        if (ma_index >= MA_WINDOW)
            ma_index = 0;

        return ma_sum / ma_filled;   // ramp-up phase
    }

    /* subtract oldest */
    ma_sum -= ma_buffer[ma_index];

    /* insert newest */
    ma_buffer[ma_index] = new_value;
    ma_sum += new_value;

    ma_index++;
    if (ma_index >= MA_WINDOW)
        ma_index = 0;

    return (uint32_t)(ma_sum >> 4);   // divide by 16
}

static inline int32_t delayed_diff(uint32_t new_ma)
{
    int32_t old_ma = 0;

    if (delay_filled < DELAY)
    {
        ma_delay_buffer[delay_index++] = new_ma;
        delay_filled++;

        if (delay_index >= DELAY)
            delay_index = 0;

        return 0;   // not enough data yet
    }

    old_ma = ma_delay_buffer[delay_index];

    ma_delay_buffer[delay_index] = new_ma;

    delay_index++;
    if (delay_index >= DELAY)
        delay_index = 0;

    return (new_ma - old_ma);	//signed difference
}


/**
 * @brief Process one completed FSK packet.
 *
 * Packet type is decided dynamically from the number of collected bits:
 *
 * - 8 bits:
 *   Response packet such as MPP_ACK, ACK, NAK, ND, ATN.
 *
 * - More than 8 bits:
 *   Data packet. BPP/EPP decode directly as 11-bit frames. MPP first removes
 *   the 3..5 preamble ONE bits and keeps the following ZERO as the start bit.
 *
 * Timing mode is independent from packet type:
 * - Before MPP_ACK: usually 512-cycle timing.
 * - After MPP_ACK : 128-cycle timing remains active for later data/response
 *   packets until the decoder is reset.
 */
static void process_frame(void)
{
    /**
     * Response packet path.
     */
    if (decoder.bit_count == FSK_RESPONSE_PATTERN_BITS)
    {
        uint8_t response = 0U;

        /**
         * Response patterns are compared in received/wire order.
         *
         * Example:
         * 10001000 -> 0x88 -> MPP_ACK
         */
        for (uint8_t i = 0U; i < FSK_RESPONSE_PATTERN_BITS; i++)
        {
            response = (uint8_t)((response << 1U) |
                                 (decoder.bit_stream[i] & 0x01U));
        }

        switch (response)
        {
            case MPP_ACK:
            {
                /**
                 * MPP_ACK received.
                 *
                 * Important:
                 * This changes only the timing mode. It does not force the
                 * next packet to be DATA. The next packet type is decided
                 * dynamically from bit_count.
                 */
                fsk_limit_mode = FSK_LIMIT_128;

                HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_10);
                break;
            }

            case ACK:
            {
                HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_10);
                break;
            }

            case NAK:
            {
                /**
                 * NAK received.
                 */
                break;
            }

            case ND:
            {
                /**
                 * ND received.
                 */
                break;
            }

            case ATN:
            {
                /**
                 * ATN received.
                 */
                break;
            }

            default:
            {
                /**
                 * Unknown 8-bit response pattern.
                 */
                break;
            }
        }

        return;
    }

    /**
     * Data packet path.
     *
     * Any packet longer than 8 bits is treated as data.
     */
    if (decoder.bit_count > FSK_RESPONSE_PATTERN_BITS)
    {
        /**
         * Apply protocol-specific data preparation.
         *
         * BPP/EPP:
         *   No MPP preamble handling.
         *
         * MPP:
         *   Strip 3..5 leading ONE preamble bits.
         */
        if (fsk_decoder_prepare_data_bits_for_active_mode() == 0U)
        {
            return;
        }

        /**
         * Decode prepared 11-bit data frames.
         */
        (void)fsk_decoder_validate_and_dispatch_data_frames();

        return;
    }

    /**
     * Less than 8 bits is incomplete/invalid.
     */
}


/**
 * @brief Try to complete a missing final bit when idle is reached.
 *
 * Dynamic rule:
 *
 * - 7 bits:
 *   Treat as response-tail recovery. Complete the 8th response bit.
 *
 * - More than 8 bits:
 *   Treat as data-tail recovery. Complete missing STOP bit if the current
 *   position is start + 8 data + parity.
 *
 * @return 1U if one bit was appended, otherwise 0U.
 */
static uint8_t fsk_decoder_try_complete_tail_on_idle(void)
{
    if (decoder.state != RX_COLLECT_BITS)
    {
        return 0U;
    }

    /**
     * Response tail recovery:
     *
     * If the response has 7 bits and idle arrives, complete the 8th bit.
     */
    if (decoder.bit_count == (FSK_RESPONSE_PATTERN_BITS - 1U))
    {
        if (decoder.half_pending != 0U)
        {
            push_bit(1U);
            decoder.half_pending = 0U;
        }
        else
        {
            push_bit(0U);
        }

        return 1U;
    }

    /**
     * Data tail recovery:
     *
     * Only packets that already crossed 8 bits can be treated as data.
     */
    if (decoder.bit_count > FSK_RESPONSE_PATTERN_BITS)
    {
        uint16_t data_bit_count;
        uint16_t bit_pos;

        if (s_fsk_power_mode == FSK_POWER_MODE_MPP)
        {
            uint16_t start_index = 0U;

            /**
             * MPP has preamble bits before the real 11-bit data frame.
             * Tail recovery only needs the real start-bit index.
             */
            if (fsk_decoder_find_mpp_start_index(&start_index) == 0U)
            {
                return 0U;
            }

            data_bit_count = decoder.bit_count - start_index;
        }
        else
        {
            /**
             * BPP/EPP:
             * No preamble. bit_count maps directly to the 11-bit frame
             * position.
             */
            data_bit_count = decoder.bit_count;
        }

        bit_pos = data_bit_count % FSK_DATA_FRAME_BITS;

        /**
         * If 10 bits of the current 11-bit data byte are collected, the
         * missing final bit is the STOP bit. STOP bit is always ONE.
         */
        if (bit_pos == FSK_DATA_STOP_BIT_INDEX)
        {
            push_bit(1U);
            decoder.half_pending = 0U;
            return 1U;
        }
    }

    return 0U;
}

/**
 * @brief Check whether the collected bit stream is ready for processing.
 *
 * Dynamic packet decision:
 *
 * - 8 bits:
 *   Response packet.
 *
 * - More than 8 bits:
 *   Data packet.
 *
 * BPP/EPP data packets must already be aligned to 11-bit frames.
 * MPP data packets are allowed to be non-11-aligned before preamble removal.
 *
 * @return 1U if process_frame() should be called.
 */
static uint8_t fsk_decoder_is_frame_ready(void)
{
    if (decoder.bit_count == FSK_RESPONSE_PATTERN_BITS)
    {
        return 1U;
    }

    if (decoder.bit_count > FSK_RESPONSE_PATTERN_BITS)
    {
        if (s_fsk_power_mode == FSK_POWER_MODE_MPP)
        {
            /**
             * MPP has preamble bits before the start bit. Let process_frame()
             * strip the preamble and then validate 11-bit alignment.
             */
            return 1U;
        }

        /**
         * BPP/EPP:
         * No preamble. Data must already be one or more complete 11-bit frames.
         */
        if ((decoder.bit_count % FSK_DATA_FRAME_BITS) == 0U)
        {
            return 1U;
        }
    }

    return 0U;
}

/**
 * @brief Handle end of the current FSK frame.
 *
 * This is used by both:
 * - SYMBOL_IDLE path
 * - idle timeout path where no further transition arrives
 */
static void fsk_decoder_handle_idle_end(void)
{
    if (decoder.state == RX_COLLECT_BITS)
    {
        /**
         * Complete only valid protocol tails.
         */
        (void)fsk_decoder_try_complete_tail_on_idle();

        /**
         * Process only complete response/data frames.
         * Do not process arbitrary partial bit counts.
         */
        if (fsk_decoder_is_frame_ready() != 0U)
        {
            process_frame();
            debugFlag = 1U;
        }
    }

    fsk_decoder_reset_collection_state();
}

/**
 * @brief Reset only the bit-collection state.
 *
 * This does not reset the moving average, delayed-difference history,
 * or HRTIM timestamp tracking. It only ends the current packet/frame.
 */
static void fsk_decoder_reset_collection_state(void)
{
    decoder.state = RX_IDLE;
    decoder.bit_count = 0U;
    decoder.half_pending = 0U;

    active = 0;
    last_sign = 0;
    period_counter = 0U;
}

static inline void push_bit(uint8_t bit)
{
    if (decoder.bit_count < sizeof(decoder.bit_stream))
        decoder.bit_stream[decoder.bit_count++] = bit;
}

static inline symbol_t classify_symbol(uint32_t count)
{
    uint16_t bit1_target, bit0_target;
    uint16_t idle_target = fsk_decoder_get_idle_target();

    if (fsk_limit_mode == FSK_LIMIT_512)
    {
        bit1_target  = BIT1_TARGET_512;
        bit0_target  = BIT0_TARGET_512;
    }
    else if(fsk_limit_mode == FSK_LIMIT_128)
    {
        bit1_target  = BIT1_TARGET_128;
        bit0_target  = BIT0_TARGET_128;
    }

    if (WITHIN(count, bit1_target))
    {
        return SYMBOL_HALF;
    }

    if (WITHIN(count, bit0_target))
    {
    	return SYMBOL_FULL;
    }

    if (WITHIN(count, idle_target))
    {
    	return SYMBOL_IDLE;
    }

    return SYMBOL_INVALID;
}

/**
 * @brief Get active idle target based on current FSK timing mode.
 *
 * The decoder uses different timing limits before and after MPP_ACK:
 *
 * - FSK_LIMIT_512:
 *   Used during response-pattern decoding.
 *
 * - FSK_LIMIT_128:
 *   Used during data-packet decoding after MPP_ACK.
 *
 * This helper ensures that idle classification and idle-timeout handling
 * always use the same target value.
 *
 * @return Active idle target count.
 */
static inline uint16_t fsk_decoder_get_idle_target(void)
{
    if (fsk_limit_mode == FSK_LIMIT_128)
    {
        return IDLE_TARGET_128;
    }

    /*
     * Default/fallback is response-phase timing.
     */
    return IDLE_TARGET_512;
}

/**
 * @brief Feed one classified symbol into the bit collector.
 *
 * Symbol meaning:
 * - SYMBOL_HALF: one half of a bit-1 symbol.
 * - SYMBOL_FULL: complete bit-0 symbol.
 * - SYMBOL_IDLE: end of current frame.
 * - SYMBOL_INVALID: timing error, reset current collection.
 *
 * The bit encoding used by the existing decoder is:
 * - two HALF symbols => bit 1
 * - one FULL symbol  => bit 0
 */
static void fsk_decoder_handle_symbol(symbol_t symbol)
{
    switch (decoder.state)
    {
        case RX_IDLE:
        {
            /**
             * The first valid half/full symbol after idle starts collection.
             *
             * Note:
             * This preserves your existing behavior, where the first detected
             * symbol is used to establish collection rather than immediately
             * pushing a bit.
             */
            if ((symbol == SYMBOL_HALF) || (symbol == SYMBOL_FULL))
            {
                decoder.state = RX_COLLECT_BITS;
                decoder.bit_count = 0U;
                decoder.half_pending = 1U;
            }
            break;
        }

        case RX_COLLECT_BITS:
        {
            switch (symbol)
            {
                case SYMBOL_HALF:
                {
                    if (decoder.half_pending == 0U)
                    {
                        decoder.half_pending = 1U;
                    }
                    else
                    {
                        /**
                         * Two half-symbols form bit 1.
                         */
                        push_bit(1U);
                        decoder.half_pending = 0U;
                    }
                    break;
                }

                case SYMBOL_FULL:
                {
                    /**
                     * One full-symbol forms bit 0.
                     */
                    push_bit(0U);
                    decoder.half_pending = 0U;
                    break;
                }

                case SYMBOL_IDLE:
                {
                    /**
                     * End-of-frame detected through normal symbol
                     * classification.
                     */
                    fsk_decoder_handle_idle_end();
                    break;
                }

                case SYMBOL_INVALID:
                default:
                {
                    /**
                     * Invalid timing means the current frame is unreliable.
                     * Reset only the collector state, not the MA/diff filters.
                     */
                    fsk_decoder_reset_collection_state();
                    break;
                }
            }
            break;
        }

        default:
        {
            fsk_decoder_reset_collection_state();
            break;
        }
    }
}

/**
 * @brief Prepare collected data bits according to active protocol mode.
 *
 * BPP/EPP:
 *   No MPP preamble handling is applied. The collected stream is expected
 *   to start directly with the 11-bit asynchronous data frame start bit.
 *
 * MPP:
 *   Data stream starts with 3..5 leading ONE preamble bits followed by
 *   the ZERO start bit. The preamble bits are removed before 11-bit decode.
 *
 * @retval 1U Data bit stream is ready for 11-bit frame decode.
 * @retval 0U Data bit stream is invalid for the active mode.
 */
static uint8_t fsk_decoder_prepare_data_bits_for_active_mode(void)
{
    switch (s_fsk_power_mode)
    {
        case FSK_POWER_MODE_MPP:
        {
            /**
             * MPP data packet must contain 3..5 preamble ONE bits.
             * Strip them before normal 11-bit data-frame decoding.
             */
            return fsk_decoder_strip_mpp_preamble();
        }

        case FSK_POWER_MODE_BPP:
        case FSK_POWER_MODE_EPP:
        default:
        {
            /**
             * BPP and EPP use the existing decode behavior.
             * No preamble bits are expected or removed.
             */
            return 1U;
        }
    }
}

/**
 * @brief Analyze current bit stream and report MPP preamble error.
 *
 * This function is called only when MPP preamble validation failed during
 * final packet processing.
 */
static void fsk_decoder_report_mpp_preamble_error(void)
{
    uint16_t i = 0U;

    while ((i < decoder.bit_count) && (decoder.bit_stream[i] == 1U))
    {
        i++;

        if (i > FSK_MPP_PREAMBLE_MAX_BITS)
        {
            g_fsk_preamble_error_count++;
            g_fsk_last_preamble_bits  = (uint8_t)i;
            g_fsk_last_preamble_error = FSK_PREAMBLE_ERROR_TOO_LONG;
            return;
        }
    }

    g_fsk_last_preamble_bits = (uint8_t)i;

    if (i < FSK_MPP_PREAMBLE_MIN_BITS)
    {
        g_fsk_preamble_error_count++;
        g_fsk_last_preamble_error = FSK_PREAMBLE_ERROR_TOO_SHORT;
        return;
    }

    if (i >= decoder.bit_count)
    {
        g_fsk_preamble_error_count++;
        g_fsk_last_preamble_error = FSK_PREAMBLE_ERROR_NO_START_BIT;
        return;
    }

    if (decoder.bit_stream[i] != 0U)
    {
        g_fsk_preamble_error_count++;
        g_fsk_last_preamble_error = FSK_PREAMBLE_ERROR_NO_START_BIT;
        return;
    }

    /**
     * Should normally not reach here because caller invokes this only after
     * validation failure.
     */
    g_fsk_last_preamble_error = FSK_PREAMBLE_ERROR_NONE;
}


/**
 * @brief Strip valid MPP preamble bits from decoder.bit_stream[].
 *
 * Before:
 *   1 1 1 0 d0 d1 ...
 *
 * After:
 *   0 d0 d1 ...
 *
 * The ZERO start bit is preserved and moved to bit_stream[0].
 *
 * @retval 1U Preamble valid and stripped.
 * @retval 0U Preamble invalid.
 */
static uint8_t fsk_decoder_strip_mpp_preamble(void)
{
    uint16_t start_index = 0U;
    uint16_t new_count;

    if (fsk_decoder_find_mpp_start_index(&start_index) == 0U)
    {
        fsk_decoder_report_mpp_preamble_error();
        return 0U;
    }

    /**
     * Record successful preamble detection.
     *
     * start_index is equal to the number of preamble ONE bits.
     */
    g_fsk_last_preamble_bits  = (uint8_t)start_index;
    g_fsk_last_preamble_error = FSK_PREAMBLE_ERROR_NONE;

    /**
     * Keep the ZERO start bit and all following bits.
     */
    new_count = decoder.bit_count - start_index;

    for (uint16_t i = 0U; i < new_count; i++)
    {
        decoder.bit_stream[i] = decoder.bit_stream[start_index + i];
    }

    /**
     * Clear unused tail for debugger readability.
     */
    for (uint16_t i = new_count; i < decoder.bit_count; i++)
    {
        decoder.bit_stream[i] = 0U;
    }

    decoder.bit_count = new_count;

    return 1U;
}

/**
 * @brief Decode prepared 11-bit asynchronous FSK data frames.
 *
 * This function expects decoder.bit_stream[0] to already be the start bit
 * of the first data byte.
 *
 * Data byte format:
 *   bit 0     = start bit, ZERO
 *   bits 1..8 = data byte, LSB-first
 *   bit 9     = even parity
 *   bit 10    = stop bit, ONE
 *
 * @retval 1U All available frames decoded successfully.
 * @retval 0U Framing/parity error detected.
 */
static uint8_t fsk_decoder_validate_and_dispatch_data_frames(void)
{
    if ((decoder.bit_count == 0U) ||
        ((decoder.bit_count % FSK_DATA_FRAME_BITS) != 0U))
    {
        g_fsk_data_frame_error_count++;
        return 0U;
    }

    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_10);

    for (uint16_t i = 0U; i < decoder.bit_count; i += FSK_DATA_FRAME_BITS)
    {
        uint8_t start = decoder.bit_stream[i];
        uint8_t data = 0U;
        uint8_t parity;
        uint8_t stop;

        for (uint8_t b = 0U; b < 8U; b++)
        {
            data |= (uint8_t)((decoder.bit_stream[i + 1U + b] & 0x01U) << b);
        }

        parity = decoder.bit_stream[i + 9U];
        stop   = decoder.bit_stream[i + 10U];

        if ((start == 0U) &&
            (stop == 1U) &&
            (parity == calculate_parity(data)))
        {
            /**
             * TODO:
             * Dispatch/store valid decoded data byte here.
             *
             * Example:
             * fsk_decoded_byte_push(data);
             */
        }
        else
        {
            g_fsk_data_frame_error_count++;
            return 0U;
        }
    }

    return 1U;
}

/**
 * @brief Find the MPP data start-bit index without reporting errors.
 *
 * This helper is used only by tail recovery.
 *
 * MPP data format:
 *     1110...
 *     11110...
 *     111110...
 *
 * The leading ONE bits are preamble bits. The first ZERO after the valid
 * preamble is the actual start bit of the first 11-bit data frame.
 *
 * This function does not increment error counters. Tail recovery should not
 * report preamble errors because process_frame() is responsible for final
 * packet validation.
 *
 * @param[out] start_index Index of the ZERO start bit.
 *
 * @retval 1U Valid MPP preamble and start bit found.
 * @retval 0U Not enough/invalid information for tail recovery.
 */
static uint8_t fsk_decoder_find_mpp_start_index(uint16_t *start_index)
{
    uint16_t i = 0U;

    if (start_index == NULL)
    {
        return 0U;
    }

    *start_index = 0U;

    while ((i < decoder.bit_count) && (decoder.bit_stream[i] == 1U))
    {
        i++;

        if (i > FSK_MPP_PREAMBLE_MAX_BITS)
        {
            return 0U;
        }
    }

    if (i < FSK_MPP_PREAMBLE_MIN_BITS)
    {
        return 0U;
    }

    if (i >= decoder.bit_count)
    {
        return 0U;
    }

    if (decoder.bit_stream[i] != 0U)
    {
        return 0U;
    }

    *start_index = i;
    return 1U;
}

static uint8_t calculate_parity(uint8_t byte)
{
    uint8_t count = 0;

    for (uint8_t i = 0; i < 8; i++)
        if (byte & (1 << i))
            count++;

    return (count & 1);   // even parity
}

void fsk_decoder_reset_state(void)
{
    uint32_t i;

    for (i = 0U; i < MA_WINDOW; i++)
    {
        ma_buffer[i] = 0U;
    }

    for (i = 0U; i < DELAY; i++)
    {
        ma_delay_buffer[i] = 0U;
    }

    ma_sum = 0U;
    ma_index = 0U;
    ma_filled = 0U;

    delay_index = 0U;
    delay_filled = 0U;

    active = 0;
    last_sign = 0;
    period_counter = 0U;

    decoder.state = RX_IDLE;
    decoder.half_pending = 0U;
    decoder.bit_count = 0U;

    for (i = 0U; i < sizeof(decoder.bit_stream); i++)
    {
        decoder.bit_stream[i] = 0U;
    }

    prev_timestamp = 0U;
    debugFlag = 0U;

    fsk_limit_mode = FSK_LIMIT_512;
    g_fsk_preamble_error_count = 0U;
    g_fsk_last_preamble_bits = 0U;
    g_fsk_last_preamble_error = FSK_PREAMBLE_ERROR_NONE;
    g_fsk_data_frame_error_count = 0U;
}

