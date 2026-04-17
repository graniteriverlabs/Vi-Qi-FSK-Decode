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
#include "main.h"
#include "dma.h"
#include "hrtim.h"
#include "spi.h"
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "cm4_grl_app.h"
#include <fsk_decoder.h>
#include <fsk_config.h>
#include <fsk_debug.h>
#include <fsk_online_stream.h>
#include <ipc_stream_shared.h>
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
volatile uint8_t debugFlag =0;
volatile uint8_t printdata =0;

volatile uint32_t max_cycles = 0;
volatile uint8_t capture_done = 0;
volatile uint8_t capture_active = 0;

volatile uint8_t dma_half_ready = 0;
volatile uint8_t dma_full_ready = 0;

volatile uint32_t dma_half_overrun = 0;
volatile uint32_t dma_full_overrun = 0;

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
int main(void)
{

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
  perform system initialization (system clock config, external memory configuration.. )
  */
  HAL_PWREx_ClearPendingEvent();
  HAL_PWREx_EnterSTOPMode(PWR_MAINREGULATOR_ON, PWR_STOPENTRY_WFE, PWR_D2_DOMAIN);
  /* Clear HSEM flag */
  __HAL_HSEM_CLEAR_FLAG(__HAL_HSEM_SEMID_TO_MASK(HSEM_ID_0));

#endif /* DUAL_CORE_BOOT_SYNC_SEQUENCE */
/* USER CODE END Boot_Mode_Sequence_1 */
  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
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
  MX_HRTIM_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
  MX_GPIO_Init();
  uint8_t local_buf[CM4_API_MAX_LEN];
  //  uint16_t len;
  __HAL_RCC_HSEM_CLK_ENABLE();
  extern volatile uint8_t packet_ready;
  ipc_shm_init();


  /**
   * @brief Initialize the shared CM4->CM7 stream ring.
   *
   * This must be done only once, and CM4 is the correct owner because
   * CM4 is the producer side of the online stream.
   */
  ipc_stream_init();

#if (!OFFLINE_CAPTURE_MODE) && (FSK_DEBUG_LIVE_PERIOD_FAST_STREAM || FSK_DEBUG_LIVE_PERIOD_BLOCK_STREAM)
    fsk_online_stream_init();
#endif

  DWT_Init();
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);

  HAL_HRTIM_WaveformCountStart(&hhrtim, HRTIM_TIMERID_TIMER_A);

#if !OFFLINE_CAPTURE_MODE
  #if !FSK_EXT_TRIGGER_DEBUG
    start_capture();
  #endif
#endif
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
#if 0
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
#endif
#if OFFLINE_CAPTURE_MODE

//	generate_fsk_test(); //pwmdebug
//	HAL_Delay(1);	//pwmdebug
	if (capture_done)
	{
		capture_done = 0;   // prevent re-entry
	    /**
	     * Reset offline write index before re-processing a new capture.
	     */
	    debug_index = 0;

	    process_block(0, CAPTURE_SAMPLES);

	    /**
	     * Send the processed offline capture over Ethernet via CM7.
	     */
	    ethernet_dump_capture();

//		while(1);
	}
#else
    if (dma_half_ready)
    {
        dma_half_ready = 0;
#if FSK_DEBUG_PROCESS_TIMING_GPIO
//        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_SET);
#endif

#if FSK_DEBUG_LIVE_PERIOD_FAST_STREAM
        /**
         * Fast period-only stream path.
         *
         * This avoids process_block(), so it avoids:
         * - moving_average_16()
         * - delayed_diff()
         * - transition detection
         * - decoder state machine
         */
        fsk_online_period_fast_block_process(0U, FSK_DMA_HALF_SAMPLES);
#else
        /**
         * Normal decode path.
         */
        process_block(0U, FSK_DMA_HALF_SAMPLES);
#endif

#if FSK_DEBUG_PROCESS_TIMING_GPIO
//        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_RESET);
#endif
    }

    if (dma_full_ready)
    {
        dma_full_ready = 0;
#if FSK_DEBUG_PROCESS_TIMING_GPIO
//        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_SET);
#endif

#if FSK_DEBUG_LIVE_PERIOD_FAST_STREAM
        /**
         * Fast period-only stream path for the second DMA half.
         */
        fsk_online_period_fast_block_process(FSK_DMA_HALF_SAMPLES,
                                             FSK_DMA_HALF_SAMPLES);
#else
        /**
         * Normal decode path.
         */
        process_block(FSK_DMA_HALF_SAMPLES, FSK_DMA_HALF_SAMPLES);
#endif

#if FSK_DEBUG_PROCESS_TIMING_GPIO
//        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_RESET);
#endif
    }
#endif
  }
  /* USER CODE END 3 */
}

/* USER CODE BEGIN 4 */
void start_capture(void)
{
    if (capture_active)
    {
        return;
    }

    capture_active = 1;
    capture_done   = 0;
    fsk_decoder_reset_state();

#if (!OFFLINE_CAPTURE_MODE) && (FSK_DEBUG_LIVE_PERIOD_FAST_STREAM || FSK_DEBUG_LIVE_PERIOD_BLOCK_STREAM)
    /**
     * Reset online stream counters and previous timestamp for each new capture.
     */
    fsk_online_stream_init();
#endif

    HRTIM1->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_A].CNTxR = 0;

    HRTIM1->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_A].TIMxDIER |=
        HRTIM_TIMDIER_CPT1DE;

    hdma_hrtim1_a.XferHalfCpltCallback = HAL_DMA_XferHalfCpltCallback;
    hdma_hrtim1_a.XferCpltCallback     = HAL_DMA_XferCpltCallback;
#if OFFLINE_CAPTURE_MODE

//    HAL_HRTIM_SimpleCaptureStart_DMA(
//        &hhrtim1,
//        HRTIM_TIMERINDEX_TIMER_A,
//        HRTIM_CAPTUREUNIT_1,
//		(uint32_t)&HRTIM1->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_A].CPT1xR,
//        (uint32_t )hrtim_capture_buf,
//        CAPTURE_SAMPLES
//    );

    HAL_DMA_Start_IT(&hdma_hrtim1_a,
  		  (uint32_t)&HRTIM1->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_A].CPT1xR,
  		  (uint32_t)hrtim_capture_buf, CAPTURE_SAMPLES
  		  );

    __HAL_DMA_DISABLE_IT(&hdma_hrtim1_a, DMA_IT_HT);

#else

    //Half Transfer interrupt will be enabled by this function itself
    HAL_DMA_Start_IT(&hdma_hrtim1_a,
  		  (uint32_t)&HRTIM1->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_A].CPT1xR,
  		  (uint32_t)hrtim_capture_buf, CAPTURE_SAMPLES
  		  );

#endif
}

void HAL_DMA_XferHalfCpltCallback(DMA_HandleTypeDef *hdma)
{
	uint32_t start = DWT->CYCCNT;
    if (hdma == &hdma_hrtim1_a)
    {

        if (dma_half_ready)
        {
            dma_half_overrun++;
        }
        dma_half_ready = 1;
    }
    uint32_t end = DWT->CYCCNT;

    uint32_t cycles = end - start;


    if (cycles > max_cycles)
    	max_cycles = cycles;
}

void HAL_DMA_XferCpltCallback(DMA_HandleTypeDef *hdma)
{
#if OFFLINE_CAPTURE_MODE
    if (hdma == &hdma_hrtim1_a)
    {
        HRTIM1->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_A].TIMxDIER &=
            ~HRTIM_TIMDIER_CPT1DE;

        HAL_DMA_Abort(&hdma_hrtim1_a);

        capture_active = 0;
        capture_done = 1;
    }
#else
	uint32_t start = DWT->CYCCNT;
    if (hdma == &hdma_hrtim1_a)
    {

        if (dma_full_ready)
        {
            dma_full_overrun++;
        }
        dma_full_ready = 1;

    }
    uint32_t end = DWT->CYCCNT;

    uint32_t cycles = end - start;

    if (cycles > max_cycles)
    	max_cycles = cycles;
#endif
}

void DWT_Init(void)
{
    /* Enable TRC (Trace) */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    /* Reset cycle counter */
    DWT->CYCCNT = 0;

    /* Enable cycle counter */
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}
/* USER CODE END 4 */

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
