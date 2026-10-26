#include "inter_flash_cfg.h"
#include <string.h>
#include <stdio.h>
#include "inter_flashif.h"

static inter_flash_cfg_param_typeDef flash_cfg_param;

/**
 * @brief 读取参数扇区数据
 * 
 * @return uint8_t 
 */
uint8_t inter_flash_cfg_get_param_sector()
{
    //读取系统配置区参数
    inter_flashif_read_page(INTER_FLASH_PARAM_ADDR, (uint8_t *)&flash_cfg_param, sizeof(flash_cfg_param));
    //校验魔术数字
    if((flash_cfg_param.magic[0] != 0xAA) || (flash_cfg_param.magic[1] != 0xBB) ||
        (flash_cfg_param.magic[2] != 0xCC) || (flash_cfg_param.magic[3] != 0xDD))
    {
        printf("[BOOR] flash magic error\r\n");
        return 1;           //扇区错误
    }

    //计算校验和
    uint8_t checksum = inter_flash_checksum((uint8_t *)&flash_cfg_param, sizeof(flash_cfg_param) - 3);
    if(checksum != flash_cfg_param.checksum)
    {
        printf("[BOOR] flash checksum error\r\n");
        return 2;           //校验和错误
    }

    return 0;
}


/**
 * @brief 获取Flash升级标志
 * 
 * @return uint8_t 
 */
int8_t inter_flash_cfg_get_app_update_flag()
{
    uint8_t ret = inter_flash_cfg_get_param_sector();
    if(ret != 0)            //获取参数
    {
        return -1;
    }

    //升级标志获取
	return flash_cfg_param.ota_flag;
}

/**
 * @brief 写升级标志
 * 
 * @param flag 
 * @return uint8_t 
 */
uint8_t inter_flash_cfg_set_app_update_flag(uint8_t flag)
{
    uint8_t ret = inter_flash_cfg_get_param_sector();
    if(ret != 0)            //获取参数
    {
        printf("[BOOR] flash get update flag error\r\n");
        return 1;
    }
    flash_cfg_param.ota_flag = flag;

    //计算校验和
    flash_cfg_param.checksum = inter_flash_checksum((uint8_t *)&flash_cfg_param, sizeof(flash_cfg_param) - 3);

    //写回参数
    inter_flashif_smart_write_page(INTER_FLASH_PARAM_ADDR, (uint32_t *)&flash_cfg_param, sizeof(flash_cfg_param)/sizeof(uint32_t));

	return 0;
}

/**
 * @brief 内部flash参数初始化
 * 
 * @return uint8_t 
 */
uint8_t inter_flash_cfg_init(void)
{
    if(sizeof(inter_flash_cfg_param_typeDef) % 4 != 0)
    {
        while(1)
        {
            printf("[BOOR] flash cfg param error\r\n");
            HAL_Delay(1000);
        }
    }

    uint8_t ret = inter_flash_cfg_get_param_sector();
    if(ret != 0)            //参数读取失败
    {
        while(1)
        {
            printf("[BOOR] flash cfg param sector error\r\n");
            HAL_Delay(1000);
        }
    }
    //测试时候可以强制在地址0x8006000,强制写入如下数据：AA BB CC DD 00 00 00 00 00 0E来完成初始化校验    

	return 0;
}


