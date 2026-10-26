#include "cyber_ui.h"
#include "lcd.h"
#include "lcd_init.h"
#include <stdio.h>
#include <string.h>

/* ==============================================================================
 *  “黑猫警长·战术拦截机（CYBER INTERCEPTOR）” 超跑级数字座舱渲染引擎实现
 * ============================================================================== */

#define RADAR_CX    160
#define RADAR_CY    322
#define RADAR_R     54

/* 36 步 360° 极坐标旋转三角函数表 (步长 10°，放大 1000 倍) */
static const int16_t cos36[36] = {
    1000, 984, 939, 866, 766, 642, 500, 342, 173, 0,
    -174, -343, -500, -643, -766, -866, -940, -985, -1000, -985,
    -940, -866, -766, -643, -500, -343, -174, 0, 173, 342,
    500, 642, 766, 866, 939, 984
};

static const int16_t sin36[36] = {
    0, 173, 342, 500, 642, 766, 866, 939, 984, 1000,
    984, 939, 866, 766, 642, 500, 342, 173, 0, -174,
    -343, -500, -643, -766, -866, -940, -985, -1000, -985, -940,
    -866, -766, -643, -500, -343, -174
};

/* 16 级转速仪表弧刻度坐标表 (半圆包络 X, Y) */
static const int16_t rpm_arc_x[16] = {
    86,  90,  98, 110, 126, 144, 164, 184,
    202, 218, 230, 238, 242, 244, 242, 238
};
static const int16_t rpm_arc_y[16] = {
    215, 198, 182, 168, 156, 148, 144, 144,
    148, 156, 168, 182, 198, 215, 230, 242
};

static uint8_t last_radar_step = 0;
static uint16_t last_speed_val = 0xFFFF;
static uint16_t last_rpm_val = 0xFFFF;

/* 绘制带科技感切角的暗夜卡片底座 */
static void Cyber_DrawChamferCard(int16_t x1, int16_t y1, int16_t x2, int16_t y2, u16 border_color, u16 fill_color)
{
    /* 填充切角主体 */
    LCD_Fill(x1 + 3, y1, x2 - 3, y2, fill_color);
    LCD_Fill(x1, y1 + 3, x1 + 2, y2 - 3, fill_color);
    LCD_Fill(x2 - 2, y1 + 3, x2, y2 - 3, fill_color);

    /* 绘制切角斜边与外轮廓 */
    LCD_DrawLine(x1 + 3, y1, x2 - 3, y1, border_color);
    LCD_DrawLine(x1 + 3, y2, x2 - 3, y2, border_color);
    LCD_DrawLine(x1, y1 + 3, x1, y2 - 3, border_color);
    LCD_DrawLine(x2, y1 + 3, x2, y2 - 3, border_color);

    /* 4 个切角斜角连线 */
    LCD_DrawLine(x1, y1 + 3, x1 + 3, y1, border_color);
    LCD_DrawLine(x2 - 3, y1, x2, y1 + 3, border_color);
    LCD_DrawLine(x1, y2 - 3, x1 + 3, y2, border_color);
    LCD_DrawLine(x2 - 3, y2, x2, y2 - 3, border_color);
}

/* 绘制大号超跑数字车速（无重绘闪烁） */
static void Cyber_DrawSpeedDigits(int16_t cx, int16_t cy, uint16_t speed, u16 color, u16 bg_color)
{
    char buf[8];
    sprintf(buf, "%3u", speed);

    /* 使用高清 32pt 点阵显示 3 位速度数字 */
    LCD_ShowString(cx - 38, cy - 16, (const u8 *)buf, color, bg_color, 32, 0);
}

/* 绘制雷达刻度盘网格 */
static void Cyber_DrawRadarGrid(void)
{
    /* 1. 双同心圆测距环 (50m, 100m) */
    for (int i = 0; i < 36; i += 2)
    {
        int16_t x50 = RADAR_CX + (cos36[i] * 28) / 1000;
        int16_t y50 = RADAR_CY + (sin36[i] * 28) / 1000;
        LCD_DrawPoint(x50, y50, 0x1925); /* 暗绿基底 */

        int16_t x100 = RADAR_CX + (cos36[i] * RADAR_R) / 1000;
        int16_t y100 = RADAR_CY + (sin36[i] * RADAR_R) / 1000;
        LCD_DrawPoint(x100, y100, 0x2287);
    }

    /* 2. 十字瞄准基准线与中心点 */
    LCD_DrawLine(RADAR_CX - RADAR_R + 2, RADAR_CY, RADAR_CX + RADAR_R - 2, RADAR_CY, COLOR_GRID_DIM);
    LCD_DrawLine(RADAR_CX, RADAR_CY - RADAR_R + 2, RADAR_CX, RADAR_CY + RADAR_R - 2, COLOR_GRID_DIM);
    LCD_Fill(RADAR_CX - 1, RADAR_CY - 1, RADAR_CX + 1, RADAR_CY + 1, COLOR_CYAN_GLOW);
}

/* 初始化整个座舱静态框架 */
void Cyber_InitCockpit(void)
{
    Cyber_DrawStaticFrame(COCKPIT_MODE_PURSUIT);
}

void Cyber_DrawStaticFrame(uint8_t mode)
{
    u16 theme_color = (mode == COCKPIT_MODE_PURSUIT) ? COLOR_PURSUIT_RED : COLOR_CYAN_GLOW;

    /* 1. 全屏沉浸式曜石黑底漆 */
    LCD_Fill(0, 0, LCD_W, LCD_H, COLOR_OBSIDIAN);

    /* 2. 顶栏：警用爆闪灯与战术拦截机标题卡片 (Y: 4 ~ 38) */
    Cyber_DrawChamferCard(50, 4, 269, 38, theme_color, COLOR_CARD_BG);
    LCD_ShowString(64, 14, (const u8 *)"[ POLICE INTERCEPTOR ]", COLOR_AMBER_GOLD, COLOR_CARD_BG, 16, 0);

    /* 3. 上部状态卡片 (Y: 44 ~ 108) */
    /* 左卡片：特警身份与状态 */
    Cyber_DrawChamferCard(6, 44, 156, 108, COLOR_CARD_BORDER, COLOR_CARD_BG);
    LCD_ShowString(14, 52,  (const u8 *)"SHERIFF# 01", COLOR_CYAN_GLOW, COLOR_CARD_BG, 12, 0);
    LCD_ShowString(14, 70,  (const u8 *)"TAG: ONE-EAR", COLOR_AMBER_GOLD, COLOR_CARD_BG, 12, 0);
    LCD_ShowString(14, 88,  (const u8 *)"SYS: 13.8V OK", COLOR_LASER_GREEN, COLOR_CARD_BG, 12, 0);

    /* 右卡片：战术模式与挡位 */
    Cyber_DrawChamferCard(163, 44, 313, 108, COLOR_CARD_BORDER, COLOR_CARD_BG);
    if (mode == COCKPIT_MODE_PURSUIT)
    {
        LCD_ShowString(172, 52, (const u8 *)"MODE: PURSUIT ", COLOR_PURSUIT_RED, COLOR_CARD_BG, 12, 0);
        LCD_ShowString(172, 70, (const u8 *)"THREAT: LEVEL 3", COLOR_PURSUIT_RED, COLOR_CARD_BG, 12, 0);
    }
    else
    {
        LCD_ShowString(172, 52, (const u8 *)"MODE: PATROL  ", COLOR_ICE_BLUE, COLOR_CARD_BG, 12, 0);
        LCD_ShowString(172, 70, (const u8 *)"THREAT: LEVEL 1", COLOR_LASER_GREEN, COLOR_CARD_BG, 12, 0);
    }
    LCD_ShowString(172, 88, (const u8 *)"GEAR: P R N[D]S", COLOR_TEXT_WHITE, COLOR_CARD_BG, 12, 0);

    /* 4. 中央超跑双弧复合仪表盘卡片 (Y: 114 ~ 254) */
    Cyber_DrawChamferCard(6, 114, 313, 254, theme_color, COLOR_CARD_BG);
    LCD_ShowString(140, 122, (const u8 *)"SPEED", COLOR_TEXT_MUTED, COLOR_CARD_BG, 12, 0);
    LCD_ShowString(146, 192, (const u8 *)"KM/H", COLOR_CYAN_GLOW, COLOR_CARD_BG, 16, 0);

    /* 绘制转速弧刻度背景槽 */
    for (int i = 0; i < 16; i++)
    {
        LCD_Fill(rpm_arc_x[i] - 2, rpm_arc_y[i] - 2, rpm_arc_x[i] + 2, rpm_arc_y[i] + 2, COLOR_GRID_DIM);
    }
    LCD_ShowString(18, 234, (const u8 *)"RPM  0K .......... 4K .......... 8K", COLOR_TEXT_MUTED, COLOR_CARD_BG, 12, 0);

    /* 5. 360° 战术雷达扫描卡片 (Y: 260 ~ 388) */
    Cyber_DrawChamferCard(6, 260, 313, 388, COLOR_CARD_BORDER, COLOR_CARD_BG);
    LCD_ShowString(14, 268, (const u8 *)"RADAR: 360 TACTICAL HUD", COLOR_CYAN_GLOW, COLOR_CARD_BG, 12, 0);
    LCD_ShowString(216, 268, (const u8 *)"RANGE: 200M", COLOR_TEXT_MUTED, COLOR_CARD_BG, 12, 0);
    Cyber_DrawRadarGrid();

    /* 6. 底部 3 颗触控交互大按键 (Y: 394 ~ 434) */
    Cyber_UpdateButtons(1, mode);

    /* 7. 底栏：CAN 总线监视器 (Y: 440 ~ 476) */
    Cyber_DrawChamferCard(6, 440, 313, 476, COLOR_CARD_BORDER, COLOR_CARD_BG);
    LCD_ShowString(14, 446, (const u8 *)"CAN1: 500K | ID:0x123 | PACKETS: 000000", COLOR_CYAN_GLOW, COLOR_CARD_BG, 12, 0);
    LCD_ShowString(14, 460, (const u8 *)"RAW : 00 01 02 03 04 05 06 07", COLOR_LASER_GREEN, COLOR_CARD_BG, 12, 0);

    last_speed_val = 0xFFFF;
    last_rpm_val = 0xFFFF;
}

/* 警车多段式流光交替高频爆闪 (5 段光柱, 80ms 真实警车节奏) */
void Cyber_UpdateStrobeLights(uint32_t step, uint8_t enabled)
{
    if (!enabled)
    {
        /* 静音模式：全部熄灭为暗蓝光槽 */
        for (int i = 0; i < 5; i++)
        {
            LCD_Fill(6 + i * 8, 10, 11 + i * 8, 32, 0x10A4);
            LCD_Fill(274 + i * 8, 10, 279 + i * 8, 32, 0x10A4);
        }
        return;
    }

    uint8_t phase = step % 8;

    /* 左侧 5 段光柱渲染 */
    if (phase == 0 || phase == 2)
    {
        /* 爆发高亮猩红 */
        for (int i = 0; i < 5; i++)
        {
            LCD_Fill(6 + i * 8, 10, 11 + i * 8, 32, COLOR_PURSUIT_RED);
        }
    }
    else if (phase == 1)
    {
        /* 衰减微红 */
        for (int i = 0; i < 5; i++)
        {
            LCD_Fill(6 + i * 8, 10, 11 + i * 8, 32, 0x8000);
        }
    }
    else
    {
        /* 熄灭期 */
        for (int i = 0; i < 5; i++)
        {
            LCD_Fill(6 + i * 8, 10, 11 + i * 8, 32, 0x0821);
        }
    }

    /* 右侧 5 段光柱渲染 */
    if (phase == 4 || phase == 6)
    {
        /* 爆发高亮极光蓝 */
        for (int i = 0; i < 5; i++)
        {
            LCD_Fill(274 + i * 8, 10, 279 + i * 8, 32, COLOR_CYAN_GLOW);
        }
    }
    else if (phase == 5)
    {
        /* 衰减微蓝 */
        for (int i = 0; i < 5; i++)
        {
            LCD_Fill(274 + i * 8, 10, 279 + i * 8, 32, 0x0115);
        }
    }
    else
    {
        /* 熄灭期 */
        for (int i = 0; i < 5; i++)
        {
            LCD_Fill(274 + i * 8, 10, 279 + i * 8, 32, 0x0821);
        }
    }
}

/* 360° 旋转激光雷达平滑扫描 (40 FPS) */
void Cyber_UpdateRadar(uint8_t angle_step, uint8_t target_locked)
{
    angle_step = angle_step % 36;

    /* 1. 擦除上一帧激光扫描线 (仅覆盖画过的线条，零闪烁) */
    int16_t old_x = RADAR_CX + (cos36[last_radar_step] * RADAR_R) / 1000;
    int16_t old_y = RADAR_CY + (sin36[last_radar_step] * RADAR_R) / 1000;
    LCD_DrawLine(RADAR_CX, RADAR_CY, old_x, old_y, COLOR_CARD_BG);

    /* 2. 绘制新一帧高亮荧光绿扫描线 */
    int16_t new_x = RADAR_CX + (cos36[angle_step] * RADAR_R) / 1000;
    int16_t new_y = RADAR_CY + (sin36[angle_step] * RADAR_R) / 1000;
    LCD_DrawLine(RADAR_CX, RADAR_CY, new_x, new_y, COLOR_LASER_GREEN);

    /* 3. 快速重补网格交点 */
    Cyber_DrawRadarGrid();

    /* 4. 目标点“一只耳”拦截标记 (方位: 右上角 30° 象限) */
    int16_t tx = RADAR_CX + 35;
    int16_t ty = RADAR_CY - 28;

    /* 扫描光线扫过目标时爆发强警报红光 */
    if (angle_step >= 28 && angle_step <= 32)
    {
        LCD_Fill(tx - 3, ty - 3, tx + 3, ty + 3, COLOR_PURSUIT_RED);
        LCD_ShowString(180, 350, (const u8 *)"LOCK: ONE-EAR!", COLOR_PURSUIT_RED, COLOR_CARD_BG, 12, 0);
    }
    else
    {
        u16 blip_col = target_locked ? COLOR_AMBER_GOLD : COLOR_GRID_DIM;
        LCD_Fill(tx - 2, ty - 2, tx + 2, ty + 2, blip_col);
        LCD_ShowString(180, 350, (const u8 *)"TRACKING 0x123", COLOR_TEXT_MUTED, COLOR_CARD_BG, 12, 0);
    }

    last_radar_step = angle_step;
}

/* 局部增量刷新车速与动态转速弧 (零全屏重绘) */
void Cyber_UpdateSpeedAndRPM(uint16_t speed, uint16_t rpm, uint8_t mode)
{
    /* 1. 车速更新 (脏矩形覆盖刷新) */
    if (speed != last_speed_val)
    {
        u16 spd_col = (mode == COCKPIT_MODE_PURSUIT) ? COLOR_PURSUIT_RED :
                      ((speed > 100) ? COLOR_AMBER_GOLD : COLOR_TEXT_WHITE);
        Cyber_DrawSpeedDigits(160, 162, speed, spd_col, COLOR_CARD_BG);
        last_speed_val = speed;
    }

    /* 2. 转速弧刻度流光更新 (16 个刻度) */
    uint8_t active_ticks = (rpm > 8000) ? 16 : (uint8_t)(rpm * 16 / 8000);
    if (rpm != last_rpm_val)
    {
        for (int i = 0; i < 16; i++)
        {
            if (i < active_ticks)
            {
                /* 低转速冰蓝 -> 中转速钛金 -> 红线暴怒红 */
                u16 tick_col = (i < 9) ? COLOR_CYAN_GLOW :
                               ((i < 13) ? COLOR_AMBER_GOLD : COLOR_PURSUIT_RED);
                LCD_Fill(rpm_arc_x[i] - 2, rpm_arc_y[i] - 2, rpm_arc_x[i] + 2, rpm_arc_y[i] + 2, tick_col);
            }
            else
            {
                LCD_Fill(rpm_arc_x[i] - 2, rpm_arc_y[i] - 2, rpm_arc_x[i] + 2, rpm_arc_y[i] + 2, COLOR_GRID_DIM);
            }
        }
        last_rpm_val = rpm;
    }
}

/* 局部更新底部 CAN 总线诊断报文 */
void Cyber_UpdateTelemetry(uint32_t rx_count, const uint8_t *payload, uint8_t len)
{
    char buf[48];
    sprintf(buf, "CAN1: 500K | ID:0x123 | PACKETS: %06lu", (unsigned long)rx_count);
    LCD_ShowString(14, 446, (const u8 *)buf, COLOR_CYAN_GLOW, COLOR_CARD_BG, 12, 0);

    if (payload != NULL && len >= 8)
    {
        sprintf(buf, "RAW : %02X %02X %02X %02X %02X %02X %02X %02X",
                payload[0], payload[1], payload[2], payload[3],
                payload[4], payload[5], payload[6], payload[7]);
        LCD_ShowString(14, 460, (const u8 *)buf, COLOR_LASER_GREEN, COLOR_CARD_BG, 12, 0);
    }
}

/* 绘制底部 3 颗切角科技触控大按键 */
void Cyber_UpdateButtons(uint8_t siren_on, uint8_t mode)
{
    /* 按钮 1: [ ⚡ SIREN ] (X: 8 ~ 106, Y: 394 ~ 434) */
    u16 btn1_border = siren_on ? COLOR_PURSUIT_RED : COLOR_GRID_DIM;
    u16 btn1_bg     = siren_on ? 0x2000 : COLOR_CARD_BG;
    u16 btn1_text   = siren_on ? COLOR_AMBER_GOLD : COLOR_TEXT_MUTED;
    Cyber_DrawChamferCard(8, 394, 106, 434, btn1_border, btn1_bg);
    LCD_ShowString(18, 406, (const u8 *)"SIREN", btn1_text, btn1_bg, 16, 0);

    /* 按钮 2: [ ⚔ PURSUIT ] (X: 112 ~ 208, Y: 394 ~ 434) */
    uint8_t is_pursuit = (mode == COCKPIT_MODE_PURSUIT);
    u16 btn2_border = is_pursuit ? COLOR_PURSUIT_RED : COLOR_CYAN_GLOW;
    u16 btn2_bg     = is_pursuit ? 0x3000 : 0x0185;
    u16 btn2_text   = is_pursuit ? COLOR_PURSUIT_RED : COLOR_CYAN_GLOW;
    Cyber_DrawChamferCard(112, 394, 208, 434, btn2_border, btn2_bg);
    LCD_ShowString(118, 406, is_pursuit ? (const u8 *)"PURSUIT" : (const u8 *)"PATROL ", btn2_text, btn2_bg, 16, 0);

    /* 按钮 3: [ 🚨 ALERT ] (X: 214 ~ 312, Y: 394 ~ 434) */
    Cyber_DrawChamferCard(214, 394, 312, 434, COLOR_AMBER_GOLD, COLOR_CARD_BG);
    LCD_ShowString(230, 406, (const u8 *)"ALERT", COLOR_AMBER_GOLD, COLOR_CARD_BG, 16, 0);
}

/* 战斗紧急状态全屏高光警示红框爆闪 */
void Cyber_TriggerCombatAlert(void)
{
    /* 屏幕四周边框爆发鲜红光芒 */
    LCD_DrawRectangle(0, 0, LCD_W - 1, LCD_H - 1, COLOR_PURSUIT_RED);
    LCD_DrawRectangle(1, 1, LCD_W - 2, LCD_H - 2, COLOR_PURSUIT_RED);
    LCD_DrawRectangle(2, 2, LCD_W - 3, LCD_H - 3, COLOR_PURSUIT_RED);

    HAL_Delay(80);

    /* 恢复正常底色 */
    LCD_DrawRectangle(0, 0, LCD_W - 1, LCD_H - 1, COLOR_OBSIDIAN);
    LCD_DrawRectangle(1, 1, LCD_W - 2, LCD_H - 2, COLOR_OBSIDIAN);
    LCD_DrawRectangle(2, 2, LCD_W - 3, LCD_H - 3, COLOR_OBSIDIAN);
}
