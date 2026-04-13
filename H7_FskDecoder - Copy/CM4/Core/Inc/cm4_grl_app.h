/*
 * cm4_grl_app.h
 *
 *  Created on: Jan 21, 2026
 *      Author: GRL
 */

#ifndef INC_CM4_GRL_APP_H_
#define INC_CM4_GRL_APP_H_
#include "main.h"

#define FW_MAJOR_VERSION		3
#define FW_MINOR_VERSION		6
#define FW_SUB_MINOR_VERSION 	9

#define HSEM_CM7_TO_CM4   1
#define HSEM_CM4_TO_CM7   2

#define CM4_API_Q_SIZE   16
#define CM4_API_MAX_LEN  256

void process_app_cmd(uint8_t *aRxBuffer);
void IPC_HandlePacket(uint8_t *buf, uint16_t len);
void set_cm4_fw_version(void);
int cm4_api_enqueue_from_shm(void);
int cm4_api_dequeue(uint8_t *buf, uint16_t *len);

#endif /* INC_CM4_GRL_APP_H_ */
