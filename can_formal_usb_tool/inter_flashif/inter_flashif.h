#include "main.h"

#ifndef __INTER_FLASHIF_H__
#define __INTER_FLASHIF_H__


#define INTER_FLASH_PARAM_ADDR      (0x8006000U)            //参数存储区地址大小
#define INTER_FLASH_APP_ADDR        (0x8008000U)            //APP程序地址

void inter_flash_test(void);
uint8_t inter_flashif_smart_write_page(uint32_t addr, uint32_t *buf, uint32_t len);
uint8_t inter_flashif_write_page(uint32_t addr, uint32_t *buf, uint32_t len);
uint8_t inter_flashif_read_page(uint32_t addr, uint8_t *buf, uint32_t len);
uint8_t inter_flashif_erase_page(uint32_t addr);
uint8_t inter_flash_checksum(uint8_t *data, uint32_t len);
#endif /* __INTER_FLASHIF_H__ */
