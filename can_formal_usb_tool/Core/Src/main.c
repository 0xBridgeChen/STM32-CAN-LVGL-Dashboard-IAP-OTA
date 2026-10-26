/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @project        : STM32F103ZET6 车载 CAN ISO-TP 固件刷写网关 (can_formal_usb_tool)
  * @brief          : 网关三阶段刷写状态机：串口握手 -> CAN 0x0B3/0x0D3 唤醒擦除确认 -> Ymodem 转 ISO-TP
  *
  * ================================================================
  * 【文件结构与网关核心工作流】：
  *   1. 硬件外设分配：
  *      - USART2 (PA2/PA3, 115200) ：连接上位机终端，接收 AF 00 01 FA 升级指令与 Ymodem .bin 文件
  *      - USART1 (PA9/PA10, 115200)：printf 调试日志输出口 (打印刷写块进度与完成状态)
  *      - CAN1   (PA11/PA12, 500k) ：与目标仪表 ECU (STM32F407VET6) 进行 ISO-TP 多帧刷写通信
  *
  *   2. 三阶段网关刷写状态机：
  *      ① 阶段一 (等待上位机指令)：USART2 收到 "AF 00 01 FA" -> 向 CAN 发送 0x0B3 (BE AD BE EF 01) 唤醒帧
  *      ② 阶段二 (等待目标板擦除)：F407 软复位进 Bootloader 擦除 448KB APP 区 -> 擦完回传 0x0D3 应答帧
  *      ③ 阶段三 (Ymodem 转 ISO-TP)：网关收到 0x0D3 后启动 Ymodem_Receive()，每攒满 2048B 经 0x0B2 发往 F407
  * ================================================================
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "can.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "boot_manager.h"
#include "inter_flashif.h"
#include "inter_flash_cfg.h"
#include "ymodem.h"
#include "common.h"
#include <stdio.h>
#include "canif.h"
#include "isotp.h"
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

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
uint8_t ymodule_run_flag = 0; // ★ Ymodem 接收与 ISO-TP 转发启动标志
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// ★ 等待 F407 擦除 Sector 4~7 (448KB) 并回复 0x0D3 的最大超时阈值 (15 秒)
#define OTA_CALL_MAX_WAIT           (15*1000)
uint8_t tx_data[8] = {0};

static uint8_t pData[128];
static uint8_t ota_call_flag = 0;   // 等待 0x0D3 应答状态标志
static uint32_t ota_call_tick = 0;  // 升级呼叫超时计时器
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
  MX_USART1_UART_Init();
  MX_CAN_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  // ================================================================
  // 1. 初始化 CAN1 接口层与 ISO 15765-2 (ISO-TP) 协议栈
  // ================================================================
  printf("uart1:usb tool...\r\n");
  HAL_UART_Transmit(&huart2, (uint8_t *)"uart2:usb tool...\r\n", strlen("uart2:usb tool\r\n"), 0xff);
  canif_init();
  iso_tp_init();

  while (1)
  {
    // ================================================================
    // 阶段一：监听上位机串口 2 (USART2) 发来的 4 字节升级握手指令 (AF 00 01 FA)
    // ================================================================
    if (HAL_UART_Receive(&huart2, pData, 4, 0xFF) == HAL_OK)
    {
      if ((pData[0] == 0xAF) && (pData[3] == 0xFA))
      {
        if (pData[2] == 0x01)
        {
            printf("Upgrade will start...\r\n");

            // 向 CAN 总线广播 0x0B3 升级唤醒帧，通知 F407 写入 ota_flag=1 并重启进 Bootloader
            uint8_t send_msg[8] = {0xBE, 0xAD, 0xBE, 0xEF, 0x00, 0xAA, 0xAA, 0xAA};
            send_msg[4] = 0x01;
            CAN1_Send_Msg(CAN_ID_CALL_OTA, (uint8_t *)send_msg, 8);
            ota_call_flag = 1;
            ota_call_tick = OTA_CALL_MAX_WAIT + HAL_GetTick();
        }
      }
    }

    // ================================================================
    // 阶段二：等待目标板 F407 擦除完 Flash 后回传 0x0D3 准备就绪应答
    // ================================================================
    if (ota_call_flag == 1)
    {
        if ((ota_call_tick >= HAL_GetTick()) && (ota_call_tick != 0))
        {
          uint32_t recv_id = 0;
          uint8_t recv_len = 0;
          uint8_t rxdata[8] = {0};

          recv_len = CAN1_Recv_Msg(&recv_id, rxdata);
          if (recv_len > 0)
          {
            if (recv_id == CAN_ID_CALL_OTA_ACK)
            {
                // ★ 收到 F407 的 0x0D3 应答，说明对端 Flash 已擦空，可以启动 Ymodem 传文件！
                printf("Upgrade ack received...\r\n");
                ota_call_tick = 0;
                ota_call_flag = 0;
                ymodule_run_flag = 1;
            }
          }
        }
        else if ((ota_call_tick < HAL_GetTick()) && (ota_call_tick != 0))
        {
            ota_call_flag = 0;
            ota_call_tick = 0;
            printf("Upgrade timeout...\r\n");
        }
    }

    // ================================================================
    // 阶段三：启动 Ymodem 协议接收上位机 .bin 固件并经 ISO-TP 实时转发
    // ================================================================
    if (ymodule_run_flag == 1)
    {
        printf("Ymodule&TPClient is run,waiting for upgrade...\r\n");
        extern uint8_t aFileName[FILE_NAME_LENGTH];

        uint8_t number[11] = {0};
        uint32_t size = 0;
        COM_StatusTypeDef result;
        result = Ymodem_Receive(&size);
        if (result == COM_OK)
        {
            printf("\n\n\r Programming Completed Successfully!\n\r--------------------------------\r\n Name: %s", aFileName);

            Int2Str(number, size);
            printf("Size: %sBytes\r\n", number);

            // 升级完成后清除标志，回到待机监听状态
            ymodule_run_flag = 0;
        }
        else
        {
            printf("=---<ret:%d\r\n", result);
        }
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

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

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

#ifdef  USE_FULL_ASSERT
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
