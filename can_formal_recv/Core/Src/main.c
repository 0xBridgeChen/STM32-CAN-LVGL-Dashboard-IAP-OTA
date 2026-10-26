/* USER CODE BEGIN Header */
/*********************************************************************************
 * @file           : main.c
 * @project        : 汽车方向盘双表盘液晶仪表系统 APP1 (STM32F407VET6 + ST7796 3.5寸 SPI LCD)
 * @author         : Antigravity & Engineering Team (电赛/工业级规范)
 * @brief          : APP1 向量表重映射、CAN 0x0B3 远程升级触发、双表盘阻尼指针与 RTC 协同调度
 * 
 * ================================================================
 * 【系统硬件外设与通信拓扑架构档案】:
 * 
 * 1. 系统主频与时钟树分配:
 *    - 外部高频晶振 HSE = 8 MHz
 *    - 经过主 PLL 倍频: /M(8) * N(336) / P(2) -> 主频 SYSCLK = 168 MHz
 *    - 高速总线 AHB = 168 MHz (驱动 Cortex-M4 内核、128KB SRAM 与 DMA)
 *    - 低速总线 APB1 = 42 MHz (驱动 CAN1 控制器, 波特率 500 kbps, 12TQ)
 *    - 高速总线 APB2 = 84 MHz (驱动 SPI1 主机模式, 42 MBits/s 极速刷屏)
 * 
 * 2. 硬件引脚与外设映射清单:
 *    - 调试串口  : USART2 (TX: PA2) -> 115200 波特率，底层重定向 fputc/printf
 *    - CAN通信   : CAN1 (RX: PB8, TX: PB9) -> 复用 AF9, 外部接 VP230/TJA1050
 *    - 液晶屏显示 : ST7796S (SCK: PA5, MOSI: PA7) -> 硬件 SPI1 主机模式
 *    - 心跳时钟  : SysTick 1ms 硬件心跳 -> LV_TICK_CUSTOM 直接对接 HAL_GetTick()
 *    - 图形引擎  : LVGL 8.3.11 + SquareLine Studio 双表盘仪表 (时速 + 转速 + 报警)
 * 
 * 3. Flash 分区与 CAN OTA 远程升级协同:
 *    - 本工程链接起始地址为 0x08010000 (Sector 4~7, 448KB)
 *    - 收到 CAN ID 0x0B3 (BE AD BE EF 01) 时，将 Sector 3 (0x0800C000) 的 ota_flag 置 1，
 *      随即调用 HAL_NVIC_SystemReset() 软复位进入 Bootloader 接收新固件 (如黑猫警长 APP2)
 * ================================================================
 *********************************************************************************/
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "can.h"
#include "rtc.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"
#include "can_handle.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "headfile.h"
#include "inter_flashif.h"
#include "inter_flash_cfg.h"
#include "lvgl.h"
#include "lv_port_disp_template.h"
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

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void App_UI_Update(void);
void App_RTC_Update(void);
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
  // ⚠️ 极其关键的手术：Bootloader 跳转前执行了 __disable_irq()，
  //    APP 启动第一件事必须将中断向量表重映射至 0x08010000 并重新打开全局中断！
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

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_CAN1_Init();
  MX_USART2_UART_Init();
  MX_SPI1_Init();
  MX_TIM6_Init();
  MX_RTC_Init();
  /* USER CODE BEGIN 2 */
  // ================================================================
  // 系统外设与 GUI 引擎初始化流水线 (严格步骤)
  // ================================================================

  // ------- 步骤 1: LVGL 图形库核心对象与内存池初始化 -------
  lv_init();

  // ------- 步骤 2: 注册显示驱动与局部行缓冲 (ST7796 480x320) -------
  lv_port_disp_init();

  // ------- 步骤 3: 构建由 SquareLine 导出的双仪表盘 UI 对象树 -------
  ui_init();

  // ------- 步骤 4: 启动硬件定时器 TIM6 -------
  HAL_TIM_Base_Start_IT(&htim6);

  // ------- 步骤 5: 配置 CAN1 硬件验收全通滤波器并校验 Sector 3 参数区 -------
  can_filter_init();
  inter_flash_cfg_init();

  // ------- 步骤 6: 实时时钟首帧零延迟秒刷 (开机即准，彻底消灭复位时的 1 秒死文本) -------
  {
      RTC_TimeTypeDef boot_time;
      RTC_DateTypeDef boot_date;
      char boot_t_buf[16];
      char boot_d_buf[16];

      // 立即读取当前 RTC 时间 (成对调用以解锁影子寄存器)
      HAL_RTC_GetTime(&hrtc, &boot_time, RTC_FORMAT_BIN);
      HAL_RTC_GetDate(&hrtc, &boot_date, RTC_FORMAT_BIN);

      // 格式化文本
      sprintf(boot_t_buf, "%02d:%02d:%02d", boot_time.Hours, boot_time.Minutes, boot_time.Seconds);
      sprintf(boot_d_buf, "20%02d-%02d-%02d", boot_date.Year, boot_date.Month, boot_date.Date);

      // 在屏幕首次刷新前，直接将标签覆盖为 RTC 真实时间，开机即准！
      lv_label_set_text(ui_time, boot_t_buf);
      lv_label_set_text(ui_data, boot_d_buf);
  }

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    // ------- 任务 1: LVGL GUI 引擎调度轮询 (脏矩形重绘与动画) -------
    lv_timer_handler();
    HAL_Delay(5);

    // ------- 任务 2: CAN1 报文非阻塞轮询捕获与 OTA 升级请求响应 -------
    can_recv_handle();
    if (flag_ota == 1)
    {
        // ★ 收到 0x0B3 升级唤醒帧：在 Sector 3 (0x0800C000) 立下 ota_flag=1 字据并软复位回 Bootloader
        flag_ota = 0;
        printf("[APP1] Received 0x0B3 OTA request, setting ota_flag=1 and resetting...\r\n");
        inter_flash_cfg_set_app_update_flag(1U);
        HAL_Delay(100);
        HAL_NVIC_SystemReset();
    }

    // ------- 任务 3: 仪表盘 UI 按需刷新 (立红旗机制) -------
    App_UI_Update();

    // ------- 任务 4: RTC 硬件时钟 1 秒周期刷新 -------
    App_RTC_Update();
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
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

  /** Initializes the CPU, AHB and APB buses clocks
  */
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
// ================================================================
// 串口输出重定向 (printf -> USART2 TX PA2)
// ================================================================
int fputc(int ch, FILE *f)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, 0xFFFF);
    return ch;
}

/**
 * @brief  仪表盘 UI 按需刷新 (检查各模块红旗标志位，只在有新数据时渲染)
 */
void App_UI_Update(void)
{
    // 1. 双表盘动力指针阻尼平滑逼近 (时速 0~180 km/h + 转速 0~8000 RPM)
    if (flag_power == 1)
    {
        flag_power = 0; // 收到新数据，红旗降下
    }

    static uint32_t needle_timer = 0;
    static int16_t cur_speed_x10 = 0; // 当前平滑车速 (x10 精度, 0~1800)
    static int16_t cur_rpm_x10 = 0;   // 当前平滑转速 (x10 精度, 0~800)

    // 每 15ms 执行一次微步进阻尼逼近 (约 66 FPS 丝滑物理体验)
    if (HAL_GetTick() - needle_timer >= 15)
    {
        needle_timer = HAL_GetTick();

        // (1) 车速平滑逼近 (限幅 180 km/h)
        int16_t target_s = (speed > 180 ? 180 : speed) * 10;
        int16_t diff_s = target_s - cur_speed_x10;
        if (diff_s != 0)
        {
            int16_t step_s = diff_s / 8;
            if (step_s == 0) step_s = (diff_s > 0) ? 1 : -1;
            cur_speed_x10 += step_s;
            int32_t angle_s = (2250 + (int32_t)cur_speed_x10 * 15 / 10) % 3600;
            lv_img_set_angle(ui_ImageSpeedNeedle, angle_s);
        }

        // (2) 转速平滑逼近 (限幅 8000 RPM, 对应 Rpm=80)
        int16_t target_r = (Rpm > 80 ? 80 : Rpm) * 10;
        int16_t diff_r = target_r - cur_rpm_x10;
        if (diff_r != 0)
        {
            int16_t step_r = diff_r / 8;
            if (step_r == 0) step_r = (diff_r > 0) ? 1 : -1;
            cur_rpm_x10 += step_r;
            int32_t angle_r = (2250 + (int32_t)cur_rpm_x10 * 270 / 80) % 3600;
            lv_img_set_angle(ui_ImageRpmNeedle, angle_r);
        }
    }

    // 2. 水温报警指示灯 (1: 红灯报警, 0: 绿灯正常)
    if (flag_temp == 1)
    {
        flag_temp = 0;
        if (temp == 1)
            lv_obj_set_style_img_recolor(ui_IconTemp, lv_color_hex(0xFF1744), 0); // 报警红
        else
            lv_obj_set_style_img_recolor(ui_IconTemp, lv_color_hex(0x00F374), 0); // 正常绿
        lv_obj_set_style_img_recolor_opa(ui_IconTemp, 255, 0);
    }

    // 3. 安全带指示灯 (1: 红灯未系, 0: 绿灯已系)
    if (flag_belt == 1)
    {
        flag_belt = 0;
        if (belt == 1)
            lv_obj_set_style_img_recolor(ui_IconSeatbelt, lv_color_hex(0xFF1744), 0); // 报警红
        else
            lv_obj_set_style_img_recolor(ui_IconSeatbelt, lv_color_hex(0x00F374), 0); // 正常绿
        lv_obj_set_style_img_recolor_opa(ui_IconSeatbelt, 255, 0);
    }

    // 4. 车灯指示灯 (1: 近光黄, 2: 远光红, 0: 正常绿)
    if (flag_light == 1)
    {
        flag_light = 0;
        if (light == 1)
            lv_obj_set_style_img_recolor(ui_IconLowBeam, lv_color_hex(0xFFD600), 0); // 近光黄
        else if (light == 2)
            lv_obj_set_style_img_recolor(ui_IconLowBeam, lv_color_hex(0xFF1744), 0); // 远光红
        else
            lv_obj_set_style_img_recolor(ui_IconLowBeam, lv_color_hex(0x00F374), 0); // 正常绿
        lv_obj_set_style_img_recolor_opa(ui_IconLowBeam, 255, 0);
    }

    // 5. 转向灯 (1: 绿色开启, 0: 暗灰关闭)
    if (flag_turn_light == 1)
    {
        flag_turn_light = 0;
        if (turn_light == 1)
            lv_obj_set_style_img_recolor(ui_IconTurnLeft, lv_color_hex(0x00F374), 0); // 开启绿
        else
            lv_obj_set_style_img_recolor(ui_IconTurnLeft, lv_color_hex(0x555555), 0); // 关闭暗灰
        lv_obj_set_style_img_recolor_opa(ui_IconTurnLeft, 255, 0);
    }
}

/**
 * @brief  RTC 硬件时钟 1 秒周期刷新 (驱动中央顶部 ui_time 与 ui_data)
 */
void App_RTC_Update(void)
{
    static uint32_t rtc_update_timer = 0;
    if (HAL_GetTick() - rtc_update_timer >= 1000)
    {
        rtc_update_timer = HAL_GetTick();

        RTC_TimeTypeDef cur_time;
        RTC_DateTypeDef cur_date;
        char time_str_buf[16];
        char date_str_buf[16];

        // STM32 HAL 硬件规则: 必须先读 Time 紧接着读 Date，成对调用方可解锁影子寄存器
        HAL_RTC_GetTime(&hrtc, &cur_time, RTC_FORMAT_BIN);
        HAL_RTC_GetDate(&hrtc, &cur_date, RTC_FORMAT_BIN);

        // 格式化时间与日期字符串
        sprintf(time_str_buf, "%02d:%02d:%02d", cur_time.Hours, cur_time.Minutes, cur_time.Seconds);
        sprintf(date_str_buf, "20%02d-%02d-%02d", cur_date.Year, cur_date.Month, cur_date.Date);

        // 刷新 LVGL 文本标签控件
        lv_label_set_text(ui_time, time_str_buf);
        lv_label_set_text(ui_data, date_str_buf);
    }
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
