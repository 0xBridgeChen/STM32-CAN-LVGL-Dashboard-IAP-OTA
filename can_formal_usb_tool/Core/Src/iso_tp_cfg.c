/**
 * @file    iso_tp_cfg.c
 * @brief   网关侧 (STM32F103ZET6) ISO 15765-2 (ISO-TP) 2KB 固件数据块封包发送模块
 *
 * 文件结构：
 *   1. iso_tp_init()          — 初始化网关 ISO-TP 链路控制块 (发送 ID: 0x0B2, 接收流控 ID: 0x0B1)
 *   2. iso_tp_send_ota_data() — 封装 9 字节协议头 (0xBF + 4B偏移 + 4B长度) 并阻塞发送 2KB 块
 *   3. iso_tp_server()        — 接收对端 F407 发回的流控帧 (FC) 并推进状态机
 */

#include "iso_tp_cfg.h"
#include "isotp.h"
#include "canif.h"

#define CAN_MSG_DATA_HEAD (0xAF) // CAN 数据有效帧头
#define ISOTP_BUFSIZE 4096       // ISO-TP 层单次最大重组/拆包缓冲区大小 (4KB)

static IsoTpLink g_link;
static uint8_t g_isotpRecvBuf[ISOTP_BUFSIZE];
static uint8_t g_isotpSendBuf[ISOTP_BUFSIZE];

uint8_t payload[4096] = {0};
uint16_t payload_size = 4096;

// ================================================================
// 1. 网关侧 ISO-TP 传输层链路初始化
// ================================================================
uint8_t iso_tp_init(void)
{
    isotp_init_link(&g_link, CAN_ID_OTA_SEND,
                    g_isotpSendBuf, sizeof(g_isotpSendBuf),
                    g_isotpRecvBuf, sizeof(g_isotpRecvBuf));

    return 0;
}

// ================================================================
// 2. 封装并发送 2048 字节固件数据块 (由 ymodem.c 攒满 2KB 时调用)
//
//    协议帧格式 (共 pack_size + 9 字节)：
//      | Byte 0 | Byte 1~4 (大端) | Byte 5~8 (大端) | Byte 9 ~ Byte 9+N |
//      |  0xBF  |   pack_offset   |    pack_size    |   固件二进制内容   |
// ================================================================
void iso_tp_send_ota_data(uint32_t pack_offset, uint8_t *data, uint32_t pack_size)
{
    uint32_t send_size = 0;

    payload[0] = 0xBF;
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

    // 阻塞式发送：自动发首帧 (FF) -> 等 F407 回复流控帧 (FC) -> 连续发送约 293 帧连续帧 (CF)
    int ret = isotp_blocking_send(&g_link, payload, send_size, 0xFFFFFF);
    printf("send daat ret:%d\r\n", ret);
}

// ================================================================
// 3. 网关侧 ISO-TP 轮询服务
// ================================================================
void iso_tp_server(void)
{
    uint32_t recv_id = 0;
    uint16_t out_size = 0;
    uint8_t recv_len = 0;
    int ret = 0;
    uint8_t rxdata[8] = {0};

    recv_len = CAN1_Recv_Msg(&recv_id, rxdata);
    if (recv_len > 0)
    {
        if (recv_id == CAN_ID_OTA_RECV)
        {
            isotp_on_can_message(&g_link, rxdata, recv_len);
        }
    }
    isotp_poll(&g_link);

    ret = isotp_receive(&g_link, payload, payload_size, &out_size);
    (void)ret;
}
