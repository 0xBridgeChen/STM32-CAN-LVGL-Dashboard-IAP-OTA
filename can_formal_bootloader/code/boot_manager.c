/**
 * @file    boot_manager.c
 * @brief   Bootloader 应用合法性校验与 Cortex-M4 底层无损跳转管理
 *
 * 文件结构：
 *   1. boot_check_app_valid()       — 校验 APP 向量表栈顶指针 (MSP) 与 Reset_Handler 合法性
 *   2. boot_check_stack2jump_app()  — 关中断、外设反初始化、重设 VTOR/MSP 并移交 CPU 控制权
 */

#include "boot_manager.h"

#include "inter_flashif.h"
#include "can.h"
#include "usart.h"
#include <stdio.h>

// ================================================================
// 1. 函数指针类型与硬件 SRAM 边界定义
//    - APP_FUNC: 指向“无参数、无返回值”函数的指针，用于承接 APP 的 Reset_Handler
//    - STM32F407VET6 的常规 SRAM (SRAM1 112KB + SRAM2 16KB) 地址为 0x20000000 ~ 0x20020000
// ================================================================
typedef void (*APP_FUNC)(void);

#define F407_SRAM_START  0x20000000U
#define F407_SRAM_END    0x20020000U

// ================================================================
// 2. 校验 0x08010000 处的用户 APP 是否存在且完整 (防变砖核心)
//
//    Cortex-M4 中断向量表硬性规定：
//      ① 偏移 +0x00 (第一个 32 位字) = 初始主栈顶指针 (MSP)
//      ② 偏移 +0x04 (第二个 32 位字) = 复位入口函数 Reset_Handler 地址 (最低位 Thumb=1)
//
//    若 Flash 被擦除过 (全 0xFFFFFFFF) 或刷写中途断电，MSP 必然不在 SRAM 范围内，
//    此时拦截跳转，留在 Bootloader 等待重新刷写。
// ================================================================
uint8_t boot_check_app_valid(uint32_t app_addr)
{
    uint32_t app_msp;
    uint32_t reset_handler;
    uint32_t reset_address;

    // ---- 第一步：解引用读取 APP 首地址处的 MSP 栈顶值与 Reset_Handler 地址 ----
    app_msp = *(volatile const uint32_t *)app_addr;
    reset_handler = *(volatile const uint32_t *)(app_addr + 4U);

    // 清除最低位 Thumb 状态位 (Bit0)，还原真实的偶数 Flash 指令地址
    reset_address = reset_handler & ~1U;

    // ---- 第二步：检查初始栈顶 MSP 是否落在 F407 的 128KB SRAM 内且 4 字节对齐 ----
    if ((app_msp < F407_SRAM_START) ||
        (app_msp > F407_SRAM_END) ||
        ((app_msp & 0x3U) != 0U))
    {
        return 0U;
    }

    // ---- 第三步：检查 Reset_Handler 最低位是否为 1 (Thumb 指令集) 且落在 APP Flash 分区内 ----
    if (((reset_handler & 1U) == 0U) ||
        (reset_address < app_addr) ||
        (reset_address >= INTER_FLASH_END_ADDR))
    {
        return 0U;
    }

    return 1U;
}

// ================================================================
// 3. 从 Bootloader 安全跳转到用户 APP (0x08010000)
//
//    ⚠️ 极其关键的七步交接手术 (缺一不可，否则跳过去必进 HardFault)：
//      ① 校验 APP 向量表合法性
//      ② 关闭全局中断 (__disable_irq)
//      ③ 反初始化 Bootloader 用过的外设 (CAN1 / USART2 / USART3)
//      ④ 彻底关停 SysTick 滴答定时器，防止在 APP 初始化完成前触发中断
//      ⑤ 清空 NVIC 所有使能位 (ICER) 与挂起位 (ICPR)
//      ⑥ 重映射向量表偏移寄存器 SCB->VTOR = 0x08010000，并写入新栈顶 __set_MSP(app_msp)
//      ⑦ 内存屏障同步 (__DSB/__ISB) 后调用 jump2app() 跳入新程序
// ================================================================
uint8_t boot_check_stack2jump_app(uint32_t app_addr)
{
    uint32_t app_msp;
    uint32_t reset_handler;
    APP_FUNC jump2app;

    // ------- 1. 合法性前置拦截 -------
    if (boot_check_app_valid(app_addr) == 0U)
    {
        printf("APP Not Found!\r\n");
        return 1U;
    }

    app_msp = *(volatile const uint32_t *)app_addr;
    reset_handler = *(volatile const uint32_t *)(app_addr + 4U);
    jump2app = (APP_FUNC)reset_handler;

    // ------- 2. 关总中断，防止交接途中被打断 -------
    __disable_irq();

    // ------- 3. 拆卸 Bootloader 启用过的外设与时钟 -------
    HAL_CAN_DeInit(&hcan1);
    HAL_UART_DeInit(&huart2);
    HAL_UART_DeInit(&huart3);
    HAL_DeInit();

    // ------- 4. 关闭并清零 SysTick 硬件定时器 -------
    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;

    // ------- 5. 清除 NVIC 全部 8 组中断使能与挂起标志 -------
    for (uint32_t i = 0U; i < 8U; ++i)
    {
        NVIC->ICER[i] = 0xFFFFFFFFU;
        NVIC->ICPR[i] = 0xFFFFFFFFU;
    }

    // ------- 6. 切换向量表地址与主栈顶指针 MSP -------
    SCB->VTOR = app_addr;
    __set_CONTROL(0U);
    __set_MSP(app_msp);

    // ------- 7. 流水线屏障同步并执行跳转 -------
    __DSB();
    __ISB();

    jump2app();

    // 正常情况下 CPU 已进入 APP，永远不会执行到这里
    while (1)
    {
    }
}
