/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usart.c
  * @brief   This file provides code for the configuration
  *          of the USART instances.
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
#include "usart.h"

/* USER CODE BEGIN 0 */
/*
 * 下面变量都加 static，表示只在 usart.c 内部使用。
 * volatile 用在“中断与主循环共享”的变量上，要求编译器每次都真正读取内存。
 */
static volatile uint8_t ota_flag = 0U;
static volatile uint8_t ota_version[3] = {0U, 0U, 0U};
static uint8_t uart3_rec_byte = 0U;
static UART_RX_typeDef uart3_rx_data = {{0U}, 0U};

/* USER CODE END 0 */

UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;

/* USART2 init function */

void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}
/* USART3 init function */

void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

void HAL_UART_MspInit(UART_HandleTypeDef* uartHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(uartHandle->Instance==USART2)
  {
  /* USER CODE BEGIN USART2_MspInit 0 */

  /* USER CODE END USART2_MspInit 0 */
    /* USART2 clock enable */
    __HAL_RCC_USART2_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**USART2 GPIO Configuration
    PA2     ------> USART2_TX
    PA3     ------> USART2_RX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN USART2_MspInit 1 */

  /* USER CODE END USART2_MspInit 1 */
  }
  else if(uartHandle->Instance==USART3)
  {
  /* USER CODE BEGIN USART3_MspInit 0 */

  /* USER CODE END USART3_MspInit 0 */
    /* USART3 clock enable */
    __HAL_RCC_USART3_CLK_ENABLE();

    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**USART3 GPIO Configuration
    PB10     ------> USART3_TX
    PB11     ------> USART3_RX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_10|GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART3;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* USART3 interrupt Init */
    HAL_NVIC_SetPriority(USART3_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(USART3_IRQn);
  /* USER CODE BEGIN USART3_MspInit 1 */

  /* USER CODE END USART3_MspInit 1 */
  }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef* uartHandle)
{

  if(uartHandle->Instance==USART2)
  {
  /* USER CODE BEGIN USART2_MspDeInit 0 */

  /* USER CODE END USART2_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_USART2_CLK_DISABLE();

    /**USART2 GPIO Configuration
    PA2     ------> USART2_TX
    PA3     ------> USART2_RX
    */
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_2|GPIO_PIN_3);

  /* USER CODE BEGIN USART2_MspDeInit 1 */

  /* USER CODE END USART2_MspDeInit 1 */
  }
  else if(uartHandle->Instance==USART3)
  {
  /* USER CODE BEGIN USART3_MspDeInit 0 */

  /* USER CODE END USART3_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_USART3_CLK_DISABLE();

    /**USART3 GPIO Configuration
    PB10     ------> USART3_TX
    PB11     ------> USART3_RX
    */
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_10|GPIO_PIN_11);

    /* USART3 interrupt Deinit */
    HAL_NVIC_DisableIRQ(USART3_IRQn);
  /* USER CODE BEGIN USART3_MspDeInit 1 */

  /* USER CODE END USART3_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

void uart_receive_enable(void)
{
  uart3_rx_data.rx_cnt = 0U;
  ota_flag = 0U;

  /*
   * &huart3：USART3 句柄的地址。
   * &uart3_rec_byte：本次接收的 1 字节要写到哪里。
   * 1U：每次只收一个字节，完成后进入 HAL_UART_RxCpltCallback()。
   */
  HAL_UART_Receive_IT(&huart3, &uart3_rec_byte, 1U);
}

uint8_t uart_get_ota_flag(void)
{
  return ota_flag;
}

void uart_get_ota_version(uint8_t version[3])
{
  if (version != NULL)
  {
    version[0] = ota_version[0];
    version[1] = ota_version[1];
    version[2] = ota_version[2];
  }
}

/*
 * USART3 每收到 1 字节，HAL_UART_IRQHandler() 最终会调用这个回调。
 * huart 是指针，所以使用 huart->Instance 访问它指向结构体中的 Instance 成员。
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if ((huart != NULL) && (huart->Instance == USART3))
  {
    uint8_t data_length;
    uint32_t frame_length;

    /* 一帧必须从 0xAF 开始；不是帧头的字节直接丢弃。 */
    if ((uart3_rx_data.rx_cnt == 0U) && (uart3_rec_byte != 0xAFU))
    {
      HAL_UART_Receive_IT(&huart3, &uart3_rec_byte, 1U);
      return;
    }

    if (uart3_rx_data.rx_cnt < sizeof(uart3_rx_data.rx_buf))
    {
      uart3_rx_data.rx_buf[uart3_rx_data.rx_cnt] = uart3_rec_byte;
      ++uart3_rx_data.rx_cnt;
    }
    else
    {
      /* 缓冲区溢出说明当前帧无效，从下一个 0xAF 重新同步。 */
      uart3_rx_data.rx_cnt = 0U;
    }

    if (uart3_rx_data.rx_cnt >= 2U)
    {
      data_length = uart3_rx_data.rx_buf[1];

      /* 帧总长 = 帧头1 + 长度1 + DATA长度 + 帧尾1。 */
      frame_length = (uint32_t)data_length + 3U;

      if ((data_length == 0U) ||
          (frame_length > sizeof(uart3_rx_data.rx_buf)))
      {
        uart3_rx_data.rx_cnt = 0U;
      }
      else if (uart3_rx_data.rx_cnt == frame_length)
      {
        if (uart3_rx_data.rx_buf[frame_length - 1U] == 0xFAU)
        {
          /* DATA 至少 3 字节时，把前三个字节解释为主/次/修订版本号。 */
          if (data_length >= 3U)
          {
            ota_version[0] = uart3_rx_data.rx_buf[2];
            ota_version[1] = uart3_rx_data.rx_buf[3];
            ota_version[2] = uart3_rx_data.rx_buf[4];
          }

          /* 中断只置标志；Flash 写入、printf 和复位都放在 main() 主循环。 */
          ota_flag = 1U;
        }

        uart3_rx_data.rx_cnt = 0U;
      }
    }

    /* HAL 的单字节中断接收完成后不会自动续收，必须再次调用进行“重新挂号”。 */
    HAL_UART_Receive_IT(&huart3, &uart3_rec_byte, 1U);
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if ((huart != NULL) && (huart->Instance == USART3))
  {
    __HAL_UART_CLEAR_OREFLAG(huart);
    __HAL_UART_CLEAR_FEFLAG(huart);
    __HAL_UART_CLEAR_NEFLAG(huart);
    __HAL_UART_CLEAR_PEFLAG(huart);
    HAL_UART_Receive_IT(&huart3, &uart3_rec_byte, 1U);
  }
}

/* USER CODE END 1 */
