/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usart.h
  * @brief   This file contains all the function prototypes for
  *          the usart.c file
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
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USART_H__
#define __USART_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

extern UART_HandleTypeDef huart2;

extern UART_HandleTypeDef huart3;

/* USER CODE BEGIN Private defines */
/*
 * APP 中“请求升级命令”的接收缓存。
 *
 * 老师协议格式：
 *   [0xAF] [DATA长度] [DATA...] [0xFA]
 *
 * rx_cnt 保存当前已经收了多少字节；rx_buf 保存一整帧的原始字节。
 */
typedef struct
{
  uint8_t rx_buf[50];
  uint8_t rx_cnt;
} UART_RX_typeDef;

/* USER CODE END Private defines */

void MX_USART2_UART_Init(void);
void MX_USART3_UART_Init(void);

/* USER CODE BEGIN Prototypes */
/* 让 USART3 开始以中断方式逐字节接收升级命令。 */
void uart_receive_enable(void);

/* 主循环通过这个函数读取中断中置位的升级请求。 */
uint8_t uart_get_ota_flag(void);

/* 取出命令 DATA 区前 3 字节所表示的版本号。 */
void uart_get_ota_version(uint8_t version[3]);

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __USART_H__ */

