/*
 * grl_peripheral.h
 *
 *  Created on: Jan 23, 2026
 *      Author: GRL
 */

#ifndef INC_QI_PERIPHERAL_H_
#define INC_QI_PERIPHERAL_H_
//#include "grl_enum.h"
#include <qi_defs.h>
#include <qi_enum.h>

/* -------- Types -------- */
/**
 * @struct uart4_pkt_t
 * @brief Structure for UART4 packet data with timestamps and payload.
 *
 * @var uart4_pkt_t::rv_ri_ts_ms
 *      Timestamp in milliseconds for RV/RI measurement.
 *
 * @var uart4_pkt_t::rv
 *      RV (Reverse Voltage) value.
 *
 * @var uart4_pkt_t::ri
 *      RI (Insulation Resistance) value.
 *
 * @var uart4_pkt_t::uart_ts_ms
 *      Timestamp in milliseconds for UART packet reception.
 *
 * @var uart4_pkt_t::len
 *      Length of the payload data in bytes.
 *
 * @var uart4_pkt_t::data
 *      Payload buffer containing packet data up to PKT_MAX_PAYLOAD bytes.
 */
typedef struct {

	uint32_t rv_ri_ts_ms;
	uint16_t rv;
	uint16_t ri;

	uint32_t uart_ts_ms;
	uint16_t len;
	uint8_t  data[PKT_MAX_PAYLOAD];
} uart4_pkt_t;

/**
 * @brief Structure for storing rectified voltage and current samples with timestamp
 * 
 * This structure holds a single sample of voltage and current measurements
 * captured at a specific point in time, used for Qi wireless power transfer
 * monitoring and control.
 * 
 * @struct qi_vi_sample_t
 * 
 * @var qi_vi_sample_t::ts_ms
 *      Timestamp in milliseconds when the VI sample was completed,
 *      obtained from HAL_GetTick()
 * 
 * @var qi_vi_sample_t::r_v
 *      Rectified voltage value in raw ADC units or mV
 * 
 * @var qi_vi_sample_t::r_i
 *      Rectified current value in raw ADC units or mA
 */
typedef struct {
  uint32_t ts_ms;   // when VI sample completed (HAL_GetTick)
  uint16_t r_v;       // rectified voltage
  uint16_t r_i;       // rectified current
//  uint32_t seq;     // increments per successful sample
//  uint32_t errs;    // cumulative I2C errors
} qi_vi_sample_t;

/**
 * @struct qi_i2c_rw_t
 * @brief Structure for I2C read/write operations with DMA support
 * 
 * @var qi_i2c_rw_t::operation
 *      Type of I2C operation to perform (read/write)
 * 
 * @var qi_i2c_rw_t::producer
 *      Source/producer identifier for the I2C operation
 * 
 * @var qi_i2c_rw_t::r_buf
 *      Pointer to read buffer for storing received data
 * 
 * @var qi_i2c_rw_t::slave_addr
 *      I2C slave device address
 * 
 * @var qi_i2c_rw_t::rw_byte_cnt
 *      Number of bytes to read or write
 * 
 * @var qi_i2c_rw_t::slave_reg_addr
 *      Register address on the slave device
 * 
 * @var qi_i2c_rw_t::i2c_intf
 *      Pointer to I2C HAL handle for interface control
 * 
 * @var qi_i2c_rw_t::w_data
 *      Write data buffer (up to 16 bytes)
 * 
 * @var qi_i2c_rw_t::eth_hdr
 *      Ethernet header buffer (64 bytes), used only for Ethernet read commands
 */
typedef struct {
	i2c_oper_type_t 	operation;
	i2c_producer_t 	producer;
	uint8_t		*r_buf;
	uint8_t 	slave_addr;
	uint16_t	rw_byte_cnt;
	uint16_t 	slave_reg_addr;
	I2C_HandleTypeDef *i2c_intf;
	uint8_t     w_data[16];
	/*use only for eth read commands*/
	uint8_t  eth_hdr[64];
}qi_i2c_rw_t;

/**
 * @struct i2c_ctx_t
 * @brief I2C transaction context structure for managing I2C communication state.
 * 
 * This structure maintains the state and data buffers for an I2C transaction,
 * including ownership, busy status, and read/write operations.
 * 
 * @member busy
 *         Flag indicating whether the I2C transaction is currently in progress.
 *         Non-zero value means the I2C controller is busy, zero means idle.
 * 
 * @member owner
 *         The producer entity that initiated this I2C transaction.
 *         Used to identify which client owns the current transaction.
 * 
 * @member byte_cnt
 *         Counter tracking the number of bytes processed in the current transaction.
 *         Used for monitoring progress of multi-byte read/write operations.
 * 
 * @member rw_buf
 *         Pointer to the read/write buffer containing data to be transmitted
 *         or storage for received data during I2C transactions.
 */
typedef struct {
  volatile uint8_t busy;
  i2c_producer_t owner;
  uint8_t byte_cnt;
  uint8_t	*rw_buf;
} i2c_ctx_t;

int i2c_q_enqueue(const qi_i2c_rw_t *in);
VOID i2c_manager_thread_entry(ULONG thread_input);
HAL_StatusTypeDef qi_i2c_rw_dma(qi_i2c_rw_t *aI2c_var);
int UART4_PktRing_Pop(uart4_pkt_t *out,uint8_t *read_idx, uint8_t *wr_idx);
void UART4_StartRxToIdle(void);
//void UART4_PoC_RxTxBridge_Poll(void);
#endif /* INC_QI_PERIPHERAL_H_ */
