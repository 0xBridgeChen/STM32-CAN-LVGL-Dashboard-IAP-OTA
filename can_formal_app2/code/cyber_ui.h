#ifndef __CYBER_UI_H
#define __CYBER_UI_H

#include "main.h"

/* ==============================================================================
 *  “黑猫警长·战术拦截机（CYBER INTERCEPTOR）” 超跑级数字座舱 UI 引擎
 *  分辨率: 320 x 480 (RGB565) | 目标帧率: 40 ~ 50 FPS
 * ============================================================================== */

/* 专属奢华暗夜座舱配色表 (RGB565) */
#define COLOR_OBSIDIAN       0x0842  /* 深邃曜石黑底色 (#080C14) */
#define COLOR_CARD_BG        0x10C4  /* 哑光钛灰卡片底色 (#101824) */
#define COLOR_CARD_BORDER    0x2187  /* 科技切角微边框 (#203040) */
#define COLOR_CYAN_GLOW      0x07FF  /* 极光冰蓝高光 (#00FFFF) */
#define COLOR_ICE_BLUE       0x34BF  /* 巡航冷蓝 (#3090FF) */
#define COLOR_AMBER_GOLD     0xFDE0  /* 钛金琥珀黄 (#FFAE00) */
#define COLOR_PURSUIT_RED    0xF800  /* 极速追捕暴怒红 (#FF0000) */
#define COLOR_LASER_GREEN    0x07E0  /* 战术雷达激光绿 (#00FF00) */
#define COLOR_GRID_DIM       0x18C5  /* 暗部刻度细线 (#181828) */
#define COLOR_TEXT_MUTED     0x8410  /* 弱化辅助字色 (#808080) */
#define COLOR_TEXT_WHITE     0xFFFF  /* 高清纯白字色 (#FFFFFF) */

/* 驾驶模式定义 */
#define COCKPIT_MODE_PATROL   0U  /* 沉静极光冰蓝·日常巡逻模式 */
#define COCKPIT_MODE_PURSUIT  1U  /* 炽热暴怒烈红·极速追捕模式 */

/* 基础 API 声明 */
void Cyber_InitCockpit(void);
void Cyber_DrawStaticFrame(uint8_t mode);

/* 高帧率无闪烁增量动效渲染函数 (40 FPS) */
void Cyber_UpdateStrobeLights(uint32_t step, uint8_t enabled);
void Cyber_UpdateRadar(uint8_t angle_step, uint8_t target_locked);
void Cyber_UpdateSpeedAndRPM(uint16_t speed, uint16_t rpm, uint8_t mode);
void Cyber_UpdateTelemetry(uint32_t rx_count, const uint8_t *payload, uint8_t len);
void Cyber_UpdateButtons(uint8_t siren_on, uint8_t mode);
void Cyber_TriggerCombatAlert(void);

#endif /* __CYBER_UI_H */

