/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @project        : STM32F407VET6 车载 CAN ISO-TP 远程固件升级引导程序 (Bootloader)
  * @brief          : Bootloader 核心启动状态机、Flash 参数区校验与 ISO-TP 升级服务入口
  *
  * ================================================================
  * 【文件结构与核心职责】：
  *   1. Flash 三段式物理分区映射 (STM32F407VET6 512KB 非对称扇区)：
  *      - Sector 0~2 (0x08000000 ~ 0x0800BFFF, 48KB)  : Bootloader 引导区 (本工程)
  *      - Sector 3   (0x0800C000 ~ 0x0800FFFF, 16KB)  : 参数区 info.bin (12字节，掉电保存 ota_flag)
  *      - Sector 4~7 (0x08010000 ~ 0x0807FFFF, 448KB) : 用户 APP 应用程序区 (APP1 双表盘 / APP2 黑猫警长)
  *
  *   2. 上电引导状态机 (三路分支)：
  *      ① ota_flag == 0 (正常启动) -> 校验 0x08010000 栈顶指针与向量表合法性 -> 直接跳转 APP
  *      ② ota_flag == 1 (请求升级) -> 擦除 Sector 4~7 (耗时约3s) -> 发送 0x0D3 应答 -> 进入 ISO-TP 接收
  *      ③ APP 区为空/损坏 (防变砖) -> 强制留在 Bootloader 的 ISO-TP 模式等待重刷，永不变砖
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
#include "boot_manager.h"
#include "inter_flashif.h"
#include "inter_flash_cfg.h"
#include "canif.h"
#include "iso_tp_cfg.h"
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
// ★ ISO-TP 升级服务运行标志：1=留在 Bootloader 接收新固件，0=正常跳转 APP
static uint8_t cantp_run_flag = 0U;
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
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */
  // ================================================================
  // 1. Bootloader 启动横幅与分区地址自检打印 (通过 USART2 PA2 输出)
  // ================================================================
  printf("\r\n                    F407 Bootloader (CAN ISO-TP OTA)\r\n");
  printf("------------------------------------------------------------\r\n");
  printf("flash cfg size: %u bytes\r\n",
         (unsigned int)sizeof(inter_flash_cfg_param_typeDef));
  printf("param address : 0x%08lX\r\n", (unsigned long)INTER_FLASH_PARAM_ADDR);
  printf("APP address   : 0x%08lX\r\n", (unsigned long)INTER_FLASH_APP_ADDR);
  printf("------------------------------------------------------------\r\n");

  // ------- 步骤 1：初始化 CAN1 硬件滤波器与 ISO 15765-2 (ISO-TP) 传输层链路 -------
  // CAN1 挂载在 APB1 (42MHz)，位时序 Prescaler=7, BS1=9TQ, BS2=2TQ -> 500 kbps (采样点 83.3%)
  canif_init();
  iso_tp_init();

  // ------- 步骤 2：初始化并校验 Sector 3 (0x0800C000) 参数区 info.bin -------
  // ★ 自动防呆机制：若首次烧录未写 Sector 3 (全 0xFF)，自动写入默认魔数 AA BB CC DD 与 ota_flag=0
  if (inter_flash_cfg_init() != 0U)
  {
      printf("[BOOT] info.bin init failed.\r\n");
      Error_Handler();
  }

  // ------- 步骤 3：读取升级标志位 ota_flag，决定“跳去 APP”还是“留下升级” -------
  //   ret ==  1 : 旧 APP 收到 0x0B3 升级指令后写下了“升级字据”并软复位回来
  //   ret ==  0 : 平时正常上电，直接检查并引导 0x08010000 处的旧 APP
  //   ret == -1 : Flash 参数区硬件读取异常
  int8_t ret = inter_flash_cfg_get_app_update_flag();

  if (ret == 1)
  {
      // ---- 分支 A：执行远程 OTA 升级准备 ----
      // ① 必须先擦除 Sector 4~7 (0x08010000 ~ 0x0807FFFF，共 448KB)，耗时约 3~5 秒
      printf("[BOOT] ota_flag = 1, erasing APP area (Sector 4~7)...\r\n");
      inter_flashif_erase_app_area();

      // ② ⚠️ 极其关键的握手同步：擦除期间 CPU 无法响应高频 CAN 中断，
      //    因此必须等 Flash 彻底擦除干净后，才向网关 ZET6 发送 0x0D3 应答帧！
      //    网关收到 0x0D3 后才会向电脑串口输出 "CCCCC..." 启动 Ymodem 传文件。
      printf("[BOOT] APP area erased, sending 0x0D3 ACK to usb_tool...\r\n");
      uint8_t send_msg[8] = {0xBE, 0xAD, 0xBE, 0xEF, 0x01, 0xAA, 0xAA, 0xAA};
      CAN1_Send_Msg(CAN_ID_CALL_OTA_ACK, send_msg, 8);
      cantp_run_flag = 1U;
  }
  else if (ret == 0)
  {
      // ---- 分支 B：正常上电引导用户 APP ----
      printf("[BOOT] ota_flag = 0, check and jump to APP.\r\n");

      // ★ 防变砖保护：先校验 0x08010000 处的栈顶 MSP 是否落在合法 SRAM 区间 (0x20000000~0x20020000)
      //   若合法则直接跳转 APP（永不返回）；若还未烧录 APP 或上次刷写断电损坏，则留在 Bootloader 等待救援！
      if (boot_check_stack2jump_app(INTER_FLASH_APP_ADDR) != 0U)
      {
          printf("[BOOT] APP is invalid, stay in CAN ISO-TP OTA mode.\r\n");
          cantp_run_flag = 1U;
      }
  }
  else
  {
      printf("[BOOT] failed to read ota_flag.\r\n");
      Error_Handler();
  }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
      // ================================================================
      // 2. CAN ISO-TP 升级服务主循环 (仅在需要 OTA 升级或救砖时运行)
      //    - 接收 0x0B2 多帧数据包 (每块 2048 字节) 并写入 0x08010000 + offset
      //    - 收到 0x0B3 结束帧 (BE AD BE EF 02) 后将 ota_flag 清零并自动跳入新 APP
      // ================================================================
      if (cantp_run_flag == 1U)
      {
          iso_tp_server();
      }
      else
      {
          /* 正常流程不会到达这里：成功跳转后 CPU 已经在 APP 中运行。 */
          printf("[BOOT] unexpected state, rebooting...\r\n");
          HAL_Delay(1000U);
          NVIC_SystemReset();
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
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
int fputc(int ch,FILE *f)
{
	HAL_UART_Transmit(&huart2,(uint8_t*)&ch,1,0xffff);
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
