#ifndef BOOT_MANAGER_H
#define BOOT_MANAGER_H

#include "main.h"

/* 判断 APP 开头保存的 MSP 和 Reset_Handler 是否像一张合法向量表。 */
uint8_t boot_check_app_valid(uint32_t app_addr);

/* 检查成功后，把 CPU 的执行环境切换到 APP，并跳到 APP 的复位函数。 */
uint8_t boot_check_stack2jump_app(uint32_t app_addr);

#endif /* BOOT_MANAGER_H */
