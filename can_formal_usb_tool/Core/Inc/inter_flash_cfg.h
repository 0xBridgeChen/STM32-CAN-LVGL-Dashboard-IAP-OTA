#include "main.h"

#ifndef __INTER_FLASH_H__
#define __INTER_FLASH_H__

#pragma pack(1)        //设置字节对齐方式为1字节
typedef struct {
    uint8_t magic[4];                   //magic number 0xAABBCCDD
    uint32_t ota_bin_version;           //将要升级的文件版本
    uint8_t ota_flag;   //升级标志
    
    uint8_t checksum; //参数校验
    //占位
    uint8_t format[2]; //占位符

}inter_flash_cfg_param_typeDef;


#pragma pack()        //恢复默认字节对齐方式


uint8_t inter_flash_cfg_init(void);
int8_t inter_flash_cfg_get_app_update_flag(void);


#endif /* __INTER_FLASH_H__ */

