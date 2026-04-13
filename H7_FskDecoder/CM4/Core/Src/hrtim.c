/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    hrtim.c
  * @brief   This file provides code for the configuration
  *          of the HRTIM instances.
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
#include "hrtim.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

HRTIM_HandleTypeDef hhrtim;
DMA_HandleTypeDef hdma_hrtim1_a;

/* HRTIM init function */
void MX_HRTIM_Init(void)
{

  /* USER CODE BEGIN HRTIM_Init 0 */

  /* USER CODE END HRTIM_Init 0 */

  HRTIM_EventCfgTypeDef pEventCfg = {0};
  HRTIM_TimeBaseCfgTypeDef pTimeBaseCfg = {0};
  HRTIM_TimerCfgTypeDef pTimerCfg = {0};
  HRTIM_CaptureCfgTypeDef pCaptureCfg = {0};

  /* USER CODE BEGIN HRTIM_Init 1 */

  /* USER CODE END HRTIM_Init 1 */
  hhrtim.Instance = HRTIM1;
  hhrtim.Init.HRTIMInterruptResquests = HRTIM_IT_NONE;
  hhrtim.Init.SyncOptions = HRTIM_SYNCOPTION_NONE;
  if (HAL_HRTIM_Init(&hhrtim) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_HRTIM_EventPrescalerConfig(&hhrtim, HRTIM_EVENTPRESCALER_DIV1) != HAL_OK)
  {
    Error_Handler();
  }
  pEventCfg.Source = HRTIM_EVENTSRC_1;
  pEventCfg.Polarity = HRTIM_EVENTPOLARITY_HIGH;
  pEventCfg.Sensitivity = HRTIM_EVENTSENSITIVITY_RISINGEDGE;
  pEventCfg.Filter = HRTIM_EVENTFILTER_NONE;
  if (HAL_HRTIM_EventConfig(&hhrtim, HRTIM_EVENT_7, &pEventCfg) != HAL_OK)
  {
    Error_Handler();
  }
  pTimeBaseCfg.Period = 0xFFFD;
  pTimeBaseCfg.RepetitionCounter = 0x00;
  pTimeBaseCfg.PrescalerRatio = HRTIM_PRESCALERRATIO_DIV1;
  pTimeBaseCfg.Mode = HRTIM_MODE_CONTINUOUS;
  if (HAL_HRTIM_TimeBaseConfig(&hhrtim, HRTIM_TIMERINDEX_TIMER_A, &pTimeBaseCfg) != HAL_OK)
  {
    Error_Handler();
  }
  pTimerCfg.InterruptRequests = HRTIM_TIM_IT_NONE;
  pTimerCfg.DMARequests = HRTIM_TIM_DMA_CPT1;
  pTimerCfg.DMASrcAddress = 0x0000;
  pTimerCfg.DMADstAddress = 0x0000;
  pTimerCfg.DMASize = 0x1;
  pTimerCfg.HalfModeEnable = HRTIM_HALFMODE_DISABLED;
  pTimerCfg.StartOnSync = HRTIM_SYNCSTART_DISABLED;
  pTimerCfg.ResetOnSync = HRTIM_SYNCRESET_DISABLED;
  pTimerCfg.DACSynchro = HRTIM_DACSYNC_NONE;
  pTimerCfg.PreloadEnable = HRTIM_PRELOAD_DISABLED;
  pTimerCfg.UpdateGating = HRTIM_UPDATEGATING_INDEPENDENT;
  pTimerCfg.BurstMode = HRTIM_TIMERBURSTMODE_MAINTAINCLOCK;
  pTimerCfg.RepetitionUpdate = HRTIM_UPDATEONREPETITION_DISABLED;
  pTimerCfg.PushPull = HRTIM_TIMPUSHPULLMODE_DISABLED;
  pTimerCfg.FaultEnable = HRTIM_TIMFAULTENABLE_NONE;
  pTimerCfg.FaultLock = HRTIM_TIMFAULTLOCK_READWRITE;
  pTimerCfg.DeadTimeInsertion = HRTIM_TIMDEADTIMEINSERTION_DISABLED;
  pTimerCfg.DelayedProtectionMode = HRTIM_TIMER_A_B_C_DELAYEDPROTECTION_DISABLED;
  pTimerCfg.UpdateTrigger = HRTIM_TIMUPDATETRIGGER_NONE;
  pTimerCfg.ResetTrigger = HRTIM_TIMRESETTRIGGER_NONE;
  pTimerCfg.ResetUpdate = HRTIM_TIMUPDATEONRESET_DISABLED;
  if (HAL_HRTIM_WaveformTimerConfig(&hhrtim, HRTIM_TIMERINDEX_TIMER_A, &pTimerCfg) != HAL_OK)
  {
    Error_Handler();
  }
  pCaptureCfg.Trigger = HRTIM_CAPTURETRIGGER_EEV_7;
  if (HAL_HRTIM_WaveformCaptureConfig(&hhrtim, HRTIM_TIMERINDEX_TIMER_A, HRTIM_CAPTUREUNIT_1, &pCaptureCfg) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN HRTIM_Init 2 */

  /* USER CODE END HRTIM_Init 2 */

}

void HAL_HRTIM_MspInit(HRTIM_HandleTypeDef* hrtimHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
  if(hrtimHandle->Instance==HRTIM1)
  {
  /* USER CODE BEGIN HRTIM1_MspInit 0 */

  /* USER CODE END HRTIM1_MspInit 0 */

  /** Initializes the peripherals clock
  */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_HRTIM1;
    PeriphClkInitStruct.Hrtim1ClockSelection = RCC_HRTIM1CLK_CPUCLK;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
      Error_Handler();
    }

    /* HRTIM1 clock enable */
    __HAL_RCC_HRTIM1_CLK_ENABLE();

    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**HRTIM GPIO Configuration
    PB5     ------> HRTIM_EEV7
    */
    GPIO_InitStruct.Pin = GPIO_PIN_5;

    /**HRTIM GPIO Configuration
       PB4     ------> HRTIM_EEV6
      */
//    GPIO_InitStruct.Pin = GPIO_PIN_4;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF3_HRTIM1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* HRTIM1 DMA Init */
    /* HRTIM1_A Init */
    hdma_hrtim1_a.Instance = DMA1_Stream6;
    hdma_hrtim1_a.Init.Request = DMA_REQUEST_HRTIM_TIMER_A;
    hdma_hrtim1_a.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_hrtim1_a.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_hrtim1_a.Init.MemInc = DMA_MINC_ENABLE;
    hdma_hrtim1_a.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
    hdma_hrtim1_a.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
    hdma_hrtim1_a.Init.Mode = DMA_CIRCULAR;
    hdma_hrtim1_a.Init.Priority = DMA_PRIORITY_HIGH;
    hdma_hrtim1_a.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_hrtim1_a) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_LINKDMA(hrtimHandle,hdmaTimerA,hdma_hrtim1_a);

  /* USER CODE BEGIN HRTIM1_MspInit 1 */

  /* USER CODE END HRTIM1_MspInit 1 */
  }
}

void HAL_HRTIM_MspDeInit(HRTIM_HandleTypeDef* hrtimHandle)
{

  if(hrtimHandle->Instance==HRTIM1)
  {
  /* USER CODE BEGIN HRTIM1_MspDeInit 0 */

  /* USER CODE END HRTIM1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_HRTIM1_CLK_DISABLE();

    /**HRTIM GPIO Configuration
    PB5     ------> HRTIM_EEV7
    */
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_5);

    /* HRTIM1 DMA DeInit */
    HAL_DMA_DeInit(hrtimHandle->hdmaTimerA);
  /* USER CODE BEGIN HRTIM1_MspDeInit 1 */

  /* USER CODE END HRTIM1_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
