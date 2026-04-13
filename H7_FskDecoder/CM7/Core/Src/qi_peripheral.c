#include <cm7_qi_app.h>
#include <qi_enum.h>
#include <qi_peripheral.h>
#include "stm32h7xx_hal.h"
#include <string.h>
#include <stdint.h>

#define DISCARD_OLD_STORE_NEW	1

qi_vi_sample_t g_qi_i2c_rx_power_data;

extern UART_HandleTypeDef huart4;
extern I2C_HandleTypeDef hi2c1;
extern TX_EVENT_FLAGS_GROUP i2cEventFlags;
extern TX_EVENT_FLAGS_GROUP i2c_q_Event_Flags;

/* -------- Packet ring buffer -------- */
static volatile uint8_t g_pkt_w = 0;
static volatile uint8_t g_pkt_r = 0;
uart4_pkt_t g_pkt_ring[PKT_RING_CAP];


/* DMA RX scratch buffer in D2 SRAM (NOLOAD section), 32-byte aligned */
__attribute__((section(".Uart4DmaSection"), aligned(32)))
static uint8_t g_uart4_dma_rx[UART4_DMA_RX_BUF_SZ];

__attribute__((section(".i2c1DmaSection"), aligned(32)))
/* ---- DMA RX buffer (consider placing in non-cacheable memory) ---- */
static uint8_t g_vi_raw[QI_READ_LEN];

__attribute__((section(".i2c1DmaSection"), aligned(32)))
uint8_t i2c_read_buf[QI_READ_LEN];

/**
 * @author: Pranay, @date 11Feb'26
 * @brief Captures a snapshot of the current VI (Voltage-Current) sample data.
 * 
 * This function safely retrieves the latest received power data by temporarily
 * disabling interrupts to ensure atomic access to the global VI sample buffer.
 * The interrupt state is restored to its previous state after the data is copied.
 * 
 * @return qi_vi_sample_t A copy of the current VI sample data containing voltage
 *                        and current measurements received via I2C DMA.
 * 
 * @note This function uses interrupt masking to provide thread-safe access to
 *       the shared global variable without requiring locks or semaphores.
 */
static inline qi_vi_sample_t vi_get_snapshot(void)
{
  qi_vi_sample_t samp;

  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  samp = g_qi_i2c_rx_power_data;
  if (!primask) __enable_irq();

  return samp;
}

#if DISCARD_OLD_STORE_NEW
/* Drop policy: drop OLD packet if ring full */
static int pkt_ring_push(const uint8_t *data, uint16_t len, uint32_t ts_ms)
{
  if (len > PKT_MAX_PAYLOAD) {
    len = PKT_MAX_PAYLOAD;
  }
  qi_vi_sample_t vi = vi_get_snapshot(); // snapshot latest V/I

  uint16_t next = (uint16_t)((g_pkt_w + 1u) % PKT_RING_CAP);

  if (next == g_pkt_r) {
    /* Ring full → drop OLDEST packet */
    g_pkt_r = (uint16_t)((g_pkt_r + 1u) % PKT_RING_CAP);
  }


  g_pkt_ring[g_pkt_w].uart_ts_ms = ts_ms;
  g_pkt_ring[g_pkt_w].len = len;
  memcpy(g_pkt_ring[g_pkt_w].data, data, len);

  g_pkt_ring[g_pkt_w].rv_ri_ts_ms = vi.ts_ms;
  g_pkt_ring[g_pkt_w].rv = vi.r_v;
  g_pkt_ring[g_pkt_w].ri = vi.r_i;
  g_pkt_w = next;

  return 1;
}
#else
/* Drop policy: drop NEW packet if ring full */
static int pkt_ring_push(const uint8_t *data, uint16_t len, uint32_t ts_ms)
{
  if (len > PKT_MAX_PAYLOAD) len = PKT_MAX_PAYLOAD;

  uint16_t next = (uint16_t)((g_pkt_w + 1u) % PKT_RING_CAP);
  if (next == g_pkt_r) {
    return 0; // full -> drop newest
  }

  grl_vi_sample_t vi = grl_vi_get_snapshot(); // snapshot latest V/I

  g_pkt_ring[g_pkt_w].ts_ms = ts_ms;
  g_pkt_ring[g_pkt_w].len = len;
  memcpy(g_pkt_ring[g_pkt_w].data, data, len);
  g_pkt_w = next;
  return 1;
}
#endif

int UART4_PktRing_Pop(uart4_pkt_t *out, uint8_t *read_idx, uint8_t *wr_idx)
{
	int ok = 0;

	uint32_t primask = __get_PRIMASK();
	__disable_irq();

	*read_idx = (uint8_t)g_pkt_r;
	*wr_idx = (uint8_t)g_pkt_w;

	if (g_pkt_r != g_pkt_w)
	{
		*out = g_pkt_ring[g_pkt_r];
		g_pkt_r = (uint16_t)((g_pkt_r + 1u) % PKT_RING_CAP);
		ok = 1;
	}
	if (!primask) __enable_irq();

	return ok;
}

/**
 * @author: Pranay, @date 11Feb'26
 * @brief Initialize UART4 reception using DMA with idle line detection.
 *
 * This function configures UART4 to receive data via DMA with automatic termination
 * when the RX line remains idle. It handles the following operations:
 * - Validates that DMA is properly linked to the UART instance
 * - Aborts any ongoing reception to ensure a clean state
 * - Clears sticky UART error flags (overrun, noise, framing, parity) that could block reception
 * - Initiates DMA reception with idle line detection for variable-length packet handling
 * - Disables the half-transfer interrupt to reduce CPU interrupt load
 *
 * @note Requires that UART4 and its associated DMA channel are properly initialized
 *       via HAL initialization functions before calling this function.
 *
 * @note Upon reception, the HAL_UARTEx_RxEventCallback() callback will be invoked
 *       with appropriate event codes when data is received or idle timeout occurs.
 *
 * @return void
 *
 * @see HAL_UARTEx_ReceiveToIdle_DMA()
 * @see HAL_UART_AbortReceive()
 */
void UART4_StartRxToIdle(void)
{
	if (huart4.hdmarx == NULL) {
		Error_Handler(); // DMA not linked in MSP init
	}
	// Force stop previous RX (IT/DMA/IDLE) so HAL won't return BUSY
	(void)HAL_UART_AbortReceive(&huart4);

	// Clear sticky UART error flags (ORE can block reception)
	__HAL_UART_CLEAR_OREFLAG(&huart4);
	__HAL_UART_CLEAR_NEFLAG(&huart4);
	__HAL_UART_CLEAR_FEFLAG(&huart4);
	__HAL_UART_CLEAR_PEFLAG(&huart4);

	/* Start RX to IDLE DMA (variable-length packets) */
	if (HAL_UARTEx_ReceiveToIdle_DMA(&huart4, g_uart4_dma_rx, UART4_DMA_RX_BUF_SZ) != HAL_OK) {
		Error_Handler();
	}

  /* Reduce interrupt load: disable Half Transfer interrupt */
  if (huart4.hdmarx != NULL) {
    __HAL_DMA_DISABLE_IT(huart4.hdmarx, DMA_IT_HT);
  }
}

/**
 * @author: Pranay, @date 11Feb'26
 * @brief UART receive event callback for DMA-based reception with idle line detection.
 * 
 * This callback is invoked when a UART receive event occurs, specifically when:
 * - A burst of data is received and the line goes idle, OR
 * - The DMA buffer is full
 * 
 * @param[in] huart Pointer to the UART handle structure containing instance information
 * @param[in] Size Number of bytes received in the current burst
 * 
 * @details
 * - Checks if the callback is for UART4 instance
 * - If data was received (Size > 0), captures the current tick timestamp and pushes 
 *   the received packet to the ring buffer (truncated to 64 bytes if necessary)
 * - Restarts the UART in idle reception mode to prepare for the next data burst
 * - Uses DMA for efficient background reception without blocking operations
 * 
 * @note This callback must be registered with HAL_UARTEx_RegisterRxEventCallback()
 *       for proper operation of DMA-based idle line detection.
 * 
 * @return void
 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
  if (huart->Instance == UART4)
  {
    if (Size > 0u)
    {
        uint32_t ts_ms = HAL_GetTick();

      /* Push packet (truncate to 64 if needed) */
      (void)pkt_ring_push(g_uart4_dma_rx, Size,ts_ms);

    }

    /* Restart for next burst */
    UART4_StartRxToIdle();
  }
}
/**
 * @author: Pranay, @date 11Feb'26
 * @brief UART error callback function.
 * 
 * This callback is invoked when a UART error occurs. It handles errors
 * specifically for UART4 by clearing all error flags (overrun, noise,
 * frame, and parity errors) and restarting the receive operation in
 * idle mode.
 * 
 * @param[in] huart Pointer to the UART handle structure containing the
 *                   instance information and configuration.
 * 
 * @note This function is called by the HAL layer when UART_IT_ERR
 *       interrupt is triggered. It should be registered via
 *       HAL_UART_RegisterCallback() or HAL_UART_UnRegisterCallback().
 * 
 * @retval None
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == UART4)
  {
    __HAL_UART_CLEAR_OREFLAG(huart);
    __HAL_UART_CLEAR_NEFLAG(huart);
    __HAL_UART_CLEAR_FEFLAG(huart);
    __HAL_UART_CLEAR_PEFLAG(huart);

    HAL_UART_AbortReceive(huart);
    UART4_StartRxToIdle();
  }
}

#ifdef UART_WRITE_EN
static volatile uint8_t g_uart4_tx_busy = 0;
static uint8_t counter;

void UART4_PoC_RxTxBridge_Poll(void)
{
  static uart4_pkt_t p;

  if (g_uart4_tx_busy) return;

  // Build a known pattern
  p.data[0] = 0xAA;
  p.data[1] = 0x55;

  // Fill next 58 bytes with counter (total so far 60 bytes: 0..59)
  memset(&p.data[2], counter, 58);

  // Add CRLF at the end
  p.data[60] = '\r';
  p.data[61] = '\n';

  p.len = 62;               // bytes 0..61

  counter++;

  g_uart4_tx_busy = 1;
  if (HAL_UART_Transmit_DMA(&huart4, p.data, p.len) != HAL_OK)
  {
    g_uart4_tx_busy = 0;
  }
}
#endif
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == UART4)
  {
  }
}




/**I2C*/
#define I2C_TIMEOUT_TICKS	20
#define I2C_WAIT_STEP_TICKS   1      // 1 tick sleep (with 1ms tick -> 1ms)
#define I2C_Q_SIZE        16
#define I2C_MAX_DATA      256   // max bytes per request (read/write payload)

static volatile uint8_t i2c_wr = 0, i2c_rd = 0;
static qi_i2c_rw_t i2c_q[I2C_Q_SIZE];
static qi_i2c_rw_t g_cur_req;


/**
 * @brief Converts two bytes from a little-endian byte array to a 16-bit unsigned integer.
 *
 * @param p Pointer to a byte array containing at least 2 bytes in little-endian format.
 *
 * @return uint16_t The 16-bit unsigned integer composed of p[0] as the low byte 
 *                   and p[1] as the high byte.
 *
 * @note This function assumes little-endian byte order. Modify the implementation
 *       if big-endian byte order is required.
 */
static inline uint16_t U16_LE(const uint8_t *p)
{
  return (uint16_t)(p[0] | ((uint16_t)p[1] << 8)); // change if BE
}

/**
 * @author: Pranay, @date 11Feb'26
 * @brief I2C memory read complete callback handler
 * 
 * This callback is invoked when an I2C memory read operation completes.
 * Checks if the completed transfer is from I2C1 instance and signals
 * the I2C event completion to waiting threads via ThreadX event flags.
 * 
 * @param hi2c Pointer to I2C_HandleTypeDef structure that contains
 *             the configuration information for the I2C module
 * 
 * @note Weak function that should be overridden by user application.
 *       Called from HAL I2C interrupt handler.
 */
void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
  if (hi2c->Instance == I2C1)
  {
//		s.ts_ms = HAL_GetTick();
//		s.r_v     = U16_LE(&g_vi_raw[0]);   // 0x0104..05
//		s.r_i     = U16_LE(&g_vi_raw[4]);   // 0x0108..09
	tx_event_flags_set(&i2cEventFlags, I2C_EVT_RX_DONE, TX_OR);
  }
}
/**
 * @author: Pranay, @date 11Feb'26
 * @brief I2C memory transmit complete callback function.
 * 
 * This callback is invoked when a DMA-based I2C memory write operation completes successfully.
 * It checks if the completed transmission is from I2C1 peripheral and signals the I2C event flags
 * to notify waiting threads that the transmission is done.
 * 
 * @param[in] hi2c Pointer to I2C_HandleTypeDef structure that contains the configuration
 *                  information for the specified I2C module.
 * 
 * @return void
 * 
 * @note This function is called by the HAL I2C driver when I2C memory transmission via DMA completes.
 *       Ensure that i2cEventFlags is properly initialized before this callback is used.
 * 
 * @warning The identifier "I2C1" must be defined in the HAL configuration headers 
 *          (typically in stm32h7xx_hal_conf.h or generated by STM32CubeMX).
 *          If undefined, enable I2C1 peripheral in your project configuration.
 */
void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
  if (hi2c->Instance == I2C1)
  {
	  tx_event_flags_set(&i2cEventFlags, I2C_EVT_TX_DONE, TX_OR);
  }

}

/**
 * @author: Pranay, @date 11Feb'26
 * @brief I2C error callback function.
 * 
 * This callback is invoked when an I2C communication error occurs. It checks if the error
 * occurred on the I2C1 peripheral and signals the I2C error event to the ThreadX event flags
 * group for error handling by other tasks.
 * 
 * @param[in] hi2c Pointer to the I2C_HandleTypeDef structure that contains the configuration
 *                  information for the I2C module.
 * 
 * @return void
 * 
 * @note This function is registered as a HAL callback and is called automatically by the
 *       I2C interrupt handler when an error is detected.
 * 
 * @see HAL_I2C_RegisterCallback()
 * @see tx_event_flags_set()
 */
void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
  if (hi2c->Instance == I2C1)
  {
    tx_event_flags_set(&i2cEventFlags, I2C_EVT_ERROR, TX_OR);
  }
}

/**
 * @author: Pranay, @date 11Feb'26
 * @brief Fetches rectified voltage data via I2C from the CPS peripheral.
 * 
 * Enqueues an I2C read operation to retrieve rectified voltage (V_RECT) 
 * register values from the CPS slave device. The data is stored in the 
 * global buffer g_vi_raw.
 * 
 * @param[in] aBytecnt Number of bytes to read from the CPS device.
 * 
 * @return void
 * 
 * @note This function uses I2C1 interface with DMA. The operation is 
 *       triggered by CM7_TIMER and added to the I2C command queue 
 *       for asynchronous processing. This function is called periodically 
 *       by a timer interrupt to sample voltage data.
 * 
 * 
 * @see i2c_q_enqueue()
 * @see qi_i2c_rw_t
 */
void fetch_rect_v_i(uint8_t aBytecnt){

	qi_i2c_rw_t i2c_var;
	i2c_var.slave_reg_addr = REG_V_RECT;
	i2c_var.r_buf = g_vi_raw;
	i2c_var.rw_byte_cnt = aBytecnt;
	i2c_var.slave_addr =  CPS_SLAVE_ADDR;
	i2c_var.i2c_intf = &hi2c1;
	i2c_var.producer = CM7_TIMER;
	i2c_var.operation = I2C_READ;

	i2c_q_enqueue(&i2c_var);
}


/**
 * @author: Pranay, @date 11Feb'26
 * @brief Enqueues an I2C read/write request into the circular queue.
 *
 * Adds a new I2C transaction request to the queue. If the queue is full,
 * the oldest request is dropped (FIFO overflow handling). Interrupts are
 * disabled during the critical section to ensure thread-safe queue access.
 *
 * @param[in] in Pointer to qi_i2c_rw_t structure containing the I2C request
 *               details (address, data buffer, byte count, direction, etc.)
 *
 * @return 0 on success, -1 if the request is invalid
 *         (byte count is 0 or exceeds I2C_MAX_DATA)
 *
 * @note This function performs interrupt masking/unmasking to protect the
 *       circular queue from concurrent access. If the queue is full, the
 *       oldest pending request is discarded.
 *
 * @note Sets the I2C_EVT_Q_AVAIL event flag after successful enqueue to
 *       signal waiting tasks via ThreadX event flags.
 *
 * @see qi_i2c_rw_t, I2C_Q_SIZE, I2C_MAX_DATA, I2C_EVT_Q_AVAIL
 */
int i2c_q_enqueue(const qi_i2c_rw_t *in)
{

  if (in->rw_byte_cnt == 0 || in->rw_byte_cnt > I2C_MAX_DATA) return -1;

  uint32_t primask = __get_PRIMASK();
  __disable_irq();

  uint8_t next = (uint8_t)((i2c_wr + 1u) % I2C_Q_SIZE);

  if (next == i2c_rd) {
    /* FULL: drop the OLDEST by advancing read index */
    i2c_rd = (uint8_t)((i2c_rd + 1u) % I2C_Q_SIZE);
    /* recompute next (optional, but clear) */
    next = (uint8_t)((i2c_wr + 1u) % I2C_Q_SIZE);
  }

  i2c_q[i2c_wr] = *in;     // struct copy
  i2c_wr = next;

  if (!primask) __enable_irq();
  tx_event_flags_set(&i2cEventFlags, I2C_EVT_Q_AVAIL, TX_OR);
  return 0;
}

/**
 * @author: Pranay, @date 11Feb'26
 * @brief Dequeues an I2C read/write request from the queue.
 * 
 * Removes and returns the oldest I2C request from the circular queue.
 * Uses interrupt disabling to ensure thread-safe access to shared queue
 * pointers during the dequeue operation.
 * 
 * @param[out] out Pointer to qi_i2c_rw_t structure to store the dequeued request.
 *                 Only valid when function returns 0.
 * 
 * @return 0 if an item was successfully dequeued, -1 if the queue is empty.
 * 
 * @note Preserves the current interrupt state (PRIMASK) and restores it
 *       after the operation to avoid unintended interrupt masking.
 * @note Queue size is defined by I2C_Q_SIZE macro.
 */
int i2c_q_dequeue(qi_i2c_rw_t *out)
{
  if (i2c_rd == i2c_wr) return -1;          // empty

  uint32_t primask = __get_PRIMASK();
  __disable_irq();

  if (i2c_rd == i2c_wr) {                   // re-check
    if (!primask) __enable_irq();
    return -1;
  }

  *out = i2c_q[i2c_rd];
  i2c_rd = (uint8_t)((i2c_rd + 1u) % I2C_Q_SIZE);

  if (!primask) __enable_irq();
  return 0;
}

/**
 * @author: Pranay, @date 11Feb'26
 * @brief Performs I2C read or write operations using DMA.
 * 
 * This function executes either a memory read or memory write operation over I2C
 * using Direct Memory Access (DMA) for efficient data transfer. The operation type
 * is determined by the operation field in the provided I2C configuration structure.
 * 
 * @param[in,out] aI2c_var Pointer to qi_i2c_rw_t structure containing:
 *                - operation: Type of operation (I2C_READ or I2C_WRITE)
 *                - i2c_intf: I2C interface handle
 *                - slave_addr: I2C slave device address
 *                - slave_reg_addr: Register address on slave device
 *                - r_buf: Pointer to read buffer (for read operations)
 *                - w_data: Pointer to write data buffer (for write operations)
 *                - rw_byte_cnt: Number of bytes to read/write (max 24 bytes for write)
 * 
 * @return HAL_StatusTypeDef Status of the operation:
 *         - HAL_OK: Operation successful
 *         - HAL_ERROR: Operation failed
 *         - Other HAL error codes from underlying I2C HAL functions
 * 
 * @note For write operations, the byte count is limited to 24 bytes maximum.
 *       If rw_byte_cnt exceeds 24, it will be truncated to 24.
 * @note Both operations use 16-bit memory address size (I2C_MEMADD_SIZE_16BIT).
 */
HAL_StatusTypeDef qi_i2c_rw_dma(qi_i2c_rw_t *aI2c_var){

	HAL_StatusTypeDef ret = HAL_OK;

	if(aI2c_var->operation == I2C_READ)
	{
		ret =  HAL_I2C_Mem_Read_DMA(aI2c_var->i2c_intf,
							aI2c_var->slave_addr,
							aI2c_var->slave_reg_addr,
							I2C_MEMADD_SIZE_16BIT,
							aI2c_var->r_buf,
							aI2c_var->rw_byte_cnt);
	}
	else if(aI2c_var->operation == I2C_WRITE)
	{
    //Pranay, TBD: this has to be relooked to make it as per received bytecount
		uint8_t lbuf[24] = {0};
		if(aI2c_var->rw_byte_cnt > 24) aI2c_var->rw_byte_cnt = 24;

		memcpy(&lbuf[0], aI2c_var->w_data, aI2c_var->rw_byte_cnt);

		ret =  HAL_I2C_Mem_Write_DMA(aI2c_var->i2c_intf,
							aI2c_var->slave_addr,
							aI2c_var->slave_reg_addr,
							I2C_MEMADD_SIZE_16BIT,
							&lbuf[0],
							aI2c_var->rw_byte_cnt
							);
	}

	return ret;
}

/**
 * @author: Pranay, @date 11Feb'26
 * @brief Handles I2C DMA receive completion for different producers
 * 
 * Processes incoming I2C data based on the producer source and routes it
 * to the appropriate handler function or updates the corresponding data structure.
 * 
 * @param g_cur_req Pointer to the current I2C read/write request containing:
 *                  - producer: Source of the I2C request (ETH_CMD, CM7_TIMER, etc.)
 *                  - r_buf: Receive buffer with raw I2C data
 * 
 * @details
 * - ETH_CMD: Transmits received data via Ethernet
 * - CM7_TIMER: Extracts power data (voltage and current) from receive buffer
 *   - Timestamp: Current system tick count
 *   - Voltage (r_v): 16-bit little-endian value at offset 0x0104-0x0105
 *   - Current (r_i): 16-bit little-endian value at offset 0x0108-0x0109
 * - CM4_CMD: Reserved for CM4 command processing (not implemented)
 * - DISP_CMD: Reserved for display command processing (not implemented)
 * 
 * @return void
 */
void i2c_dma_rx_handler(qi_i2c_rw_t *g_cur_req){

	switch((i2c_producer_t)g_cur_req->producer){
	case ETH_CMD:
		i2c_data_eth_transmit(g_cur_req);
		break;
	case CM7_TIMER:
		g_qi_i2c_rx_power_data.ts_ms   = HAL_GetTick();
		g_qi_i2c_rx_power_data.r_v     = U16_LE(&g_cur_req->r_buf[0]);   // 0x0104..05
		g_qi_i2c_rx_power_data.r_i     = U16_LE(&g_cur_req->r_buf[4]);   // 0x0108..09

		break;
	case CM4_CMD:
		break;
	case DISP_CMD:
		break;
	}
}

/**
 * @author: Pranay, @date 11Feb'26
 * @brief I2C manager thread entry point for processing I2C requests via DMA.
 * 
 * This is the main thread function that manages I2C read/write operations using DMA transfers.
 * It operates as a state machine that:
 * 1. Waits for I2C requests to be queued
 * 2. Initiates DMA-based I2C read/write operations
 * 3. Handles completion events (RX_DONE, TX_DONE, or ERROR)
 * 
 * The thread blocks on event flags waiting for:
 * - I2C_EVT_Q_AVAIL: Indicates a new request is available in the queue
 * - I2C_EVT_RX_DONE: Indicates a read operation has completed
 * - I2C_EVT_TX_DONE: Indicates a write operation has completed
 * - I2C_EVT_ERROR: Indicates an I2C error occurred during transfer
 * 
 * @param[in] thread_input ULONG - Thread input parameter (unused)
 * 
 * @note This function runs indefinitely in a loop and should be executed as a ThreadX thread.
 * @note Handler stubs for TX_DONE and ERROR events are placeholder implementations.
 * 
 * @see qi_i2c_rw_dma()
 * @see i2c_dma_rx_handler()
 * @see i2c_q_dequeue()
 */
VOID i2c_manager_thread_entry(ULONG thread_input){

	ULONG flags;
	while(1){

		   // Wait until something is in queue (simple)
			while (i2c_q_dequeue(&g_cur_req) != 0) {
			//				tx_thread_sleep(1); // yield; or wait on "enqueue event" flag if you want
			   tx_event_flags_get(&i2cEventFlags, (I2C_EVT_Q_AVAIL), TX_OR_CLEAR, &flags, TX_WAIT_FOREVER);

			}
			HAL_StatusTypeDef st = qi_i2c_rw_dma(&g_cur_req);
			if (st != HAL_OK) {
			  // start failed; continue to next request
			  continue;
			}
	       tx_event_flags_get(&i2cEventFlags, (I2C_EVT_RX_DONE | I2C_EVT_TX_DONE | I2C_EVT_ERROR), TX_OR_CLEAR, &flags, TX_WAIT_FOREVER);
	       if (flags & I2C_EVT_RX_DONE){
	    	   i2c_dma_rx_handler(&g_cur_req);
	       }
	       if(flags & I2C_EVT_TX_DONE){
	    	   ;
	       }
	       if(flags & I2C_EVT_ERROR){
	    	   ;
	       }

	}
}
