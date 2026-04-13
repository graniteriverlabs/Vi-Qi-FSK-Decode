/*
 * cm4_grl_app.c
 *
 *  Created on: Jan 21, 2026
 *      Author: GRL
 */
#include "cm4_grl_app.h"
#include "ipc_shared.h"



/**
 * @brief Decodes a 16-bit length value from a receive buffer.
 * 
 * Extracts the length from bytes 1 and 2 of the receive buffer,
 * treating byte 1 as the low byte and byte 2 as the high byte.
 * 
 * @param rxBuf Pointer to the receive buffer containing the length data
 * @return uint16_t The decoded 16-bit length value
 */
#define API_LENGTH_DECODE(rxBuf)    ((uint16_t)(rxBuf[1]) | ((uint16_t)(rxBuf[2] << 8)))

/**
 * @brief Encodes a 16-bit length value into a receive buffer.
 * 
 * Stores a 16-bit length value into bytes 1 and 2 of the receive buffer,
 * with byte 1 containing the low byte and byte 2 containing the high byte
 * (little-endian format).
 * 
 * @param rxBuf Pointer to the receive buffer where length data will be stored
 * @param len The 16-bit length value to encode
 */
#define API_LENGTH_ENCODE(rxBuf, len) do { \
    rxBuf[1] = (uint8_t)(len & 0xFF);       \
    rxBuf[2] = (uint8_t)((len >> 8) & 0xFF);\
} while(0)

void fw_version_fetch(uint8_t *aRxBuffer){

	uint16_t l = API_LENGTH_DECODE(aRxBuffer);

	switch(aRxBuffer[8]){

		case 0x02://CM4 FW version Fetch
		    uint8_t resp[API_MAX_LEN];
		    uint16_t resp_len = API_LENGTH_DECODE(aRxBuffer);

		    memcpy(resp, aRxBuffer, resp_len);
		    resp[resp_len++] = FW_MAJOR_VERSION;
		    resp[resp_len++] = FW_MINOR_VERSION;
		    resp[resp_len++] = FW_SUB_MINOR_VERSION;

		    API_LENGTH_ENCODE(resp, resp_len);

		    cm4_send_to_cm7(resp, resp_len);

		break;

	}

//	tcp_send_response(ltx_buffer, Data_length);
}
void get_system_details_handler(uint8_t *aRxBuffer){
	uint8_t lParType = aRxBuffer[7];
//	uint8_t lsubParType = aRxBuffer[8];
	switch(lParType){
		case 0x01:
			fw_version_fetch(aRxBuffer);
		break;
	}
}

void get_cmd_handler(uint8_t *aRxBuffer){

	uint8_t lFuncType = aRxBuffer[5];
	switch(lFuncType){

		case 0x08:
			get_system_details_handler(aRxBuffer);
		break;
	}
}

void process_app_cmd(uint8_t *aRxBuffer){

	uint8_t lCmdType = aRxBuffer[0];
	switch(lCmdType ){
		case 0x1:
//			set_cmd_handler(aRxBuffer);
			break;
		case 0x07:
			get_cmd_handler(aRxBuffer);
			break;
		case 0x02:
			break;
	}
}
