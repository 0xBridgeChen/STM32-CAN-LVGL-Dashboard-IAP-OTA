#ifndef _WRAP_H
#define _WRAP_H

/*********************************************************************************
 * @file    wrap.h
 * @brief   CAN 总线底层应用封装层头文件 (电赛/工业级高可靠通信接口)
 *********************************************************************************/
#include "main.h"

// ================================================================
// 对外函数原型声明
// ================================================================

/**
 * @brief  CAN1 硬件 32 位全通滤波器配置并启动 CAN 控制器
 * @note   配置 FilterBank 0 为全 0 掩码，匹配所有标准帧与扩展帧并压入 FIFO0
 */
void can_filter_init(void);

/**
 * @brief  CAN 标准数据帧快速发送
 * @param  tx_stdid  标准 11 位标识符 (如 0x123)
 * @param  dlc_len   数据长度 (0~8 字节)
 * @param  data      指向待发送字节缓冲区的指针
 */
void can_send_message(uint32_t tx_stdid, uint32_t dlc_len, uint8_t* data);

/**
 * @brief  CAN1 FIFO0 报文非阻塞轮询读取
 * @param  rx_id    输出参数：接收到的标准 ID
 * @param  rx_len   输出参数：接收到的 DLC 字节长度
 * @param  rx_data  输出参数：接收到的 8 字节数据缓存
 * @retval 0: 成功读取到有效报文; 1: FIFO 为空或读取异常
 */
uint32_t can_get_message(uint32_t* rx_id, uint32_t* rx_len, uint8_t* rx_data);

#endif /* _WRAP_H */
