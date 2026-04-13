/*
 * grl_app.c
 *
 *  Created on: Jan 21, 2026
 *      Author: GRL
 */
#include <cm7_qi_app.h>
#include <qi_defs.h>
#include <qi_enum.h>
#include <qi_peripheral.h>
#include <qi_tim.h>
#include "main.h"
#include "ipc_shared.h"
#include "app_netxduo.h"
#include "mongoose_glue.h"


uint8_t  api_buf[API_QUEUE_SIZE][API_MAX_LEN];
uint8_t eth_dequeue_buf[TCP_CHUNK_SIZE];
volatile uint8_t api_len[API_QUEUE_SIZE];
volatile uint8_t api_wr_idx = 0;
volatile uint8_t api_rd_idx = 0;

extern NX_TCP_SOCKET TCPSocket;
extern NX_PACKET_POOL NxAppPool;
extern volatile uint8_t streamer_stop;

uint16_t Data_length;
TX_EVENT_FLAGS_GROUP ApiEventFlags;
TX_EVENT_FLAGS_GROUP i2cEventFlags;

uint8_t ltx_buffer[API_MAX_LEN];
volatile uint8_t api_rx;

//__attribute__((section(".i2c1DmaSection"), aligned(32)))
extern uint8_t i2c_read_buf[QI_READ_LEN];

/* Set by command handler */
static volatile uint8_t g_ota_active = 0;
/* Ensures mongoose_init runs once */
static volatile uint8_t g_mongoose_ready = 0;

//extern TX_EVENT_FLAGS_GROUP ApiEventFlags;
extern I2C_HandleTypeDef hi2c1;

/**
 * @brief Enables OTA (Over-The-Air) update mode and signals the OTA event.
 * 
 * Sets the global OTA active flag and triggers the OTA_ARM_EVT event flag
 * to notify the system that an OTA update operation should be initiated.
 * Uses TX_OR logic for event flag setting, allowing multiple events to be
 * set concurrently.
 * 
 * @return void
 * 
 * @note This function is thread-safe for use with ThreadX RTOS event flags.
 */
static inline void ota_arm_enable(void)
{
    g_ota_active = 1;
    tx_event_flags_set(&ApiEventFlags, OTA_ARM_EVT, TX_OR);
}

/**
 * @brief Disables OTA (Over-The-Air) update functionality.
 * 
 * This function sets the OTA active flag to inactive and signals the OTA stop event
 * to the API event flags, allowing other threads to be notified of the OTA termination.
 * 
 * @return void
 * 
 * @note This is an inline function for performance optimization.
 */
static inline void ota_arm_disable(void)
{
    g_ota_active = 0;
    tx_event_flags_set(&ApiEventFlags, OTA_STOP_EVT, TX_OR);
}

/**
 * @brief Enqueues data into the API queue buffer.
 *
 * Attempts to add a new message to the API queue. The function validates
 * the input length, checks for queue overflow, and copies the data to
 * the queue buffer if space is available.
 *
 * @param buf Pointer to the data buffer to enqueue. Must not be NULL.
 * @param len Length of the data in bytes. Must be greater than 0 and
 *            not exceed API_MAX_LEN.
 *
 * @return 0 on successful enqueue operation.
 * @return -1 if the length is invalid (0 or exceeds API_MAX_LEN) or
 *            if the queue is full (buffer overflow condition detected).
 *
 * @note This function is not thread-safe. Caller must provide
 *       synchronization if used in multi-threaded context.
 * @note API_QUEUE_SIZE must be defined as the circular queue capacity.
 * @note API_MAX_LEN must be defined as the maximum allowed message length.
 */
int api_enqueue(const uint8_t *buf, uint16_t len)
 {
     uint8_t next;

     /* validate length */
     if (len == 0 || len > API_MAX_LEN)
         return -1;

     /* calculate next write index */
     next = (api_wr_idx + 1U) % API_QUEUE_SIZE;

     /* check buffer full */
     if (next == api_rd_idx)
         return -1;

     /* copy data */
     memcpy(api_buf[api_wr_idx], buf, len);
     api_len[api_wr_idx] = len;

     /* commit write */
     api_wr_idx = next;

     return 0;
 }


/**
 * @brief Dequeues data from the API queue buffer.
 * 
 * Removes the oldest entry from the API queue and copies its contents
 * to the provided output buffer. Updates the read index to point to the
 * next queued entry.
 * 
 * @param[out] out_buf Pointer to buffer where dequeued data will be copied.
 *                      Must be large enough to hold the queued data.
 * @param[out] out_len Pointer to variable that will receive the length of
 *                      the dequeued data in bytes.
 * 
 * @return 0 on successful dequeue, -1 if queue is empty.
 * 
 * @note Ensure out_buf has sufficient capacity before calling this function.
 * @note This function assumes api_buf, api_len, api_rd_idx, and api_wr_idx
 *       are properly initialized.
 * @note API_QUEUE_SIZE must be defined as the maximum number of queue entries.
 */
int api_dequeue(uint8_t *out_buf, uint16_t *out_len)
 {
     /* check buffer empty */
     if (api_rd_idx == api_wr_idx)
         return -1;

     /* fetch data */
     *out_len = api_len[api_rd_idx];
     memcpy(out_buf, api_buf[api_rd_idx], *out_len);

     /* advance read index */
     api_rd_idx = (api_rd_idx + 1U) % API_QUEUE_SIZE;

     return 0;
 }

/**
 * @brief Sends a TCP response packet with the provided data.
 *
 * @param buf       Pointer to the data buffer to be sent. Must not be NULL.
 * @param len       Length of the data in bytes. Must be greater than 0.
 * @param wait_ticks Timeout value in ticks for packet allocation and send operations.
 *
 * @return UINT Status code indicating the result of the operation.
 *         - NX_SUCCESS: Packet was successfully sent.
 *         - NX_INVALID_PARAMETERS: If buf is NULL or len is 0.
 *         - Other status codes from nx_packet_allocate(), nx_packet_data_append(),
 *           or nx_tcp_socket_send() on failure.
 *
 * @note The packet is automatically released on error. On success, the NetX Duo
 *       stack manages the packet lifecycle.
 */
UINT tcp_send_response(const UCHAR *buf, USHORT len, ULONG wait_ticks)
{
    NX_PACKET *txp;
    UINT status;

    if ((buf == NX_NULL) || (len == 0))
        return NX_INVALID_PARAMETERS;

    status = nx_packet_allocate(&NxAppPool, &txp, NX_TCP_PACKET, wait_ticks);
    if (status != NX_SUCCESS)
    {
    	printf("send error.2 : %x\n ",status);
    	return status;
    }

    status = nx_packet_data_append(txp, (VOID *)buf, len, &NxAppPool, wait_ticks);
    if (status != NX_SUCCESS)
    {
    	printf("send error.3 : %x\n ",status);
        nx_packet_release(txp);
        return status;
    }

    status = nx_tcp_socket_send(&TCPSocket, txp, wait_ticks);
    if (status != NX_SUCCESS)
    {
    	printf("send error.4 : %x\n ",status);
        nx_packet_release(txp);
        return status;
    }

    return NX_SUCCESS;
}
//#define CPS_SLAVE_ADDR		(0x30<<1)
static inline uint16_t U16_LE(const uint8_t *p)
{
  return (uint16_t)(p[0] | ((uint16_t)p[1] << 8)); // change if BE
}

void fetch_cps_data(uint8_t *aRxBuffer){

	HAL_StatusTypeDef ret = HAL_OK;

	uint16_t l = API_LENGTH_DECODE(aRxBuffer);
	qi_i2c_rw_t i2c_var;
	i2c_var.slave_reg_addr = U16_LE(&aRxBuffer[10]);
	i2c_var.r_buf = i2c_read_buf;
	i2c_var.rw_byte_cnt = aRxBuffer[9];
	i2c_var.slave_addr =  CPS_SLAVE_ADDR;
	i2c_var.i2c_intf = &hi2c1;
	i2c_var.producer = ETH_CMD;
	i2c_var.operation = I2C_READ;

	memcpy(i2c_var.eth_hdr, aRxBuffer, l);   // snapshot header

	ret = i2c_q_enqueue(&i2c_var);

	if(ret != HAL_OK) printf("I2C read ERR : %d\n",ret);

}

/**
 * @brief Transmits I2C data over Ethernet via TCP.
 * 
 * Appends the I2C read buffer data to an Ethernet header, updates the total
 * packet length, and sends the complete frame over TCP.
 * 
 * @param[in,out] g_cur_req Pointer to the I2C read/write request structure containing:
 *                           - eth_hdr: Ethernet header buffer with encoded length
 *                           - r_buf: Input buffer containing I2C read data
 *                           - rw_byte_cnt: Number of bytes to transmit from r_buf
 * 
 * @return void
 * 
 * @note Assumes eth_hdr has sufficient buffer space for appended data.
 * @note TCP transmission timeout is set to (2 * NX_IP_PERIODIC_RATE).
 */
void i2c_data_eth_transmit(qi_i2c_rw_t *g_cur_req){

	uint16_t l = API_LENGTH_DECODE(g_cur_req->eth_hdr);
	memcpy(&g_cur_req->eth_hdr[l],g_cur_req->r_buf,g_cur_req->rw_byte_cnt);
	l += g_cur_req->rw_byte_cnt;
	API_LENGTH_ENCODE(g_cur_req->eth_hdr,l);

	tcp_send_response(g_cur_req->eth_hdr,l,(2 * NX_IP_PERIODIC_RATE));

}
/**
 * @brief Sets CPS (Wireless Power Receiver) data via I2C communication.
 * 
 * This function extracts data from the received buffer and enqueues an I2C write
 * operation to the CPS device. It constructs an I2C read/write structure with the
 * slave register address, write data, and byte count from the input buffer, then
 * adds this operation to the I2C queue for DMA-based transmission.
 * 
 * @param[in] aRxBuffer Pointer to the received data buffer containing:
 *                      - [9]: Byte count (BC) of data to write
 *                      - [10-11]: Slave register address (little-endian 16-bit)
 *                      - [12...12+BC-1]: Data to write to the CPS device
 * 
 * @return void
 * 
 * @note The function sets the I2C_EVT_Q_AVAIL flag after enqueueing to signal
 *       that data is available for processing.
 * @note On error, prints "I2C Write ERR : <error_code>" to console.
 * @note On success, prints "I2C write Success" to console.
 * @note Uses ThreadX event flags for synchronization (i2cEventFlags).
 * 
 * @see qi_i2c_rw_t
 * @see i2c_q_enqueue()
 */
void set_cps_data(uint8_t *aRxBuffer){
	HAL_StatusTypeDef ret = HAL_OK;
	uint8_t BC = aRxBuffer[9];
//	uint8_t regIdx[] = {aRxBuffer[10],aRxBuffer[11]};
//	uint8_t *Wbuf = &aRxBuffer[12];

	qi_i2c_rw_t i2c_var;
//	i2c_var.slave_reg_addr = (uint16_t)(aRxBuffer[10] & 0xFF | ((aRxBuffer[11] & 0xFF) << 8)) ;
	i2c_var.slave_reg_addr = U16_LE(&aRxBuffer[10]);
	memcpy(&i2c_var.w_data[0], &aRxBuffer[12], BC);
//	i2c_var.rw_buf = Wbuf;
	i2c_var.rw_byte_cnt = BC;
	i2c_var.slave_addr =  CPS_SLAVE_ADDR;
	i2c_var.i2c_intf = &hi2c1;
	i2c_var.producer = ETH_CMD;
	i2c_var.operation = I2C_WRITE;

//	ret = qi_i2c_rw_dma(&i2c_var);
	ret = i2c_q_enqueue(&i2c_var);

	if(ret != HAL_OK) printf("I2C Write ERR : %d\n",ret);
	else printf("I2C write Success \n");

	tx_event_flags_set(&i2cEventFlags, I2C_EVT_Q_AVAIL, TX_OR);

}
/**
 * @brief This function fetches the FW version based on controller type
 * 
 * @param aRxBuffer 
 */
void fw_version_fetch(uint8_t *aRxBuffer){

	uint16_t l = API_LENGTH_DECODE(aRxBuffer);

	switch((controllerType_e)aRxBuffer[8]){
		case CONTROLLLER_UNIT://CM7 FW version Fetch
			aRxBuffer[l++] = FW_MAJOR_VERSION;
			aRxBuffer[l++] = FW_MINOR_VERSION;
			aRxBuffer[l++] = FW_SUB_MINOR_VERSION;

			API_LENGTH_ENCODE(aRxBuffer,l);

			tcp_send_response(aRxBuffer,l,(2 * NX_IP_PERIODIC_RATE));
		break;
		case ELOAD_UNIT://CM4 FW version Fetch
		    cm7_send_to_cm4(aRxBuffer, API_LENGTH_DECODE(aRxBuffer));
		break;
		case QI_UNIT:
//			cps_fw_version();
			fetch_cps_data(aRxBuffer);
		break;
		case DISPLAY_UNIT:
		break;
		case ALL_UNITS:
		break;
	}
}

/**
 * @brief Decodes polling data and sends a TCP response with either queued UART data or current I2C power data.
 *
 * This function attempts to pop a packet from the UART ring buffer. If successful, it encodes
 * the queued packet data (timestamp, voltage, current, queue indices, and payload) into the
 * response buffer and sends it via TCP. If no queued data is available, it retrieves the latest
 * I2C power measurement data, encodes it with queue indices and validity flags, and sends that instead.
 *
 * @param[out] aRxBuffer Pointer to the buffer where response data will be encoded and prepared
 *                       for transmission. Must be large enough to hold the encoded response.
 *
 * @return None
 *
 * @note The function disables interrupts temporarily when accessing shared I2C data to ensure
 *       data consistency. Queue indices (rd_idx, wr_idx) are included in both response types
 *       to indicate the state of the UART ring buffer.
 *
 * @warning Ensure aRxBuffer is adequately sized to prevent buffer overflow when copying payload data.
 */
void polling_data_decode(uint8_t *aRxBuffer){
	uint16_t l = API_LENGTH_DECODE(aRxBuffer);
	uart4_pkt_t g_poll_ring;
	uint8_t rd_idx = 0, wr_idx = 0;

	if(UART4_PktRing_Pop(&g_poll_ring, &rd_idx, &wr_idx)){

		PUT_U32_LE(aRxBuffer, l, g_poll_ring.rv_ri_ts_ms);
		PUT_U8(aRxBuffer, l, 4);
		PUT_U16_LE(aRxBuffer, l, g_poll_ring.rv);
		PUT_U16_LE(aRxBuffer, l, g_poll_ring.ri);

		UART_QUEUE_IDX_FILL(aRxBuffer, l, rd_idx, wr_idx);
		DATA_VALIDITY_CDWORD_FILL(aRxBuffer, l,VALID_QUEUE_DATA);

		// [ts_ms:4][len:2][payload:len]
		PUT_U32_LE(aRxBuffer, l, g_poll_ring.uart_ts_ms);
		PUT_U16_LE(aRxBuffer, l, g_poll_ring.len);

		if (g_poll_ring.len > 0u)
		{
			memcpy(&aRxBuffer[l], g_poll_ring.data, g_poll_ring.len);
			l += g_poll_ring.len;
		}

		API_LENGTH_ENCODE(aRxBuffer,l);
		tcp_send_response(aRxBuffer,l,(2 * NX_IP_PERIODIC_RATE));
	}else{

		extern volatile qi_vi_sample_t g_qi_i2c_rx_power_data;

		uint32_t primask = __get_PRIMASK();
		__disable_irq();

		if(streamer_stop)
			g_qi_i2c_rx_power_data.ts_ms = HAL_GetTick();

		PUT_U32_LE(aRxBuffer, l, g_qi_i2c_rx_power_data.ts_ms);
		PUT_U8(aRxBuffer, l, 4);
		PUT_U16_LE(aRxBuffer, l, g_qi_i2c_rx_power_data.r_v);
		PUT_U16_LE(aRxBuffer, l, g_qi_i2c_rx_power_data.r_i);

		if (!primask) __enable_irq();

		UART_QUEUE_IDX_FILL(aRxBuffer, l, rd_idx, wr_idx);
		DATA_VALIDITY_CDWORD_FILL(aRxBuffer, l,NO_QUEUE_DATA);
		POLL_FILL_NO_DATA(aRxBuffer, l, 4);

		printf("POLLING INFO: no UART data\n");

		API_LENGTH_ENCODE(aRxBuffer, l);
		tcp_send_response(aRxBuffer, l, (2 * NX_IP_PERIODIC_RATE));
		return;
	}
}

void get_system_details_handler(uint8_t *aRxBuffer){
	uint8_t lParType = aRxBuffer[7];
//	uint8_t lsubParType = aRxBuffer[8];
	switch((sys_details_e)lParType){
		case SYS_FW_VERSION:
			fw_version_fetch(aRxBuffer);
		break;
		case POLLING_DATA:
			polling_data_decode(aRxBuffer);
			break;
	}
}
void FW_upd_handler(uint8_t *aRxBuffer){
	uint8_t lctrlType = (aRxBuffer[8] & 0x0F);
	uint8_t en_dis = ( (aRxBuffer[8] & 0xF0) >> 4);
	switch(lctrlType){
	case CONTROLLLER_UNIT:

		if(en_dis) ota_arm_enable();
		else ota_arm_disable();

		break;

	}
}
void pgm_cmd_handler(uint8_t *aRxBuffer){

	uint8_t lFuncType = aRxBuffer[5];
	switch(lFuncType){
	case FW_UPDATE:
		FW_upd_handler(aRxBuffer);
		break;
	}
}
/**
 * @brief Get the cmd handler object for processing GET commands
 * 
 * @param aRxBuffer : incoming API buffer
 */
void get_cmd_handler(uint8_t *aRxBuffer){

	uint8_t lFuncType = aRxBuffer[5];
	switch((func_type_e)lFuncType){
		case POWER_MGMT:
			break;
		case QI_PKT_MSKG:
			break;
		case QI_PKT_CGF:
			break;
		case QI_PKT_CTRL:
				break;
		case QI_TIM_CFG:
			break;
		case QI_TSTR_MODE_CFG:
			break;
		case CALIB:
			break;
		case SYS_DETAILS:
			get_system_details_handler(aRxBuffer);
		break;
		case MISC:
		
		break;
		case FW_UPDATE://No use
			break;
	}
}
void misc_handler(uint8_t *aRxBuffer){
	uint8_t lParType = aRxBuffer[7];
	switch(lParType){
		case 0x01://streamer ctrl
			streamer_stop = aRxBuffer[8];
		break;
	}
}
void set_cmd_handler(uint8_t *aRxBuffer){
	uint8_t lFuncType = aRxBuffer[5];
	switch((func_type_e)lFuncType){
	case POWER_MGMT:
		break;
	case QI_PKT_MSKG:
		break;
	case QI_PKT_CGF:
		break;
	case QI_PKT_CTRL:
			break;
	case QI_TIM_CFG:
		break;
	case QI_TSTR_MODE_CFG:
		break;
	case CALIB:
		break;
	case SYS_DETAILS:
		set_cps_data(aRxBuffer);
		break;
	case MISC:
		misc_handler(aRxBuffer);
		break;
	default:
		break;
	}
	//echoback for set commands
    tcp_send_response(aRxBuffer, API_LENGTH_DECODE(aRxBuffer), (2 * NX_IP_PERIODIC_RATE));

}
/**
 * @brief Process the incoming application command, 
 * Entry point for API command processing that differentiates SET/GET/PGM commands
 * 
 * @param aRxBuffer 
 */
void process_app_cmd(uint8_t *aRxBuffer){

	uint8_t lCmdType = aRxBuffer[0];
	switch((cmd_type_e)lCmdType ){
		case SET_CMD:
			set_cmd_handler(aRxBuffer);
			break;
		case PGM_CMD:
			pgm_cmd_handler(aRxBuffer);
			break;
		case GET_CMD:
			get_cmd_handler(aRxBuffer);
			break;

	}
}
void grl_app_init(){
	ipc_shm_init();
    __HAL_RCC_HSEM_CLK_ENABLE();
	cm4_reply_ready = 0;
	api_rx = 0;
	g_ota_active = 0;
	g_mongoose_ready = 0;
	streamer_stop = 0;
	//	sentindex = 0;
    printf("M7_TO_M4 @ %p\n", &SHM_M7_TO_M4);
    printf("M4_TO_M7 @ %p\n", &SHM_M4_TO_M7);
}


/**
 * @brief ApiProcess_Thread_Entry - Main application processing thread
 * @param thread_input - Thread input parameter (unused)
 * 
 * @details
 * This is the main processing thread for the API layer. It handles:
 * - Incoming API commands from Ethernet (API_RX_EVT)
 * - Inter-core communication with CM4 processor (CM4_INTR)
 * - OTA (Over-The-Air) update initialization (OTA_ARM_EVT)
 * - OTA update termination (OTA_STOP_EVT)
 * - Mongoose polling when OTA is active
 * 
 * The thread waits on event flags with adaptive timeout:
 * - TX_WAIT_FOREVER when OTA is inactive
 * - 1 tick when OTA is active (for periodic mongoose polling)
 * 
 * When API_RX_EVT is triggered, dequeued Ethernet messages are processed.
 * When CM4_INTR is triggered, shared memory data from CM4 is cache-invalidated,
 * validated, and sent as TCP response.
 * When OTA events occur, mongoose is initialized/terminated as needed.
 * 
 * @note Thread runs indefinitely in a while(1) loop
 * @note Cache coherency is maintained via SCB_InvalidateDCache_by_Addr()
 * @note Shared memory length validation prevents buffer overflow attacks
 */
VOID ApiProcess_Thread_Entry(ULONG thread_input)
{
    ULONG flags;

    uint16_t len;
    uint32_t shm_len = 0;
    grl_app_init();
    msg_tim_init();
    while (1)
    {
        /* If OTA is active, don't block forever; wake periodically to poll mongoose */
        ULONG wait_ticks = (g_ota_active) ? 1 : TX_WAIT_FOREVER;

        tx_event_flags_get(&ApiEventFlags, (API_RX_EVT | CM4_INTR | OTA_ARM_EVT | OTA_STOP_EVT), TX_OR_CLEAR, &flags, wait_ticks);

        if (flags & API_RX_EVT)
//    	if(api_rx)
        {
//    		api_rx = 0;
            if(api_dequeue(eth_dequeue_buf, &len) == 0)
            {
                process_app_cmd(eth_dequeue_buf);
            }
        }
        if(flags & CM4_INTR)
        {
        	if(cm4_reply_ready)
        	{
        		cm4_reply_ready = 0;

        		SCB_InvalidateDCache_by_Addr((uint32_t*)&SHM_M4_TO_M7,
        		                                     cache_align_size(sizeof(SHM_M4_TO_M7)));
        		__DMB();

        		shm_len = SHM_M4_TO_M7.len;
        		if (shm_len == 0 || shm_len > API_MAX_LEN) {
        		    printf("SHM len bad: %lu\n", shm_len);
        		    continue;
        		}
        		memcpy(ltx_buffer, (void*)SHM_M4_TO_M7.data, shm_len);
                tcp_send_response(ltx_buffer, (USHORT)shm_len, (2 * NX_IP_PERIODIC_RATE));
        	}
        }

        /* Handle OTA arm event */
        if (flags & OTA_ARM_EVT)
        {
            if (!g_mongoose_ready)
            {
                /* Safe: init only once, when explicitly armed */
                mongoose_init();
                g_mongoose_ready = 1;
            }
            /* g_ota_active already set by command handler, but safe if you want: */
            g_ota_active = 1;
        }

        /* Handle OTA stop event */
        if (flags & OTA_STOP_EVT)
        {
            g_ota_active = 0;

            /* Optional:
             * If your mongoose glue has a deinit/stop API, call it here.
             * Otherwise leave mongoose initialized; just stop polling.
             */
            // mongoose_deinit_or_stop();
        }
        if (g_ota_active)
		{
			/* If not ready (edge case), init now */
			if (!g_mongoose_ready)
			{
				mongoose_init();
				g_mongoose_ready = 1;
			}

			mongoose_poll();

			/* Yield a little so TCP RX thread always wins */
			tx_thread_sleep(1);
		}

    }
}



