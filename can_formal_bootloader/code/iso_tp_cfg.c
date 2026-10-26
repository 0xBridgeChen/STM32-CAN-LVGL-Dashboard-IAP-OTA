/**
 * @file    iso_tp_cfg.c
 * @brief   Bootloader 侧 ISO 15765-2 (ISO-TP) 多帧接收重组与 Flash 实时烧写服务
 *
 * 文件结构：
 *   1. iso_tp_init()          — 绑定 CAN 发送/接收 ID 与 4KB 重组缓冲区
 *   2. iso_tp_send_ota_data() — 封装带偏移量与长度头的 ISO-TP 长帧发送
 *   3. iso_tp_server()        — 核心服务：重组 2048B 固件块写入 Flash，收到结束帧跳转新 APP
 */

#include "iso_tp_cfg.h"
#include "isotp.h"
#include "canif.h"
#include "inter_flashif.h"
#include "inter_flash_cfg.h"
#include "boot_manager.h"
#include <stdio.h>
#include <string.h>

// ================================================================
// 1. ISO-TP 链路控制块与重组缓冲区定义
//    - 传统 CAN 单帧最大仅 8 字节；通过 ISO-TP (SF/FF/FC/CF) 可传输最大 4095 字节长包
//    - 本系统网关每攒满 2048 字节 (2KB) 固件数据，加上 9 字节协议头 (共 2057 字节) 发一次 ISO-TP
// ================================================================
#define ISOTP_BUFSIZE 4096

static IsoTpLink g_link;
static uint8_t g_isotpRecvBuf[ISOTP_BUFSIZE];
static uint8_t g_isotpSendBuf[ISOTP_BUFSIZE];

uint8_t payload[4096] = {0};
uint16_t payload_size = 4096;
uint8_t ota_recv_data[2048] = {0};

uint8_t iso_tp_init(void)
{
    isotp_init_link(&g_link, CAN_ID_OTA_RECV,
                    g_isotpSendBuf, sizeof(g_isotpSendBuf),
                    g_isotpRecvBuf, sizeof(g_isotpRecvBuf));

    return 0;
}

void iso_tp_send_ota_data(uint32_t pack_offset, uint8_t *data, uint32_t pack_size)
{
    uint32_t send_size = 0;

    payload[0] = 0xAF;
    payload[1] = (pack_offset >> 24) & 0xFF;
    payload[2] = (pack_offset >> 16) & 0xFF;
    payload[3] = (pack_offset >> 8) & 0xFF;
    payload[4] = (pack_offset >> 0) & 0xFF;
    payload[5] = (pack_size >> 24) & 0xFF;
    payload[6] = (pack_size >> 16) & 0xFF;
    payload[7] = (pack_size >> 8) & 0xFF;
    payload[8] = (pack_size >> 0) & 0xFF;
    memcpy(payload + 9, data, pack_size);

    send_size = pack_size + 9;

    int ret = isotp_blocking_send(&g_link, payload, send_size, 0xFFFFFF);
    printf("send data ret:%d\r\n", ret);
    isotp_poll(&g_link);
}

// ================================================================
// 2. ISO-TP 升级状态机主循环 (在 Bootloader 的 while(1) 中高频轮询)
//
//    处理的三种 CAN 报文：
//      ① ID = 0x0B2 (CAN_ID_OTA_SEND)：网关发来的 ISO-TP 固件分片帧 -> 喂给 isotp_on_can_message()
//      ② ID = 0x0B3 且 data[4]=0x01  ：网关再次发起升级请求 -> 重新擦除 APP 区并回 0x0D3 ACK
//      ③ ID = 0x0B3 且 data[4]=0x02  ：网关发来升级结束通知 -> 校验新 APP、清零 ota_flag 并跳转！
// ================================================================
void iso_tp_server(void)
{
    uint32_t recv_id = 0;
    uint16_t out_size = 0;
    uint8_t recv_len = 0;
    int ret = 0;
    uint8_t rxdata[8] = {0};

    // ------- 1. 从 CAN1 FIFO0 抽取底层报文 -------
    recv_len = CAN1_Recv_Msg(&recv_id, rxdata);
    if (recv_len > 0)
    {
        if (recv_id == CAN_ID_OTA_SEND)
        {
            // 喂入 ISO-TP 协议栈状态机（自动处理首帧 FF、回复流控帧 FC、拼接连续帧 CF）
            isotp_on_can_message(&g_link, rxdata, recv_len);
        }
        else if (recv_id == CAN_ID_CALL_OTA)
        {
            if ((rxdata[0] == 0xBE) &&
                (rxdata[1] == 0xAD) &&
                (rxdata[2] == 0xBE) &&
                (rxdata[3] == 0xEF))
            {
                if (rxdata[4] == 0x01)
                {
                    // ★ 若芯片本来就停留在 Bootloader (如空片救砖)，收到 0x0B3 请求时直接擦除并回 0x0D3
                    printf("[tp server] ota start request in bootloader, erasing APP area...\r\n");
                    inter_flashif_erase_app_area();
                    iso_tp_init();
                    uint8_t send_msg[8] = {0xBE, 0xAD, 0xBE, 0xEF, 0x01, 0xAA, 0xAA, 0xAA};
                    CAN1_Send_Msg(CAN_ID_CALL_OTA_ACK, send_msg, 8);
                    printf("[tp server] sent 0x0D3 ACK\r\n");
                }
                else if (rxdata[4] == 0x02)
                {
                    // ★ 收到固件传输完毕指令：校验新固件合法性 -> 撕掉升级字据 (ota_flag=0) -> 跳转新 APP！
                    printf("[tp server] ota end\r\n");

                    if (boot_check_app_valid(INTER_FLASH_APP_ADDR) == 0U)
                    {
                        printf("[tp server] downloaded APP vector table invalid!\r\n");
                    }
                    else
                    {
                        inter_flash_cfg_set_app_update_flag(0U);
                        printf("--->jump app\r\n");
                        HAL_Delay(20U);
                        boot_check_stack2jump_app(INTER_FLASH_APP_ADDR);
                    }
                }
            }
        }
    }

    // ------- 2. 驱动 ISO-TP 定时器与流控帧发送 -------
    isotp_poll(&g_link);

    // ------- 3. 检查是否已完整拼好一整包 2KB 固件数据块 -------
    // 载荷格式：[0]=0xBF | [1..4]=pack_offset(大端) | [5..8]=pack_size(大端) | [9..]=固件数据
    ret = isotp_receive(&g_link, payload, payload_size, &out_size);
    if (ISOTP_RET_OK == ret)
    {
        if (payload[0] == 0xBF)
        {
            uint32_t pack_offset = ((uint32_t)payload[1] << 24) |
                                   ((uint32_t)payload[2] << 16) |
                                   ((uint32_t)payload[3] << 8) |
                                   ((uint32_t)payload[4] << 0);
            uint32_t pack_size = ((uint32_t)payload[5] << 24) |
                                 ((uint32_t)payload[6] << 16) |
                                 ((uint32_t)payload[7] << 8) |
                                 ((uint32_t)payload[8] << 0);

            if (pack_size > sizeof(ota_recv_data))
            {
                pack_size = sizeof(ota_recv_data);
            }

            memcpy(ota_recv_data, &payload[9], pack_size);

            printf("\r\npack_offset:%lu, pack_size:%lu\r\n",
                   (unsigned long)pack_offset, (unsigned long)pack_size);

            // 将拼装好的 2KB 数据块按半字/字写入 0x08010000 + pack_offset
            inter_flashif_write(INTER_FLASH_APP_ADDR + pack_offset, ota_recv_data, pack_size);
            printf("write end\r\n");
        }
    }
}
