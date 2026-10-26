#include "bsp_lv_port_indev.h"
#include "bsp_touch.h"

static lv_indev_drv_t indev_drv;

static void touchpad_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    static uint16_t last_x = 0;
    static uint16_t last_y = 0;
    uint16_t x = 0;
    uint16_t y = 0;

    if (Touch_Scan(&x, &y) != 0U)
    {
        data->state = LV_INDEV_STATE_PR;
        last_x = x;
        last_y = y;
    }
    else
    {
        data->state = LV_INDEV_STATE_REL;
    }

    data->point.x = (lv_coord_t)last_x;
    data->point.y = (lv_coord_t)last_y;
}

void lv_port_indev_init(void)
{
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read;
    lv_indev_drv_register(&indev_drv);
}

