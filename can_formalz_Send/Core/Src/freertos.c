/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
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
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stdio.h"
#include "headfile.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for CommandTask */
osThreadId_t CommandTaskHandle;
const osThreadAttr_t CommandTask_attributes = {
  .name = "CommandTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};
/* Definitions for CanTxTask */
osThreadId_t CanTxTaskHandle;
const osThreadAttr_t CanTxTask_attributes = {
  .name = "CanTxTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for CommandQueue */
osMessageQueueId_t CommandQueueHandle;
const osMessageQueueAttr_t CommandQueue_attributes = {
  .name = "CommandQueue"
};
/* Definitions for CanTxQueue */
osMessageQueueId_t CanTxQueueHandle;
const osMessageQueueAttr_t CanTxQueue_attributes = {
  .name = "CanTxQueue"
};
/* Definitions for UsartMutex */
osMutexId_t UsartMutexHandle;
const osMutexAttr_t UsartMutex_attributes = {
  .name = "UsartMutex"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
// ★ 车规级 E2E CRC8 计算函数 (SAE J1850 标准：多项式 0x1D，初值 0xFF，结果异或 0xFF)
static uint8_t e2e_calc_crc8(const uint8_t *data, uint8_t len)
{
  uint8_t crc = 0xFFU;
  for (uint8_t i = 0U; i < len; i++)
  {
    crc ^= data[i];
    for (uint8_t bit = 0U; bit < 8U; bit++)
    {
      if ((crc & 0x80U) != 0U)
        crc = (uint8_t)((crc << 1U) ^ 0x1DU);
      else
        crc <<= 1U;
    }
  }
  return (uint8_t)(crc ^ 0xFFU);
}
/* USER CODE END FunctionPrototypes */

void StartCommandTask(void *argument);
void StartCanTxTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */
  /* Create the mutex(es) */
  /* creation of UsartMutex */
  UsartMutexHandle = osMutexNew(&UsartMutex_attributes);

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* creation of CommandQueue */
  CommandQueueHandle = osMessageQueueNew (64, sizeof(uint8_t), &CommandQueue_attributes);

  /* creation of CanTxQueue */
  CanTxQueueHandle = osMessageQueueNew (64, sizeof(uint8_t), &CanTxQueue_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of CommandTask */
  CommandTaskHandle = osThreadNew(StartCommandTask, NULL, &CommandTask_attributes);

  /* creation of CanTxTask */
  CanTxTaskHandle = osThreadNew(StartCanTxTask, NULL, &CanTxTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartCommandTask */
/**
  * @brief  Function implementing the CommandTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartCommandTask */
void StartCommandTask(void *argument)
{
  /* USER CODE BEGIN StartCommandTask */
	uint8_t rx_byte = 0;
	uint8_t rx_buf[8] = {0};
	uint8_t rx_cnt = 0;
  /* Infinite loop */
  for(;;)
  {
	  osMessageQueueGet(CommandQueueHandle, &rx_byte, NULL, osWaitForever);
	  rx_buf[rx_cnt] = rx_byte;
	  rx_cnt++;
	  if(rx_buf[0]!=0xAF)
	  {
		  rx_cnt=0;
	  }
	  else if(rx_cnt>=8)
	  {
		  if(rx_buf[1]>=0x01&&rx_buf[1]<=0x07)
		  {
			  for(uint8_t i=0;i<8;i++)
			  {
				  osMessageQueuePut(CanTxQueueHandle, &rx_buf[i], 0, 0);
			  }
		  }
		  else
		  {
		osMutexAcquire(UsartMutexHandle, osWaitForever);
        printf("[Error] Invalid Sub-ID: 0x%02X\r\n", rx_buf[1]);
        osMutexRelease(UsartMutexHandle);
      }
		  rx_cnt=0;
		  
	  }
    
  }
  /* USER CODE END StartCommandTask */
}

/* USER CODE BEGIN Header_StartCanTxTask */
/**
* @brief Function implementing the CanTxTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartCanTxTask */
void StartCanTxTask(void *argument)
{
  /* USER CODE BEGIN StartCanTxTask */
  uint8_t tx_byte = 0;
  uint8_t tx_buf[8] = {0};
  uint8_t tx_cnt = 0;
  uint8_t rolling_cnt = 0; // ★ E2E Rolling Counter (0 ~ 15 循环递增)
  /* Infinite loop */
  for(;;)
  {
    osMessageQueueGet(CanTxQueueHandle, &tx_byte, NULL, osWaitForever);
    tx_buf[tx_cnt] = tx_byte;
    tx_cnt++;
    if(tx_cnt>=8)
    {
      // ★ E2E 数据保护封装：Byte6 填入 0~15 滚动计数器，Byte7 填入前 7 字节的 SAE J1850 CRC8
      tx_buf[6] = rolling_cnt & 0x0FU;
      rolling_cnt = (rolling_cnt + 1U) & 0x0FU;
      tx_buf[7] = e2e_calc_crc8(tx_buf, 7U);

      osMutexAcquire(UsartMutexHandle, osWaitForever); // 借串口锁
      // ★★★ 这里调用你的 CAN 发射函数！★★★
      can_send_message(0x0B4, 8, tx_buf);
      // 串口 1 打印发送成功回显 (含 E2E Counter 与 CRC8)
      printf("[CAN TX OK] Sub-ID: 0x%02X, Cnt: %u, CRC8: 0x%02X, Data: ",
             tx_buf[1], tx_buf[6], tx_buf[7]);
      for (uint8_t i = 0; i < 8; i++)
      {
        printf("%02X ", tx_buf[i]);
      }
      printf("\r\n");
      osMutexRelease(UsartMutexHandle); // 还串口锁
      tx_cnt = 0; // 清零计数，等下一帧
    }
  }
  /* USER CODE END StartCanTxTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

