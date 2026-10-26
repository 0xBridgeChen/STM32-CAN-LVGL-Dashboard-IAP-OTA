#ifndef __BSP_TOUCH_H
#define __BSP_TOUCH_H

#include "main.h"

/* 触控芯片 XPT2046 默认引脚分配（兼容中景园标准接线） */
#define T_CS_PIN         GPIO_PIN_4
#define T_CS_PORT        GPIOA
#define T_PEN_PIN        GPIO_PIN_0
#define T_PEN_PORT       GPIOB

#define T_CS_LOW()       HAL_GPIO_WritePin(T_CS_PORT, T_CS_PIN, GPIO_PIN_RESET)
#define T_CS_HIGH()      HAL_GPIO_WritePin(T_CS_PORT, T_CS_PIN, GPIO_PIN_SET)
#define T_PEN_READ()     HAL_GPIO_ReadPin(T_PEN_PORT, T_PEN_PIN)

/* 触控初始化与扫描接口 */
void Touch_Init(void);
uint8_t Touch_Scan(uint16_t *x, uint16_t *y);
void Touch_SimulatePress(uint16_t x, uint16_t y);

#endif /* __BSP_TOUCH_H */

