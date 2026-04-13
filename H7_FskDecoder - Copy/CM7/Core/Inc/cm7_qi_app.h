/*
 * grl_app.h
 *
 *  Created on: Jan 21, 2026
 *      Author: GRL
 */

#ifndef INC_CM7_QI_APP_H_
#define INC_CM7_QI_APP_H_
#include <qi_defs.h>
#include <qi_peripheral.h>
#include "main.h"
//void i2c_data_eth_transmit(uint8_t *i2cRxBuf, uint8_t read_bc);
void i2c_data_eth_transmit(qi_i2c_rw_t *g_cur_req);
int api_enqueue(const uint8_t *buf, uint16_t len);
int api_dequeue(uint8_t *out_buf, uint16_t *out_len);
void set_cm7_fw_version(void);
void get_cm4_fw_version(uint8_t *aRxBuffer);
void process_app_cmd(uint8_t *aRxBuffer);
VOID ApiProcess_Thread_Entry(ULONG thread_input);

extern volatile uint8_t cm4_reply_ready;
extern volatile uint8_t cm7_reply_ready;

extern volatile uint8_t api_len[API_QUEUE_SIZE];
extern volatile uint8_t api_wr_idx;
extern volatile uint8_t api_rd_idx;

#endif /* INC_CM7_QI_APP_H_ */
