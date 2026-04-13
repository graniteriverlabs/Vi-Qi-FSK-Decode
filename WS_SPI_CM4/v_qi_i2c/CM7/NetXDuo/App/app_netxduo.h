/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_netxduo.h
  * @author  MCD Application Team
  * @brief   NetXDuo applicative header file
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
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __APP_NETXDUO_H__
#define __APP_NETXDUO_H__

#ifdef __cplusplus
extern "C" {
#endif

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/* Includes ------------------------------------------------------------------*/
#include "nx_api.h"

/* Private includes ----------------------------------------------------------*/
#include "nx_stm32_eth_driver.h"

/* USER CODE BEGIN Includes */
#include <qi_defs.h>
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */
/* The DEFAULT_PAYLOAD_SIZE should match with RxBuffLen configured via MX_ETH_Init */
#ifndef DEFAULT_PAYLOAD_SIZE
#define DEFAULT_PAYLOAD_SIZE      1536//(TCP_CHUNK_SIZE*2)
#endif

#ifndef DEFAULT_ARP_CACHE_SIZE
#define DEFAULT_ARP_CACHE_SIZE    (1024)
#endif

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
UINT MX_NetXDuo_Init(VOID *memory_ptr);

/* USER CODE BEGIN EFP */
//#define LOG_ETH_RX_TX	1
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define GATEWAY_ADDR								IP_ADDRESS(192,168,255,1)     // your LAN gateway

#if 1
#define NX_IP_THREAD_STACK_SIZE				(4*1024)
#define APP_TCP_THREAD_STACK_SIZE			(4*1024)
#define NX_THREAD_STACK_SIZE				(4*1024)
//#define AUTH_THREAD_STACK_SIZE				(16*1024)
#define BSD_APP_THREAD_STACK_SIZE           (4*1024)
//#define MG_APP_THREAD_STACK_SIZE            (23*1024)
#define NX_APP_NUM_PACKETS         					64u      // tune: 50, 64, 80...
#define API_THREAD_STACK_SIZE				(2*1024)
#define I2C_THREAD_STACK_SIZE				(2*1024)

#else

#define NX_IP_THREAD_STACK_SIZE				(9*1024)
#define APP_TCP_THREAD_STACK_SIZE			(9*1024)
#define NX_THREAD_STACK_SIZE				(6*1024)
#define AUTH_THREAD_STACK_SIZE				(8*1024)
#define BSD_APP_THREAD_STACK_SIZE           (4*1024)
#define MG_APP_THREAD_STACK_SIZE            (8*1024)
#define NX_APP_NUM_PACKETS         					32u      // tune: 50, 64, 80...

#endif


#define BSD_APP_THREAD_PRIORITY					11

/* USER CODE END PD */

#define NX_APP_DEFAULT_TIMEOUT               (10 * NX_IP_PERIODIC_RATE)

//#define NX_APP_PACKET_POOL_SIZE              ((DEFAULT_PAYLOAD_SIZE + sizeof(NX_PACKET)) * 50)
#define NX_APP_PACKET_POOL_SIZE    			 ((DEFAULT_PAYLOAD_SIZE + sizeof(NX_PACKET)) * NX_APP_NUM_PACKETS)


//#define NX_APP_THREAD_STACK_SIZE             (1024*2)
//#define Nx_IP_INSTANCE_THREAD_SIZE           (1024*2)

#define NX_APP_THREAD_PRIORITY               10

#ifndef NX_APP_INSTANCE_PRIORITY
#define NX_APP_INSTANCE_PRIORITY             NX_APP_THREAD_PRIORITY
#endif

#define NX_APP_DEFAULT_IP_ADDRESS                   IP_ADDRESS(192, 168, 255, 10)
#define NX_APP_DEFAULT_NET_MASK                     IP_ADDRESS(255, 255, 255, 0)

/* USER CODE BEGIN 2 */
/* USER CODE END 2 */

#ifdef __cplusplus
}
#endif
#endif /* __APP_NETXDUO_H__ */
