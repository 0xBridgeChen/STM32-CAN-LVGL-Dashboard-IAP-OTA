#include "main.h"

#ifndef __USER_BOOT_MANAGER_H__
#define __USER_BOOT_MANAGER_H__

#define BOOT_VERSION_MAJOR			01U						//主版本号
#define BOOT_VERSION_MINOR			01U						//子版本号
#define BOOT_VERSION_PATCH			00U						//修订版本号



uint8_t boot_check_stack2jump_app(uint32_t addr); // APP_ADDR


#endif  // __USER_BOOT_MANAGER_H__
