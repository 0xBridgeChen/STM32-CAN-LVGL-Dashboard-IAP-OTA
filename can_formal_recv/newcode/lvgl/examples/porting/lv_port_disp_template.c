/**
 * @file lv_port_disp_template.c
 * @brief LVGL 8.3.11 屏幕接口驱动移植层 (ST7796 3.5寸 480x320 SPI 屏)
 * 
 * ================================================================
 * 【嵌入式 GUI 显存瓶颈与架构解法】:
 * 
 * 1. 物理 SRAM 极限约束:
 *    - ST7796 分辨率: 480 × 320 像素
 *    - 16 位色彩深度 (RGB565): 1 个像素占用 2 字节
 *    - 理论全屏显存: 480 × 320 × 2 字节 = 307,200 字节 ≈ 300 KB！
 *    - STM32F407VET6 内部通用 SRAM 仅有 128 KB (另有 64KB CCM RAM 不可直接用于 DMA/外设)。
 *    - 结论：试图开辟整屏 FrameBuffer 会直接爆 RAM 导致芯片进入 HardFault 崩溃死机！
 * 
 * 2. 工业级行缓冲 (Line Buffer) 局部渲染方案:
 *    - 本工程采用局部行缓冲策略：开辟 10 行局部缓冲区:
 *      480 × 10 × 2 字节 = 9,600 字节 ≈ 9.38 KB (极小内存消耗)。
 *    - LVGL 内部渲染器采用【脏矩形 (Dirty Areas) 裁剪切片算法】，
 *      仅对发生变化的仪表指针、文字等区域分片重绘，并通过 disp_flush 流式推送到屏，
 *      在 128KB 紧凑内存下实现 60+ FPS 的极速丝滑动态渲染！
 * 
 * 3. ⚠️ 极其关键的生命线 (防假死死锁):
 *    - disp_flush() 发送完毕后，必须无条件执行 `lv_disp_flush_ready(disp_drv)`！
 *    - 若漏调或延时调用，LVGL 核心调度器会判定屏幕总线一直处于 Hardware Busy 状态，
 *      导致主循环 lv_timer_handler() 彻底挂起，整个仪表系统永久假死！
 * ================================================================
 */

/*Copy this file as "lv_port_disp.c" and set this value to "1" to enable content*/
#if 1

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_disp_template.h"
#include <stdbool.h>
#include "lcd_init.h"
#include "lcd.h"

/*********************
 *      DEFINES
 *********************/
#ifndef MY_DISP_HOR_RES
    #define MY_DISP_HOR_RES    LCD_W    // 屏幕物理水平分辨率 (480)
#endif

#ifndef MY_DISP_VER_RES
    #define MY_DISP_VER_RES    LCD_H    // 屏幕物理垂直分辨率 (320)
#endif

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void disp_init(void);
static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p);

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

// ================================================================
// 1. lv_port_disp_init: LVGL 显示驱动注册与显存缓冲初始化
// ================================================================
void lv_port_disp_init(void)
{
    // ------- 步骤 1: 底层硬件屏幕与 SPI 总线初始化 -------
    disp_init();

    // ------- 步骤 2: 开辟局部行绘制缓冲区 -------
    // 480 × 10 像素行缓冲，占用内存 9.6 KB，由 LVGL 动态分片填装像素
    static lv_disp_draw_buf_t draw_buf_dsc_1;
    static lv_color_t buf_1[MY_DISP_HOR_RES * 10];
    lv_disp_draw_buf_init(&draw_buf_dsc_1, buf_1, NULL, MY_DISP_HOR_RES * 10);

    // ------- 步骤 3: 初始化显示驱动描述符 -------
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);

    // ------- 步骤 4: 绑定屏幕物理分辨率 -------
    disp_drv.hor_res = MY_DISP_HOR_RES;  // 480 像素
    disp_drv.ver_res = MY_DISP_VER_RES;  // 320 像素

    // ------- 步骤 5: 挂载核心刷屏回调函数 -------
    disp_drv.flush_cb = disp_flush;

    // ------- 步骤 6: 关联绘图缓冲区 -------
    disp_drv.draw_buf = &draw_buf_dsc_1;

    // ------- 步骤 7: 正式向 LVGL 核心注册显示设备 -------
    lv_disp_drv_register(&disp_drv);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

// ================================================================
// 2. disp_init: 屏幕硬件外设启动
// ================================================================
static void disp_init(void)
{
    LCD_Init(); // 执行 ST7796 硬件硬复位、Sleep-Out 与 MADCTL 横屏初始化
}

volatile bool disp_flush_enabled = true;

void disp_enable_update(void)
{
    disp_flush_enabled = true;
}

void disp_disable_update(void)
{
    disp_flush_enabled = false;
}

// ================================================================
// 3. disp_flush: LVGL 局部脏矩形刷屏回调接口 (核心渲染通道)
//
//    参数说明：
//      - disp_drv : 显示驱动结构体指针
//      - area     : 本次重绘脏矩形的绝对物理坐标 (x1, y1) 到 (x2, y2)
//      - color_p  : 局部行缓冲区首地址 (16 位 RGB565 像素流)
//
//    流水线执行机制：
//      1) 判断是否使能刷新；
//      2) 调用底层 LCD_Color_Fill 进行单周期 REV16 字节序转换并灌入 ST7796；
//      3) ★ 必须立即通知 LVGL 本次分片完成，释放缓冲区进行下一次渲染！
// ================================================================
static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p)
{
    // ------- 1. 硬件刷屏输出 -------
    if(disp_flush_enabled) {
        LCD_Color_Fill(area->x1, area->y1, area->x2, area->y2, color_p);
    }

    // ------- 2. ⚠️ 释放缓冲区并唤醒 LVGL 渲染管线 (绝对不可遗漏) -------
    lv_disp_flush_ready(disp_drv);
}

#endif
