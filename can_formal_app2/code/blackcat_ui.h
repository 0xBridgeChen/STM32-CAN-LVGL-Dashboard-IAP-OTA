#ifndef __BLACKCAT_UI_H
#define __BLACKCAT_UI_H

#include "main.h"

/* 模式定义 */
#define MODE_PATROL     0   /* 巡逻模式（赛博青蓝） */
#define MODE_PURSUIT    1   /* 追击模式（警用战斗红） */

/* 界面与动效接口 */
void BlackCat_DrawStaticLayout(void);
void BlackCat_DrawAvatar(uint16_t x, uint16_t y, uint8_t blink_state);
void BlackCat_UpdateSiren(uint8_t tick_step, uint8_t siren_on);
void BlackCat_UpdateRadar(uint8_t step, uint8_t target_detected);
void BlackCat_UpdateButtons(uint8_t siren_on, uint8_t pursuit_mode);
void BlackCat_TriggerAlertEffect(void);

#endif /* __BLACKCAT_UI_H */

