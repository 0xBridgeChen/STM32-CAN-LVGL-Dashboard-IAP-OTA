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
#include "can_handle.h"
#include "rtc.h"
#include "inter_flash_cfg.h"
#include "lvgl.h"
#include "ui.h"
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
/* Definitions for GuiTask */
osThreadId_t GuiTaskHandle;
const osThreadAttr_t GuiTask_attributes = {
  .name = "GuiTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for CanTask */
osThreadId_t CanTaskHandle;
const osThreadAttr_t CanTask_attributes = {
  .name = "CanTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};
/* Definitions for RtcTask */
osThreadId_t RtcTaskHandle;
const osThreadAttr_t RtcTask_attributes = {
  .name = "RtcTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for CanRxQueue */
osMessageQueueId_t CanRxQueueHandle;
const osMessageQueueAttr_t CanRxQueue_attributes = {
  .name = "CanRxQueue"
};
/* Definitions for LvglMutex */
osMutexId_t LvglMutexHandle;
const osMutexAttr_t LvglMutex_attributes = {
  .name = "LvglMutex"
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

void StartDefaultTask(void *argument);
void StartCanTask(void *argument);
void StartRtcTask(void *argument);

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
  /* creation of LvglMutex */
  LvglMutexHandle = osMutexNew(&LvglMutex_attributes);

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
  /* creation of CanRxQueue */
  CanRxQueueHandle = osMessageQueueNew (90, sizeof(uint8_t), &CanRxQueue_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of GuiTask */
  GuiTaskHandle = osThreadNew(StartDefaultTask, NULL, &GuiTask_attributes);

  /* creation of CanTask */
  CanTaskHandle = osThreadNew(StartCanTask, NULL, &CanTask_attributes);

  /* creation of RtcTask */
  RtcTaskHandle = osThreadNew(StartRtcTask, NULL, &RtcTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the GuiTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  static int16_t cur_speed_x10 = 0; // 当前时速指针 (0 ~ 1800, 对应 0 ~ 180 km/h)
  static int16_t cur_rpm_x10 = 0;   // 当前转速指针 (0 ~ 800, 对应 0 ~ 8000 RPM)

  /* Infinite loop */
  for(;;)
  {
    // 步骤 1：借钥匙进入 LVGL 安全区
    osMutexAcquire(LvglMutexHandle, osWaitForever);

    // 步骤 2：车速指针平滑逼近 (限幅 180 km/h)
    int16_t target_s = (speed > 180 ? 180 : speed) * 10;
    int16_t diff_s = target_s - cur_speed_x10;
    if (diff_s != 0)
    {
      int16_t step_s = diff_s / 8;
      if (step_s == 0) step_s = (diff_s > 0) ? 1 : -1;
      cur_speed_x10 += step_s;
      int32_t angle_s = (2250 + (int32_t)cur_speed_x10 * 15 / 10) % 3600;
      lv_img_set_angle(ui_ImageSpeedNeedle, angle_s); // 旋转时速表指针
    }

    // 步骤 3：转速指针平滑逼近 (限幅 8000 RPM, 对应 Rpm=80)
    int16_t target_r = (Rpm > 80 ? 80 : Rpm) * 10;
    int16_t diff_r = target_r - cur_rpm_x10;
    if (diff_r != 0)
    {
      int16_t step_r = diff_r / 8;
      if (step_r == 0) step_r = (diff_r > 0) ? 1 : -1;
      cur_rpm_x10 += step_r;
      int32_t angle_r = (2250 + (int32_t)cur_rpm_x10 * 270 / 80) % 3600;
      lv_img_set_angle(ui_ImageRpmNeedle, angle_r);   // 旋转转速表指针
    }

    // 步骤 4：LVGL 渲染引擎重绘脏矩形
    lv_timer_handler();

    // 步骤 5：还钥匙，退出安全区
    osMutexRelease(LvglMutexHandle);

    // 步骤 6：上电秒显后，精准休眠 15ms，保持 66 FPS 丝滑动画
    osDelay(15);
  }
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_StartCanTask */
/**
* @brief Function implementing the CanTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartCanTask */
void StartCanTask(void *argument)
{
  /* USER CODE BEGIN StartCanTask */
  uint8_t rx_byte = 0;       // 1. 用来接队列里弹出来的单字节
  uint8_t msg_buf[9] = {0};  // 2. 用来拼装 1帧完整的报文 (1字节ID + 8字节数据 = 9字节)
  uint8_t msg_idx = 0;       // 3. 记录当前拼到了第几个字节 (0 ~ 9)
  /* Infinite loop */
  for(;;)
  {
    osMessageQueueGet(CanRxQueueHandle, &rx_byte, NULL, osWaitForever);
    // 1. 如果当前处于第 0 步：找有效报文 ID (0x0B3 升级帧 或 0x0B4 仪表帧)
    if (msg_idx == 0)
    {
      if (rx_byte == CAN_ID_CALL_OTA || rx_byte == CAN_ID_UI)
      {
        msg_buf[msg_idx++] = rx_byte; // 抓到合法 ID，存进第 0 位，msg_idx 变成 1
      }
    }
    // 2. 已经抓到了 ID，接下来老老实实接收后面的 8 个数据字节
    else
    {
      msg_buf[msg_idx++] = rx_byte;
      // 3. 凑齐了 9 个字节！一帧完整的报文到手！
      if (msg_idx == 9)
      {
        uint8_t r_id = msg_buf[0];
        uint8_t *get_data = &msg_buf[1];
        // ---- 分支 A：收到 0x0B3 远程 OTA 升级唤醒帧 ----
        if (r_id == CAN_ID_CALL_OTA)
        {
          // 校验暗号：BE AD BE EF 01
          if (get_data[0] == 0xBE && get_data[1] == 0xAD &&
              get_data[2] == 0xBE && get_data[3] == 0xEF &&
              get_data[4] == 0x01)
          {
            // ★ ISO 26262 功能安全前置互锁：必须车速=0 且 转速=0 才允许进入刷写会话
            if (speed == 0 && Rpm == 0)
            {
              printf("[Safe-OTA] Interlock PASS (Speed=0, RPM=0). Resetting to Bootloader...\r\n");
              inter_flash_cfg_set_app_update_flag(1U); // 写入 Sector 3 升级标志位
              osDelay(100);                            // RTOS 延时 100ms 保证串口打印发完
              HAL_NVIC_SystemReset();                  // 软复位，直接跳进 Bootloader！
            }
            else
            {
              // 行车中拒绝升级（对标 UDS NRC 0x22 ConditionsNotCorrect），不写标志位、不复位
              printf("[Safe-OTA] REJECTED (NRC 0x22)! Vehicle moving: Speed=%d km/h, RPM=%d00\r\n", speed, Rpm);
            }
          }
        }
        // ---- 分支 B：收到 0x0B4 仪表盘控制帧 (首字节必须是 0xAF 帧头) ----
        else if (r_id == CAN_ID_UI && get_data[0] == CAN_MSG_HEAD)
        {
          // ★ E2E 校验：核对 Byte7 的 SAE J1850 CRC8 是否匹配前 7 字节
          uint8_t calc_crc = e2e_calc_crc8(get_data, 7U);
          if (get_data[7] != calc_crc)
          {
            printf("[E2E Error] CRC8 mismatch! Recv: 0x%02X, Calc: 0x%02X\r\n", get_data[7], calc_crc);
            msg_idx = 0;
            continue;
          }

          switch (get_data[1])
          {
            case E_UI_TYPE_RPM: // 0x01: 发动机转速与时速
              Rpm = get_data[2];
              speed = get_data[3];
              break;

            case E_UI_TYPE_TEMP: // 0x02: 水温报警
              temp = get_data[2];
              osMutexAcquire(LvglMutexHandle, osWaitForever);
              if (temp == 1)
                lv_obj_set_style_img_recolor(ui_IconTemp, lv_color_hex(0xFF1744), 0); // 报警红
              else
                lv_obj_set_style_img_recolor(ui_IconTemp, lv_color_hex(0x00F374), 0); // 正常绿
              lv_obj_set_style_img_recolor_opa(ui_IconTemp, 255, 0);
              osMutexRelease(LvglMutexHandle);
              break;

            case E_UI_TYPE_BELT: // 0x03: 安全带报警
              belt = get_data[2];
              osMutexAcquire(LvglMutexHandle, osWaitForever); // 1. 借钥匙
              if (belt == 1)
                lv_obj_set_style_img_recolor(ui_IconSeatbelt, lv_color_hex(0xFF1744), 0); // 报警红
              else
                lv_obj_set_style_img_recolor(ui_IconSeatbelt, lv_color_hex(0x00F374), 0); // 正常绿
              lv_obj_set_style_img_recolor_opa(ui_IconSeatbelt, 255, 0);
              osMutexRelease(LvglMutexHandle);                // 2. 还钥匙
              break;

            case E_UI_TYPE_LIGHT: // 0x04: 大灯指示
              light = get_data[2];
              osMutexAcquire(LvglMutexHandle, osWaitForever); // 1. 借钥匙
              if (light == 1)
                lv_obj_set_style_img_recolor(ui_IconLowBeam, lv_color_hex(0xFFD600), 0); // 近光黄
              else if (light == 2)
                lv_obj_set_style_img_recolor(ui_IconLowBeam, lv_color_hex(0xFF1744), 0); // 远光红
              else
                lv_obj_set_style_img_recolor(ui_IconLowBeam, lv_color_hex(0x00F374), 0); // 正常绿
              lv_obj_set_style_img_recolor_opa(ui_IconLowBeam, 255, 0);
              osMutexRelease(LvglMutexHandle);
              break;

            case E_UI_TYPE_TURN_LIGHT: // 0x05: 转向灯
              turn_light = get_data[2];
              osMutexAcquire(LvglMutexHandle, osWaitForever); // 1. 借钥匙
              if (turn_light == 1)
                lv_obj_set_style_img_recolor(ui_IconTurnLeft, lv_color_hex(0x00F374), 0); // 开启绿
              else
                lv_obj_set_style_img_recolor(ui_IconTurnLeft, lv_color_hex(0x555555), 0); // 关闭暗灰
              lv_obj_set_style_img_recolor_opa(ui_IconTurnLeft, 255, 0);
              osMutexRelease(LvglMutexHandle);
              break;

            case E_UI_TYPE_DATE: // 0x06: 年月日校准
            {
              RTC_DateTypeDef sDate = {0};
              sDate.Year = get_data[3];
              sDate.Month = get_data[4];
              sDate.Date = get_data[5];
              sDate.WeekDay = RTC_WEEKDAY_MONDAY;
              HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
              break;
            }

            case E_UI_TYPE_TIME: // 0x07: 时分秒校准
            {
              RTC_TimeTypeDef sTime = {0};
              sTime.Hours = get_data[2];
              sTime.Minutes = get_data[3];
              sTime.Seconds = get_data[4];
              HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
              break;
            }
            default:
              break;
          }
        }
        msg_idx = 0; // 处理完，重置状态机计数器，准备等下一帧！
      }
    }
  }
  /* USER CODE END StartCanTask */
}

/* USER CODE BEGIN Header_StartRtcTask */
/**
* @brief Function implementing the RtcTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartRtcTask */
void StartRtcTask(void *argument)
{
  /* USER CODE BEGIN StartRtcTask */
	    RTC_TimeTypeDef cur_time;
    RTC_DateTypeDef cur_date;
    char time_str_buf[16];
    char date_str_buf[16];
  /* Infinite loop */
  for(;;)
  {
	    // 2. 读取硬件 RTC (成对读取，解锁影子寄存器)
    HAL_RTC_GetTime(&hrtc, &cur_time, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &cur_date, RTC_FORMAT_BIN);
    // 3. 格式化为字符串
    sprintf(time_str_buf, "%02d:%02d:%02d", cur_time.Hours, cur_time.Minutes, cur_time.Seconds);
    sprintf(date_str_buf, "20%02d-%02d-%02d", cur_date.Year, cur_date.Month, cur_date.Date);
	      // 3. 借钥匙进入安全区 (防止与 GuiTask 冲突)
    osMutexAcquire(LvglMutexHandle, osWaitForever);

    // 4. 极速更新屏幕顶部的两个文本标签
    lv_label_set_text(ui_time, time_str_buf);
    lv_label_set_text(ui_data, date_str_buf);

    // 5. 立刻还钥匙，退出安全区
    osMutexRelease(LvglMutexHandle);
	  
    osDelay(1000);
  }
  /* USER CODE END StartRtcTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */
