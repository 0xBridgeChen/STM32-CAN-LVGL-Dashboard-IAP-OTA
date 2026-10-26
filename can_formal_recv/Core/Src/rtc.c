/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    rtc.c
  * @brief   This file provides code for the configuration
  *          of the RTC instances.
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
#include "rtc.h"

/* USER CODE BEGIN 0 */
// ==============================================================================
// 编译期时间自动提取核心算法 (零开销、零动态内存、杜绝 sscanf 堆栈溢出风险)
// ==============================================================================

/**
  * @brief  从编译器内置宏 __TIME__ ("hh:mm:ss") 中提取当前小时、分钟、秒数
  * @param  sTime: 指向 HAL 库 RTC 时间结构体的指针
  * @note   利用字符下标与 ASCII 码偏移直取计算，耗时仅数个时钟周期，绝对安全可靠
  */
static void RTC_GetCompileTime(RTC_TimeTypeDef *sTime)
{
    // 下标 0, 1: 小时十位与个位 (例如 "22" -> 2 * 10 + 2 = 22)
    sTime->Hours   = (__TIME__[0] - '0') * 10 + (__TIME__[1] - '0');
    // 下标 3, 4: 分钟十位与个位 (跳过下标 2 的冒号 ':')
    sTime->Minutes = (__TIME__[3] - '0') * 10 + (__TIME__[4] - '0');
    // 下标 6, 7: 秒钟十位与个位 (跳过下标 5 的冒号 ':')
    sTime->Seconds = (__TIME__[6] - '0') * 10 + (__TIME__[7] - '0');
    sTime->DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
    sTime->StoreOperation = RTC_STOREOPERATION_RESET;
}

/**
  * @brief  从编译器内置宏 __DATE__ ("Mmm dd yyyy") 中提取当前年份、月份、日期
  * @param  sDate: 指向 HAL 库 RTC 日期结构体的指针
  * @note   支持单数日空格占位(如 "Oct  7")与双数日(如 "Oct 17")，兼容 C 标准规范
  */
static void RTC_GetCompileDate(RTC_DateTypeDef *sDate)
{
    // 1. 年份提取: 取最后两位数字 (例如 "2026" 取 '2' 和 '6' -> 26)
    sDate->Year = (__DATE__[9] - '0') * 10 + (__DATE__[10] - '0');
    
    // 2. 日期提取: 若小于 10 号，C 宏以下标 4 补空格 (如 "Oct  7 2026")
    if (__DATE__[4] == ' ') {
        sDate->Date = (__DATE__[5] - '0');
    } else {
        sDate->Date = (__DATE__[4] - '0') * 10 + (__DATE__[5] - '0');
    }

    // 3. 月份提取: 比对前 3 个字符快速映射为 HAL 库标准枚举宏 (比 strcmp 快数十倍)
    const char *m = __DATE__;
    if      (m[0]=='J' && m[1]=='a' && m[2]=='n') sDate->Month = RTC_MONTH_JANUARY;
    else if (m[0]=='F' && m[1]=='e' && m[2]=='b') sDate->Month = RTC_MONTH_FEBRUARY;
    else if (m[0]=='M' && m[1]=='a' && m[2]=='r') sDate->Month = RTC_MONTH_MARCH;
    else if (m[0]=='A' && m[1]=='p' && m[2]=='r') sDate->Month = RTC_MONTH_APRIL;
    else if (m[0]=='M' && m[1]=='a' && m[2]=='y') sDate->Month = RTC_MONTH_MAY;
    else if (m[0]=='J' && m[1]=='u' && m[2]=='n') sDate->Month = RTC_MONTH_JUNE;
    else if (m[0]=='J' && m[1]=='u' && m[2]=='l') sDate->Month = RTC_MONTH_JULY;
    else if (m[0]=='A' && m[1]=='u' && m[2]=='g') sDate->Month = RTC_MONTH_AUGUST;
    else if (m[0]=='S' && m[1]=='e' && m[2]=='p') sDate->Month = RTC_MONTH_SEPTEMBER;
    else if (m[0]=='O' && m[1]=='c' && m[2]=='t') sDate->Month = RTC_MONTH_OCTOBER;
    else if (m[0]=='N' && m[1]=='o' && m[2]=='v') sDate->Month = RTC_MONTH_NOVEMBER;
    else if (m[0]=='D' && m[1]=='e' && m[2]=='c') sDate->Month = RTC_MONTH_DECEMBER;

    // 默认工作日设置为星期一 (不影响时钟走时)
    sDate->WeekDay = RTC_WEEKDAY_MONDAY;
}
/* USER CODE END 0 */

RTC_HandleTypeDef hrtc;

/* RTC init function */
void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef sDate = {0};

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */

  /** Initialize RTC Only
  */
  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv = 127;
  hrtc.Init.SynchPrediv = 255;
  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN Check_RTC_BKUP */
  // 必须使能备份域写访问权限，才能正常读写 RTC 与 BKP 备份数据寄存器
  HAL_PWR_EnableBkUpAccess();

  // 检查备份数据寄存器 1 (RTC_BKP_DR1) 是否已经写入过防倒流特征字 0xA0A0:
  // - 若 != 0xA0A0: 说明是初次上电/固件首次烧录，立即写入电脑当下的真实编译时间
  // - 若 == 0xA0A0: 说明此前已完成初始化(例如用户按下了 Reset 键)，直接跳过，RTC 芯片硬件继续走时，杜绝时间倒流！
  if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR1) != 0xA0A0)
  {
      // 1. 从编译宏提取当下准确的电脑时分秒与年月日
      RTC_GetCompileTime(&sTime);
      RTC_GetCompileDate(&sDate);

      // 2. 写入硬件 RTC 寄存器 (使用 BIN 进制格式)
      if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK)
      {
          Error_Handler();
      }
      if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK)
      {
          Error_Handler();
      }

      // 3. 向备份域寄存器写入 0xA0A0 保护特征字
      HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR1, 0xA0A0);
  }

  // 关键保护：直接 return，彻底跳过下方 CubeMX 默认生成的 00:00:00 覆盖代码！
  return;
  /* USER CODE END Check_RTC_BKUP */

  /** Initialize RTC and set the Time and Date
  */
  sTime.Hours = 0x0;
  sTime.Minutes = 0x0;
  sTime.Seconds = 0x0;
  sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sTime.StoreOperation = RTC_STOREOPERATION_RESET;
  if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }
  sDate.WeekDay = RTC_WEEKDAY_MONDAY;
  sDate.Month = RTC_MONTH_JANUARY;
  sDate.Date = 0x1;
  sDate.Year = 0x0;

  if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN RTC_Init 2 */

  /* USER CODE END RTC_Init 2 */

}

void HAL_RTC_MspInit(RTC_HandleTypeDef* rtcHandle)
{

  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
  if(rtcHandle->Instance==RTC)
  {
  /* USER CODE BEGIN RTC_MspInit 0 */

  /* USER CODE END RTC_MspInit 0 */

  /** Initializes the peripherals clock
  */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_RTC;
    PeriphClkInitStruct.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
      Error_Handler();
    }

    /* RTC clock enable */
    __HAL_RCC_RTC_ENABLE();
  /* USER CODE BEGIN RTC_MspInit 1 */

  /* USER CODE END RTC_MspInit 1 */
  }
}

void HAL_RTC_MspDeInit(RTC_HandleTypeDef* rtcHandle)
{

  if(rtcHandle->Instance==RTC)
  {
  /* USER CODE BEGIN RTC_MspDeInit 0 */

  /* USER CODE END RTC_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_RTC_DISABLE();
  /* USER CODE BEGIN RTC_MspDeInit 1 */

  /* USER CODE END RTC_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
