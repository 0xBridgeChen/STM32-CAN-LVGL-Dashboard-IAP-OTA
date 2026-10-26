/*********************************************************************************
 * @file    lcd_init.c
 * @brief   ST7796 3.5寸 IPS TFT-LCD (480x320) 底层总线与控制器初始化
 * 
 * ================================================================
 * 硬件外设与引脚电气定义：
 *   - 通信接口 : 4线制硬件高速 SPI1 (主机模式，波特率预分频 /2 -> 42 MBits/s)
 *   - SCK 时钟 : PA5 (SPI1_SCK)
 *   - MOSI数据 : PA7 (SPI1_MOSI，单向高速像素输出流)
 *   - CS 片选  : 软件 GPIO 控制，低电平有效
 *   - DC 数据/命令 : 低电平为命令 (Command)，高电平为数据 (Data)
 *   - RES 复位 : 硬件低电平复位
 *   - BLK 背光 : 高电平点亮背光
 * ================================================================
 *********************************************************************************/
#include "lcd_init.h"
#include "spi.h"

// ================================================================
// 1. LCD_Writ_Bus: SPI1 单字节阻塞发送接口
// ================================================================
void LCD_Writ_Bus(u8 dat)
{
    HAL_SPI_Transmit(&hspi1, &dat, 1, HAL_MAX_DELAY);
}

// ================================================================
// 2. LCD_WR_DATA8: 向屏幕写入 8 位数据 (Data)
// ================================================================
void LCD_WR_DATA8(u8 dat)
{
    LCD_Writ_Bus(dat);
}

// ================================================================
// 3. LCD_WR_DATA: 向屏幕写入 16 位数据 (RGB565 像素)
//    大端发送：先发送高 8 位，再发送低 8 位
// ================================================================
void LCD_WR_DATA(u16 dat)
{
    LCD_Writ_Bus(dat >> 8);
    LCD_Writ_Bus(dat);
}

// ================================================================
// 4. LCD_WR_REG: 向屏幕写入 8 位命令寄存器 (Command)
//    时序要求：DC 拉低写入指令，写入完毕后立即将 DC 恢复高电平准备接收数据
// ================================================================
void LCD_WR_REG(u8 dat)
{
    LCD_DC_Clr();  // DC 拉低：声明接下来写入的是控制命令
    LCD_Writ_Bus(dat);
    LCD_DC_Set();  // DC 拉高：默认恢复为数据接收状态
}

// ================================================================
// 5. LCD_Writ_Bus_Len: SPI1 连续突发数据流发送 (DMA/高速批量传输)
//    入参：*dat 数据起始指针, len 发送总字节数
// ================================================================
void LCD_Writ_Bus_Len(u8 *dat, uint32_t len)
{
    LCD_CS_Clr();  // 激活片选
    LCD_DC_Set();  // 确保处于数据通道模式
    HAL_SPI_Transmit(&hspi1, dat, len, 1000);
}

// ================================================================
// 6. LCD_Address_Set: 硬件物理显示开窗 (锁定重绘区域坐标)
//
//    ST7796 硬件寻址指令：
//      - 0x2A (CASET) : 列地址设置 (Column Address Set)，参数为 (Xstart_High, Xstart_Low, Xend_High, Xend_Low)
//      - 0x2B (RASET) : 行地址设置 (Row Address Set)，参数为 (Ystart_High, Ystart_Low, Yend_High, Yend_Low)
//      - 0x2C (RAMWR) : 存储器连续写入 (Memory Write)，后续写入的数据将直接刷入锁定的 GRAM
// ================================================================
void LCD_Address_Set(u16 x1, u16 y1, u16 x2, u16 y2)
{
    uint8_t xy_Buf[4];

    // ------- 1. 设置列地址 (X 坐标 0 ~ 479) -------
    LCD_WR_REG(0x2A);
    xy_Buf[0] = x1 >> 8;
    xy_Buf[1] = x1 & 0xFF;
    xy_Buf[2] = x2 >> 8;
    xy_Buf[3] = x2 & 0xFF;
    LCD_Writ_Bus_Len(xy_Buf, 4);

    // ------- 2. 设置行地址 (Y 坐标 0 ~ 319) -------
    LCD_WR_REG(0x2B);
    xy_Buf[0] = y1 >> 8;
    xy_Buf[1] = y1 & 0xFF;
    xy_Buf[2] = y2 >> 8;
    xy_Buf[3] = y2 & 0xFF;
    LCD_Writ_Bus_Len(xy_Buf, 4);

    // ------- 3. 激活 GRAM 写入指针 -------
    LCD_WR_REG(0x2C);
}

// ================================================================
// 7. LCD_Init: ST7796 液晶屏上电初始化序列
//
//    硬件复位与唤醒时序规范：
//      1) 硬件硬复位：RES 低电平持续 ≥10ms (本代码给 100ms)，拉高后需等待 ≥120ms (本代码给 200ms) 让芯片内部 LDO 稳定；
//      2) 0x11 (Sleep Out 退出睡眠)：⚠️ 关键手册要求，发送 0x11 后必须强延时 ≥120ms，
//         等待内部电荷泵倍压电路升压完毕，过早发送配置会导致寄存器写入失效或花屏！
//      3) 0x36 (MADCTL 存储器访问控制)：控制横屏模式、XY镜像、RGB/BGR通道；
//      4) 0x3A (COLMOD 像素格式)：0x05/0x55 表示 16-bit/pixel (RGB 5-6-5 格式)；
//      5) 0x29 (Display ON 开启显示)：点亮像素阵列。
// ================================================================
void LCD_Init(void)
{
    // ------- 1. 硬件引脚初始电平建立 -------
    LCD_CS_Clr();  // 默认使能片选
    LCD_DC_Set();  // 默认处于数据通道

    // ------- 2. 硬件硬复位时序 (保障寄存器彻底清零) -------
    LCD_RES_Clr();
    HAL_Delay(100);
    LCD_RES_Set();
    HAL_Delay(200);

    // ------- 3. 打开背光 -------
    LCD_BLK_Set();
    HAL_Delay(100);

    // ------- 4. 退出睡眠模式 (Sleep Out) -------
    LCD_WR_REG(0x11);
    HAL_Delay(120); // 手册规定必须延时 120ms 等待电荷泵起振稳定

    // ------- 5. 解锁 ST7796 扩展命令集 -------
    LCD_WR_REG(0xF0);
    LCD_WR_DATA8(0xC3);
    LCD_WR_REG(0xF0);
    LCD_WR_DATA8(0x96);

    // ------- 6. 屏幕旋转方向与显存映射 (MADCTL 0x36) -------
    LCD_WR_REG(0x36);
    if(USE_HORIZONTAL == 0)      LCD_WR_DATA8(0x48); // 竖屏
    else if(USE_HORIZONTAL == 1) LCD_WR_DATA8(0x88); // 竖屏翻转
    else if(USE_HORIZONTAL == 2) LCD_WR_DATA8(0x28); // 横屏 (270度)
    else                         LCD_WR_DATA8(0xE8); // 横屏 (90度推荐，双仪表显示方向)

    // ------- 7. 像素色彩格式 (COLMOD 0x3A) -------
    LCD_WR_REG(0x3A);
    LCD_WR_DATA8(0x05); // 16-bit RGB 5-6-5 色彩格式

    // ------- 8. 内部电源控制与驱动电压精细配置 -------
    LCD_WR_REG(0xE8);
    LCD_WR_DATA8(0x40);
    LCD_WR_DATA8(0x82);
    LCD_WR_DATA8(0x07);
    LCD_WR_DATA8(0x18);
    LCD_WR_DATA8(0x27);
    LCD_WR_DATA8(0x0A);
    LCD_WR_DATA8(0xB6);
    LCD_WR_DATA8(0x33);

    LCD_WR_REG(0xC5); // VCOM 控制
    LCD_WR_DATA8(0x27);

    LCD_WR_REG(0xC2); // 电源控制 3
    LCD_WR_DATA8(0xA7);

    // ------- 9. 正极性伽马校准 (Positive Gamma 0xE0) -------
    LCD_WR_REG(0xE0);
    LCD_WR_DATA8(0xF0);
    LCD_WR_DATA8(0x01);
    LCD_WR_DATA8(0x06);
    LCD_WR_DATA8(0x0F);
    LCD_WR_DATA8(0x12);
    LCD_WR_DATA8(0x1D);
    LCD_WR_DATA8(0x36);
    LCD_WR_DATA8(0x54);
    LCD_WR_DATA8(0x44);
    LCD_WR_DATA8(0x0C);
    LCD_WR_DATA8(0x18);
    LCD_WR_DATA8(0x16);
    LCD_WR_DATA8(0x13);
    LCD_WR_DATA8(0x15);

    // ------- 10. 负极性伽马校准 (Negative Gamma 0xE1) -------
    LCD_WR_REG(0xE1);
    LCD_WR_DATA8(0xF0);
    LCD_WR_DATA8(0x01);
    LCD_WR_DATA8(0x05);
    LCD_WR_DATA8(0x0A);
    LCD_WR_DATA8(0x0B);
    LCD_WR_DATA8(0x07);
    LCD_WR_DATA8(0x32);
    LCD_WR_DATA8(0x44);
    LCD_WR_DATA8(0x44);
    LCD_WR_DATA8(0x0C);
    LCD_WR_DATA8(0x18);
    LCD_WR_DATA8(0x17);
    LCD_WR_DATA8(0x13);
    LCD_WR_DATA8(0x16);

    // ------- 11. 重新锁定扩展寄存器 -------
    LCD_WR_REG(0xF0);
    LCD_WR_DATA8(0x3C);
    LCD_WR_REG(0xF0);
    LCD_WR_DATA8(0x69);

    // ------- 12. 正式开启显示输出 (Display ON) -------
    LCD_WR_REG(0x29);
}
