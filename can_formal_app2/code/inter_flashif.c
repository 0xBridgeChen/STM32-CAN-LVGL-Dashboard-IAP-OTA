#include "inter_flashif.h"

#include <string.h>

/*
 * 检查 [addr, addr + len) 是否完全位于本芯片的内部 Flash 中。
 *
 * 这里使用减法判断长度，是为了避免 addr + len 发生无符号整数溢出。
 */
static uint8_t inter_flashif_range_is_valid(uint32_t addr, uint32_t len)
{
    if ((addr < FLASH_BASE) || (addr >= INTER_FLASH_END_ADDR))
    {
        return 0U;
    }

    if (len > (INTER_FLASH_END_ADDR - addr))
    {
        return 0U;
    }

    return 1U;
}

/*
 * 计算 8 位累加校验和。
 *
 * const uint8_t *data：
 *   data 是一个“指向只读字节的指针”。函数可以沿着地址读取字节，
 *   但不允许通过这个指针修改原数据。
 *
 * data[i] 等价于 *(data + i)：先把指针向后移动 i 个字节，再取出该字节。
 */
uint8_t inter_flash_checksum(const uint8_t *data, uint32_t len)
{
    uint8_t checksum = 0U;

    if (data == NULL)
    {
        return 0U;
    }

    for (uint32_t i = 0U; i < len; ++i)
    {
        checksum = (uint8_t)(checksum + data[i]);
    }

    return checksum;
}

/*
 * 从内部 Flash 读取任意长度的字节。
 *
 * STM32 的 Flash 是“内存映射”的：Flash 地址可以像普通内存地址一样读取。
 * (volatile const uint8_t *)addr 的含义：
 *   1. 把整数地址 addr 强制转换成字节指针；
 *   2. const 表示只读；
 *   3. volatile 告诉编译器每次都必须真正访问该地址，不能凭缓存结果猜测。
 */
uint8_t inter_flashif_read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    volatile const uint8_t *flash_data;

    if ((buf == NULL) || (inter_flashif_range_is_valid(addr, len) == 0U))
    {
        return 1U;
    }

    flash_data = (volatile const uint8_t *)addr;

    for (uint32_t i = 0U; i < len; ++i)
    {
        buf[i] = flash_data[i];
    }

    return 0U;
}

/* 擦除保存 info 参数的 Sector 3。F407 不能像 F103 那样按 2 KB 页擦除。 */
uint8_t inter_flashif_erase_param_sector(void)
{
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t sector_error = 0U;
    HAL_StatusTypeDef status;

    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.Sector = FLASH_SECTOR_3;
    erase.NbSectors = 1U;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    HAL_FLASH_Unlock();
    status = HAL_FLASHEx_Erase(&erase, &sector_error);
    HAL_FLASH_Lock();

    return (status == HAL_OK) ? 0U : 1U;
}

/*
 * 擦除整个 APP 区（Sector 4~7）。
 * 教学实验只保存一个 APP，因此升级时直接清空完整 APP 区，流程最直观。
 */
uint8_t inter_flashif_erase_app_area(void)
{
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t sector_error = 0U;
    HAL_StatusTypeDef status;

    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.Sector = FLASH_SECTOR_4;
    erase.NbSectors = 4U;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    HAL_FLASH_Unlock();
    status = HAL_FLASHEx_Erase(&erase, &sector_error);
    HAL_FLASH_Lock();

    return (status == HAL_OK) ? 0U : 1U;
}

/*
 * 把字节数组写入 Flash。
 *
 * F407 的 HAL_FLASH_Program(...WORD...) 每次写 4 字节，所以：
 *   - address 必须 4 字节对齐；
 *   - 最后一组不足 4 字节时，用 0xFF 补齐；
 *   - 0xFF 是 Flash 擦除后的默认值，不会额外制造有效数据。
 */
uint8_t inter_flashif_write(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    uint32_t offset = 0U;
    HAL_StatusTypeDef status = HAL_OK;

    if ((buf == NULL) || ((addr & 0x3U) != 0U) ||
        (inter_flashif_range_is_valid(addr, len) == 0U))
    {
        return 1U;
    }

    HAL_FLASH_Unlock();

    while (offset < len)
    {
        uint32_t word = 0xFFFFFFFFU;
        uint32_t copy_len = len - offset;

        if (copy_len > sizeof(word))
        {
            copy_len = sizeof(word);
        }

        /*
         * &word 是 uint32_t *，&buf[offset] 是 const uint8_t *。
         * memcpy 按“字节数”复制，因此不同指针类型可以安全地交给它。
         */
        memcpy(&word, &buf[offset], copy_len);

        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + offset, word);
        if (status != HAL_OK)
        {
            break;
        }

        /* 写完立即回读 4 字节，确认实际 Flash 内容与准备写入的 word 一致。 */
        if (*(volatile const uint32_t *)(addr + offset) != word)
        {
            status = HAL_ERROR;
            break;
        }

        offset += sizeof(word);
    }

    HAL_FLASH_Lock();

    return (status == HAL_OK) ? 0U : 1U;
}
