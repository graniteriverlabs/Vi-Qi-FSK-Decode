/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
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
#include "main.h"
#include "dma.h"
#include "gpio.h"
#include "spi.h"
#include "tim.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "cm4_grl_app.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

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

#include "ipc_shared.h"
#include "qi.h"
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
#define SPI_LEN 36

uint8_t spi_tx_buf[SPI_LEN];
uint8_t spi_pkt_buf[SPI_LEN];
HAL_StatusTypeDef ret = 0;
uint8_t spi_tx = 0;
Qi_Packet_t pkt;
void SPI_PrepareData(void) {
  //    for(int i=0; i<SPI_LEN; i++)
  //        spi_tx_buf[i] = 0x01;
  spi_pkt_buf[0] = 0x25;

  uint16_t payload_len = 1;

  //	memcpy(&pkt.payload[0],&spi_pkt_buf[0],pkt.payload_len);
  Qi_BuildPacket(&pkt, 0x01, spi_pkt_buf, payload_len);

  //	pkt.checksum = pkt.header ^ pkt.payload[0] ^ pkt.payload[1];

  //	Qi_BuildPacket(&pkt, pkt.header, spi_pkt_buf, pkt.payload_len);
  //	pkt.checksum = 0x30;
}

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi) {
  if (hspi->Instance == SPI1) {
    // Transfer complete
	// Optional: disable SPI to force MOSI idle (prevents extra shifting)
	__HAL_SPI_DISABLE(&hspi1);
    // Example: toggle debug pin
//    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0);
    HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
    spi_tx = 1;

  }
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi) {
  if (hspi->Instance == SPI1) {
    // Clear overrun if needed
    __HAL_SPI_CLEAR_OVRFLAG(hspi);
    //        HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
    spi_tx = 1;

  }
}

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

/* USER CODE BEGIN Boot_Mode_Sequence_1 */
#if defined(DUAL_CORE_BOOT_SYNC_SEQUENCE)
  /*HW semaphore Clock enable*/
  __HAL_RCC_HSEM_CLK_ENABLE();
  /* Activate HSEM notification for Cortex-M4*/
  HAL_HSEM_ActivateNotification(__HAL_HSEM_SEMID_TO_MASK(HSEM_ID_0));

  /*
  Domain D2 goes to STOP mode (Cortex-M4 in deep-sleep) waiting for Cortex-M7 to
  perform system initialization (system clock config, external memory
  configuration.. )
  */
  HAL_PWREx_ClearPendingEvent();
  HAL_PWREx_EnterSTOPMode(PWR_MAINREGULATOR_ON, PWR_STOPENTRY_WFE,
                          PWR_D2_DOMAIN);
  /* Clear HSEM flag */
  __HAL_HSEM_CLEAR_FLAG(__HAL_HSEM_SEMID_TO_MASK(HSEM_ID_0));

#endif /* DUAL_CORE_BOOT_SYNC_SEQUENCE */
       /* USER CODE END Boot_Mode_Sequence_1 */
  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick.
   */
  HAL_Init();

  /* USER CODE BEGIN Init */
  __HAL_RCC_HSEM_CLK_ENABLE();
  HAL_HSEM_FastTake(HSEM_CM4_TO_CM7);
  //  HAL_HSEM_Release(HSEM_CM4_TO_CM7, 0);
  HAL_HSEM_ActivateNotification(__HAL_HSEM_SEMID_TO_MASK(HSEM_CM7_TO_CM4));

  HAL_NVIC_SetPriority(HSEM2_IRQn, 12, 0);
  HAL_NVIC_EnableIRQ(HSEM2_IRQn);
  /* USER CODE END Init */

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_DMA_Init();
  MX_GPIO_Init();
  MX_SPI1_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */
  MX_GPIO_Init();
  uint8_t local_buf[CM4_API_MAX_LEN];
  //  uint16_t len;
  __HAL_RCC_HSEM_CLK_ENABLE();
  extern volatile uint8_t packet_ready;
  ipc_shm_init();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  spi_tx = 0;

  SPI_PrepareData();
  int encoded_len;
  encoded_len = Qi_EncodeToSPI(&pkt, spi_tx_buf, SPI_LEN);
  if (encoded_len <= 0)
    __asm__ volatile("bkpt");

  ret = HAL_SPI_TransmitReceive_DMA(&hspi1, spi_tx_buf, spi_pkt_buf, encoded_len);
  //	ret = HAL_SPI_Transmit_DMA(&hspi1, spi_tx_buf, encoded_len);
  if (ret != HAL_OK)
    __asm__ volatile("bkpt");
  //	HAL_Delay(10);
  __HAL_TIM_SET_COUNTER(&htim2, 0);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);

  while (1) {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    if (packet_ready) {
      packet_ready = 0;

      uint32_t len = SHM_M7_TO_M4.len;
      if (len == 0 || len > API_MAX_LEN) {
        // bad request, ignore
      } else {
        memcpy(local_buf, (void *)SHM_M7_TO_M4.data, len);
        process_app_cmd(local_buf); // your existing command processing
      }
    }
    if (spi_tx) {
      spi_tx = 0;
      __HAL_SPI_ENABLE(&hspi1);
      // Restart DMA if continuous mode desired
      ret = HAL_SPI_TransmitReceive_DMA(&hspi1, spi_tx_buf, spi_pkt_buf,
                                        encoded_len);
      //		  ret = HAL_SPI_Transmit_DMA(&hspi1, spi_tx_buf,
      // encoded_len);
      if (ret != HAL_OK)
        __asm__ volatile("bkpt");
      __HAL_TIM_SET_COUNTER(&htim2, 0);
      HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
    }
  }
  /* USER CODE END 3 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1) {
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
void assert_failed(uint8_t *file, uint32_t line) {
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line
     number, ex: printf("Wrong parameters value: file %s on line %d\r\n", file,
     line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
