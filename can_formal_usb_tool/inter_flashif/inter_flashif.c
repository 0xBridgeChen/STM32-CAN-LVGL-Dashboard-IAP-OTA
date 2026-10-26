#include "inter_flashif.h"
#include "stm32f1xx_hal_flash_ex.h"
#include "common.h"

/**
 * @brief 累加校验和计算器
 * 
 * @param data 需要校验的数据
 * @param len 数据长度
 * @return uint8_t 校验和
 */
uint8_t inter_flash_checksum(uint8_t *data, uint32_t len)
{
    uint8_t checksum = 0;
    for(uint32_t i = 0; i < len; i++)
    {
        checksum += data[i];
    }
    return checksum & 0xFF;
}


/**
 * @brief 内部flash数据读取
 * 
 * @param addr 读取数据的地址
 * @param buf 数据存放缓存区
 * @param len 读取数据的长度
 * @return uint8_t 
 */
uint8_t inter_flashif_read_page(uint32_t addr, uint8_t *buf, uint32_t len)
{
    uint32_t flash_addr = addr;

    if(len > FLASH_PAGE_SIZE)          //数据超长
    {
        return 1;   
    }

    for(uint32_t i = 0; i < len; i++)
    {
        buf[i] = *(volatile uint8_t *)(flash_addr + i);
    }

    return 0;
}

uint8_t inter_flashif_smart_write_page(uint32_t addr, uint32_t *buf, uint32_t len)
{
    FLASH_EraseInitTypeDef user_flash;  //声明 FLASH_EraseInitTypeDef 结构体为 My_Flash
    
    /* 解锁 */
    HAL_FLASH_Unlock();

    user_flash.TypeErase = FLASH_TYPEERASE_PAGES;  //标明Flash执行页面只做擦除操作
    user_flash.PageAddress = addr;  //声明要擦除的地址
    user_flash.NbPages = 1;  //说明要擦除的页数，此参数必须是Min_Data = 1和Max_Data =(最大页数-初始页的值)之间的值

    uint32_t PageError = 0;                    //设置PageError,如果出现错误这个变量会被设置为出错的FLASH地址
    HAL_FLASHEx_Erase(&user_flash, &PageError);  //调用擦除函数擦除

    for(uint32_t i = 0; i < len; i++)
    {
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr+i*4, buf[i]); 
       // i = i + 4;
    }

    /* 上锁 */
    HAL_FLASH_Lock();
    
    return 0;
}

uint8_t inter_flashif_erase_page(uint32_t addr)
{
    FLASH_EraseInitTypeDef user_flash;  //声明 FLASH_EraseInitTypeDef 结构体为 My_Flash

    /* 解锁 */
    HAL_FLASH_Unlock();

    user_flash.TypeErase = FLASH_TYPEERASE_PAGES;  //标明Flash执行页面只做擦除操作
    user_flash.PageAddress = addr;  //声明要擦除的地址
    user_flash.NbPages = 1;  //说明要擦除的页数，此参数必须是Min_Data = 1和Max_Data =(最大页数-初始页的值)之间的值

    uint32_t PageError = 0;                    //设置PageError,如果出现错误这个变量会被设置为出错的FLASH地址
    HAL_FLASHEx_Erase(&user_flash, &PageError);  //调用擦除函数擦除

    /* 上锁 */
    HAL_FLASH_Lock();
    
    return 0;
}

/**
 * @brief 内部flash数据写入
 * 
 * @param addr 写入数据的地址(必须4字节对齐)
 * @param buf 需要写入数据的缓存器
 * @param len  写入数据长度
 * @return uint8_t 
 */
uint8_t inter_flashif_write_page(uint32_t addr, uint32_t *buf, uint32_t len)
{
    // FLASH_EraseInitTypeDef user_flash;  //声明 FLASH_EraseInitTypeDef 结构体为 My_Flash
    
    // /* 解锁 */
    HAL_FLASH_Unlock();

    // user_flash.TypeErase = FLASH_TYPEERASE_PAGES;  //标明Flash执行页面只做擦除操作
    // user_flash.PageAddress = addr;  //声明要擦除的地址
    // user_flash.NbPages = 1;  //说明要擦除的页数，此参数必须是Min_Data = 1和Max_Data =(最大页数-初始页的值)之间的值

    // uint32_t PageError = 0;                    //设置PageError,如果出现错误这个变量会被设置为出错的FLASH地址
    // HAL_FLASHEx_Erase(&user_flash, &PageError);  //调用擦除函数擦除

    for(uint32_t i = 0; i < len; i++)
    {
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr+i*4, buf[i]); 
       // i = i + 4;
    }

    /* 上锁 */
    HAL_FLASH_Lock();
    
    return 0;
}



#define TEST_ADDR       0x8007400

void inter_flash_test()
{
    uint8_t write[] = "1234FFO222-?";
    
    uint8_t read[32] = {0};
    
    inter_flashif_write_page(TEST_ADDR, (uint32_t *)write, 12);


    inter_flashif_read_page(TEST_ADDR, read, 12);

    dump_hex(read, 12, 16);
}

