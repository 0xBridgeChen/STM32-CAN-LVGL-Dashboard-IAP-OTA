#include "blackcat_ui.h"
#include "lcd.h"
#include "lcd_init.h"
#include <stdio.h>

#define RADAR_CX    160
#define RADAR_CY    295
#define RADAR_R     50

/* 24 个角度的 sin/cos 预计算表（快速无浮点计算，确保极速渲染） */
static const int8_t cos_table[24] = {
    50, 48, 43, 35, 25, 13, 0, -13, -25, -35, -43, -48,
    -50, -48, -43, -35, -25, -13, 0, 13, 25, 35, 43, 48
};
static const int8_t sin_table[24] = {
    0, 13, 25, 35, 43, 48, 50, 48, 43, 35, 25, 13,
    0, -13, -25, -35, -43, -48, -50, -48, -43, -35, -25, -13
};

static uint8_t last_radar_step = 0;

/* 绘制黑猫警长 36x36 经典像素风立体头像（带眨眼微动效） */
void BlackCat_DrawAvatar(uint16_t x, uint16_t y, uint8_t blink_state)
{
    /* 1. 白色大沿警帽顶部 */
    LCD_Fill(x + 7, y + 0, x + 28, y + 2, WHITE);
    LCD_Fill(x + 4, y + 3, x + 31, y + 6, WHITE);

    /* 2. 金黄警徽（五角警星） */
    LCD_Fill(x + 16, y + 2, x + 19, y + 5, YELLOW);
    LCD_DrawPoint(x + 17, y + 1, YELLOW);
    LCD_DrawPoint(x + 18, y + 1, YELLOW);

    /* 3. 黑色高光帽檐 */
    LCD_Fill(x + 2, y + 7, x + 33, y + 9, 0x2104);
    LCD_DrawLine(x + 5, y + 8, x + 30, y + 8, CYAN);

    /* 4. 警猫耳朵（黑外耳 + 粉内耳） */
    LCD_Fill(x + 3, y + 10, x + 8, y + 15, BLACK);
    LCD_Fill(x + 4, y + 11, x + 7, y + 14, 0xFD34); /* 粉色内耳 */
    LCD_Fill(x + 27, y + 10, x + 32, y + 15, BLACK);
    LCD_Fill(x + 28, y + 11, x + 31, y + 14, 0xFD34);

    /* 5. 黑猫头部轮廓 */
    LCD_Fill(x + 8, y + 10, x + 27, y + 34, BLACK);
    LCD_Fill(x + 6, y + 15, x + 29, y + 31, BLACK);

    /* 6. 机敏警瞳 */
    if (blink_state == 0)
    {
        /* 绿宝石大眼睛 + 黑色竖瞳 */
        LCD_Fill(x + 9, y + 18, x + 15, y + 23, GREEN);
        LCD_Fill(x + 11, y + 17, x + 13, y + 24, GREEN);
        LCD_Fill(x + 12, y + 18, x + 13, y + 23, BLACK);
        LCD_DrawPoint(x + 10, y + 19, WHITE);

        LCD_Fill(x + 20, y + 18, x + 26, y + 23, GREEN);
        LCD_Fill(x + 22, y + 17, x + 24, y + 24, GREEN);
        LCD_Fill(x + 22, y + 18, x + 23, y + 23, BLACK);
        LCD_DrawPoint(x + 24, y + 19, WHITE);
    }
    else
    {
        /* 眨眼微动效：利落的水平白线 */
        LCD_Fill(x + 9, y + 17, x + 15, y + 24, BLACK);
        LCD_Fill(x + 20, y + 17, x + 26, y + 24, BLACK);
        LCD_DrawLine(x + 9, y + 20, x + 15, y + 20, WHITE);
        LCD_DrawLine(x + 20, y + 20, x + 26, y + 20, WHITE);
    }

    /* 7. 粉色倒三角猫鼻与嘴部 */
    LCD_Fill(x + 16, y + 26, x + 19, y + 27, 0xFD34);
    LCD_DrawPoint(x + 17, y + 28, 0xFD34);
    LCD_DrawLine(x + 17, y + 29, x + 18, y + 29, WHITE);

    /* 8. 经典三大白胡须（左右两侧放射状） */
    LCD_DrawLine(x + 1, y + 24, x + 10, y + 26, WHITE);
    LCD_DrawLine(x + 0, y + 28, x + 11, y + 28, WHITE);
    LCD_DrawLine(x + 1, y + 32, x + 10, y + 30, WHITE);

    LCD_DrawLine(x + 25, y + 26, x + 34, y + 24, WHITE);
    LCD_DrawLine(x + 24, y + 28, x + 35, y + 28, WHITE);
    LCD_DrawLine(x + 25, y + 30, x + 34, y + 32, WHITE);
}

/* 警用红蓝高频交替爆闪警灯（100ms 爆闪动效） */
void BlackCat_UpdateSiren(uint8_t tick_step, uint8_t siren_on)
{
    if (!siren_on)
    {
        LCD_Fill(6, 6, 48, 34, 0x18C3); /* 静默暗灰蓝 */
        LCD_Fill(272, 6, 314, 34, 0x18C3);
        return;
    }

    uint8_t phase = tick_step % 8;

    if (phase == 0 || phase == 2)
    {
        /* 左红高频亮，右暗蓝 */
        LCD_Fill(6, 6, 48, 34, RED);
        LCD_Fill(272, 6, 314, 34, 0x000F);
    }
    else if (phase == 4 || phase == 6)
    {
        /* 左暗红，右蓝高频亮 */
        LCD_Fill(6, 6, 48, 34, 0x3000);
        LCD_Fill(272, 6, 314, 34, CYAN);
    }
    else
    {
        /* 爆闪间歇灭 */
        LCD_Fill(6, 6, 48, 34, BLACK);
        LCD_Fill(272, 6, 314, 34, BLACK);
    }
}

/* 绘制雷达静态刻度圆盘 */
static void BlackCat_DrawRadarRings(void)
{
    /* 同心圆与十字准星 */
    for (int r = 16; r <= RADAR_R; r += 17)
    {
        for (int i = 0; i < 24; i++)
        {
            int16_t px = RADAR_CX + (cos_table[i] * r) / RADAR_R;
            int16_t py = RADAR_CY + (sin_table[i] * r) / RADAR_R;
            LCD_DrawPoint(px, py, 0x0320); /* 军工暗绿色 */
        }
    }
    LCD_DrawLine(RADAR_CX - RADAR_R, RADAR_CY, RADAR_CX + RADAR_R, RADAR_CY, 0x0208);
    LCD_DrawLine(RADAR_CX, RADAR_CY - RADAR_R, RADAR_CX, RADAR_CY + RADAR_R, 0x0208);
}

/* “搜捕一只耳”动态旋转雷达扫描光束 */
void BlackCat_UpdateRadar(uint8_t step, uint8_t target_detected)
{
    step = step % 24;

    /* 擦除上一帧扫描线 */
    int16_t old_x = RADAR_CX + cos_table[last_radar_step];
    int16_t old_y = RADAR_CY + sin_table[last_radar_step];
    LCD_DrawLine(RADAR_CX, RADAR_CY, old_x, old_y, BLACK);

    /* 绘制新一帧荧光绿雷达扫描线 */
    int16_t new_x = RADAR_CX + cos_table[step];
    int16_t new_y = RADAR_CY + sin_table[step];
    LCD_DrawLine(RADAR_CX, RADAR_CY, new_x, new_y, GREEN);

    /* 快速修补被扫描线经过的刻度点 */
    BlackCat_DrawRadarRings();

    /* “一只耳”目标光点（方位：右上象限，相对坐标 +26, -24） */
    int16_t target_x = RADAR_CX + 26;
    int16_t target_y = RADAR_CY - 24;

    if (target_detected)
    {
        /* 扫描线划过目标方位时高亮爆发 */
        if (step >= 18 && step <= 22)
        {
            LCD_Fill(target_x - 3, target_y - 3, target_x + 3, target_y + 3, RED);
            LCD_ShowString(185, 260, (const u8 *)"ONE-EAR!", RED, BLACK, 12, 0);
        }
        else
        {
            LCD_Fill(target_x - 2, target_y - 2, target_x + 2, target_y + 2, YELLOW);
            LCD_ShowString(185, 260, (const u8 *)"LOCKED  ", YELLOW, BLACK, 12, 0);
        }
    }
    else
    {
        LCD_Fill(target_x - 3, target_y - 3, target_x + 3, target_y + 3, BLACK);
    }

    last_radar_step = step;
}

/* 绘制底部 3 个战术交互触控按钮 */
void BlackCat_UpdateButtons(uint8_t siren_on, uint8_t pursuit_mode)
{
    /* 按钮 1: [ 警灯 SIREN ] (X: 8 ~ 106, Y: 388 ~ 426) */
    LCD_DrawRectangle(8, 388, 106, 426, siren_on ? RED : GRAYBLUE);
    LCD_Fill(9, 389, 105, 425, siren_on ? 0x6000 : DARKBLUE);
    LCD_ShowString(16, 396, (const u8 *)"SIREN", WHITE, siren_on ? 0x6000 : DARKBLUE, 16, 0);
    LCD_ShowString(65, 396, siren_on ? (const u8 *)"ON " : (const u8 *)"OFF",
                   siren_on ? YELLOW : GRAY, siren_on ? 0x6000 : DARKBLUE, 16, 0);

    /* 按钮 2: [ 追捕 PURSUIT ] (X: 112 ~ 210, Y: 388 ~ 426) */
    u16 p_bg = (pursuit_mode == MODE_PURSUIT) ? 0x8000 : DARKBLUE;
    LCD_DrawRectangle(112, 388, 210, 426, (pursuit_mode == MODE_PURSUIT) ? RED : CYAN);
    LCD_Fill(113, 389, 209, 425, p_bg);
    LCD_ShowString(120, 396, (pursuit_mode == MODE_PURSUIT) ? (const u8 *)"PURSUIT" : (const u8 *)"PATROL ",
                   WHITE, p_bg, 16, 0);

    /* 按钮 3: [ 警笛 HORN/ALERT ] (X: 216 ~ 312, Y: 388 ~ 426) */
    LCD_DrawRectangle(216, 388, 312, 426, YELLOW);
    LCD_Fill(217, 389, 311, 425, 0x4200);
    LCD_ShowString(228, 396, (const u8 *)"ALERT", YELLOW, 0x4200, 16, 0);
}

/* 全屏高光警笛警报动效 */
void BlackCat_TriggerAlertEffect(void)
{
    LCD_Fill(10, 348, 310, 382, RED);
    LCD_ShowString(18, 356, (const u8 *)"*** PURSUE ONE-EAR! ***", WHITE, RED, 16, 0);
    HAL_Delay(120);
    LCD_Fill(10, 348, 310, 382, BLACK);
}

/* 绘制黑猫警长静态总仪表框架 */
void BlackCat_DrawStaticLayout(void)
{
    LCD_Fill(0, 0, LCD_W, LCD_H, BLACK);

    /* 1. 顶部警车双侧爆闪灯底座与标题 */
    LCD_DrawRectangle(4, 4, 315, 36, CYAN);
    LCD_Fill(49, 5, 271, 35, DARKBLUE);
    LCD_ShowString(58, 12, (const u8 *)"BLACK CAT DETECTIVE", YELLOW, DARKBLUE, 16, 0);

    /* 2. 左上角黑猫警长战术卡片 (Y: 42 ~ 124) */
    LCD_DrawRectangle(6, 42, 140, 124, CYAN);
    LCD_Fill(7, 43, 139, 60, DARKBLUE);
    LCD_ShowString(12, 44, (const u8 *)"CHIEF CAT", WHITE, DARKBLUE, 16, 0);
    /* 绘制经典头像 */
    BlackCat_DrawAvatar(12, 70, 0);
    LCD_ShowString(60, 72, (const u8 *)"BADGE#", GRAY, BLACK, 12, 0);
    LCD_ShowString(60, 88, (const u8 *)"000001", YELLOW, BLACK, 16, 0);
    LCD_ShowString(60, 106, (const u8 *)"ACTIVE", GREEN, BLACK, 12, 0);

    /* 3. 右上角战术状态卡片 (Y: 42 ~ 124) */
    LCD_DrawRectangle(146, 42, 313, 124, GRAYBLUE);
    LCD_Fill(147, 43, 312, 60, DARKBLUE);
    LCD_ShowString(154, 44, (const u8 *)"TACTICAL HUD", WHITE, DARKBLUE, 16, 0);
    LCD_ShowString(154, 68, (const u8 *)"THREAT : LEVEL 3", RED, BLACK, 12, 0);
    LCD_ShowString(154, 86, (const u8 *)"TARGET : ONE-EAR", YELLOW, BLACK, 12, 0);
    LCD_ShowString(154, 104, (const u8 *)"STATUS : LOCKED", GREEN, BLACK, 12, 0);

    /* 4. 中央警用极速仪表盘 (Y: 130 ~ 230) */
    LCD_DrawRectangle(6, 130, 313, 230, GRAYBLUE);
    LCD_DrawLine(6, 150, 313, 150, CYAN);
    LCD_Fill(7, 131, 312, 149, DARKBLUE);
    LCD_ShowString(100, 133, (const u8 *)"POLICE PURSUIT", YELLOW, DARKBLUE, 16, 0);

    LCD_ShowString(200, 172, (const u8 *)"KM/H", CYAN, BLACK, 16, 0);
    LCD_ShowString(18, 206, (const u8 *)"RPM:", GRAY, BLACK, 12, 0);

    /* 绘制 14 格动态转速能量条背景槽 */
    for (int i = 0; i < 14; i++)
    {
        LCD_DrawRectangle(50 + i * 18, 204, 50 + i * 18 + 14, 222, GRAYBLUE);
        LCD_Fill(51 + i * 18, 205, 50 + i * 18 + 13, 221, BLACK);
    }

    /* 5. 战术雷达盘区域 (Y: 236 ~ 382) */
    LCD_DrawRectangle(6, 236, 313, 382, GRAYBLUE);
    LCD_Fill(7, 237, 312, 252, DARKBLUE);
    LCD_ShowString(14, 238, (const u8 *)"TARGET RADAR: ONE-EAR LOCATOR", CYAN, DARKBLUE, 12, 0);
    BlackCat_DrawRadarRings();

    /* 6. 绘制触控按钮 (Y: 388 ~ 426) */
    BlackCat_UpdateButtons(1, MODE_PURSUIT);

    /* 7. 底部 CAN 总线监控条 (Y: 432 ~ 476) */
    LCD_DrawRectangle(4, 432, 315, 476, CYAN);
    LCD_ShowString(10, 438, (const u8 *)"CAN1: 500K | ID:0x123 | RX: 000000", CYAN, BLACK, 12, 0);
    LCD_ShowString(10, 456, (const u8 *)"RAW : 00 01 02 03 04 05 06 07", GREEN, BLACK, 12, 0);
}

