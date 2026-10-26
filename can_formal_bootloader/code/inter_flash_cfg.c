/**
 * @file    inter_flash_cfg.c
 * @brief   Flash 参数扇区 (Sector 3: 0x0800C000, 12B info.bin) 升级标志位管理
 *
 * 文件结构：
 *   1. inter_flash_cfg_fill_default()       — 构造默认 12B 参数结构体 (AA BB CC DD + ota_flag + 校验和)
 *   2. inter_flash_cfg_get_param_sector()   — 读取 Sector 3 并校验魔数与累加和
 *   3. inter_flash_cfg_init()               — 参数区上电自检（若为空白扇区则自动初始化默认值）
 *   4. inter_flash_cfg_get_app_update_flag()— 获取当前升级请求标志 ota_flag (0 或 1)
 *   5. inter_flash_cfg_set_app_update_flag()— 擦除 Sector 3 并写入新的 ota_flag（立旗 / 撕旗）
 */

#include "inter_flash_cfg.h"

#include "inter_flashif.h"
#include <stdio.h>

// ================================================================
// 1. 参数区 12 字节结构体说明 (位于 0x0800C000，跨掉电/软复位保持)
//
//    偏移   字段               大小   说明
//    0~3    magic[4]           4B     固定魔数：0xAA, 0xBB, 0xCC, 0xDD（验证扇区是否已初始化）
//    4~7    ota_bin_version    4B     固件版本号（默认 0x00000001）
//    8      ota_flag           1B     ★ 核心升级标志：0=正常进APP，1=留在Bootloader执行OTA
//    9      checksum           1B     前 9 字节累加和校验（防 Flash 位翻转误触发）
//    10~11  format[2]          2B     尾标识：0xEE, 0xFF
// ================================================================
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

// ================================================================
// 2. 从 Flash Sector 3 (0x0800C000) 读出参数并校验魔数与累加和
// ================================================================
static uint8_t inter_flash_cfg_get_param_sector(void)
{
    uint8_t checksum;

    // ---- 第一步：从 0x0800C000 拷贝 12 字节到 SRAM 副本 ----
    if (inter_flashif_read(INTER_FLASH_PARAM_ADDR,
                           (uint8_t *)&flash_cfg_param,
                           sizeof(flash_cfg_param)) != 0U)
    {
        return 1U;
    }

    // ---- 第二步：校验魔数头 0xAA 0xBB 0xCC 0xDD ----
    if ((flash_cfg_param.magic[0] != 0xAAU) ||
        (flash_cfg_param.magic[1] != 0xBBU) ||
        (flash_cfg_param.magic[2] != 0xCCU) ||
        (flash_cfg_param.magic[3] != 0xDDU))
    {
        printf("flash magic error\r\n");
        return 2U;
    }

    // ---- 第三步：校验前 9 字节累加和 ----
    checksum = inter_flash_checksum((const uint8_t *)&flash_cfg_param,
                                    sizeof(flash_cfg_param) - 3U);
    if (checksum != flash_cfg_param.checksum)
    {
        printf("flash checksum error\r\n");
        return 3U;
    }

    return 0U;
}

// ================================================================
// 3. 参数区初始化（支持免手动烧录 info.bin 的自动格式化）
// ================================================================
uint8_t inter_flash_cfg_init(void)
{
    if (sizeof(inter_flash_cfg_param_typeDef) != 12U)
    {
        printf("flash cfg struct size error: %u\r\n",
               (unsigned int)sizeof(inter_flash_cfg_param_typeDef));
        return 1U;
    }

    // ★ 若检测到 Sector 3 还是未初始化的空扇区 (0xFF)，自动擦写一份默认 info.bin (ota_flag=0)
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

// ================================================================
// 4. 修改升级标志位 ota_flag（APP 收到 0x0B3 时置 1，Bootloader 刷完后置 0）
//    ⚠️ Flash 硬件特性：只能把 bit 从 1 写为 0，不能直接从 0 改回 1，
//       因此每次修改标志位都必须执行“读出修改 -> 擦除 Sector 3 -> 重新写入 12 字节”。
// ================================================================
uint8_t inter_flash_cfg_set_app_update_flag(uint8_t flag)
{
    if (flag > 1U)
    {
        return 1U;
    }

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

    return inter_flash_cfg_get_param_sector();
}
