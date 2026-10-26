#include "bsp_lv_port_disp.h"
#include "main.h"
#include "spi.h"

extern SPI_HandleTypeDef hspi1;

/* ST7796 SPI & Control Pin Macros */
#define LCD_CS_LOW()   HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET)
#define LCD_CS_HIGH()  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET)
#define LCD_DC_CMD()   HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_RESET)
#define LCD_DC_DATA()  HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_SET)

static void LCD_WriteReg(uint8_t reg)
{
    LCD_DC_CMD();
    LCD_CS_LOW();
    HAL_SPI_Transmit(&hspi1, &reg, 1, 10);
    LCD_CS_HIGH();
}

static void LCD_SetAddress(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    uint8_t x_buf[4];
    uint8_t y_buf[4];

    x_buf[0] = (uint8_t)(x1 >> 8);
    x_buf[1] = (uint8_t)x1;
    x_buf[2] = (uint8_t)(x2 >> 8);
    x_buf[3] = (uint8_t)x2;

    y_buf[0] = (uint8_t)(y1 >> 8);
    y_buf[1] = (uint8_t)y1;
    y_buf[2] = (uint8_t)(y2 >> 8);
    y_buf[3] = (uint8_t)y2;

    LCD_WriteReg(0x2A);
    LCD_DC_DATA();
    LCD_CS_LOW();
    HAL_SPI_Transmit(&hspi1, x_buf, 4, 10);
    LCD_CS_HIGH();

    LCD_WriteReg(0x2B);
    LCD_DC_DATA();
    LCD_CS_LOW();
    HAL_SPI_Transmit(&hspi1, y_buf, 4, 10);
    LCD_CS_HIGH();

    LCD_WriteReg(0x2C);
}

/* Double buffer: 480 * 20 pixels each (480 * 20 * 2 = 19,200 bytes, total 38.4KB) */
#define DISP_BUF_SIZE (LV_DISP_HOR_RES * 20)
static lv_color_t buf_1[DISP_BUF_SIZE];
static lv_color_t buf_2[DISP_BUF_SIZE];
static lv_disp_draw_buf_t draw_buf_dsc;
static lv_disp_drv_t disp_drv;

static void disp_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t width = (uint32_t)(area->x2 - area->x1 + 1);
    uint32_t height = (uint32_t)(area->y2 - area->y1 + 1);
    uint32_t total_bytes = width * height * 2U;

    LCD_SetAddress((uint16_t)area->x1, (uint16_t)area->y1,
                   (uint16_t)area->x2, (uint16_t)area->y2);

    LCD_DC_DATA();
    LCD_CS_LOW();

    /* Fast hardware SPI register transfer (wire-speed 21MHz) */
    SPI_TypeDef *SPIx = SPI1;
    const uint8_t *p = (const uint8_t *)color_p;
    while (total_bytes > 0U)
    {
        while (!(SPIx->SR & SPI_SR_TXE));
        *(volatile uint8_t *)&SPIx->DR = *p++;
        total_bytes--;
    }
    while (SPIx->SR & SPI_SR_BSY);

    LCD_CS_HIGH();

    lv_disp_flush_ready(drv);
}

void lv_port_disp_init(void)
{
    lv_disp_draw_buf_init(&draw_buf_dsc, buf_1, buf_2, DISP_BUF_SIZE);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LV_DISP_HOR_RES;
    disp_drv.ver_res = LV_DISP_VER_RES;
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &draw_buf_dsc;

    lv_disp_drv_register(&disp_drv);
}

