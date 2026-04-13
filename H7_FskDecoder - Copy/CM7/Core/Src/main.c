/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "app_threadx.h"
#include "main.h"
#include "dma.h"
#include "eth.h"
#include "i2c.h"
#include "rng.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "string.h"
#include "stdio.h"
#include "stm32h7xx_hal_hsem.h"
#include "stm32h7xx_hal_rcc_ex.h"
#include "ipc_shared.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#define ALIGNED_BYTES_TO_ULONGS(bytes) (((bytes) + sizeof(ULONG) - 1) / sizeof(ULONG))
/* Query required sizes at runtime (once), then size conservatively. */
enum { ECDSA_MD_MIN = 2048, SHA256_MD_MIN = 256 }; // safe floors

static UCHAR device_challenge[]= {0x41, 0xC7, 0xF4, 0x30, 0xD8, 0x43, 0xEB, 0x4A, 0x6F, 0xDA, 0x84, 0x1D, 0xB1, 0x3E, 0x20, 0xEF, 0x78, 0xD8, 0xB0, 0xA6, 0x67, 0x6F, 0xF3, 0x6A, 0xFB, 0xD9, 0x39, 0xC2, 0xF0, 0xC3, 0xCF, 0xF6, 0xB3, 0x1B, 0x00, 0x00, 0x77, 0x32, 0x3E, 0x5C, 0xF8, 0xA3, 0x55, 0x59, 0x3E, 0xC8, 0xCF, 0xA7, 0x3A, 0x5B, 0xE9, 0x13, 0x11, 0xB3};        /* message that was signed */
static UINT  device_challenge_len = sizeof(device_challenge);

static UCHAR device_sig_ptr[]={ 0xD1, 0xAD, 0x1B, 0xCD, 0x53, 0x3F, 0xE1, 0x52,
								0xDD, 0x9F, 0xC7, 0x13, 0xF3, 0x78, 0xA6, 0x33,
								0xD9, 0x80, 0x5D, 0x78, 0xE5, 0x14, 0x4F, 0x69,
								0xF2, 0xE7, 0xAA, 0xFB, 0xA0, 0x35, 0xB3, 0xC3,
								0xDA, 0x74, 0xFD, 0x33, 0x98, 0xE9, 0xA2, 0x9B,
								0x28, 0x89, 0xC9, 0x64, 0x97, 0x99, 0x95, 0x96,
								0x5F, 0x1B, 0x0E, 0x2D, 0xFE, 0x34, 0x0A, 0xA3,
								0xD3, 0x6B, 0x3F, 0xF2, 0x8D, 0x40, 0xF2, 0x65
							  };          /* bytes received from device */
static UINT  device_sig_len = sizeof(device_sig_ptr);
static UINT  device_sig_is_der;         /* 1 = ASN.1 DER SEQUENCE, 0 = raw r||s */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* DUAL_CORE_BOOT_SYNC_SEQUENCE: Define for dual core boot synchronization    */
/*                             demonstration code based on hardware semaphore */
/* This define is present in both CM7/CM4 projects                            */
/* To comment when developping/debugging on a single core                     */
#define DUAL_CORE_BOOT_SYNC_SEQUENCE

#if defined(DUAL_CORE_BOOT_SYNC_SEQUENCE)
#ifndef HSEM_ID_0
#define HSEM_ID_0 (0U) /* HW semaphore 0*/
#endif
#endif /* DUAL_CORE_BOOT_SYNC_SEQUENCE */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
/* USER CODE BEGIN PFP */
static UINT grl_build_der_from_rs64(const UCHAR *rs64, UCHAR *der, UINT *der_len_out);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
extern uint8_t rx_byte;

int _write(int file, char *ptr, int len)
{
	int i=0;
	for(i=0; i<len; i++){
		ITM_SendChar((*ptr++));
	}
	HAL_UART_Transmit(&huart3, (uint8_t*)ptr - len, len, HAL_MAX_DELAY);
	return len;
}

void grl_print_sp_rh_1(){

	uint32_t cur = FLASH->OPTSR_CUR;
	uint32_t prg = FLASH->OPTSR_PRG;
	printf("OPTSR_CUR=0x%08lx OPTSR_PRG=0x%08lx\n", cur, prg);

	printf("OPTSR_CUR=0x%08lx SWAP=%lu\n",
	       cur, (cur & FLASH_OPTSR_SWAP_BANK_OPT) ? 1UL : 0UL);

//	uint32_t cur = FLASH->OPTSR_CUR;
	int active = (cur & FLASH_OPTSR_SWAP_BANK_OPT) ? 2 : 1;
	printf("Active boot bank = %d (SWAP=%d)\n", active, active==2);

	uint32_t b1_sp = *(uint32_t*)0x08000000, b1_rh = *(uint32_t*)0x08000004;
	uint32_t b2_sp = *(uint32_t*)0x08100000, b2_rh = *(uint32_t*)0x08100004;

	printf("B1 SP=%08lx RH=%08lx | B2 SP=%08lx RH=%08lx\n", b1_sp,b1_rh,b2_sp,b2_rh);

}
void grl_print_version(){
  printf("CM7 FW V: %x.%x.%x\r\n",FW_MAJOR_VERSION,FW_MINOR_VERSION,FW_SUB_MINOR_VERSION);
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */
/* USER CODE BEGIN Boot_Mode_Sequence_0 */
#if defined(DUAL_CORE_BOOT_SYNC_SEQUENCE)
  int32_t timeout;
#endif /* DUAL_CORE_BOOT_SYNC_SEQUENCE */
/* USER CODE END Boot_Mode_Sequence_0 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* Enable the CPU Cache */

  /* Enable I-Cache---------------------------------------------------------*/
  SCB_EnableICache();

  /* Enable D-Cache---------------------------------------------------------*/
  SCB_EnableDCache();

/* USER CODE BEGIN Boot_Mode_Sequence_1 */
#if defined(DUAL_CORE_BOOT_SYNC_SEQUENCE)
  /* Wait until CPU2 boots and enters in stop mode or timeout*/
  timeout = 0xFFFF;
  while((__HAL_RCC_GET_FLAG(RCC_FLAG_D2CKRDY) != RESET) && (timeout-- > 0));
  if ( timeout < 0 )
  {
  Error_Handler();
  }
#endif /* DUAL_CORE_BOOT_SYNC_SEQUENCE */
/* USER CODE END Boot_Mode_Sequence_1 */
  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();
/* USER CODE BEGIN Boot_Mode_Sequence_2 */
#if defined(DUAL_CORE_BOOT_SYNC_SEQUENCE)
/* When system initialization is finished, Cortex-M7 will release Cortex-M4 by means of
HSEM notification */
/*HW semaphore Clock enable*/
__HAL_RCC_HSEM_CLK_ENABLE();
/*Take HSEM */
HAL_HSEM_FastTake(HSEM_ID_0);
/*Release HSEM in order to notify the CPU2(CM4)*/
HAL_HSEM_Release(HSEM_ID_0,0);
/* wait until CPU2 wakes up from stop mode */
timeout = 0xFFFF;
while((__HAL_RCC_GET_FLAG(RCC_FLAG_D2CKRDY) == RESET) && (timeout-- > 0));
if ( timeout < 0 )
{
Error_Handler();
}
HAL_HSEM_ActivateNotification(__HAL_HSEM_SEMID_TO_MASK(HSEM_CM4_TO_CM7));

HAL_NVIC_SetPriority(HSEM1_IRQn, 12, 0);
HAL_NVIC_EnableIRQ(HSEM1_IRQn);


#endif /* DUAL_CORE_BOOT_SYNC_SEQUENCE */
/* USER CODE END Boot_Mode_Sequence_2 */

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_DMA_Init();
  MX_GPIO_Init();
  MX_UART4_Init();
  MX_RNG_Init();
  MX_ETH_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */
  MX_USART3_UART_Init();
  /* USER CODE END 2 */

  MX_ThreadX_Init();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_DIRECT_SMPS_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48|RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 50;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 5;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  HAL_RCC_MCOConfig(RCC_MCO1, RCC_MCO1SOURCE_HSI, RCC_MCODIV_1);
}

/* USER CODE BEGIN 4 */
static UINT grl_build_der_from_rs64(const UCHAR *rs64, UCHAR *der, UINT *der_len_out)
{
    /* r and s are 32-byte big-endian each */
    const UCHAR *r = rs64;
    const UCHAR *s = rs64 + 32;
    UCHAR tmp[2 + 33 + 2 + 33]; /* INT r (up to 33 with pad) + INT s (up to 33) + tags/len */
    UINT ro = 0, so = 0, rlen = 32, slen = 32;

    /* strip leading zeros */
    while (rlen > 1 && *r == 0x00) { r++; rlen--; }
    while (slen > 1 && *s == 0x00) { s++; slen--; }

    /* INTEGER tag */
    tmp[ro++] = 0x02;
    /* r needs a leading 0x00 if MSB set */
    if (r[0] & 0x80) { tmp[ro++] = (UCHAR)(rlen + 1); tmp[ro++] = 0x00; }
    else             { tmp[ro++] = (UCHAR) rlen; }
    NX_SECURE_MEMCPY(&tmp[ro], r, rlen); ro += rlen;

    /* INTEGER tag for s */
    tmp[ro++] = 0x02;
    if (s[0] & 0x80) { tmp[ro++] = (UCHAR)(slen + 1); tmp[ro++] = 0x00; }
    else             { tmp[ro++] = (UCHAR) slen; }
    NX_SECURE_MEMCPY(&tmp[so = ro], s, slen); ro += slen;

    /* Wrap as SEQUENCE */
    der[0] = 0x30;
    der[1] = (UCHAR)ro;
    NX_SECURE_MEMCPY(&der[2], tmp, ro);
    *der_len_out = ro + 2;
    return NX_CRYPTO_SUCCESS;
}
//#define TLS_PRINTS	1
VOID Auth_Thread_Entry(ULONG thread_input)
{
    UINT status;
//    printf("Auth_Thread_Entry start\n");
#if TLS_PRINTS
    printf("\n");
    printf("=========================================================\n");
    printf("  STM32H755 AUTHENTICATION SYSTEM\n");
    printf("  Certificate Chain Verification + Challenge-Response\n");
    printf("=========================================================\n\n");

    /* ========================================================================
     * PART 1: CERTIFICATE CHAIN VERIFICATION
     * ======================================================================== */

    printf("PART 1: CERTIFICATE CHAIN VERIFICATION\n");
    printf("---------------------------------------\n");
#endif
    /* Certificate objects for the chain */
    NX_SECURE_X509_CERT root_ca_certificate;
    NX_SECURE_X509_CERT intermediate_ca_certificate;
    NX_SECURE_X509_CERT product_unit_certificate;

    /* Certificate data buffers - Replace with your actual certificate data */
    /* These should be your DER-encoded certificates as byte arrays */
    UCHAR root_ca_cert_der[] = {
        /* Your Root CA certificate data in DER format */
    		0x30, 0x82, 0x01, 0x2c, 0x30, 0x81, 0xd3, 0xa0, 0x03, 0x02, 0x01, 0x02, 0x02, 0x08, 0x77, 0x61,
    		0x12, 0xb4, 0x11, 0x47, 0x9a, 0xac, 0x30, 0x0a, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04,
    		0x03, 0x02, 0x30, 0x11, 0x31, 0x0f, 0x30, 0x0d, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c, 0x06, 0x57,
    		0x50, 0x43, 0x43, 0x41, 0x31, 0x30, 0x20, 0x17, 0x0d, 0x32, 0x31, 0x30, 0x33, 0x30, 0x33, 0x31,
    		0x36, 0x30, 0x34, 0x30, 0x31, 0x5a, 0x18, 0x0f, 0x39, 0x39, 0x39, 0x39, 0x31, 0x32, 0x33, 0x31,
    		0x32, 0x33, 0x35, 0x39, 0x35, 0x39, 0x5a, 0x30, 0x11, 0x31, 0x0f, 0x30, 0x0d, 0x06, 0x03, 0x55,
    		0x04, 0x03, 0x0c, 0x06, 0x57, 0x50, 0x43, 0x43, 0x41, 0x31, 0x30, 0x59, 0x30, 0x13, 0x06, 0x07,
    		0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01,
    		0x07, 0x03, 0x42, 0x00, 0x04, 0x04, 0x84, 0x07, 0x20, 0xb3, 0xa3, 0xb1, 0x79, 0xd0, 0x6c, 0x77,
    		0xad, 0xa0, 0x00, 0x44, 0xd2, 0xcf, 0x20, 0x9b, 0xc3, 0x3c, 0xe3, 0x44, 0x9a, 0x08, 0xc6, 0x7f,
    		0x8f, 0xf5, 0x03, 0x73, 0x63, 0xa0, 0x43, 0xf5, 0x90, 0x71, 0x90, 0xaf, 0xb7, 0xbc, 0xf1, 0xfc,
    		0xa6, 0x67, 0xcb, 0x5e, 0xc3, 0xea, 0x19, 0xf9, 0x6a, 0x7b, 0x41, 0x1f, 0x3d, 0x8d, 0x63, 0x30,
    		0x48, 0x2d, 0x7e, 0x28, 0x12, 0xa3, 0x13, 0x30, 0x11, 0x30, 0x0f, 0x06, 0x03, 0x55, 0x1d, 0x13,
    		0x01, 0x01, 0xff, 0x04, 0x05, 0x30, 0x03, 0x01, 0x01, 0xff, 0x30, 0x0a, 0x06, 0x08, 0x2a, 0x86,
    		0x48, 0xce, 0x3d, 0x04, 0x03, 0x02, 0x03, 0x48, 0x00, 0x30, 0x45, 0x02, 0x20, 0x69, 0xc0, 0xb7,
    		0x15, 0xe2, 0xc0, 0x6a, 0xf6, 0x88, 0xf9, 0xb6, 0x47, 0xa3, 0x8f, 0x53, 0x67, 0xd5, 0x02, 0xa6,
    		0xf5, 0xde, 0x55, 0xb9, 0x05, 0xa1, 0x02, 0x5a, 0xbd, 0x42, 0x71, 0xf4, 0x68, 0x02, 0x21, 0x00,
    		0x9f, 0x5f, 0x4f, 0x3f, 0x76, 0x20, 0xae, 0x17, 0x74, 0xd8, 0x04, 0xbc, 0x71, 0x9a, 0xa1, 0x9b,
    		0x50, 0x55, 0xbb, 0xa2, 0xed, 0x4d, 0xe6, 0x40, 0xfd, 0x54, 0xfa, 0x39, 0x5b, 0xed, 0x5d, 0xb8
    		};
    UINT root_ca_cert_der_len = sizeof(root_ca_cert_der);

    UCHAR intermediate_ca_cert_der[] = {
        /* Your Manufacturer CA certificate in DER format */
    		0x30,0x82,0x01,0x42,0x30,0x81,0xEB,0xA0,0x03,0x02,0x01,0x02,0x02,0x08,0x4C,0x4E,0x8F,0x52,0x70,0x72,
			0x7A,0x62,0x30,0x0A,0x06,0x08,0x2A,0x86,0x48,0xCE,0x3D,0x04,0x03,0x02,0x30,0x11,0x31,0x0F,0x30,0x0D,
			0x06,0x03,0x55,0x04,0x03,0x0C,0x06,0x57,0x50,0x43,0x43,0x41,0x31,0x30,0x20,0x17,0x0D,0x32,0x33,0x31,
			0x30,0x31,0x39,0x31,0x31,0x33,0x32,0x33,0x35,0x5A,0x18,0x0F,0x39,0x39,0x39,0x39,0x31,0x32,0x33,0x31,
			0x32,0x33,0x35,0x39,0x35,0x39,0x5A,0x30,0x12,0x31,0x10,0x30,0x0E,0x06,0x03,0x55,0x04,0x03,0x0C,0x07,
			0x30,0x30,0x46,0x32,0x2D,0x30,0x31,0x30,0x59,0x30,0x13,0x06,0x07,0x2A,0x86,0x48,0xCE,0x3D,0x02,0x01,
			0x06,0x08,0x2A,0x86,0x48,0xCE,0x3D,0x03,0x01,0x07,0x03,0x42,0x00,0x04,0x8A,0xE7,0x75,0x86,0x23,0x3C,
			0x78,0xCE,0xEC,0x83,0x2C,0xC3,0x45,0xB7,0x1F,0x6D,0xB2,0xED,0x6A,0xCE,0x06,0x1B,0x41,0xD1,0xBE,0x81,
			0xD3,0xB2,0x3C,0xE6,0x5B,0x89,0xC3,0x4D,0x3C,0xF4,0xCC,0x07,0xDA,0xAD,0xBA,0x3F,0xA5,0x7C,0xEC,0xBD,
			0x4E,0x59,0x2F,0x0F,0x55,0xCC,0x9C,0xC7,0x60,0x00,0xB1,0x24,0x73,0x57,0xD9,0xDB,0xA9,0xF4,0xA3,0x2A,
			0x30,0x28,0x30,0x12,0x06,0x03,0x55,0x1D,0x13,0x01,0x01,0xFF,0x04,0x08,0x30,0x06,0x01,0x01,0xFF,0x02,
			0x01,0x00,0x30,0x12,0x06,0x05,0x67,0x81,0x14,0x01,0x01,0x01,0x01,0xFF,0x04,0x06,0x04,0x04,0x00,0x00,
			0x00,0x01,0x30,0x0A,0x06,0x08,0x2A,0x86,0x48,0xCE,0x3D,0x04,0x03,0x02,0x03,0x46,0x00,0x30,0x43,0x02,
			0x20,0x52,0x5C,0xFF,0xA9,0x4D,0x59,0x68,0xAE,0x1A,0xB1,0xC2,0x8D,0x7C,0xD1,0x1E,0x47,0xED,0x72,0x5A,
			0xF2,0x68,0xC5,0xB9,0x3A,0xD2,0xCB,0xF2,0x97,0x76,0x8F,0x7A,0x67,0x02,0x1F,0x02,0xA7,0x7D,0xE9,0xF6,
			0x3F,0xA9,0x79,0x15,0xEA,0x14,0x6A,0x84,0x91,0x0A,0x71,0x05,0xF0,0x88,0x08,0xE7,0x3E,0x1B,0xCB,0x5D,
			0xE7,0xB0,0x32,0x01,0xC8,0x84
    };
    UINT intermediate_ca_cert_der_len = sizeof(intermediate_ca_cert_der);

    UCHAR product_cert_der[] = {
        /* Your Product Unit certificate in DER format */
    		0x30,0x82,0x01,0x5A,0x30,0x82,0x01,0x00,0xA0,0x03,0x02,0x01,0x02,0x02,0x03,0x02,0x42,0x51,0x30,0x0A,
			0x06,0x08,0x2A,0x86,0x48,0xCE,0x3D,0x04,0x03,0x02,0x30,0x12,0x31,0x10,0x30,0x0E,0x06,0x03,0x55,0x04,
			0x03,0x0C,0x07,0x30,0x30,0x46,0x32,0x2D,0x30,0x31,0x30,0x1E,0x17,0x0D,0x32,0x34,0x30,0x31,0x33,0x30,
			0x31,0x36,0x30,0x30,0x30,0x30,0x5A,0x17,0x0D,0x33,0x34,0x30,0x31,0x33,0x31,0x31,0x35,0x35,0x39,0x35,
			0x39,0x5A,0x30,0x3F,0x31,0x25,0x30,0x23,0x06,0x03,0x55,0x04,0x03,0x0C,0x1C,0x30,0x32,0x30,0x33,0x39,
			0x35,0x2D,0x50,0x68,0x6F,0x6E,0x65,0x4D,0x6F,0x75,0x6E,0x74,0x77,0x69,0x74,0x68,0x4D,0x61,0x67,0x53,
			0x61,0x66,0x65,0x31,0x16,0x30,0x14,0x06,0x0A,0x09,0x92,0x26,0x89,0x93,0xF2,0x2C,0x64,0x01,0x01,0x0C,
			0x06,0x30,0x30,0x46,0x32,0x2D,0x32,0x30,0x59,0x30,0x13,0x06,0x07,0x2A,0x86,0x48,0xCE,0x3D,0x02,0x01,
			0x06,0x08,0x2A,0x86,0x48,0xCE,0x3D,0x03,0x01,0x07,0x03,0x42,0x00,0x04,0x04,0x43,0x54,0x45,0x9B,0x6B,
			0x65,0x81,0x5D,0xD8,0xAF,0x5C,0x0C,0xBF,0xAB,0x21,0x33,0x82,0x9A,0x16,0x48,0xD7,0xD7,0x70,0xFA,0xD0,
			0xB3,0x56,0x98,0x16,0x38,0xF7,0xF1,0xCD,0x11,0x09,0x3D,0xE5,0x10,0x0D,0x84,0x44,0x5B,0x0A,0xE7,0x96,
			0x1F,0xB6,0xC4,0x7B,0x73,0xAD,0xAD,0x86,0xE3,0x9C,0xBD,0x12,0x0E,0x88,0x96,0x3E,0x70,0xDF,0xA3,0x18,
			0x30,0x16,0x30,0x14,0x06,0x05,0x67,0x81,0x14,0x01,0x02,0x01,0x01,0xFF,0x04,0x08,0x00,0x00,0x00,0x00,
			0x00,0x02,0x42,0x51,0x30,0x0A,0x06,0x08,0x2A,0x86,0x48,0xCE,0x3D,0x04,0x03,0x02,0x03,0x48,0x00,0x30,
			0x45,0x02,0x21,0x00,0xB6,0x85,0xD7,0x32,0x55,0xB6,0xD2,0x1C,0x4A,0x65,0xCF,0x1C,0x0B,0x67,0xE7,0xB3,
			0x51,0x69,0xBB,0xBB,0xA6,0xEF,0xD3,0x8A,0x5A,0x6C,0x17,0x88,0x95,0x93,0x58,0x54,0x02,0x20,0x36,0x8D,
			0x3F,0x81,0x02,0x38,0xA7,0xF4,0x9D,0xDF,0x64,0xBC,0x89,0x25,0x6F,0x64,0xB7,0xB5,0x67,0x66,0xE5,0xB1,
			0xA6,0x8B,0xF7,0x6F,0x13,0x3E,0x02,0xA9,0x3B,0xDF
    };
    UINT product_cert_der_len = sizeof(product_cert_der);

    /* Certificate store for chain verification */
    NX_SECURE_X509_CERTIFICATE_STORE certificate_store;
    memset(&certificate_store, 0, sizeof(NX_SECURE_X509_CERTIFICATE_STORE));

    /* ------------------------------------------------------------------------
     * STEP 1: Initialize Root CA Certificate
     * ------------------------------------------------------------------------ */
#if TLS_PRINTS
    printf("\n[1/8] Initializing Root CA Certificate...\n");
#endif
    status = nx_secure_x509_certificate_initialize(
        &root_ca_certificate,
        root_ca_cert_der,
        (USHORT)root_ca_cert_der_len,
        NX_NULL,
        0,
        NX_NULL,
        0,
        NX_SECURE_X509_KEY_TYPE_NONE
    );

    if (status != NX_SUCCESS)
    {
        printf("  ✗ Root CA initialization FAILED: 0x%x\n", status);
        return;
    }
#if TLS_PRINTS
    printf("  ✓ Root CA initialized successfully\n");
#endif
    static ULONG root_hash_md[ ALIGNED_BYTES_TO_ULONGS( (SHA256_MD_MIN) ) ];
    static ULONG root_pub_md[ ALIGNED_BYTES_TO_ULONGS( (ECDSA_MD_MIN) ) ];

//    static UCHAR root_hash_md[512];
//    static UCHAR root_pub_md[1024];

    /* Manufacturer (intermediate) cert: */
    root_ca_certificate.nx_secure_x509_hash_metadata_area  = (UCHAR*)root_hash_md;
    root_ca_certificate.nx_secure_x509_hash_metadata_size  = sizeof(root_hash_md)*sizeof(ULONG);

    root_ca_certificate.nx_secure_x509_public_cipher_metadata_area = (UCHAR*)root_pub_md;
    root_ca_certificate.nx_secure_x509_public_cipher_metadata_size = sizeof(root_pub_md)*sizeof(ULONG);

	root_ca_certificate.nx_secure_x509_cipher_table      = _nx_crypto_x509_cipher_lookup_table_ecc;
    root_ca_certificate.nx_secure_x509_cipher_table_size = (USHORT)_nx_crypto_x509_cipher_lookup_table_ecc_size;

    /* ------------------------------------------------------------------------
     * STEP 2: Initialize Manufacturer CA Certificate
     * ------------------------------------------------------------------------ */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET); // Set PA10 HIGH to indicate start of authentication process

#if TLS_PRINTS
    printf("\n[2/8] Initializing Manufacturer CA Certificate...\n");
#endif
    status = nx_secure_x509_certificate_initialize(
        &intermediate_ca_certificate,
        intermediate_ca_cert_der,
        (USHORT)intermediate_ca_cert_der_len,
        NX_NULL,
        0,
        NX_NULL,
        0,
        NX_SECURE_X509_KEY_TYPE_NONE
    );

    if (status != NX_SUCCESS)
    {
        printf("  ✗ Manufacturer CA initialization FAILED: 0x%x\n", status);
        return;
    }

//    printf("  ✓ Manufacturer CA initialized successfully\n");

    static ULONG man_hash_md[ ALIGNED_BYTES_TO_ULONGS( (SHA256_MD_MIN) ) ];
    static ULONG man_pub_md[ ALIGNED_BYTES_TO_ULONGS( (ECDSA_MD_MIN) ) ];

//    static UCHAR root_hash_md[512];
//    static UCHAR root_pub_md[1024];

    /* Manufacturer (intermediate) cert: */
    intermediate_ca_certificate.nx_secure_x509_hash_metadata_area  = (UCHAR*)man_hash_md;
    intermediate_ca_certificate.nx_secure_x509_hash_metadata_size  = sizeof(man_hash_md)*sizeof(ULONG);

    intermediate_ca_certificate.nx_secure_x509_public_cipher_metadata_area = (UCHAR*)man_pub_md;
    intermediate_ca_certificate.nx_secure_x509_public_cipher_metadata_size = sizeof(man_pub_md)*sizeof(ULONG);

    intermediate_ca_certificate.nx_secure_x509_cipher_table      = _nx_crypto_x509_cipher_lookup_table_ecc;
    intermediate_ca_certificate.nx_secure_x509_cipher_table_size = (USHORT)_nx_crypto_x509_cipher_lookup_table_ecc_size;

    /* ------------------------------------------------------------------------
     * STEP 3: Initialize Product Unit Certificate
     * ------------------------------------------------------------------------ */
//    printf("\n[3/8] Initializing Product Unit Certificate...\n");
    status = nx_secure_x509_certificate_initialize(
        &product_unit_certificate,
        product_cert_der,
        (USHORT)product_cert_der_len,
        NX_NULL,
        0,
        NX_NULL,
        0,
        NX_SECURE_X509_KEY_TYPE_NONE
    );

    if (status != NX_SUCCESS)
    {
        printf("  ✗ Product Unit Certificate initialization FAILED: 0x%x\n", status);
        return;
    }
//    printf("  ✓ Product Unit Certificate initialized successfully\n");

    static ULONG leaf_hash_md[ ALIGNED_BYTES_TO_ULONGS( (SHA256_MD_MIN) ) ];
    static ULONG leaf_pub_md[ ALIGNED_BYTES_TO_ULONGS( (ECDSA_MD_MIN) ) ];

//    static UCHAR root_hash_md[512];
//    static UCHAR root_pub_md[1024];

    /* Manufacturer (intermediate) cert: */
    product_unit_certificate.nx_secure_x509_hash_metadata_area  = (UCHAR*)leaf_hash_md;
    product_unit_certificate.nx_secure_x509_hash_metadata_size  = sizeof(leaf_hash_md)*sizeof(ULONG);

    product_unit_certificate.nx_secure_x509_public_cipher_metadata_area = (UCHAR*)leaf_pub_md;
    product_unit_certificate.nx_secure_x509_public_cipher_metadata_size = sizeof(leaf_pub_md)*sizeof(ULONG);

    product_unit_certificate.nx_secure_x509_cipher_table      = _nx_crypto_x509_cipher_lookup_table_ecc;
    product_unit_certificate.nx_secure_x509_cipher_table_size = (USHORT)_nx_crypto_x509_cipher_lookup_table_ecc_size;


    /* ------------------------------------------------------------------------
     * STEP 4: Add Root CA to Trusted Store
     * ------------------------------------------------------------------------ */
//    printf("\n[4/8] Adding Root CA to trusted certificate store...\n");
    status = _nx_secure_x509_store_certificate_add(
        &root_ca_certificate,
        &certificate_store,
        NX_SECURE_X509_CERT_LOCATION_TRUSTED
    );

    if (status != NX_SUCCESS)
    {
        printf("  ✗ Failed to add Root CA to trusted store: 0x%x\n", status);
        return;
    }
//    printf("  ✓ Root CA added to trusted store\n");


    status = _nx_secure_x509_store_certificate_add(
        &intermediate_ca_certificate,
        &certificate_store,
        NX_SECURE_X509_CERT_LOCATION_REMOTE
    );
    if (status != NX_SUCCESS)
    {
        printf("Failed to add intermediate certificate: 0x%x", status);
        return;
    }

    // Example: 2025-01-01 00:00:00 UTC
    ULONG now_unix = 1735689600UL;	// RTC required here

#if 0

    /* Product <- Manufacturer */
    status = _nx_secure_x509_certificate_verify(&certificate_store,
                                           &product_unit_certificate,
                                           &intermediate_ca_certificate);
//    printf("Verify(prod <- man) = 0x%X\n", status);

    if (status != NX_SUCCESS)
    {
        printf("Failed to add intermediate certificate: 0x%x", status);
//        return;
    }

    /* Manufacturer <- Root */
    status = _nx_secure_x509_certificate_verify(&certificate_store,
                                           &intermediate_ca_certificate,
                                           &root_ca_certificate);
//    printf("Verify(man <- root) = 0x%X\n", status);

    if (status != NX_SUCCESS)
    {
        printf("Failed to add intermediate certificate: 0x%x", status);
//        return;
    }
#endif


    /* ------------------------------------------------------------------------
     * STEP 5: Verify Certificate Chain
     * Verifies:
     *   - Product Unit Cert signed by Manufacturer CA
     *   - Manufacturer CA signed by Root CA
     *   - All signatures valid (ECDSA with SHA-256)
     *   - Certificate validity periods
     * ------------------------------------------------------------------------ */
#if TLS_PRINTS
    printf("\n[5/8] Verifying certificate chain...\n");
    printf("  → Checking Product Unit Certificate signature...\n");
    printf("  → Checking Manufacturer CA Certificate signature...\n");
    printf("  → Validating chain to Root CA...\n");
#endif
    status = _nx_secure_x509_certificate_chain_verify(
        &certificate_store,
        &product_unit_certificate,
		now_unix
    );
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_RESET); // Set PA10 HIGH to indicate start of authentication process

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET); // Set PA10 HIGH to indicate start of authentication process

    if (status == NX_SUCCESS)
    {
#if TLS_PRINTS
        printf("  ✓✓✓ CERTIFICATE CHAIN VERIFICATION PASSED ✓✓✓\n");
        printf("  ✓ All signatures validated successfully\n");
        printf("  ✓ Chain integrity confirmed\n");
#endif
    }
    else
    {
        printf("  ✗✗✗ CERTIFICATE CHAIN VERIFICATION FAILED ✗✗✗\n");
        printf("  Error code: 0x%x\n", status);
        printf("  Common errors:\n");
        printf("    0x1A4 - Certificate not found (missing issuer)\n");
        printf("    0x1A6 - Invalid certificate sequence\n");
        printf("    0x1A8 - Signature check failed\n");
        return;
    }
#if TLS_PRINTS
    /* ------------------------------------------------------------------------
     * STEP 6: Display Certificate Fields
     * ------------------------------------------------------------------------ */
    printf("\n[6/8] Inspecting Product Unit Certificate fields...\n");

    printf("  Certificate Fields:\n");
    printf("  ------------------\n");

    /* Serial Number */
    printf("  Serial Number: ");
    for (UINT i = 0; i < product_unit_certificate.nx_secure_x509_serial_number_length; i++)
    {
        printf("%02X ", product_unit_certificate.nx_secure_x509_serial_number[i]);
    }
    printf("\n");

    /* Issuer */
    printf("  Issuer: %.*s\n",
        product_unit_certificate.nx_secure_x509_issuer.nx_secure_x509_common_name_length,
        product_unit_certificate.nx_secure_x509_issuer.nx_secure_x509_common_name);

    /* Subject */
    printf("  Subject: %.*s\n",
        product_unit_certificate.nx_secure_x509_distinguished_name.nx_secure_x509_common_name_length,
        product_unit_certificate.nx_secure_x509_distinguished_name.nx_secure_x509_common_name);

    /* Validity Period */
    printf("Valid From: %.*s\n",
           product_unit_certificate.nx_secure_x509_not_before_length,
           product_unit_certificate.nx_secure_x509_not_before);

    printf("Valid Until: %.*s\n",
           product_unit_certificate.nx_secure_x509_not_after_length,
           product_unit_certificate.nx_secure_x509_not_after);

    /* Signature Algorithm */
    printf("  Signature Algorithm: ");
    if (product_unit_certificate.nx_secure_x509_signature_algorithm == NX_SECURE_TLS_X509_TYPE_ECDSA_SHA_256)
        printf("ECDSA with SHA-256\n");
    else
        printf("OID %d\n", product_unit_certificate.nx_secure_x509_signature_algorithm);

    printf("  ✓ All certificate fields accessible and valid\n");

    /* ========================================================================
     * PART 2: CHALLENGE-RESPONSE AUTHENTICATION
     * ======================================================================== */

    printf("\n\nPART 2: CHALLENGE-RESPONSE AUTHENTICATION\n");
    printf("------------------------------------------\n");
#endif
#if 1
    /* ------------------------------------------------------------------------
     * STEP 7: Generate and Send Random Challenge
     * ------------------------------------------------------------------------ */
//    printf("\n[7/8] Generating random challenge...\n");

//    RNG_HandleTypeDef hrng;
    UCHAR challenge[32];
    uint32_t random_word;

    hrng.Instance = RNG;
    if (HAL_RNG_Init(&hrng) != HAL_OK)
    {
        printf("  ✗ RNG initialization failed!\n");
        return;
    }

    /* Generate 32-byte random challenge using hardware RNG */
    for (int i = 0; i < 8; i++)
    {
        if (HAL_RNG_GenerateRandomNumber(&hrng, &random_word) != HAL_OK)
        {
            printf("  ✗ Random number generation failed!\n");
            return;
        }
        memcpy(&challenge[i * 4], &random_word, 4);
    }
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_RESET); // Set PA10 HIGH to indicate start of authentication process
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET); // Set PA10 HIGH to indicate start of authentication process

//    printf("  ✓ 32-byte challenge generated\n");
//    printf("  Challenge: ");
#if TLS_PRINTS
    for (int i = 0; i < 32; i++)
        printf("%02X", challenge[i]);
    printf("\n");
#endif
#endif
#if 0
    /* Send challenge to Tx device */
    printf("  → Sending challenge to Tx device...\n");
    tx_send_challenge_to_remote(challenge, 32);
    printf("  ✓ Challenge sent\n");


    printf("  → Waiting for signature from Tx device...\n");
    rx_receive_signature_from_tx(signature_r, signature_s);
    printf("  ✓ Signature received\n");

    printf("  Signature R: ");
    for (int i = 0; i < 32; i++)
        printf("%02X", signature_r[i]);
    printf("\n");

    printf("  Signature S: ");
    for (int i = 0; i < 32; i++)
        printf("%02X", signature_s[i]);
    printf("\n");
#endif
    /* ------------------------------------------------------------------------
     * Hash the Challenge using SHA-256
     * ------------------------------------------------------------------------ */
    /* ------------------------------------------------------------------------
     * [7/8] Hash the 32-byte challenge with SHA-256 (correct metadata usage)
     * ------------------------------------------------------------------------ */
//    printf("\n[7/8] Hashing challenge with SHA-256...\n");

    UCHAR challenge_hash[32];
    UCHAR sha256_md_area[512];

    status = crypto_method_sha256.nx_crypto_operation(
        NX_CRYPTO_AUTHENTICATE,               // triggers default case
        NX_NULL,
        &crypto_method_sha256,
        NX_NULL, 0,
        device_challenge, (ULONG)device_challenge_len,
        NX_NULL,
        challenge_hash, sizeof(challenge_hash),
        sha256_md_area, sizeof(sha256_md_area),
        NX_NULL, NX_NULL);


    if (crypto_method_sha256.nx_crypto_cleanup) {
        (void)crypto_method_sha256.nx_crypto_cleanup((UCHAR*)sha256_md_area);
    }

//    printf("  ✓ Challenge hashed.\n");

    /* ------------------------------------------------------------------------
     * [8/8] Verify ECDSA(challenge_hash) using issuer's EC public key
     * ------------------------------------------------------------------------ */
//    printf("\n[8/8] Verifying ECDSA signature...\n");


    UCHAR sig_der[80]; /* P-256 DER sig fits well under this */
    UINT  sig_der_len = 0;

    if (device_sig_is_der) {
        if (device_sig_len > sizeof(sig_der))
        {
        	printf("Sig too large\n");
        	return;
        }
        NX_SECURE_MEMCPY(sig_der, device_sig_ptr, device_sig_len);
        sig_der_len = device_sig_len;
    } else {
        /* expect raw r||s of 64 bytes */
        if (device_sig_len != 64U)
        {
        	printf("Raw r||s must be 64 bytes\n");
        	return;
        }
        status = grl_build_der_from_rs64(device_sig_ptr, sig_der, &sig_der_len);
        if (status != NX_CRYPTO_SUCCESS)
        {
        	printf("DER build failed\n");
        	return;
        }
    }

    /* ---- 3) Resolve curve from Product Unit certificate ---- */
    NX_SECURE_EC_PUBLIC_KEY *pub = &product_unit_certificate.nx_secure_x509_public_key.ec_public_key;

    /* Expect uncompressed point: 0x04 | X(32) | Y(32) for P-256 */
    if (pub->nx_secure_ec_public_key_length == 33 &&
        (pub->nx_secure_ec_public_key[0] == 0x02 || pub->nx_secure_ec_public_key[0] == 0x03))
    {
        printf("Compressed EC public key (0x%02X) not supported here. Provide uncompressed (0x04).\n",
               pub->nx_secure_ec_public_key[0]);
        return;
    }
    /* Sanity: Product Unit pubkey should be uncompressed 65 bytes: 0x04 | X(32) | Y(32) */
    if (pub->nx_secure_ec_public_key_length != 65U || pub->nx_secure_ec_public_key[0] != 0x04)
    {
        printf("Unexpected EC public key format/length: %u\n",
               (unsigned)pub->nx_secure_ec_public_key_length);
        return;
    }

    /* ---- 4) ECDSA verify ---- */
    VOID *ecdsa_handle = NX_NULL;

    UINT ecdsa_md_need = (UINT)crypto_method_ecdsa.nx_crypto_metadata_area_size;
    if (ecdsa_md_need < 512U) ecdsa_md_need = 512U;  /* generous floor */
    static UCHAR ecdsa_md_area[2048];

    /* init with public key */
    status = crypto_method_ecdsa.nx_crypto_init(
        &crypto_method_ecdsa,
        (UCHAR*)pub->nx_secure_ec_public_key,
        (NX_CRYPTO_KEY_SIZE)(pub->nx_secure_ec_public_key_length << 3),
        &ecdsa_handle,
        ecdsa_md_area, ecdsa_md_need);
    if (status != NX_CRYPTO_SUCCESS)
    {
    	printf("ECDSA init fail: 0x%X\n", status);
    	return;
    }

    /* set curve */
    status = crypto_method_ecdsa.nx_crypto_operation(
        NX_CRYPTO_EC_CURVE_SET,
        ecdsa_handle,
        &crypto_method_ecdsa,
        NX_CRYPTO_NULL, 0,
        (UCHAR*)&crypto_method_ec_secp256, (ULONG)sizeof(crypto_method_ec_secp256),
        NX_CRYPTO_NULL,
        NX_CRYPTO_NULL, 0,
        ecdsa_md_area, ecdsa_md_need,
        NX_CRYPTO_NULL, NX_CRYPTO_NULL);
    if (status != NX_CRYPTO_SUCCESS)
    {
    	printf("Curve set fail: 0x%X\n", status);
    	return;
    }

    /* verify */
    status = crypto_method_ecdsa.nx_crypto_operation(
        NX_CRYPTO_VERIFY,
        ecdsa_handle,
        &crypto_method_ecdsa,
        (UCHAR*)pub->nx_secure_ec_public_key,
        (NX_CRYPTO_KEY_SIZE)(pub->nx_secure_ec_public_key_length << 3),
        /* input = hash */ challenge_hash, 32,
        NX_CRYPTO_NULL,
        /* signature (DER) */ sig_der, sig_der_len,
        ecdsa_md_area, ecdsa_md_need,
        NX_CRYPTO_NULL, NX_CRYPTO_NULL);

    if (crypto_method_ecdsa.nx_crypto_cleanup)
    {
        (VOID)crypto_method_ecdsa.nx_crypto_cleanup(ecdsa_md_area);
    }

    if (status == NX_CRYPTO_SUCCESS) {
//        printf("ECDSA verify: SUCCESS\n");
    } else {
        printf("ECDSA verify: FAIL (0x%X)\n", status);
    }
#if TLS_PRINTS
    /* ------------------------------------------------------------------------
     * Display Final Result
     * ------------------------------------------------------------------------ */
    printf("\n");
    printf("=========================================================\n");
    if (status == NX_CRYPTO_SUCCESS)
    {
        printf("  ✓✓✓ AUTHENTICATION SUCCESSFUL ✓✓✓\n");
        printf("=========================================================\n");
        printf("  ✓ Certificate chain verified\n");
        printf("  ✓ Signature verification passed\n");
        printf("  ✓ Tx device authenticated\n");
        printf("  ✓ Tx possesses valid private key\n");
        printf("\n  >>> DEVICE IS TRUSTED AND AUTHENTICATED <<<\n");
    }
    else
    {
        printf("  ✗✗✗ AUTHENTICATION FAILED ✗✗✗\n");
        printf("=========================================================\n");
        printf("  Error code: 0x%x\n", status);
        printf("  Possible reasons:\n");
        printf("    - Invalid signature (r,s)\n");
        printf("    - Wrong public key\n");
        printf("    - Challenge was tampered\n");
        printf("    - Tx does not possess the private key\n");
        printf("\n  >>> DEVICE AUTHENTICATION REJECTED <<<\n");
    }
    printf("=========================================================\n\n");
#endif
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_RESET);
    printf("TLS DONE\r\n");
//    printf("CM7 FW V: %x.%x.%x\r\n",FW_MAJOR_VERSION,FW_MINOR_VERSION,FW_SUB_MINOR_VERSION);
//    grl_print_sp_rh_1();
    while (1)
    {
        // Your application code here
        // e.g., TLS verification, certificate checks, RNG, etc.
        tx_thread_sleep(100);   // Sleep or wait for events
    }
}

/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number           = MPU_REGION_NUMBER1;
  MPU_InitStruct.BaseAddress = 0x30040000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_128KB;
  MPU_InitStruct.SubRegionDisable = 0x0;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable           = MPU_REGION_ENABLE;
  MPU_InitStruct.Number           = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress      = 0x30000000U;
  MPU_InitStruct.Size             = MPU_REGION_SIZE_256KB;
  MPU_InitStruct.SubRegionDisable = 0x00U;
  MPU_InitStruct.TypeExtField     = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.DisableExec      = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable      = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable      = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable     = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
