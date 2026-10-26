/**
 * @file    canif.h
 * @brief   STM32F103ZET6 (usb_tool 网关) CAN 接口层头文件与 OTA 报文 ID 矩阵定义
 */

#ifndef __CANIF_H__
#define __CANIF_H__

#include "main.h"
#include "can.h"

// ================================================================
// CAN OTA 升级与仪表业务报文 ID 矩阵定义 (注意：网关侧收发方向与 F407 互补)
// ================================================================
#define CAN_ID_OTA_SEND     0x0B1U  // F407(Bootloader) -> ZET6(网关)：网关监听的流控帧 (FC) ID
#define CAN_ID_OTA_RECV     0x0B2U  // ZET6(网关) -> F407(Bootloader)：网关发送的固件数据帧 (FF/CF) ID
#define CAN_ID_CALL_OTA     0x0B3U  // ZET6 -> F407：OTA 开始唤醒帧 (data[4]=0x01) / 结束帧 (data[4]=0x02)
#define CAN_ID_UI           0x0B4U  // 日常仪表控制报文 ID
#define CAN_ID_CALL_OTA_ACK 0x0D3U  // F407(Bootloader) -> ZET6：APP 扇区擦除完毕应答帧

uint8_t canif_init(void);
uint8_t CAN1_Send_Msg(uint32_t id, uint8_t *msg, uint8_t len);
uint8_t CAN1_Recv_Msg(uint32_t *id, uint8_t *buf);

#endif /* __CANIF_H__ */
