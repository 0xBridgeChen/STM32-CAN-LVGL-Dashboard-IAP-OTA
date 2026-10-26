#include "inter_flash_cfg.h"

#include "inter_flashif.h"
#include <stdio.h>

/*
 * static 表示该变量只在本 .c 文件内部可见。
 * 它是参数区 12 字节数据在 SRAM 中的临时副本。
 */
static inter_flash_cfg_param_typeDef flash_cfg_param;

static void inter_flash_cfg_fill_default(uint8_t flag)
{
    flash_cfg_param.magic[0] = 0xAAU;
    flash_cfg_param.magic[1] = 0xBBU;
    flash_cfg_param.magic[2] = 0xCCU;
    flash_cfg_param.magic[3] = 0xDDU;
    flash_cfg_param.ota_bin_version = 0x00000001U;
    flash_cfg_param.ota_flag = flag;
    flash_cfg_param.format[0] = 0xEEU;
    flash_cfg_param.format[1] = 0xFFU;
    flash_cfg_param.checksum =
        inter_flash_checksum((const uint8_t *)&flash_cfg_param,
                             sizeof(flash_cfg_param) - 3U);
}

/* 从 Flash 读出参数，并依次检查魔数与校验和。 */
static uint8_t inter_flash_cfg_get_param_sector(void)
{
    uint8_t checksum;

    if (inter_flashif_read(INTER_FLASH_PARAM_ADDR,
                           (uint8_t *)&flash_cfg_param,
                           sizeof(flash_cfg_param)) != 0U)
    {
        return 1U;
    }

    if ((flash_cfg_param.magic[0] != 0xAAU) ||
        (flash_cfg_param.magic[1] != 0xBBU) ||
        (flash_cfg_param.magic[2] != 0xCCU) ||
        (flash_cfg_param.magic[3] != 0xDDU))
    {
        printf("flash magic error\r\n");
        return 2U;
    }

    checksum = inter_flash_checksum((const uint8_t *)&flash_cfg_param,
                                    sizeof(flash_cfg_param) - 3U);
    if (checksum != flash_cfg_param.checksum)
    {
        printf("flash checksum error\r\n");
        return 3U;
    }

    return 0U;
}

uint8_t inter_flash_cfg_init(void)
{
    if (sizeof(inter_flash_cfg_param_typeDef) != 12U)
    {
        printf("flash cfg struct size error: %u\r\n",
               (unsigned int)sizeof(inter_flash_cfg_param_typeDef));
        return 1U;
    }

    if (inter_flash_cfg_get_param_sector() != 0U)
    {
        printf("[PARAM] Sector 3 uninitialized, writing default info.bin (ota_flag=0)...\r\n");
        inter_flash_cfg_fill_default(0U);
        if (inter_flashif_erase_param_sector() != 0U)
        {
            return 2U;
        }
        if (inter_flashif_write(INTER_FLASH_PARAM_ADDR,
                                (const uint8_t *)&flash_cfg_param,
                                sizeof(flash_cfg_param)) != 0U)
        {
            return 3U;
        }
        return inter_flash_cfg_get_param_sector();
    }

    return 0U;
}

int8_t inter_flash_cfg_get_app_update_flag(void)
{
    if (inter_flash_cfg_get_param_sector() != 0U)
    {
        return -1;
    }

    return (int8_t)flash_cfg_param.ota_flag;
}

uint8_t inter_flash_cfg_set_app_update_flag(uint8_t flag)
{
    if (flag > 1U)
    {
        return 1U;
    }

    /* 先读出整个旧结构体；若 Sector 3 尚未初始化，则使用默认参数结构。 */
    if (inter_flash_cfg_get_param_sector() != 0U)
    {
        inter_flash_cfg_fill_default(flag);
    }
    else
    {
        flash_cfg_param.ota_flag = flag;
        flash_cfg_param.checksum =
            inter_flash_checksum((const uint8_t *)&flash_cfg_param,
                                 sizeof(flash_cfg_param) - 3U);
    }

    /* Flash 不能把 0 直接改回 1，所以写入前必须先擦除整个 Sector 3。 */
    if (inter_flashif_erase_param_sector() != 0U)
    {
        return 3U;
    }

    if (inter_flashif_write(INTER_FLASH_PARAM_ADDR,
                            (const uint8_t *)&flash_cfg_param,
                            sizeof(flash_cfg_param)) != 0U)
    {
        return 4U;
    }

    /* 再读一次，既验证写入，也刷新 SRAM 中的副本。 */
    return inter_flash_cfg_get_param_sector();
}
