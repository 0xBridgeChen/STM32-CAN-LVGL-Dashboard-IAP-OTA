#ifndef __BLACKCAT_LVGL_UI_H
#define __BLACKCAT_LVGL_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl/lvgl.h"

#define BLACKCAT_MODE_PATROL    0U
#define BLACKCAT_MODE_PURSUIT   1U

typedef struct {
    uint16_t speed_kph;
    uint16_t rpm;
    uint32_t can_id;
    uint32_t rx_count;
    uint8_t  dlc;
    uint8_t  data[8];
    uint8_t  can_online;
    uint8_t  siren_on;
    uint8_t  mode;
    uint8_t  headlight;
    uint8_t  seatbelt;
    uint8_t  turn_signal; // 0=off, 1=left, 2=right, 3=hazard
    int16_t  water_temp;  // 80~120 C
} BlackCat_VehicleState;

void BlackCat_LVGL_Init(void);
void BlackCat_LVGL_Update(const BlackCat_VehicleState *state, uint32_t now_ms);
void BlackCat_LVGL_TriggerSalute(void);
void BlackCat_LVGL_ToggleMode(void);
void BlackCat_LVGL_ToggleSiren(void);

#ifdef __cplusplus
}
#endif

#endif /* __BLACKCAT_LVGL_UI_H */

