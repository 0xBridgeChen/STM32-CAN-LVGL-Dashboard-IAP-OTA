#ifndef INTER_FLASHIF_H
#define INTER_FLASHIF_H

#include "main.h"

/*
 * F407VET6 的 512 KB Flash 教学分区。
 *
 * 0x08000000 ~ 0x0800BFFF：Bootloader（Sector 0、1、2，共 48 KB）
 * 0x0800C000 ~ 0x0800FFFF：参数区    （Sector 3，共 16 KB）
 * 0x08010000 ~ 0x0807FFFF：APP       （Sector 4~7，共 448 KB）
 *
 * 与老师的 F103 不同：
 * F103 按“页”擦除，F407 按“扇区”擦除，所以擦除函数不能直接照搬。
 */
#define INTER_FLASH_PARAM_ADDR      0x0800C000U
#define INTER_FLASH_APP_ADDR        0x08010000U
#define INTER_FLASH_END_ADDR        0x08080000U
#define INTER_FLASH_APP_MAX_SIZE    (INTER_FLASH_END_ADDR - INTER_FLASH_APP_ADDR)

/* 下面函数均约定：返回 0 表示成功，非 0 表示失败。 */
uint8_t inter_flashif_read(uint32_t addr, uint8_t *buf, uint32_t len);
uint8_t inter_flashif_erase_param_sector(void);
uint8_t inter_flashif_erase_app_area(void);
uint8_t inter_flashif_write(uint32_t addr, const uint8_t *buf, uint32_t len);
uint8_t inter_flash_checksum(const uint8_t *data, uint32_t len);

#endif /* INTER_FLASHIF_H */
