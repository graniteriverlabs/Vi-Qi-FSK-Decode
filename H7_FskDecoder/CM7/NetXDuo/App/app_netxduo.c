/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_netxduo.c
  * @author  MCD Application Team
  * @brief   NetXDuo applicative file
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2020-2021 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* USER CODE END Header */

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/* Includes ------------------------------------------------------------------*/
#include <cm7_qi_app.h>
#include "app_netxduo.h"

/* Private includes ----------------------------------------------------------*/
#include "nxd_dhcp_client.h"
/* USER CODE BEGIN Includes */
//#include "tcp_server.h"
#include <qi_peripheral.h>
#include "stdlib.h"
#include "stdint.h"
#include "main.h"
#include "nxd_dhcp_client.h"
#include "nxd_bsd.h"
#include "mongoose.h"
#include "mongoose_glue.h"
#include "nx_api.h"
#include "ipc_shared.h"

#include "cm7_stream_drain.h"
#include <ipc_stream_shared.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#define FSK_DEBUG_OFFLINE_PERIOD_ONLY	1
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/**
 * @brief UDP port used by the PC tool to receive FSK debug stream packets.
 */
#define FSK_STREAM_UDP_PORT          9000U

/**
 * @brief Local UDP source port used by the CM7 streaming socket.
 *
 * This is the port the STM32 binds to locally before transmitting
 * UDP debug packets.
 */
#define FSK_STREAM_UDP_LOCAL_PORT   9001U

/**
 * @brief Stack size for the dedicated CM7 UDP streaming thread.
 *
 * This thread only drains shared memory and sends UDP packets, so
 * its stack requirement is moderate.
 */
#define FSK_STREAM_THREAD_STACK_SIZE     (8U*1024U)

/**
 * @brief Thread priority for the dedicated CM7 UDP streaming thread.
 *
 * This should be below the most critical real-time threads, but high
 * enough to keep the stream drained regularly.
 */
#define FSK_STREAM_THREAD_PRIORITY       (NX_APP_THREAD_PRIORITY + 1)

/**
 * @brief Magic value placed at the beginning of every FSK UDP packet.
 *
 * The value corresponds to ASCII "FSK1". It is used by the PC application
 * to verify that the received UDP payload matches the expected protocol.
 *
 * @note UDP already preserves packet boundaries, so this magic is not used
 *       for stream framing. It is used as a protocol sanity check.
 */
#define FSK_UDP_MAGIC                    (0x314B5346UL) /* "FSK1" */

/**
 * @brief Protocol version for the FSK UDP packet format.
 *
 * Increment this value whenever the packet header or record layout changes
 * in an incompatible way.
 */
#define FSK_UDP_VERSION                  (1U)

/**
 * @brief Maximum total UDP payload size used for FSK streaming.
 *
 * This value is intentionally kept below the standard Ethernet MTU-safe UDP
 * payload size.
 */
#define FSK_UDP_MAX_PAYLOAD_SIZE         (1400U)

/**
 * @brief Size of the fixed UDP stream header.
 *
 * Current header fields:
 * magic        : 4 bytes
 * version      : 2 bytes
 * header_size  : 2 bytes
 * sequence     : 4 bytes
 * record_count : 2 bytes
 * record_size  : 2 bytes
 *
 * Total = 16 bytes.
 */
#define FSK_UDP_HEADER_SIZE_BYTES        (16U)

/**
 * @brief FSK UDP record size selected from the debug format.
 *
 * Period-only offline mode sends uint16_t records.
 * Full-record mode sends ipc_stream_record_t records.
 */
#if FSK_DEBUG_OFFLINE_PERIOD_ONLY
#define FSK_UDP_RECORD_SIZE              (sizeof(uint16_t))
#else
#define FSK_UDP_RECORD_SIZE              (sizeof(ipc_stream_record_t))
#endif


/**
 * @brief Maximum body bytes that fit in one MTU-safe UDP payload.
 *
 * The body is rounded down to a whole number of records so CM7 never sends
 * partial records to the PC.
 */
#define FSK_UDP_MAX_BODY_BYTES_RAW       (FSK_UDP_MAX_PAYLOAD_SIZE - FSK_UDP_HEADER_SIZE_BYTES)

#define FSK_UDP_TARGET_BODY_BYTES        \
    ((FSK_UDP_MAX_BODY_BYTES_RAW / FSK_UDP_RECORD_SIZE) * FSK_UDP_RECORD_SIZE)

/**
 * @brief Maximum records per UDP packet for the selected record format.
 */
#define FSK_UDP_MAX_RECORDS_PER_PACKET   \
    (FSK_UDP_TARGET_BODY_BYTES / FSK_UDP_RECORD_SIZE)
/**
 * @brief Maximum time to wait before flushing a partially filled UDP packet.
 *
 * This prevents excessive latency when the incoming stream rate is low.
 *
 * @note Value is in ThreadX ticks.
 */
#define FSK_UDP_FLUSH_TICKS                 (10U)

/**
 * @brief Header placed at the beginning of every UDP FSK stream packet.
 *
 * This header helps the PC-side decoder validate the packet and parse the
 * payload safely.
 */
typedef struct __attribute__((packed))
{
    uint32_t magic;        /**< Packet signature, always @ref FSK_UDP_MAGIC */
    uint16_t version;      /**< Protocol version */
    uint16_t header_size;  /**< Size of this header in bytes */
    uint32_t sequence;     /**< Monotonic packet sequence counter */
    uint16_t record_count; /**< Number of records carried in this packet */
    uint16_t record_size;  /**< Size of one record in bytes */
} fsk_udp_header_t;

/**
 * @brief Static UDP transmit buffer used by the CM7 stream thread.
 *
 * Keeping this buffer in static storage avoids large ThreadX stack usage.
 */
static uint8_t g_fsk_udp_tx_buffer[FSK_UDP_MAX_PAYLOAD_SIZE];

/**
 * @brief Monotonic sequence number assigned to outgoing FSK UDP packets.
 */
static uint32_t g_fsk_udp_sequence = 0U;


/**
 * @brief Number of UDP packet build attempts.
 */
static volatile uint32_t g_udp_attempt_count = 0U;

/**
 * @brief Number of UDP packets successfully sent.
 */
static volatile uint32_t g_udp_sent_count = 0U;

/**
 * @brief Number of UDP packet allocation failures.
 */
static volatile uint32_t g_udp_alloc_fail_count = 0U;

/**
 * @brief Number of UDP payload append failures.
 */
static volatile uint32_t g_udp_append_fail_count = 0U;

/**
 * @brief Number of UDP socket send failures.
 */
static volatile uint32_t g_udp_send_fail_count = 0U;

/**
 * @brief Last status returned by packet allocation.
 */
static volatile UINT g_udp_last_alloc_status = NX_SUCCESS;

/**
 * @brief Last status returned by payload append.
 */
static volatile UINT g_udp_last_append_status = NX_SUCCESS;

/**
 * @brief Last status returned by UDP send.
 */
static volatile UINT g_udp_last_send_status = NX_SUCCESS;

/**
 * @brief Sequence number of the last allocation failure.
 */
static volatile uint32_t g_udp_last_alloc_fail_seq = 0U;

/**
 * @brief Sequence number of the last append failure.
 */
static volatile uint32_t g_udp_last_append_fail_seq = 0U;

/**
 * @brief Sequence number of the last send failure.
 */
static volatile uint32_t g_udp_last_send_fail_seq = 0U;

/**
 * @brief Current number of available packets in the NetX packet pool.
 */
static volatile ULONG g_udp_pool_available = 0U;

/**
 * @brief Lowest observed number of available packets in the NetX packet pool.
 */
static volatile ULONG g_udp_pool_lowest_available = 0xFFFFFFFFUL;

/**
 * @brief Number of packet pool empty requests observed.
 */
static volatile ULONG g_udp_pool_empty_requests = 0U;

/**
 * @brief Snapshot of shared CM4->CM7 ring occupancy in bytes.
 */
static volatile uint32_t g_stream_ring_used = 0U;

/**
 * @brief Highest occupancy ever observed in the shared CM4->CM7 ring.
 */
static volatile uint32_t g_stream_ring_high_water = 0U;

/**
 * @brief Number of CM4 stream writes dropped due to ring-full conditions.
 */
static volatile uint32_t g_stream_ring_dropped = 0U;

/**
 * @brief NetX IP-layer total packets accepted for transmit.
 */
static volatile ULONG g_udp_ip_total_packets_sent = 0U;

/**
 * @brief NetX IP-layer total bytes accepted for transmit.
 */
static volatile ULONG g_udp_ip_total_bytes_sent = 0U;

/**
 * @brief NetX IP-layer packets dropped during transmit handling.
 */
static volatile ULONG g_udp_ip_send_packets_dropped = 0U;

/**
 * @brief NetX IP-layer transmit resource errors.
 *
 * This counter is not exposed by nx_ip_info_get(), so it is read directly
 * from the NetX IP instance for debugger visibility.
 */
static volatile ULONG g_udp_ip_transmit_resource_errors = 0U;

/**
 * @brief NetX IP-layer count of packet send requests submitted by the stack.
 */
static volatile ULONG g_udp_ip_total_packet_send_requests = 0U;
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#define GRL_TCP_PORT	5002
#define MAX_TCP_CLIENTS                       2
#define WINDOW_SIZE                           (1024*8)

#define IP_CHANGED_EVENT   0x01
#define TCP_DISCONNECT_EVENT	0x02

#define API_RX_EVENT   0x01
#define API_STACK_SIZE 512
#define API_TX_EVENT   0x02

/* Pre-fill 1 KB buffer with counter pattern: every byte = (counter & 0xFF) */
//static uint8_t tx_buffer[TCP_CHUNK_SIZE];
//#if defined(__GNUC__)
//    __attribute__((aligned(32)))
//#endif

//static uint32_t counter = 0;
//
//static void init_tx_buffer(uint32_t size) {
//    uint8_t v = (uint8_t)(counter & 0xFF);
//    /* unrolled-ish fill for speed; plain loop is fine too */
//    for (uint32_t i = 0; i < size; i++) {
//        tx_buffer[i] = v;
//    }
//    counter++;  /* bump once per 1KB response */
//}
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TX_THREAD      NxAppThread;
NX_PACKET_POOL NxAppPool;
NX_IP          NetXDuoEthIpInstance;
//TX_SEMAPHORE   DHCPSemaphore;
NX_DHCP        DHCPClient;
/* USER CODE BEGIN PV */
//TX_SEMAPHORE   TCPSemaphore;
TX_THREAD AppTCPThread;
TX_THREAD AuthThread;
TX_THREAD MG_THREAD;

NX_TCP_SOCKET TCPSocket;

ULONG IpAddress;
ULONG NetMask;
ULONG dhcp_alloc_ip, dhcp_alloc_mask;
TX_EVENT_FLAGS_GROUP IpEventFlags;
TX_THREAD ApiProcessThread;
TX_THREAD i2c_manager_thread;
extern TX_EVENT_FLAGS_GROUP ApiEventFlags;
extern TX_EVENT_FLAGS_GROUP i2cEventFlags;


static UCHAR data_buffer[TCP_CHUNK_SIZE];
NX_PACKET *txp = NX_NULL;

/**
 * @brief UDP socket used for FSK online streaming.
 */
static NX_UDP_SOCKET g_fsk_stream_udp_socket;

/**
 * @brief CM7 thread responsible for draining the CM4->CM7 stream ring
 *        and transmitting UDP packets to the host PC.
 */
static TX_THREAD g_fsk_stream_thread;

/**
 * @brief IP address of the host PC that should receive the UDP stream.
 *
 * This is captured from the currently connected TCP client.
 */
static volatile ULONG g_fsk_stream_host_ip = 0U;

/**
 * @brief UDP destination port on the host PC.
 */
static volatile UINT g_fsk_stream_host_port = FSK_STREAM_UDP_PORT;

/**
 * @brief Indicates whether the CM7 UDP streaming socket has been
 *        created and bound successfully.
 *
 * The dedicated UDP stream thread checks this flag before attempting
 * to send packets.
 */
static volatile uint8_t g_fsk_stream_udp_ready = 0U;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
//static VOID nx_app_thread_entry (ULONG thread_input);
static VOID ip_address_change_notify_callback(NX_IP *ip_instance, VOID *ptr);
/* USER CODE BEGIN PFP */
static VOID App_TCP_Thread_Entry(ULONG thread_input);
extern void Error_Handler(void);

VOID tx_stack_error_handler(TX_THREAD *thread_ptr);
void print_dhcp_times(void);
void print_mac(uint8_t *mac);
//static void mg_app_thread_entry(ULONG thread_input);
void dump_packet_pool(NX_PACKET_POOL *pool);

/**
 * @brief Create the UDP socket used for FSK online streaming.
 *
 * @retval NX_SUCCESS on success
 * @retval NetX error code on failure
 */
static UINT FSK_Stream_UDP_Socket_Create(void);

/**
 * @brief Entry function of the CM7 UDP streaming thread.
 *
 * This thread periodically drains the CM4->CM7 shared stream ring using
 * cm7_stream_drain_fill_packet() and forwards the payload through UDP.
 *
 * @param[in] thread_input Unused ThreadX thread input parameter.
 */
static VOID FSK_Stream_Thread_Entry(ULONG thread_input);

/* USER CODE END PFP */

/**
  * @brief  Application NetXDuo Initialization.
  * @param memory_ptr: memory pointer
  * @retval int
  */
UINT MX_NetXDuo_Init(VOID *memory_ptr)
{
  UINT ret = NX_SUCCESS;
  TX_BYTE_POOL *byte_pool = (TX_BYTE_POOL*)memory_ptr;
  CHAR *pointer;
  CHAR *bsd_pointer;
  /* USER CODE BEGIN MX_NetXDuo_MEM_POOL */
  /* USER CODE END MX_NetXDuo_MEM_POOL */

  /* USER CODE BEGIN 0 */
#if MG_STMPACK_TLS != 0
  SCB->CCR &= ~SCB_CCR_UNALIGN_TRP_Msk;
  __DSB(); __ISB();
#endif
  /* USER CODE END 0 */

  /* Initialize the NetXDuo system. */
  nx_system_initialize();
  tx_thread_stack_error_notify(tx_stack_error_handler);

    /* Allocate the memory for packet_pool.  */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer, NX_APP_PACKET_POOL_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
	  printf("bytepoll failed \n");
    return TX_POOL_ERROR;
  }

  /* Create the Packet pool to be used for packet allocation,
   * If extra NX_PACKET are to be used the NX_APP_PACKET_POOL_SIZE should be increased
   */
  ret = nx_packet_pool_create(&NxAppPool,
		  	  	  	  	  	  "NetXDuo App Pool",
							  DEFAULT_PAYLOAD_SIZE,
							  pointer,
							  NX_APP_PACKET_POOL_SIZE);

  if (ret != NX_SUCCESS)
  {
    return NX_POOL_ERROR;
  }

    /* Allocate the memory for Ip_Instance */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer, NX_IP_THREAD_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }

   /* Create the main NX_IP instance */
  ret = nx_ip_create(&NetXDuoEthIpInstance,
		  	  	  	  "NetX Ip instance",
					  NX_APP_DEFAULT_IP_ADDRESS,
					  NX_APP_DEFAULT_NET_MASK,
					  &NxAppPool,
					  nx_stm32_eth_driver,
					  pointer,
					  NX_IP_THREAD_STACK_SIZE,
					  NX_APP_INSTANCE_PRIORITY);

  if (ret != NX_SUCCESS)
  {
    return NX_NOT_SUCCESSFUL;
  }

    /* Allocate the memory for ARP */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer, DEFAULT_ARP_CACHE_SIZE*2, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }

  /* Enable the ARP protocol and provide the ARP cache size for the IP instance */

  /* USER CODE BEGIN ARP_Protocol_Initialization */

  /* USER CODE END ARP_Protocol_Initialization */

  ret = nx_arp_enable(&NetXDuoEthIpInstance, (VOID *)pointer, DEFAULT_ARP_CACHE_SIZE*2);

  if (ret != NX_SUCCESS)
  {
    return NX_NOT_SUCCESSFUL;
  }

  /* Enable the ICMP */

  /* USER CODE BEGIN ICMP_Protocol_Initialization */

  /* USER CODE END ICMP_Protocol_Initialization */

  ret = nx_icmp_enable(&NetXDuoEthIpInstance);

  if (ret != NX_SUCCESS)
  {
    return NX_NOT_SUCCESSFUL;
  }

  /* Enable TCP Protocol */

  /* USER CODE BEGIN TCP_Protocol_Initialization */
  /* Add this line — sets the default gateway */
  ret = nx_ip_gateway_address_set(&NetXDuoEthIpInstance, GATEWAY_ADDR);
  if (ret != NX_SUCCESS)
  {
      printf("Gateway set failed!\n");
      return NX_NOT_SUCCESSFUL;
  }

  /* Allocate the memory for TCP server thread   */
   if (tx_byte_allocate(byte_pool, (VOID **) &pointer,APP_TCP_THREAD_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
   {
	   printf("TX_POOL_ERROR \n");
	   return TX_POOL_ERROR;
   }

   /* Create the TCP server thread */
   ret = tx_thread_create(&AppTCPThread,
		   	   	   	   	   "App TCP Thread",
						   App_TCP_Thread_Entry,
						   0,
						   pointer,
						   APP_TCP_THREAD_STACK_SIZE,
						   NX_APP_THREAD_PRIORITY,
						   NX_APP_THREAD_PRIORITY,
						   TX_NO_TIME_SLICE,
						   TX_AUTO_START);

   if (ret != TX_SUCCESS)
   {
     return NX_NOT_SUCCESSFUL;
   }

  /* USER CODE END TCP_Protocol_Initialization */

  ret = nx_tcp_enable(&NetXDuoEthIpInstance);

  if (ret != NX_SUCCESS)
  {
    return NX_NOT_SUCCESSFUL;
  }

  /* Enable the UDP protocol required for  DHCP communication */

  /* USER CODE BEGIN UDP_Protocol_Initialization */
  ret = nx_udp_enable(&NetXDuoEthIpInstance);

  if (ret != NX_SUCCESS)
  {
    return NX_NOT_SUCCESSFUL;
  }
  /* USER CODE END UDP_Protocol_Initialization */


#if 0
   /* Allocate the memory for main thread   */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer, NX_THREAD_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
	  printf("TX_POOL_ERROR -1 \n");
    return TX_POOL_ERROR;
  }

  /* Create the main thread */
  ret = tx_thread_create(&NxAppThread,
						 "NetXDuo App thread",
						  nx_app_thread_entry ,
						  0,
						  pointer,
						  NX_THREAD_STACK_SIZE,
						  NX_APP_THREAD_PRIORITY,
						  NX_APP_THREAD_PRIORITY,
						  TX_NO_TIME_SLICE,
						  TX_AUTO_START);

  if (ret != TX_SUCCESS)
  {
    return TX_THREAD_ERROR;
  }
#endif
  /* Create the DHCP client */

  /* USER CODE BEGIN DHCP_Protocol_Initialization */

  /* USER CODE END DHCP_Protocol_Initialization */

//  ret = nx_dhcp_create(&DHCPClient, &NetXDuoEthIpInstance, "DHCP Client");

  if (ret != NX_SUCCESS)
  {
    return NX_DHCP_ERROR;
  }

  /* set DHCP notification callback  */
//  tx_semaphore_create(&DHCPSemaphore, "DHCP Semaphore", 0);

  /* USER CODE BEGIN MX_NetXDuo_Init */
  tx_event_flags_create(&IpEventFlags, "IP Change Events");
  tx_event_flags_create(&ApiEventFlags, "API Events");
  tx_event_flags_create(&i2cEventFlags, "I2C Events");


//  tx_semaphore_create(&TCPSemaphore, "TCP Semaphore", 0);
//KANNAN
#if 0
  /* Allocate memory for your custom thread stack */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer,AUTH_THREAD_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
	  printf("TX_POOL_ERROR -2\n");
    return TX_POOL_ERROR;
  }
  /* Create your custom thread for application logic */
  ret = tx_thread_create(&AuthThread,            // Thread handle
                         "My Auth Thread",        // Thread name
                         Auth_Thread_Entry,    // Your thread entry function
                         0,                      // Thread input
                         pointer,                // Stack pointer
						 AUTH_THREAD_STACK_SIZE, // Stack size (e.g., 1024)
                         NX_APP_THREAD_PRIORITY, // Priority (choose appropriate value)
                         NX_APP_THREAD_PRIORITY, // Preemption threshold (can be same as Priority)
                         TX_NO_TIME_SLICE,       // Time slice
                         TX_AUTO_START);         // Start immediately

  /* Check if thread creation was successful */
  if (ret != TX_SUCCESS)
  {
      return TX_THREAD_ERROR;
  }
#endif

	if (tx_byte_allocate(byte_pool, (VOID **) &bsd_pointer,BSD_APP_THREAD_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
	{
		return TX_POOL_ERROR;
	}

	ret = bsd_initialize(&NetXDuoEthIpInstance,
					&NxAppPool,
					bsd_pointer,
					BSD_APP_THREAD_STACK_SIZE,
					BSD_APP_THREAD_PRIORITY);
	/* Check if thread creation was successful */
	if (ret != TX_SUCCESS)
	{
		return TX_THREAD_ERROR;
	}

    /**
     * Allocate stack for the dedicated CM7 UDP stream thread.
     */
    if (tx_byte_allocate(byte_pool,
                         (VOID **)&pointer,
                         FSK_STREAM_THREAD_STACK_SIZE,
                         TX_NO_WAIT) != TX_SUCCESS)
    {
        return TX_POOL_ERROR;
    }

    /**
     * Create the dedicated UDP streaming thread.
     *
     * This thread drains the shared CM4->CM7 stream ring independently
     * of the TCP command server.
     */
    ret = tx_thread_create(&g_fsk_stream_thread,
                           "FSK Stream Thread",
                           FSK_Stream_Thread_Entry,
                           0,
                           pointer,
                           FSK_STREAM_THREAD_STACK_SIZE,
                           FSK_STREAM_THREAD_PRIORITY,
                           FSK_STREAM_THREAD_PRIORITY,
                           TX_NO_TIME_SLICE,
                           TX_AUTO_START);

    if (ret != TX_SUCCESS)
    {
        return TX_THREAD_ERROR;
    }
#if 0
	/* Allocate memory for your custom thread stack */
	if (tx_byte_allocate(byte_pool, (VOID **) &pointer,MG_APP_THREAD_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
	{
		return TX_POOL_ERROR;
	}
	/* Create your custom thread for application logic */
	ret = tx_thread_create(&MG_THREAD,            // Thread handle
						 "MG Thread",        // Thread name
						 mg_app_thread_entry,    // Your thread entry function
						 0,                      // Thread input
						 pointer,                // Stack pointer
						 MG_APP_THREAD_STACK_SIZE, // Stack size (e.g., 1024)
						 NX_APP_THREAD_PRIORITY+2, // Priority (choose appropriate value)
						 NX_APP_THREAD_PRIORITY+2, // Preemption threshold (can be same as Priority)
						 TX_NO_TIME_SLICE,       // Time slice
						 TX_AUTO_START);         // Start immediately

	/* Check if thread creation was successful */
	if (ret != TX_SUCCESS)
	{
	  return TX_THREAD_ERROR;
	}
#endif
	/* Allocate the memory for API hanlding thread   */
	if (tx_byte_allocate(byte_pool, (VOID **) &pointer, API_THREAD_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
	{
	   printf("TX_POOL_ERROR \n");
	   return TX_POOL_ERROR;
	}
	ret = tx_thread_create(&ApiProcessThread,
						  "API Process Thread",
						  ApiProcess_Thread_Entry,
						  0,
						  pointer,
						  API_THREAD_STACK_SIZE,
						  NX_APP_THREAD_PRIORITY+3,
						  NX_APP_THREAD_PRIORITY+3,
						  TX_NO_TIME_SLICE,
						  TX_AUTO_START);
	/* Check if thread creation was successful */
	if (ret != TX_SUCCESS)
	{
	  return TX_THREAD_ERROR;
	}


	/* Allocate the memory for API hanlding thread   */
	if (tx_byte_allocate(byte_pool, (VOID **) &pointer, I2C_THREAD_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
	{
	   printf("TX_POOL_ERROR \n");
	   return TX_POOL_ERROR;
	}
	ret = tx_thread_create(&i2c_manager_thread,
						  "i2c Process Thread",
						  i2c_manager_thread_entry,
						  0,
						  pointer,
						  I2C_THREAD_STACK_SIZE,
						  NX_APP_THREAD_PRIORITY+2,
						  NX_APP_THREAD_PRIORITY+2,
						  TX_NO_TIME_SLICE,
						  TX_AUTO_START);
	/* Check if thread creation was successful */
	if (ret != TX_SUCCESS)
	{
	  return TX_THREAD_ERROR;
	}
  /* USER CODE END MX_NetXDuo_Init */

  return ret;
}

/**
* @brief  ip address change callback.
* @param ip_instance: NX_IP instance
* @param ptr: user data
* @retval none
*/
static VOID ip_address_change_notify_callback(NX_IP *ip_instance, VOID *ptr)
{
  /* USER CODE BEGIN ip_address_change_notify_callback */
    nx_ip_address_get(ip_instance, &dhcp_alloc_ip, &dhcp_alloc_mask);

    printf("IP CHANGED: %lu.%lu.%lu.%lu\n",
           (dhcp_alloc_ip>>24)&0xFF, (dhcp_alloc_ip>>16)&0xFF, (dhcp_alloc_ip>>8)&0xFF, dhcp_alloc_ip&0xFF);

    /* relaunch TCP server*/
//     restart_tcp_server();
    tx_event_flags_set(&IpEventFlags, IP_CHANGED_EVENT, TX_OR);
  /* USER CODE END ip_address_change_notify_callback */
}
void print_mac(uint8_t *mac)
{
    printf("\nMAC Address: %02X:%02X:%02X:%02X:%02X:%02X\r\n",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void boot_time_fw_info(){

	extern ETH_HandleTypeDef heth;
	print_mac(heth.Init.MACAddr);

	printf("Static IP: %lu.%lu.%lu.%lu\n",
			 (NX_APP_DEFAULT_IP_ADDRESS>>24)&0xFF, (NX_APP_DEFAULT_IP_ADDRESS>>16)&0xFF, (NX_APP_DEFAULT_IP_ADDRESS>>8)&0xFF, NX_APP_DEFAULT_IP_ADDRESS&0xFF);
	grl_print_version();
	grl_print_sp_rh_1();
//  dump_packet_pool(&NxAppPool);

}
#if 0
/**
* @brief  Main thread entry.
* @param thread_input: ULONG user argument used by the thread entry
* @retval none
*/
static VOID nx_app_thread_entry (ULONG thread_input)
{
  /* USER CODE BEGIN Nx_App_Thread_Entry 0 */

  /* USER CODE END Nx_App_Thread_Entry 0 */

  UINT ret = NX_SUCCESS;

  /* USER CODE BEGIN Nx_App_Thread_Entry 1 */

//  dump_packet_pool(&NxAppPool);
  /* USER CODE END Nx_App_Thread_Entry 1 */

  /* register the IP address change callback */
  ret = nx_ip_address_change_notify(&NetXDuoEthIpInstance, ip_address_change_notify_callback, NULL);
  if (ret != NX_SUCCESS)
  {
    /* USER CODE BEGIN IP address change callback error */

    /* USER CODE END IP address change callback error */
  }

  /* start the DHCP client */
  ret = nx_dhcp_start(&DHCPClient);
  if (ret != NX_SUCCESS)
  {
    /* USER CODE BEGIN DHCP client start error */

    /* USER CODE END DHCP client start error */
  }
  printf("Looking for DHCP server, till then work with static..\r\n");
  /* wait until an IP address is ready */
//  if(tx_semaphore_get(&DHCPSemaphore, TX_16_ULONG) != TX_SUCCESS)
//  {
    /* USER CODE BEGIN DHCPSemaphore get error */

    /* USER CODE END DHCPSemaphore get error */
//  }

  /* USER CODE BEGIN Nx_App_Thread_Entry 2 */

  /* USER CODE END Nx_App_Thread_Entry 2 */

}
#endif
/* USER CODE BEGIN 2 */


//#include "../../I-CUBE-Mongoose/App/mongoose_glue.h"
void grl_mg_init(){
	ULONG ip_status;

	// Wait for IP address (if DHCP)
	nx_ip_status_check(&NetXDuoEthIpInstance, NX_IP_ADDRESS_RESOLVED,
				   	   	   &ip_status, TX_WAIT_FOREVER);
	// Or link up
	nx_ip_status_check(&NetXDuoEthIpInstance, NX_IP_LINK_ENABLED,
				   	   	   &ip_status, TX_WAIT_FOREVER);
	mongoose_init();
}
#if 0
static void mg_app_thread_entry(ULONG thread_input){
	grl_mg_init();
//	mongoose_init();
	while (1) {
		mongoose_poll();
		tx_thread_sleep(1);     // yield to other threads
	}

}
#endif
void print_dhcp_times(void)
{
//    UCHAR option_buf[4];
//    UINT  option_size;
    UCHAR option_buf[16];
    UINT option_size = sizeof(option_buf);
    UINT  status;
    ULONG lease_time;
    ULONG t1_time;
    ULONG t2_time;

    /* Make sure DHCP is bound before calling this (state == NX_DHCP_STATE_BOUND). */

    /* 1) Lease time (Option 51) */
    status = nx_dhcp_user_option_retrieve(&DHCPClient,
                                          NX_DHCP_OPTION_DHCP_LEASE,
                                          option_buf,
                                          &option_size);

    if (status == NX_SUCCESS && option_size == 4)
    {
        lease_time = nx_dhcp_user_option_convert(option_buf);
        printf("DHCP lease time : %lu seconds\n", lease_time);
    }
    else
    {
        printf("Failed to get lease time, status=0x%X\n", status);
    }

    /* 2) Renewal time T1 */
    status = nx_dhcp_user_option_retrieve(&DHCPClient,
                                          NX_DHCP_OPTION_RENEWAL,
                                          option_buf,
                                          &option_size);

    if (status == NX_SUCCESS && option_size == 4)
    {
        t1_time = nx_dhcp_user_option_convert(option_buf);
        printf("DHCP T1 (renew) : %lu seconds\n", t1_time);
    }

    /* 3) Rebind time T2 */
    status = nx_dhcp_user_option_retrieve(&DHCPClient,
                                          NX_DHCP_OPTION_REBIND,
                                          option_buf,
                                          &option_size);

    if (status == NX_SUCCESS && option_size == 4)
    {
        t2_time = nx_dhcp_user_option_convert(option_buf);
        printf("DHCP T2 (rebind): %lu seconds\n", t2_time);
    }
}

VOID tx_stack_error_handler(TX_THREAD *thread_ptr)
{
//	printf("stack overflow\n");
    printf("STACK OVERFLOW: %s\n", thread_ptr->tx_thread_name);
    // Put a breakpoint here or blink LED / print thread name
}

/**
* @brief  TCP listen call back
* @param socket_ptr: NX_TCP_SOCKET socket registered for the callback
* @param port: UINT  the port on which the socket is listening
* @retval none
*/
//static VOID tcp_listen_callback(NX_TCP_SOCKET *socket_ptr, UINT port)
//{
//  tx_semaphore_put(&TCPSemaphore);
//}
static VOID on_disconnect(NX_TCP_SOCKET *socket_ptr)
{
    if (!socket_ptr) {
    	printf("Disc_no socket\r\n");
    	return;
    }

//    nx_tcp_socket_disconnect(socket_ptr, NX_IP_PERIODIC_RATE);
//    nx_tcp_server_socket_unaccept(socket_ptr);
    tx_event_flags_set(&IpEventFlags, TCP_DISCONNECT_EVENT, TX_OR);
    printf("conn disconnected\r\n");
//    nx_tcp_socket_delete(socket_ptr);
}

void dump_packet_pool(NX_PACKET_POOL *pool)
{
#ifdef LOG_ETH_RX_TX
    if (pool == NX_NULL)
    {
        printf("POOL: null pointer\r\n");
        return;
    }

    printf("Total packets  : %lu\r\n", pool->nx_packet_pool_total);
    printf("Available      : %lu\r\n", pool->nx_packet_pool_available);
    printf("Pool size (B)  : %lu\r\n", pool->nx_packet_pool_size);
    printf("Payload size   : %lu\r\n", pool->nx_packet_pool_payload_size);

    printf("Empty requests : %lu\r\n", pool->nx_packet_pool_empty_requests);
    printf("Empty suspends : %lu\r\n", pool->nx_packet_pool_empty_suspensions);
    printf("Invalid rels   : %lu\r\n", pool->nx_packet_pool_invalid_releases);

#ifdef NX_ENABLE_LOW_WATERMARK
    printf("Low watermark  : %u\r\n", pool->nx_packet_pool_low_watermark);
#endif

    printf("===========================\r\n");
#endif
}


void log_send_event(const char *tag, ULONG seq)
{
#ifdef LOG_ETH_RX_TX
    ULONG t = tx_time_get();
    printf("[%lu] %s seq=%lu\n", t, tag, seq);
#endif
}
/**
* @brief  TCP server thread entry
* @param thread_input: ULONG thread parameter
* @retval none
*/

static VOID App_TCP_Thread_Entry(ULONG thread_input)
{
    UINT status;
    NX_PACKET *data_packet;
    ULONG bytes_read;
//    uint16_t Data_length = TCP_CHUNK_SIZE;
//    UINT ret = NX_SUCCESS;

    boot_time_fw_info();

    ULONG ip_status;
    UINT udp_status;

    /**
     * @brief Wait until Ethernet link is enabled.
     *
     * This ensures the network interface is operational before attempting
     * to create and bind the UDP streaming socket.
     */
//    nx_ip_status_check(&NetXDuoEthIpInstance,
//                       NX_IP_LINK_ENABLED,
//                       &ip_status,
//                       TX_WAIT_FOREVER);

    /**
     * @brief Wait until the IP layer is resolved.
     *
     * This is useful for both static-IP and DHCP-based configurations,
     * because it guarantees that the IP instance is in a ready state.
     */
//    nx_ip_status_check(&NetXDuoEthIpInstance,
//                       NX_IP_ADDRESS_RESOLVED,
//                       &ip_status,
//                       TX_WAIT_FOREVER);

    /**
     * @brief Create and bind the UDP socket used for FSK online streaming.
     *
     * This must be done from a running ThreadX thread context, not from
     * MX_NetXDuo_Init(), otherwise NetX may return NX_CALLER_ERROR.
     */
    udp_status = FSK_Stream_UDP_Socket_Create();
    if (udp_status != NX_SUCCESS)
    {
        printf("FSK UDP socket setup failed: 0x%X\r\n", udp_status);
    }
    else
    {
        g_fsk_stream_udp_ready = 1U;
    }

    /* register the IP address change callback */
    nx_ip_address_change_notify(&NetXDuoEthIpInstance, ip_address_change_notify_callback, NULL);

    /* start the DHCP client */
//    nx_dhcp_start(&DHCPClient);

//    grl_mg_init();

    printf("Looking for DHCP server, till then work with static..\r\n");

    /* Create the TCP socket */
    status = nx_tcp_socket_create(&NetXDuoEthIpInstance, &TCPSocket,
                                  "TCP Server Socket",
                                  NX_IP_NORMAL, NX_FRAGMENT_OKAY,
                                  NX_IP_TIME_TO_LIVE, WINDOW_SIZE,
                                  NX_NULL, on_disconnect);
    if (status != NX_SUCCESS)
    {
        Error_Handler();
    }

    /* Start listening (no callback for now) */
    status = nx_tcp_server_socket_listen(&NetXDuoEthIpInstance,
                                         GRL_TCP_PORT,
                                         &TCPSocket,
                                         MAX_TCP_CLIENTS,
                                         NX_NULL);
    if (status != NX_SUCCESS)
    {
        Error_Handler();
    }

    printf("TCP Server listening on PORT %d ..\n", GRL_TCP_PORT);
	static ULONG loop_counter = 0;

    while (1)
    {
//    	static ULONG heart = 0;
//    	printf("TCP loop %lu\n", heart++);
//    	tx_thread_sleep(10);
    	if ((loop_counter++ % 5) == 0)
		{
			dump_packet_pool(&NxAppPool);
		}


        ULONG flags;

        /* Handle IP change event */
        if (tx_event_flags_get(&IpEventFlags, IP_CHANGED_EVENT | TCP_DISCONNECT_EVENT,
                               TX_OR_CLEAR, &flags, TX_NO_WAIT) == TX_SUCCESS)
        {
            printf("Restarting TCP server due to IP change / disc ...\n");
            print_dhcp_times();

            nx_tcp_socket_disconnect(&TCPSocket, NX_IP_PERIODIC_RATE);
            nx_tcp_server_socket_unaccept(&TCPSocket);
            nx_tcp_server_socket_relisten(&NetXDuoEthIpInstance, GRL_TCP_PORT, &TCPSocket);
            /* Go accept a new connection */
        }

//        /* Handle disconnect event from callback */
//        if (tx_event_flags_get(&IpEventFlags, TCP_DISCONNECT_EVENT,
//                               TX_OR_CLEAR, &flags, TX_NO_WAIT) == TX_SUCCESS)
//        {
//            printf("TCP: disconnect event\n");
//            nx_tcp_socket_disconnect(&TCPSocket, NX_IP_PERIODIC_RATE);
//            nx_tcp_server_socket_unaccept(&TCPSocket);
//            nx_tcp_server_socket_relisten(&NetXDuoEthIpInstance, GRL_TCP_PORT, &TCPSocket);
//            /* Go accept a new connection */
//        }

        /* 1) Block until a client connects */
        status = nx_tcp_server_socket_accept(&TCPSocket, NX_WAIT_FOREVER );
        if (status != NX_SUCCESS)
        {
            printf("accept failed: 0x%X\n", status);
            /* On failure, re-listen and try again */
            nx_tcp_server_socket_relisten(&NetXDuoEthIpInstance, GRL_TCP_PORT, &TCPSocket);
            continue;
        }

        printf("Client connected\n");

        /**
         * Capture the IP address of the currently connected TCP client.
         *
         * The UDP stream thread will use this address as the destination
         * for outgoing debug stream packets.
         *
         * We intentionally force the destination UDP port to the fixed
         * debug receiver port rather than reusing the TCP peer port.
         */
        nx_tcp_socket_peer_info_get(&TCPSocket,
                                    (ULONG *)&g_fsk_stream_host_ip,
                                    (UINT *)&g_fsk_stream_host_port);

        g_fsk_stream_host_port = FSK_STREAM_UDP_PORT;

        printf("FSK stream target: %lu.%lu.%lu.%lu:%u\r\n",
               (g_fsk_stream_host_ip >> 24) & 0xFF,
               (g_fsk_stream_host_ip >> 16) & 0xFF,
               (g_fsk_stream_host_ip >> 8) & 0xFF,
               g_fsk_stream_host_ip & 0xFF,
               g_fsk_stream_host_port);

        /* 2) Serve this connection until error/close */
        while (1)
        {
//        	ULONG flags;

            TX_MEMSET(data_buffer, 0, sizeof(data_buffer));

            status = nx_tcp_socket_receive(&TCPSocket, &data_packet, NX_WAIT_FOREVER);
            if (status != NX_SUCCESS)
            {
                printf("recv error: 0x%X\n", status);
                if (data_packet)
                {
                    nx_packet_release(data_packet);
                }
                break;  /* break inner loop, go clean up and relisten */
            }

            /* Retrieve payload */
            nx_packet_data_retrieve(data_packet, data_buffer, &bytes_read);

            api_enqueue((uint8_t *)data_buffer,bytes_read);

            tx_event_flags_set(&ApiEventFlags, API_RX_EVT, TX_OR);
            extern volatile uint8_t api_rx;
            api_rx = 1;

            nx_packet_release(data_packet);

#ifdef ECHOBACK_IMMD
            /* Prepare 1KB response */
            init_tx_buffer(TCP_CHUNK_SIZE);

			status = nx_packet_allocate(&NxAppPool, &txp, NX_TCP_PACKET, NX_WAIT_FOREVER);
			if (status != NX_SUCCESS)
			{
				printf("TX alloc failed: 0x%X\n", status);
				break;
			}

			status = nx_packet_data_append(txp, tx_buffer, Data_length, &NxAppPool, NX_WAIT_FOREVER);
			if (status != NX_SUCCESS)
			{
				nx_packet_release(txp);
				printf("TX append failed: 0x%X\n", status);
				break;
			}

			status = nx_tcp_socket_send(&TCPSocket, txp, NX_IP_PERIODIC_RATE);
			if (status != NX_SUCCESS)
			{
				nx_packet_release(txp);
				printf("TX send failed: 0x%X\n", status);

				break;
			}
#endif /*ECHOBACK_IMMD*/
        }
        /* 3) Clean close & relisten for next client */
        nx_tcp_socket_disconnect(&TCPSocket, NX_IP_PERIODIC_RATE);
        nx_tcp_server_socket_unaccept(&TCPSocket);
        nx_tcp_server_socket_relisten(&NetXDuoEthIpInstance, GRL_TCP_PORT, &TCPSocket);

        /**
         * Stop UDP streaming until a new TCP client connects and provides
         * a fresh destination IP address.
         */
//        g_fsk_stream_host_ip = 0U;

        printf("Connection closed, relistening...\n");
    }
}


/**
 * @brief Snapshot the current NetX packet pool health.
 *
 * This helper reads the current packet-pool state from NetX and stores
 * selected values into debug variables that can be watched in the debugger.
 *
 * Useful fields:
 * - total packets in the pool
 * - currently free packets
 * - number of times the pool was requested while empty
 */
static void FSK_Stream_UpdatePoolStats(void)
{
    ULONG total_packets = 0U;
    ULONG free_packets = 0U;
    ULONG empty_pool_requests = 0U;
    ULONG empty_pool_suspensions = 0U;
    ULONG invalid_packet_releases = 0U;
    UINT status;

    status = nx_packet_pool_info_get(&NxAppPool,
                                     &total_packets,
                                     &free_packets,
                                     &empty_pool_requests,
                                     &empty_pool_suspensions,
                                     &invalid_packet_releases);

    if (status == NX_SUCCESS)
    {
        g_udp_pool_available = free_packets;
        g_udp_pool_empty_requests = empty_pool_requests;

        if (free_packets < g_udp_pool_lowest_available)
        {
            g_udp_pool_lowest_available = free_packets;
        }
    }
}

/**
 * @brief Snapshot shared-stream ring counters for debugger inspection.
 *
 * These values help distinguish:
 * - CM4 producer overrun in the shared ring
 * - healthy ring transport with loss happening later in CM7/networking
 */
static void FSK_Stream_UpdateRingStats(void)
{
    g_stream_ring_used       = ipc_stream_used();
    g_stream_ring_high_water = g_ipc_stream_cm4_to_cm7.high_water;
    g_stream_ring_dropped    = g_ipc_stream_cm4_to_cm7.dropped;
}

/**
 * @brief Snapshot NetX IP-layer transmit statistics.
 *
 * This lets us tell whether UDP packets are being dropped inside the NetX/IP
 * path after nx_udp_socket_send() has accepted them.
 */
static void FSK_Stream_UpdateIpStats(void)
{
    ULONG total_packets_sent = 0U;
    ULONG total_bytes_sent = 0U;
    ULONG total_packets_received = 0U;
    ULONG total_bytes_received = 0U;
    ULONG invalid_packets = 0U;
    ULONG receive_packets_dropped = 0U;
    ULONG receive_checksum_errors = 0U;
    ULONG send_packets_dropped = 0U;
    ULONG total_fragments_sent = 0U;
    ULONG total_fragments_received = 0U;
    UINT status;

    status = nx_ip_info_get(&NetXDuoEthIpInstance,
                            &total_packets_sent,
                            &total_bytes_sent,
                            &total_packets_received,
                            &total_bytes_received,
                            &invalid_packets,
                            &receive_packets_dropped,
                            &receive_checksum_errors,
                            &send_packets_dropped,
                            &total_fragments_sent,
                            &total_fragments_received);

    if (status == NX_SUCCESS)
    {
        g_udp_ip_total_packets_sent = total_packets_sent;
        g_udp_ip_total_bytes_sent = total_bytes_sent;
        g_udp_ip_send_packets_dropped = send_packets_dropped;
    }

    g_udp_ip_transmit_resource_errors =
        NetXDuoEthIpInstance.nx_ip_transmit_resource_errors;
    g_udp_ip_total_packet_send_requests =
        NetXDuoEthIpInstance.nx_ip_total_packet_send_requests;
}

/**
 * @brief Dedicated CM7 thread that forwards CM4 online stream data via UDP.
 *
 * This thread continuously:
 * 1. checks whether a valid host IP address is known
 * 2. drains up to one MTU-safe packet worth of bytes from the CM4->CM7 ring
 * 3. allocates a NetX UDP packet
 * 4. appends the drained payload
 * 5. sends the packet to the host PC
 *
 * Design rationale:
 * - The TCP thread remains dedicated to command/control traffic.
 * - The UDP stream remains independent of blocking TCP receive calls.
 * - The already-created cm7_stream_drain module is used exactly as intended.
 *
 * @param[in] thread_input Unused.
 */
static VOID FSK_Stream_Thread_Entry(ULONG thread_input)
{
    UINT status;
    NX_PACKET *packet;
    fsk_udp_header_t *hdr;
    uint32_t drained_bytes;
    uint32_t body_bytes = 0U;
    uint32_t record_count;
    uint32_t seq_to_send;
    ULONG flush_deadline = 0U;
    ULONG now_tick = 0U;
    ULONG last_stats_print_tick = 0U;

    (void)thread_input;

    /**
     * @brief Initialize the CM7 stream drain helper.
     */
    cm7_stream_drain_init();

    while (1)
    {
        /**
         * @brief Wait until the UDP streaming socket is ready.
         */
        if (g_fsk_stream_udp_ready == 0U)
        {
            tx_thread_sleep(10);
            continue;
        }

        /**
         * @brief Wait until a valid host IP has been learned from the TCP side.
         */
        if (g_fsk_stream_host_ip == 0U)
        {
            tx_thread_sleep(10);
            continue;
        }

        /**
         * @brief Try to pull more bytes from the CM4->CM7 ring into the
         *        local transmit buffer after the packet header.
         *
         * Only the remaining free body space is offered to the drain helper.
         */
        drained_bytes = cm7_stream_drain_fill_packet(
            &g_fsk_udp_tx_buffer[sizeof(fsk_udp_header_t) + body_bytes],
            FSK_UDP_TARGET_BODY_BYTES - body_bytes
        );

        /**
         * @brief If new bytes were added and this is the first data in the
         *        packet under construction, start a flush deadline timer.
         */
        if ((drained_bytes > 0U) && (body_bytes == 0U))
        {
            flush_deadline = tx_time_get() + FSK_UDP_FLUSH_TICKS;
        }

        body_bytes += drained_bytes;

        /**
         * @brief Ensure body length remains a multiple of the record size.
         *
         * This is mainly defensive. In the current design CM4 always writes
         * full 8-byte records.
         */
        body_bytes = (body_bytes / FSK_UDP_RECORD_SIZE) * FSK_UDP_RECORD_SIZE;

        /**
         * @brief Decide whether the packet should be sent now.
         *
         * Conditions:
         * 1. body reached target size
         * 2. body is non-empty and flush timeout expired
         */
        if ((body_bytes < FSK_UDP_TARGET_BODY_BYTES) &&
            ((body_bytes == 0U) || ((LONG)(tx_time_get() - flush_deadline) < 0)))
        {
            /**
             * Packet not full yet and timeout not reached.
             * Sleep briefly and continue accumulating.
             */
            tx_thread_sleep(1);
            continue;
        }

        /**
         * @brief Nothing to send.
         */
        if (body_bytes == 0U)
        {
            tx_thread_sleep(1);
            continue;
        }

        /**
         * @brief Compute number of records carried in this packet.
         */
        record_count = body_bytes / FSK_UDP_RECORD_SIZE;

        /**
         * @brief Assign sequence number for the packet being attempted.
         *
         * This sequence number is only committed globally after successful send.
         * This avoids false sequence gaps in Wireshark caused by local failures.
         */
        seq_to_send = g_fsk_udp_sequence;
        g_udp_attempt_count++;

        hdr = (fsk_udp_header_t *)g_fsk_udp_tx_buffer;
        hdr->magic        = FSK_UDP_MAGIC;
        hdr->version      = FSK_UDP_VERSION;
        hdr->header_size  = (uint16_t)sizeof(fsk_udp_header_t);
        hdr->sequence     = seq_to_send;
        hdr->record_count = (uint16_t)record_count;
        hdr->record_size  = (uint16_t)FSK_UDP_RECORD_SIZE;

        /**
         * @brief Update packet-pool stats before attempting allocation.
         */
        FSK_Stream_UpdatePoolStats();
        FSK_Stream_UpdateRingStats();
        FSK_Stream_UpdateIpStats();

        /**
         * @brief Allocate a NetX UDP packet.
         */
        status = nx_packet_allocate(&NxAppPool,
                                    &packet,
                                    NX_UDP_PACKET,
                                    NX_NO_WAIT);

        if (status != NX_SUCCESS)
        {
            g_udp_alloc_fail_count++;
            g_udp_last_alloc_status = status;
            g_udp_last_alloc_fail_seq = seq_to_send;

            /**
             * Keep the accumulated body for retry on next loop iteration.
             */
            tx_thread_sleep(1);
            continue;
        }

        /**
         * @brief Append header + body into the UDP packet.
         */
        status = nx_packet_data_append(packet,
                                       g_fsk_udp_tx_buffer,
                                       (ULONG)(sizeof(fsk_udp_header_t) + body_bytes),
                                       &NxAppPool,
                                       NX_NO_WAIT);

        if (status != NX_SUCCESS)
        {
            g_udp_append_fail_count++;
            g_udp_last_append_status = status;
            g_udp_last_append_fail_seq = seq_to_send;

            nx_packet_release(packet);

            /**
             * Keep the accumulated body for retry on next iteration.
             */
            tx_thread_sleep(1);
            continue;
        }

        /**
         * @brief Send the UDP packet to the host PC.
         */
        status = nx_udp_socket_send(&g_fsk_stream_udp_socket,
                                    packet,
                                    g_fsk_stream_host_ip,
                                    g_fsk_stream_host_port);

        if (status != NX_SUCCESS)
        {
            g_udp_send_fail_count++;
            g_udp_last_send_status = status;
            g_udp_last_send_fail_seq = seq_to_send;

            nx_packet_release(packet);

            /**
             * Keep the accumulated body for retry on next iteration.
             */
            tx_thread_sleep(1);
            continue;
        }

        /**
         * @brief Packet sent successfully.
         */
        g_udp_sent_count++;
        g_fsk_udp_sequence++;
        FSK_Stream_UpdateIpStats();

        /**
         * @brief Packet sent successfully. Reset accumulation state for the
         *        next UDP packet.
         */
        body_bytes = 0U;
    }
}

/**
 * @brief Create and bind the UDP socket used for online FSK debug streaming.
 *
 * NetX Duo requires a UDP socket to be bound to a local port before
 * nx_udp_socket_send() can be used. Without binding, send operations fail
 * with NX_NOT_BOUND.
 *
 * @retval NX_SUCCESS on success
 * @retval NetX error code on failure
 */
static UINT FSK_Stream_UDP_Socket_Create(void)
{
    UINT status;

    /**
     * @brief Create the UDP socket object.
     */
    status = nx_udp_socket_create(&NetXDuoEthIpInstance,
                                  &g_fsk_stream_udp_socket,
                                  "FSK Stream UDP Socket",
                                  NX_IP_NORMAL,
                                  NX_FRAGMENT_OKAY,
                                  NX_IP_TIME_TO_LIVE,
                                  0U);

    if (status != NX_SUCCESS)
    {
        printf("FSK UDP socket create failed: 0x%X\r\n", status);
        return status;
    }

    /**
     * @brief Bind the UDP socket to a local source port.
     *
     * This step is mandatory before nx_udp_socket_send() is used.
     */
    status = nx_udp_socket_bind(&g_fsk_stream_udp_socket,
                                FSK_STREAM_UDP_LOCAL_PORT,
                                NX_NO_WAIT);

    if (status != NX_SUCCESS)
    {
        printf("FSK UDP socket bind failed: 0x%X\r\n", status);

        /**
         * @brief Clean up the socket if bind fails.
         */
        nx_udp_socket_delete(&g_fsk_stream_udp_socket);
        return status;
    }

    printf("FSK UDP socket created and bound successfully\r\n");
    return NX_SUCCESS;
}




/* USER CODE END 2 */
