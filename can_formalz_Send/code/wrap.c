/**
 * @file    wrap.c
 * @brief   STM32F103ZET6 发送端 CAN1 硬件滤波器、发送邮箱投递与 FIFO0 接收封装
 *
 * 文件结构：
 *   1. can_filter_init()   — 配置 32 位掩码模式全通滤波器并启动 CAN1
 *   2. can_send_message()  — 检查 Bus-Off 与空闲发送邮箱，投递 8 字节标准帧
 *   3. can_get_message()   — 从 CAN_RX_FIFO0 轮询提取接收报文
 */

#include "headfile.h"

// ================================================================
// 1. CAN1 硬件验收滤波器初始化 (FilterBank 0 -> FIFO0，全通掩码模式)
// ================================================================
void can_filter_init(void)
{
  CAN_FilterTypeDef Can_ConfigFilter = {0};
  Can_ConfigFilter.FilterActivation = CAN_FILTER_ENABLE;
  Can_ConfigFilter.FilterBank = 0;
  Can_ConfigFilter.FilterFIFOAssignment = CAN_FILTER_FIFO0;
  Can_ConfigFilter.FilterIdHigh = 0x0000;
  Can_ConfigFilter.FilterIdLow = 0x0000;
  Can_ConfigFilter.FilterMaskIdHigh = 0x0000;
  Can_ConfigFilter.FilterMaskIdLow = 0x0000;
  Can_ConfigFilter.FilterMode = CAN_FILTERMODE_IDMASK;
  Can_ConfigFilter.FilterScale = CAN_FILTERSCALE_32BIT;
  Can_ConfigFilter.SlaveStartFilterBank = 14;

  if (HAL_CAN_ConfigFilter(&hcan, &Can_ConfigFilter) != HAL_OK)
  {
    printf("filter error\r\n");
    Error_Handler();
  }
  printf("filter ok\r\n");
  HAL_CAN_Start(&hcan);
}

// ================================================================
// 2. CAN1 标准数据帧发送封装
//    - 返回值：0 = 成功压入发送邮箱，1 = 总线离线(Bus-Off)或邮箱已满
// ================================================================
uint32_t can_send_message(uint32_t tx_stdid,uint32_t dlc_len,uint8_t* data)
{
   CAN_TxHeaderTypeDef TxMessage = {0};
   TxMessage.StdId = tx_stdid;
   TxMessage.IDE = CAN_ID_STD;
   TxMessage.DLC = dlc_len;
   TxMessage.RTR = CAN_RTR_DATA;
   uint32_t TxMailbox;

   // ---- 第一步：检查 ESR 寄存器是否处于 Bus-Off 离线状态 ----
   if((hcan.Instance->ESR&CAN_ESR_BOFF)!=0u)
   {
	   printf("esr_error");
	   return 1;
   }

   // ---- 第二步：检查 3 个硬件发送邮箱是否还有空位 ----
   if(HAL_CAN_GetTxMailboxesFreeLevel(&hcan) == 0U)
   {
	   printf("GetTxMailboxesFreeLevel_error");
       return 1;
   }

   // ---- 第三步：将报文填入空闲发送邮箱并由硬件自动仲裁上总线 ----
   if((HAL_CAN_AddTxMessage(&hcan,
                     &TxMessage,
                     data,
                     &TxMailbox))!=HAL_OK)
   {
	   printf("add_error");
       return 1;
   }
   printf("tx_ok");
   return 0;
}

// ================================================================
// 3. CAN1 接收 FIFO0 非阻塞读取
// ================================================================
uint32_t can_get_message(uint32_t* rx_id,uint32_t* rx_len,uint8_t*rx_data)
{
	CAN_RxHeaderTypeDef Rxmessage = {0};
	if(HAL_CAN_GetRxFifoFillLevel(&hcan,CAN_RX_FIFO0)>0)
	{
		if(HAL_CAN_GetRxMessage(&hcan,CAN_RX_FIFO0,&Rxmessage,rx_data)==HAL_OK)
		{
			*rx_id = Rxmessage.StdId;
			*rx_len = Rxmessage.DLC;
			return 1;
		}
	}
	return 0;
}
