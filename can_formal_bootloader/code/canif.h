/**
 * @file    canif.h
 * @brief   CAN 总线接口层 (CAN Interface) 头文件与 OTA 专用报文 ID 定义
 */

#ifndef __CANIF_H__
#define __CANIF_H__

#include "main.h"
#include "can.h"

// ================================================================
// CAN OTA 升级与仪表业务报文 ID 矩阵定义
// ================================================================
#define CAN_ID_OTA_RECV     0x0B1U  // F407(Bootloader) -> ZET6(网关)：ISO-TP 流控帧 (FC) 通道
#define CAN_ID_OTA_SEND     0x0B2U  // ZET6(网关) -> F407(Bootloader)：ISO-TP 固件数据帧 (FF/CF) 通道
#define CAN_ID_CALL_OTA     0x0B3U  // ZET6 -> F407：OTA 开始唤醒帧 (data[4]=0x01) / 结束帧 (data[4]=0x02)
#define CAN_ID_UI           0x0B4U  // 方向盘发送端 -> F407(APP)：日常仪表控制报文 (车速/转速/灯光/RTC)
#define CAN_ID_CALL_OTA_ACK 0x0D3U  // F407(Bootloader) -> ZET6：APP 扇区擦除完毕，准备接收固件应答帧

uint8_t canif_init(void);
uint8_t CAN1_Send_Msg(uint32_t id, uint8_t *msg, uint8_t len);
uint8_t CAN1_Recv_Msg(uint32_t *id, uint8_t *buf);

#endif /* __CANIF_H__ */
