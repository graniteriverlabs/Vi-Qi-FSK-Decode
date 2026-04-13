/*
 * fsk_decoder.c
 *
 *  Created on: Feb 24, 2026
 *      Author: kanna
 */

#include "main.h"
#include <fsk_decoder.h>
#include <fsk_config.h>
#include <fsk_online_stream.h>

#if OFFLINE_CAPTURE_MODE
#define PERIOD_BUF_SIZE		16384
uint16_t period_buf[PERIOD_BUF_SIZE];
volatile uint32_t period_wr_idx = 0;


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
__attribute__((aligned(32)))
ipc_stream_record_t  debug_buf[CAPTURE_SAMPLES];
volatile uint32_t debug_index =0;
volatile uint8_t toggle =0;

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
static inline int32_t delayed_abs_diff(uint32_t new_ma);
static inline symbol_t classify_symbol(uint32_t count);
static inline void push_bit(uint8_t bit);
static uint8_t calculate_parity(uint8_t byte);
static void process_frame(void);
/* Function Declarations END*/


static uint32_t prev_timestamp = 0;

#if !OFFLINE_CAPTURE_MODE



void process_block(uint32_t start, uint32_t length)
{
    for (uint32_t i = 0; i < length; i++)
    {
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
            period = (HRTIM_TIMER_MAX - prev_timestamp) + curr + 1;

//        period = (period) / 32;	//(period * RES) / 32, TO avoid floating point multiplication, removed *RES. So 1unit = 5.8823ns ,
        						// All calculations are done based on SYSCLK cycles, instead of time or HRTIM cycles
//        period = (period >> 5);		// Changed to >> from / as part of optimisation
        prev_timestamp = curr;

        uint32_t ma = moving_average_16(period);
        int32_t diff = delayed_abs_diff(ma);
        int32_t abs_diff = (diff >=0) ? diff : -diff;



        int8_t sign = 0;

        if (diff > 0)	sign = 1;
        else if (diff < 0)	sign = -1;

        /* Valid transition condition */
        if (abs_diff > DELTA_MIN && abs_diff < DELTA_MAX)
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

                HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_14);
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

					switch (decoder.state)
					{
					    case RX_IDLE:

					        if (symbol == SYMBOL_HALF || symbol == SYMBOL_FULL)
					        {
					            decoder.state = RX_COLLECT_BITS;
					            decoder.bit_count = 0;
					            decoder.half_pending = 1;
					        }

					        break;

					    case RX_COLLECT_BITS:

							switch (symbol)
							{
								case SYMBOL_HALF:
									if (!decoder.half_pending)
									{
										decoder.half_pending = 1;
									}
									else
									{
										// Two halves → BIT 1
										push_bit(1);
										decoder.half_pending = 0;
									}
									break;

								case SYMBOL_FULL:
									push_bit(0);
									decoder.half_pending = 0;
									break;

								case SYMBOL_IDLE:
									// End of frame
									if (decoder.bit_count > 0)
									{
										process_frame();   // interpret response or data
										debugFlag = 1;
									}

									decoder.state = RX_IDLE;
									decoder.bit_count = 0;
									decoder.half_pending = 0;
									break;

								case SYMBOL_INVALID:
								default:
									// Reset on invalid timing
									decoder.state = RX_IDLE;
									decoder.bit_count = 0;
									decoder.half_pending = 0;
									break;
							}
						break;
						}
					HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_14);
                }
            }
        }

        /* If active, keep counting */
        if (active)
        {
        	if (period_counter < IDLE_TARGET)
        	{
        		period_counter++;
        	}
        	else
        	{
//        		HAL_GPIO_WritePin(DebugIO0_GPIO_Port, DebugIO0_Pin, GPIO_PIN_SET);
//        		HAL_GPIO_WritePin(DebugIO0_GPIO_Port, DebugIO0_Pin, GPIO_PIN_RESET);
        	}
        }

        /**
         * @brief Export the current observation to the online stream layer.
         *
         * The stream layer is responsible for:
         * - deciding whether this sample should be emitted
         * - applying event-only or downsampled policies
         * - packing the record into the shared CM4->CM7 ring buffer
         *
         * This keeps process_block() focused on decode logic only.
         */
#if FSK_STREAM_TEST_MODE
    fsk_stream_test_emit();
#else
        fsk_online_stream_maybe_log(period,
                                    ma,
                                    diff,
                                    toggled,
                                    valid_transition,
                                    sign,
                                    (uint8_t)decoder.state);
#endif
        // Example classification
//        if (filtered < 20000)
//        {
//            // bit = 1
//        }
//        else
//        {
//            // bit = 0
//        }
    }
}
#else

void process_block(uint32_t start, uint32_t length)
{
    for (uint32_t i = 0; i < length; i++)
    {
        if (debug_index >= length)
            return;

        uint32_t curr = hrtim_capture_buf[start + i];
        uint32_t period;

        if (curr >= prev_timestamp)
            period = curr - prev_timestamp;
        else
            period = (HRTIM_TIMER_MAX - prev_timestamp) + curr + 1;

        period = period / 32;
        prev_timestamp = curr;

        uint32_t ma = moving_average_16(period);
        int32_t diff = delayed_abs_diff(ma);
        int32_t abs_diff = (diff >=0) ? diff : -diff;



        int8_t sign = 0;

        if (diff > 0)	sign = 1;
        else if (diff < 0)	sign = -1;

        /* Valid transition condition */
        if (abs_diff > DELTA_MIN && abs_diff < DELTA_MAX)
        {
            if (!active)
            {
                // First transition detected
                active = 1;
                last_sign = sign;
                period_counter = 0;
                toggle = !toggle;
            }
            else
            {
                // Already counting — check if opposite sign
                if (sign == -last_sign)
                {
                    // Opposite edge detected

//                    record_period(period_counter);

                    // Restart counting for next segment
                    period_counter = 0;
                    last_sign = sign;
                    toggle = !toggle;
                }
            }
        }

        /* If active, keep counting */
        if (active)
        {
            period_counter++;
        }

//        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_9);

        debug_buf[debug_index].period = (period > 0xFFFFU) ? 0xFFFFU : (uint16_t)period;
        debug_buf[debug_index].ma     = (ma > 0xFFFFU) ? 0xFFFFU : (uint16_t)ma;

        debug_buf[debug_index].delta = (int16_t)abs_diff;

        debug_buf[debug_index].flags = 0U;
        if (toggle)
        {
            debug_buf[debug_index].flags |= IPC_STREAM_FLAG_TOGGLED;
        }

        if (abs_diff > DELTA_MIN && abs_diff < DELTA_MAX)
        {
            debug_buf[debug_index].flags |= IPC_STREAM_FLAG_VALID_TRANSITION;
        }

        if (sign > 0)
        {
            debug_buf[debug_index].flags |= IPC_STREAM_FLAG_SIGN_POSITIVE;
        }
        debug_buf[debug_index].state = toggle;

        debug_index++;
    }
}

void ethernet_dump_capture(void)
{
    uint32_t notify_accum = 0U;

    for (uint32_t i = 0; i < debug_index; i++)
    {
        while (ipc_stream_write((const uint8_t *)&debug_buf[i], sizeof(debug_buf[i])) != 0)
        {
            HAL_Delay(1);
        }

        notify_accum += sizeof(debug_buf[i]);

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
void process_fsk_capture(void)
{
    static uint32_t prev = 0;
    static uint8_t  first = 1;

    static uint32_t read_idx = 0;

    uint32_t curr, period;

	curr = hrtim_capture_buf[read_idx];

	read_idx++;
	if (read_idx >= CAPTURE_SAMPLES)
		read_idx = 0;

	if (first)
	{
		prev = curr;
		first = 0;
		return;
	}

	if (curr >= prev)
		period = curr - prev;
	else
		period = (HRTIM_TIMER_MAX - prev) + curr + 1;

	prev = curr;

	/* ---- SAVE PERIOD INTO BUFFER ---- */
	period_buf[period_wr_idx++] = period;
	if (period_wr_idx >= PERIOD_BUF_SIZE)
		period_wr_idx = 0;

	/* Example: classify frequency */
	if (period < 750)        // ~210 kHz
	{
		// bit = 1
	}
	else if (period > 850)   // ~105 kHz
	{
		// bit = 0
	}
}

void post_processing(void)
{
    uint32_t prev = hrtim_capture_buf[0];

    for (uint32_t i = 1; i < CAPTURE_SAMPLES; i++)
    {
        uint32_t curr = hrtim_capture_buf[i];
        uint32_t period;

        if (curr >= prev)
            period = curr - prev;
        else
            period = (HRTIM_TIMER_MAX - prev) + curr + 1;

        prev = curr;

        /* Save period */
        period_buf[i - 1] = period;

        /* Example classification */
        if (period < 750)
        {
            // bit = 1
        }
        else if (period > 850)
        {
            // bit = 0
        }
    }
}
#endif

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

static inline int32_t delayed_abs_diff(uint32_t new_ma)
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

static void process_frame(void)
{
    if (decoder.bit_count == 8)
    {
        // RESPONSE (8-bit)
        uint8_t response = 0;

        for (uint8_t i = 0; i < 8; i++)
            response |= (decoder.bit_stream[i] << i);


        // Handle response here
//        if(response == 0xFF)
//        	HAL_GPIO_TogglePin(DebugIO0_GPIO_Port, DebugIO0_Pin);
//        else if(response == 0x00)
//        {
//        	HAL_GPIO_TogglePin(DebugIO0_GPIO_Port, DebugIO0_Pin);
//        	HAL_GPIO_TogglePin(DebugIO0_GPIO_Port, DebugIO0_Pin);
//        }
    }
    else if (decoder.bit_count % 11 == 0 && decoder.bit_count > 0)
    {
        // DATA FRAME
        for (uint16_t i = 0; i < decoder.bit_count; i += 11)
        {
            uint8_t start  = decoder.bit_stream[i];
            uint8_t data   = 0;

            for (uint8_t b = 0; b < 8; b++)
                data |= (decoder.bit_stream[i + 1 + b] << b);

            uint8_t parity = decoder.bit_stream[i + 9];
            uint8_t stop   = decoder.bit_stream[i + 10];

            if (start == 0 &&
                stop == 1 &&
                parity == calculate_parity(data))
            {
                // Store valid byte
            }
            else
            {
                // framing error
                break;
            }
        }
    }

    decoder.bit_count = 0;
}

static inline void push_bit(uint8_t bit)
{
    if (decoder.bit_count < sizeof(decoder.bit_stream))
        decoder.bit_stream[decoder.bit_count++] = bit;
}

static inline symbol_t classify_symbol(uint32_t count)
{
    if (WITHIN(count, BIT1_TARGET))
    {
//		HAL_GPIO_WritePin(DebugIO0_GPIO_Port, DebugIO0_Pin, GPIO_PIN_SET);
//		HAL_GPIO_WritePin(DebugIO0_GPIO_Port, DebugIO0_Pin, GPIO_PIN_RESET);
        return SYMBOL_HALF;
    }

    if (WITHIN(count, BIT0_TARGET))
    {
    	return SYMBOL_FULL;
    }

    if (WITHIN(count, IDLE_TARGET))
    {
    	return SYMBOL_IDLE;
    }

    return SYMBOL_INVALID;
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
}

