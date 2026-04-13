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
