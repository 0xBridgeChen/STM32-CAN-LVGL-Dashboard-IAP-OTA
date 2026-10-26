/**
 * @file    canif.c
 * @brief   STM32F407VET6 (Bootloader) CAN1 底层接口封装 (滤波器、阻塞同步发送、FIFO0 非阻塞接收)
 *
 * 文件结构：
 *   1. canif_init()       — 配置 CAN1 硬件滤波器并启动控制器
 *   2. CANFilter_Config() — 配置 32 位掩码全通滤波器 (绑定 FIFO0)
 *   3. CAN1_Send_Msg()    — 发送 8 字节标准帧并等待硬件邮箱发送完成
 *   4. CAN1_Recv_Msg()    — 非阻塞读取 FIFO0，返回实际接收字节数 DLC (无报文返回 0)
 */

#include "canif.h"
#include <stdio.h>

static CAN_TxHeaderTypeDef TxMessage;
static CAN_RxHeaderTypeDef RxMessage;

static void CANFilter_Config(void);

// ================================================================
// 1. CAN 接口层初始化
// ================================================================
uint8_t canif_init(void)
{
    CANFilter_Config();

    if (HAL_CAN_Start(&hcan1) != HAL_OK)
    {
        printf("CAN Start Fail\r\n");
        Error_Handler();
    }
    printf("CAN Start Success\r\n");

    return 0;
}

// ================================================================
// 2. CAN1 硬件滤波器配置 (32位掩码全通模式，接收所有 ID 进入 FIFO0)
// ================================================================
static void CANFilter_Config(void)
{
    CAN_FilterTypeDef sFilterConfig = {0};

    sFilterConfig.FilterBank = 0;
    sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
    sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
    sFilterConfig.FilterIdHigh = 0x0000;
    sFilterConfig.FilterIdLow = 0x0000;
    sFilterConfig.FilterMaskIdHigh = 0x0000;
    sFilterConfig.FilterMaskIdLow = 0x0000;
    sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
    sFilterConfig.FilterActivation = CAN_FILTER_ENABLE;
    sFilterConfig.SlaveStartFilterBank = 14;

    if (HAL_CAN_ConfigFilter(&hcan1, &sFilterConfig) != HAL_OK)
    {
        printf("CAN Filter Config Fail!\r\n");
        Error_Handler();
    }
    printf("CAN Filter Config Success!\r\n");
}

// ================================================================
// 3. CAN1 报文发送函数（等待 3 个发送邮箱全部空闲，确保 ISO-TP 流控帧顺序严密）
// ================================================================
uint8_t CAN1_Send_Msg(uint32_t id, uint8_t *msg, uint8_t len)
{
    uint16_t i = 0;
    uint32_t txMailBox = CAN_TX_MAILBOX0;
    uint8_t send_buf[8] = {0};

    TxMessage.StdId = id;
    TxMessage.ExtId = id;
    TxMessage.IDE = CAN_ID_STD;
    TxMessage.RTR = CAN_RTR_DATA;
    TxMessage.DLC = len;

    for (i = 0; i < len; i++)
    {
        send_buf[i] = msg[i];
    }

    if (HAL_CAN_AddTxMessage(&hcan1, &TxMessage, send_buf, &txMailBox) != HAL_OK)
    {
        return 1;
    }

    // ★ 等待邮箱发送完毕，防止高频连续帧把 3 个硬件邮箱塞满导致丢包
    while (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) != 3)
    {
    }

    return 0;
}

// ================================================================
// 4. CAN1 FIFO0 非阻塞接收函数
//    - 返回值：0 = 当前无新报文，1~8 = 收到的报文数据长度 (DLC)
// ================================================================
uint8_t CAN1_Recv_Msg(uint32_t *id, uint8_t *buf)
{
    if (HAL_CAN_GetRxFifoFillLevel(&hcan1, CAN_RX_FIFO0) == 0)
    {
        return 0;
    }

    if (HAL_CAN_GetRxMessage(&hcan1, CAN_RX_FIFO0, &RxMessage, buf) != HAL_OK)
    {
        return 0;
    }

    if (id != 0)
    {
        *id = RxMessage.StdId;
    }

    return RxMessage.DLC;
}
