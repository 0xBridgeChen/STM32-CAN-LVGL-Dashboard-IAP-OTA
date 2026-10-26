#ifndef INTER_FLASH_CFG_H
#define INTER_FLASH_CFG_H

#include "main.h"

/*
 * 参数区中的 12 字节数据结构，与老师的 info.bin 保持一致。
 *
 * #pragma pack(1) 表示结构体成员按 1 字节紧密排列，不插入对齐填充字节。
 * 因此该结构体在 F103 和 F407 上都固定为 12 字节：
 *   4 + 4 + 1 + 1 + 2 = 12。
 */
#pragma pack(1)
typedef struct
{
    uint8_t magic[4];          /* 固定为 AA BB CC DD，用于识别参数是否有效。 */
    uint32_t ota_bin_version;  /* 目标固件版本；教学实验中只保存，不参与版本比较。 */
    uint8_t ota_flag;          /* 0：正常启动 APP；1：Bootloader 等待 Ymodem。 */
    uint8_t checksum;          /* 前 9 字节的 8 位累加校验和。 */
    uint8_t format[2];         /* 预留字节，当前固定保留。 */
} inter_flash_cfg_param_typeDef;
#pragma pack()

/* 检查参数结构体大小、魔数和校验和。返回 0 表示参数合法。 */
uint8_t inter_flash_cfg_init(void);

/* 返回 0/1 表示 ota_flag；返回 -1 表示参数读取或校验失败。 */
int8_t inter_flash_cfg_get_app_update_flag(void);

/* 修改 ota_flag、重算校验和，并把完整 12 字节结构体写回参数扇区。 */
uint8_t inter_flash_cfg_set_app_update_flag(uint8_t flag);

#endif /* INTER_FLASH_CFG_H */
