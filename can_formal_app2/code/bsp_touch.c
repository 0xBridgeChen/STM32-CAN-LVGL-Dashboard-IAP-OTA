#include "bsp_touch.h"
#include "spi.h"
#include "lcd_init.h"
#include <stdlib.h>

extern SPI_HandleTypeDef hspi1;

static uint8_t sim_pressed = 0;
static uint16_t sim_x = 0;
static uint16_t sim_y = 0;

void Touch_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* T_CS: PA4 输出，默认拉高释放总线 */
    HAL_GPIO_WritePin(T_CS_PORT, T_CS_PIN, GPIO_PIN_SET);
    GPIO_InitStruct.Pin = T_CS_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(T_CS_PORT, &GPIO_InitStruct);

    /* T_PEN: PB0 输入，开启上拉 */
    GPIO_InitStruct.Pin = T_PEN_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(T_PEN_PORT, &GPIO_InitStruct);
}

static uint16_t Touch_Read_AD(uint8_t cmd)
{
    uint8_t rx_data[2] = {0};
    uint8_t tx_data[2] = {0};
    uint16_t val = 0;

    T_CS_LOW();
    HAL_SPI_Transmit(&hspi1, &cmd, 1, 10);
    HAL_SPI_TransmitReceive(&hspi1, tx_data, rx_data, 2, 10);
    T_CS_HIGH();

    val = ((uint16_t)rx_data[0] << 8) | rx_data[1];
    val >>= 4; /* XPT2046 只有高 12 位有效 */
    return val;
}

uint8_t Touch_Scan(uint16_t *x, uint16_t *y)
{
    /* 1. 优先检查软件模拟触控 */
    if (sim_pressed)
    {
        *x = sim_x;
        *y = sim_y;
        sim_pressed = 0;
        return 1;
    }

    /* 2. 硬件检测：PEN 脚拉低表示有物理触摸 */
    if (T_PEN_READ() == GPIO_PIN_RESET)
    {
        uint16_t ad_x = 0, ad_y = 0;
        ad_x = Touch_Read_AD(0xD0);
        ad_y = Touch_Read_AD(0x90);

        if (ad_x > 150 && ad_x < 3950 && ad_y > 150 && ad_y < 3950)
        {
            /* 映射到屏幕 320 x 480 分辨率 */
            int32_t cal_x = (int32_t)(ad_x - 200) * 320 / (3850 - 200);
            int32_t cal_y = (int32_t)(ad_y - 250) * 480 / (3850 - 250);

            if (cal_x < 0) cal_x = 0;
            if (cal_x >= 320) cal_x = 319;
            if (cal_y < 0) cal_y = 0;
            if (cal_y >= 480) cal_y = 479;

            *x = (uint16_t)cal_x;
            *y = (uint16_t)cal_y;
            return 1;
        }
    }
    return 0;
}

void Touch_SimulatePress(uint16_t x, uint16_t y)
{
    sim_x = x;
    sim_y = y;
    sim_pressed = 1;
}

