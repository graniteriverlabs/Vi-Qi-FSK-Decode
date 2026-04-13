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

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

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

  ret = nx_dhcp_create(&DHCPClient, &NetXDuoEthIpInstance, "DHCP Client");

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

    /* register the IP address change callback */
    nx_ip_address_change_notify(&NetXDuoEthIpInstance, ip_address_change_notify_callback, NULL);

    /* start the DHCP client */
    nx_dhcp_start(&DHCPClient);

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

        printf("Connection closed, relistening...\n");
    }
}








/* USER CODE END 2 */
