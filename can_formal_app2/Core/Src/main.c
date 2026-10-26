/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @project        : STM32F407VET6 汽车数字仪表 APP2 (黑猫警长主题 LVGL 8 座舱系统)
  * @brief          : APP2 向量表重映射、CAN 0x0B3 远程升级响应与 50FPS 座舱动画调度
  *
  * ================================================================
  * 【文件结构与核心机制】：
  *   1. 启动重映射 (0x08010000)：
  *      - 配合 Keil IROM1 起始地址 0x08010000 (大小 0x70000 = 448KB)
  *      - 入口第一句执行 SCB->VTOR = 0x08010000 并调用 __set_PRIMASK(0U) 重新开启总中断
  *   2. 双向 CAN OTA 升级闭环：
  *      - 实时监听 CAN ID 0x0B3 (BE AD BE EF 01 AA AA AA)
  *      - 收到后将 Sector 3 (0x0800C000) 的 ota_flag 置 1，立即调用 HAL_NVIC_SystemReset()
  *      - 重启进入 Bootloader 后即可无缝刷回 APP1 (APP_Car.bin) 或更新版本
  *   3. 车载数据联动与离线巡航演示：
  *      - 在线模式：收到 0x0B4 (0xAF 0x01 RPM Speed) 实时驱动黑猫警长仪表车速与转速
  *      - 离线模式：超过 1.2s 未收到 CAN 报文时自动切入三角波平滑演示动效
  * ================================================================
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "can.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "headfile.h"
#include "inter_flashif.h"
#include "inter_flash_cfg.h"
#include "bsp_touch.h"
#include "bsp_lv_port_disp.h"
#include "bsp_lv_port_indev.h"
#include "blackcat_lvgl_ui.h"
#include <stdio.h>
#include <string.h>
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

/* USER CODE BEGIN PV */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
  // ⚠️ 极其关键的手术：Bootloader 跳转前关闭了总中断 (__disable_irq)，
  //    因此 APP 入口必须第一时间重设中断向量表偏移 VTOR = 0x08010000 并开启总中断！
  SCB->VTOR = INTER_FLASH_APP_ADDR;
  __set_PRIMASK(0U);
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  __set_PRIMASK(0U);

  uint32_t rx_id = 0;
  uint32_t rx_len = 0;
  uint8_t rx_data[8] = {0};
  BlackCat_VehicleState v_state = {0};
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_CAN1_Init();
  MX_USART2_UART_Init();
  MX_SPI1_Init();
  MX_USART3_UART_Init();

  /* USER CODE BEGIN 2 */
  // ================================================================
  // 系统外设与 LVGL 黑猫警长座舱初始化流水线
  // ================================================================

  // ------- 1. 硬件屏幕 (ST7796 SPI1) 与底层接口初始化 -------
  LCD_Init();
  Touch_Init();

  // ------- 2. LVGL 8 图形系统与黑猫警长仪表盘页面初始化 -------
  lv_init();
  lv_port_disp_init();
  lv_port_indev_init();
  BlackCat_LVGL_Init();

  // ------- 3. 启动 CAN1 硬件滤波器与接收中断 -------
  can_filter_init();

  // ------- 4. 开启 USART3 备用升级命令监听 -------
  uart_receive_enable();

  printf("\r\n============================================================\r\n");
  printf("       F407 APP2: BLACK CAT DETECTIVE LVGL 8 COCKPIT        \r\n");
  printf("============================================================\r\n");
  printf("Firmware Version : V2.2.0 (Black Cat LVGL Edition)\r\n");
  printf("Vector Table     : 0x%08lX\r\n", (unsigned long)SCB->VTOR);
  printf("LVGL Version     : %d.%d.%d\r\n", LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);
  printf("USART3 OTA Listen: AF LEN DATA... FA\r\n");
  printf("------------------------------------------------------------\r\n");

  if (inter_flash_cfg_init() != 0U)
  {
      printf("APP2: info.bin not found at 0x%08lX (using defaults)\r\n",
             (unsigned long)INTER_FLASH_PARAM_ADDR);
  }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  uint32_t rx_count = 0;
  uint32_t last_can_tick = 0U;
  uint32_t last_ui_tick = HAL_GetTick();
  uint32_t last_lvgl_tick = HAL_GetTick();

  while (1)
  {
      uint32_t now = HAL_GetTick();

      // ================================================================
      // 任务 1：CAN 报文非阻塞接收与协议分流 (0x0B3 升级唤醒 / 0x0B4 仪表数据)
      // ================================================================
      if (can_get_message(&rx_id, &rx_len, rx_data) == 0U)
      {
          // ---- ① 检测 CAN 0x0B3 远程 OTA 升级指令 (BE AD BE EF 01 AA AA AA) ----
          if ((rx_id == 0x0B3U) && (rx_len == 8U) &&
              (rx_data[0] == 0xBEU) && (rx_data[1] == 0xADU) &&
              (rx_data[2] == 0xBEU) && (rx_data[3] == 0xEFU) &&
              (rx_data[4] == 0x01U))
          {
              printf("APP2: CAN 0x0B3 OTA Triggered! Setting ota_flag=1 and resetting...\r\n");
              inter_flash_cfg_set_app_update_flag(1U);
              HAL_Delay(100);
              HAL_NVIC_SystemReset();
          }

          ++rx_count;
          last_can_tick = now;
          v_state.can_online = 1U;
          v_state.can_id = rx_id;
          v_state.rx_count = rx_count;
          v_state.dlc = (rx_len <= 8U) ? (uint8_t)rx_len : 8U;
          memcpy(v_state.data, rx_data, v_state.dlc);

          // ---- ② 解析 0x0B4 标准仪表数据帧 (0xAF + 0x01 + RPM + Speed) ----
          if ((rx_id == 0x0B4U) && (rx_data[0] == 0xAFU))
          {
              if (rx_data[1] == 0x01U)
              {
                  v_state.rpm = (uint16_t)rx_data[2] * 100U;
                  v_state.speed_kph = (uint16_t)rx_data[3];
                  if (v_state.rpm > 8000U) v_state.rpm = 8000U;
              }
          }
          else
          {
              uint16_t base_spd = (v_state.mode == BLACKCAT_MODE_PURSUIT) ? 120U : 45U;
              v_state.speed_kph = base_spd + (uint16_t)((rx_data[0] * 3U + rx_count) % 55U);
              v_state.rpm = (uint16_t)(900U + v_state.speed_kph * 38U);
              if (v_state.rpm > 8000U) v_state.rpm = 8000U;
          }

          printf("[CAN] ID=0x%03lX DLC=%lu RX=%lu\r\n",
                 (unsigned long)rx_id, (unsigned long)rx_len, (unsigned long)rx_count);
      }
      else if ((last_can_tick == 0U) || ((now - last_can_tick) > 1200U))
      {
          // ---- ③ 无 CAN 报文时的平滑自动巡航演示动效 ----
          v_state.can_online = 0U;
          uint16_t phase = (uint16_t)((now / 80U) % 240U);
          uint16_t wave = (phase <= 120U) ? phase : (uint16_t)(240U - phase);
          v_state.speed_kph = (v_state.mode == BLACKCAT_MODE_PURSUIT) ?
                              (uint16_t)(70U + wave / 2U) : (uint16_t)(25U + wave / 3U);
          v_state.rpm = (uint16_t)(900U + v_state.speed_kph * 38U);
      }

      // ================================================================
      // 任务 2：仪表盘业务动效定时更新 (20ms 周期 = 50 FPS)
      // ================================================================
      if ((now - last_ui_tick) >= 20U)
      {
          last_ui_tick = now;
          BlackCat_LVGL_Update(&v_state, now);
      }

      // ================================================================
      // 任务 3：LVGL 核心时间片引擎调度 (5ms 周期)
      // ================================================================
      if ((now - last_lvgl_tick) >= 5U)
      {
          last_lvgl_tick = now;
          lv_timer_handler();
      }

      // ================================================================
      // 任务 4：USART3 备用串口 OTA 协议检测与自动跳转
      // ================================================================
      if (uart_get_ota_flag() == 1U)
      {
          inter_flash_cfg_set_app_update_flag(1U);
          printf("APP2: OTA Triggered! Resetting to Bootloader...\r\n");
          HAL_Delay(100);
          HAL_NVIC_SystemReset();
      }
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

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
int fputc(int ch, FILE *f)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, 10);
    return ch;
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  (void)file;
  (void)line;
}
#endif /* USE_FULL_ASSERT */
