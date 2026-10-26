#ifndef BSP_LV_PORT_DISP_H
#define BSP_LV_PORT_DISP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl/lvgl.h"

#define LV_DISP_HOR_RES 480
#define LV_DISP_VER_RES 320

void lv_port_disp_init(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_LV_PORT_DISP_H */

