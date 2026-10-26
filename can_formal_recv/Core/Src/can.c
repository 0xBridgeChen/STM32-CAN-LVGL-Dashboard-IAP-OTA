/* USER CODE BEGIN Header */
/*********************************************************************************
  * @file    can.c
  * @brief   CAN 控制器底层硬件驱动配置 (ISO 11898 工业车载 500kbps 规范)
  * 
  * ================================================================
  * 【CAN1 核心时钟树与位时序推导档案】:
  * 
  * 1. 物理时钟溯源:
  *    外部晶振 HSE (8 MHz)
  *    -> PLL: /M(8) * N(336) / P(2) = 主频 SYSCLK 168 MHz
  *    -> AHB 预分频器 (/1) = HCLK 168 MHz
  *    -> APB1 预分频器 (/4) = PCLK1 = 42 MHz
  *    ⚠️ 致命避坑警告：CAN 控制器直接由 APB1 外设时钟驱动，
  *       基准时钟恒为 42 MHz！绝对不能按定时器倍频后的 84 MHz 计算！
  * 
  * 2. 500 kbps 严格位时序数学推导:
  *    - 预分频系数 Prescaler = 7
  *    - 最小时间量子 1 TQ = Prescaler / PCLK1 = 7 / 42 MHz = 1/6 µs ≈ 166.67 ns
  *    - 标称位时间 (Bit Time) = Sync_Seg(1) + BS1(9) + BS2(2) = 12 TQ
  *    - 1 位总持续时间 = 12 × (1/6 µs) = 2.0 µs
  *    - 理论物理波特率 = 1 / 2.0 µs = 500,000 bps (500 kbps)
  * 
  * 3. 采样点 (Sample Point) 安全裕度:
  *    - 采样点公式: (1 + BS1) / (1 + BS1 + BS2) = (1 + 9) / 12 = 10 / 12 ≈ 83.33%
  *    - 行业符合性: 完美契合 CiA 301 / 汽车电子 ISO 11898 建议的 75%~87.5% 理想区间，
  *      确保在长总线延迟与恶劣共模干扰下依然具备极强容错能力。
  * 
  * 4. 跨平台移植对比 (F103 vs F407):
  *    - STM32F103 (如 can_test01c): APB1 为 36 MHz，必须设 Prescaler=6 才是 500 kbps。
  *    - STM32F407 (本工程): APB1 为 42 MHz，必须设 Prescaler=7 才是 500 kbps！
  *    - ⚠️ 若在 F407 上盲目照抄 F103 的 Prescaler=6，实际波特率将暴增至 583.33 kbps，
  *      导致两端节点因位同步丢失产生连续 ACK 错误和位填充错误，最终 Bus-Off 锁死！
  * ================================================================
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "can.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

CAN_HandleTypeDef hcan1;

/* CAN1 init function */
void MX_CAN1_Init(void)
{

  /* USER CODE BEGIN CAN1_Init 0 */

  /* USER CODE END CAN1_Init 0 */

  /* USER CODE BEGIN CAN1_Init 1 */

  /* USER CODE END CAN1_Init 1 */
  hcan1.Instance = CAN1;
  hcan1.Init.Prescaler = 7;
  hcan1.Init.Mode = CAN_MODE_NORMAL;
  hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan1.Init.TimeSeg1 = CAN_BS1_9TQ;
  hcan1.Init.TimeSeg2 = CAN_BS2_2TQ;
  hcan1.Init.TimeTriggeredMode = DISABLE;
  hcan1.Init.AutoBusOff = DISABLE;
  hcan1.Init.AutoWakeUp = DISABLE;
  hcan1.Init.AutoRetransmission = DISABLE;
  hcan1.Init.ReceiveFifoLocked = DISABLE;
  hcan1.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN1_Init 2 */

  /* USER CODE END CAN1_Init 2 */

}

void HAL_CAN_MspInit(CAN_HandleTypeDef* canHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(canHandle->Instance==CAN1)
  {
  /* USER CODE BEGIN CAN1_MspInit 0 */

  /* USER CODE END CAN1_MspInit 0 */
    /* CAN1 clock enable */
    __HAL_RCC_CAN1_CLK_ENABLE();

    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**CAN1 GPIO Configuration
    PB8     ------> CAN1_RX
    PB9     ------> CAN1_TX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF9_CAN1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN CAN1_MspInit 1 */

  /* USER CODE END CAN1_MspInit 1 */
  }
}

void HAL_CAN_MspDeInit(CAN_HandleTypeDef* canHandle)
{

  if(canHandle->Instance==CAN1)
  {
  /* USER CODE BEGIN CAN1_MspDeInit 0 */

  /* USER CODE END CAN1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_CAN1_CLK_DISABLE();

    /**CAN1 GPIO Configuration
    PB8     ------> CAN1_RX
    PB9     ------> CAN1_TX
    */
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_8|GPIO_PIN_9);

  /* USER CODE BEGIN CAN1_MspDeInit 1 */

  /* USER CODE END CAN1_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
