/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @project        : STM32F103ZET6 汽车方向盘/车辆状态模拟 CAN 发送节点 (can_formalz_Send)
  * @brief          : 串口2 (USART2) 控制指令防错位解析与 CAN1 (0x0B4) 车载矩阵报文广播
  *
  * ================================================================
  * 【文件结构与通信协议规范】：
  *   1. 硬件外设分配：
  *      - USART2 (PA2/PA3, 115200)：接收上位机/方向盘模拟器发来的 8 字节控制指令
  *      - USART1 (PA9/PA10, 115200)：printf 调试监控回显 ([CAN TX OK] Sub-ID...)
  *      - CAN1   (PA11/PA12, 500kbps)：挂载在 36MHz APB1，向仪表 ECU 广播 0x0B4 帧
  *
  *   2. 8 字节串口/CAN 报文矩阵 (CAN ID = 0x0B4，帧头 Byte0 固定为 0xAF)：
  *      - 0xAF 0x01 [RPM] [Speed] 0 0 0 0  — 同时控制转速 (×100 RPM) 与车速 (km/h)
  *      - 0xAF 0x02 [0/1] 0 0 0 0 0        — 控制水温报警指示灯 (1=红灯报警, 0=绿灯正常)
  *      - 0xAF 0x03 [0/1] 0 0 0 0 0        — 控制安全带指示灯   (1=未系红灯, 0=已系绿灯)
  *      - 0xAF 0x04 [0/1/2] 0 0 0 0 0      — 控制车灯指示灯     (1=近光黄, 2=远光红, 0=绿)
  *      - 0xAF 0x05 [0/1] 0 0 0 0 0        — 控制左转向灯       (1=开启绿, 0=关闭暗灰)
  *      - 0xAF 0x06 0x14 [Year] [Mon] [Day]— 远程校准仪表 RTC 日期 (如 0x1A=26年)
  *      - 0xAF 0x07 [Hour] [Min] [Sec] 0 0 — 远程校准仪表 RTC 时间
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
#include <stdio.h>
#include "headfile.h"
typedef struct{
	uint8_t rx_buf[12];
	uint8_t rx_cnt;
} UART_RX_typedef;
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
//#define UART_TEST_ONLY  0
//#define UART_TEST_USE_HSI  0

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
// ★ 串口 2 单字节中断接收缓存与帧组装结构体
uint8_t uart2_rx_byte = 0;
UART_RX_typedef uart2_rx = {0};
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
  MX_CAN_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  // ------- 1. 开启 USART2 单字节接收中断与 CAN1 滤波器 -------
  HAL_UART_Receive_IT(&huart2, &uart2_rx_byte, 1);
  can_filter_init();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
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
// ================================================================
// USART2 接收中断回调：8 字节定长帧组装 + 0xAF 帧头防错位对齐 + CAN 0x0B4 转发
// ================================================================
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if (huart->Instance == USART2)
	{
		// ---- 第一步：收到 1 字节塞入缓存数组 ----
		uart2_rx.rx_buf[uart2_rx.rx_cnt] = uart2_rx_byte;
		uart2_rx.rx_cnt = uart2_rx.rx_cnt + 1;

		// ---- 第二步：★ 防错位机制：只要第 0 个字节不是帧头 0xAF，立刻清零丢弃杂波 ----
		if (uart2_rx.rx_buf[0] != 0xAF)
		{
			uart2_rx.rx_cnt = 0;
		}
		// ---- 第三步：攒齐 8 字节后校验子功能码并投递到 CAN1 总线 (ID: 0x0B4) ----
		else if (uart2_rx.rx_cnt >= 8)
		{
			// 校验：第 1 字节必须是合法子功能 0x01 ~ 0x07
			if (uart2_rx.rx_buf[1] >= 0x01 && uart2_rx.rx_buf[1] <= 0x07)
			{
				// 直接原封不动将 8 字节打入 CAN1 邮箱 (ID: 0x0B4)
				can_send_message(0x0B4, 8, uart2_rx.rx_buf);

				// 串口 1 打印监控反馈
				printf("[CAN TX OK] Sub-ID: 0x%02X, Data: ", uart2_rx.rx_buf[1]);
				for (uint8_t i = 0; i < 8; i++)
				{
					printf("%02X ", uart2_rx.rx_buf[i]);
				}
				printf("\r\n");
			}
			else
			{
				printf("[Error] Invalid Sub-ID: 0x%02X\r\n", uart2_rx.rx_buf[1]);
			}

			// 处理完毕，清空计数等待下一帧
			uart2_rx.rx_cnt = 0;
		}

		// ---- 第四步：重新开启下一次 1 字节接收中断 ----
		HAL_UART_Receive_IT(&huart2, &uart2_rx_byte, 1);
	}
}

int fputc(int ch, FILE *f)
{
  HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 0xFFFF);
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
