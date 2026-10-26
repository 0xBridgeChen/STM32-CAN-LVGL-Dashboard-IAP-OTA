#include "headfile.h"

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

  if (HAL_CAN_ConfigFilter(&hcan1, &Can_ConfigFilter) != HAL_OK)
  {
    printf("filter error\r\n");
    Error_Handler();
  }
  printf("filter ok\r\n");
  if(HAL_CAN_Start(&hcan1)!=HAL_OK)
  {
	  printf("can start error\r\n");
	  Error_Handler();
  }
  if(HAL_CAN_ActivateNotification(&hcan1,CAN_IT_RX_FIFO0_MSG_PENDING)!=HAL_OK)
  {
	  printf("can activate error\r\n");
	  Error_Handler();
  }
  printf("can rx0 it activate\r\n");
  
}


void can_send_message(uint32_t tx_stdid,uint32_t dlc_len,uint8_t* data)
{
   CAN_TxHeaderTypeDef TxMessage = {0};
   TxMessage.StdId = tx_stdid;
   TxMessage.IDE = CAN_ID_STD;
   TxMessage.DLC = dlc_len;
   TxMessage.RTR = CAN_RTR_DATA;
   uint32_t TxMailbox;

   HAL_CAN_AddTxMessage(&hcan1,
                     &TxMessage,
                     data,
                     &TxMailbox);
}
/* ================= 旧版轮询接收函数（已弃用，留作参考） ================= */
#if 0
uint32_t can_get_message_polling(uint32_t* rx_id,uint32_t* rx_len,uint8_t*rx_data)
{
	CAN_RxHeaderTypeDef Rxmessage = {0};
	if(HAL_CAN_GetRxFifoFillLevel(&hcan1,CAN_RX_FIFO0)==0u)
	{
		return 1;
	}
	printf("fifo_ok\r\n");
	
	/* 2. FIFO有报文，尝试把这一帧取出来 */
    if (HAL_CAN_GetRxMessage(&hcan1,
                             CAN_RX_FIFO0,
                             &Rxmessage,
                             rx_data) != HAL_OK)
    {
        printf("get_error\r\n");
        return 1;
    }

    /* 3. 把接收到的ID和DLC传回main */
    *rx_id  = Rxmessage.StdId;
    *rx_len = Rxmessage.DLC;

    /* 4. 打印这一帧基本信息 */
    printf("rx_ok ID=0x%03lX DLC=%lu\r\n",
           (unsigned long)(*rx_id),
           (unsigned long)(*rx_len));

    /* 5. 打印收到的数据 */
    printf("data: ");
    for (uint32_t i = 0; i < *rx_len; i++)
    {
        printf("%02X ", rx_data[i]);
    }
    printf("\r\n");

    return 0;
}
#endif


/* ================= 【阶段二：全局中转站】中断暂存与标志位 ================= */
volatile uint8_t g_can_rx_flag = 0;   // 状态旗帜：1 = 中断立起红旗(有新数据); 0 = main放下红旗(数据已取走)
uint32_t         g_can_rx_id   = 0;   // g_can_rx_id  = 中断暂存的 ID
uint32_t         g_can_rx_len  = 0;   // g_can_rx_len = 中断暂存的长度 DLC
uint8_t          g_can_data[8] = {0}; // g_can_data   = 中断暂存的 8 字节数据

/**
  * @brief  【阶段一：硬件层取样】CAN1 FIFO0 硬件中断回调函数
  * @note   触发时机：硬件 FIFO0 收到新数据的一瞬间，CPU 自动跳转进来
  */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
	if(hcan->Instance==CAN1)
	{
		CAN_RxHeaderTypeDef rx_header;

		/* 1. 从硬件 FIFO0 中掏出数据（掏空后硬件自动清除中断标志，防止卡死） */
		if(HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx_header, g_can_data) == HAL_OK)
		{
			/* 2. 将刚从硬件掏出来的数据，转存到全局变量中（跨中断保存） */
			g_can_rx_id   = rx_header.StdId; // rx_header.StdId = CAN硬件刚取出来的ID   -> g_can_rx_id  = 中断暂存的ID
			g_can_rx_len  = rx_header.DLC;   // rx_header.DLC   = CAN硬件刚取出来的长度 -> g_can_rx_len = 中断暂存的长度
			                                 // g_can_data      = CAN硬件刚取出来的数据写入全局缓存
			
			/* 3. 立起红旗，通知 main() 主循环：“包裹已到，请来取件” */
			g_can_rx_flag = 1;
		}
	}
}

/**
  * @brief  【阶段三：应用层交接】供 main() 主循环调用的接收接口
  * @param  rx_id   : *rx_id   = 交给 main 的 ID
  * @param  rx_len  : *rx_len  = 交给 main 的长度
  * @param  rx_data : rx_data  = 交给 main 的 8 字节数据数组
  * @retval 0: 成功取出一帧新数据
  *         1: 暂无新数据
  */
uint32_t can_get_message(uint32_t *rx_id, uint32_t *rx_len, uint8_t *rx_data)
{
    /* 1. 检查红旗：如果旗子没立（0），说明中断还没收到新数据，直接返回 */
    if (g_can_rx_flag == 0)
    {
        return 1;
    }

    /* 2. 红旗立起（1）：把中断暂存在全局变量里的包裹交接给 main */
    *rx_id  = g_can_rx_id;  // g_can_rx_id  = 中断暂存的ID   -> *rx_id  = 交给 main 的ID
    *rx_len = g_can_rx_len; // g_can_rx_len = 中断暂存的长度 -> *rx_len = 交给 main 的长度
    for (uint32_t i = 0; i < *rx_len; i++)
    {
        rx_data[i] = g_can_data[i]; // g_can_data[i] = 中断暂存的数据 -> rx_data[i] = 交给 main 的数据
    }

    /* 3. 放下红旗（清零标志位）：包裹已被 main 取走，等待下一次中断立旗 */
    g_can_rx_flag = 0;

    return 0; // 成功交接
}



















