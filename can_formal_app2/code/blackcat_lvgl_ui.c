#include "blackcat_lvgl_ui.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ==============================================================================
 * Production Automotive Indicator Icons & Baked 8-Bit Clean Background
 * ============================================================================== */
LV_IMG_DECLARE(dashboard_bg_img);
LV_IMG_DECLARE(ui_img_safety_belt_png);
LV_IMG_DECLARE(ui_img_turn_light_png);

/* Standard Montserrat Fonts enabled in lv_conf.h */
LV_FONT_DECLARE(lv_font_montserrat_36);
LV_FONT_DECLARE(lv_font_montserrat_16);
LV_FONT_DECLARE(lv_font_montserrat_14);

/* ==============================================================================
 * Color Palette - Volkswagen / Audi Virtual Cockpit Authentic Theme
 * ============================================================================== */
#define COLOR_BG_VOID         0x040608
#define COLOR_TEXT_WHITE      0xFFFFFF
#define COLOR_TEXT_SILVER     0xCBD5E1
#define COLOR_TEXT_SLATE      0x8296B0
#define COLOR_VW_GOLD         0xF5B041
#define COLOR_VW_RED          0xEB3C32
#define COLOR_READY_GREEN     0x00E676
#define COLOR_LAMP_OFF        0x1B2636
#define COLOR_SLOT_DARK       0x141A24

/* Dial Center Coordinates & Needle Length (matching baked photo background) */
#define DIAL_L_CX             118
#define DIAL_L_CY             154
#define DIAL_R_CX             362
#define DIAL_R_CY             154
#define NEEDLE_LEN_TOTAL      68
#define NEEDLE_LEN_BODY       50
#define NEEDLE_LEN_START      16

#define DEG2RAD(d)            ((float)(d) * 0.0174532925f)

/* ==============================================================================
 * UI Widget Handles
 * ============================================================================== */
static lv_obj_t *scr_main;
static lv_obj_t *img_bg;

/* Top Status Bar */
static lv_obj_t *img_turn_l;
static lv_obj_t *img_turn_r;
static lv_obj_t *lbl_clock;
static lv_obj_t *lbl_temp_out;

/* Left Dial (Tachometer) Dynamic Widgets */
static lv_obj_t *line_rpm_body;
static lv_obj_t *line_rpm_tip;
static lv_point_t rpm_body_pts[2];
static lv_point_t rpm_tip_pts[2];
static lv_obj_t *hub_l;
static lv_obj_t *lbl_gear_hero;

/* Right Dial (Speedometer) Dynamic Widgets */
static lv_obj_t *line_spd_body;
static lv_obj_t *line_spd_tip;
static lv_point_t spd_body_pts[2];
static lv_point_t spd_tip_pts[2];
static lv_obj_t *hub_r;
static lv_obj_t *lbl_spd_hero;
static lv_obj_t *lbl_spd_unit;

/* Center HUD Display */
static lv_obj_t *hub_c;
static lv_obj_t *lbl_range_title;
static lv_obj_t *lbl_range_val;
static lv_obj_t *lbl_autohold;
static lv_obj_t *img_seatbelt;
static lv_obj_t *lbl_epc;

/* Side Submeter Dynamic Bars */
static lv_obj_t *obj_coolant_bars[4];
static lv_obj_t *obj_fuel_bars[4];

/* ==============================================================================
 * State Tracking & Physics Engine
 * ============================================================================== */
static uint32_t boot_start_ms   = 0;
static uint8_t  boot_sweep_done = 0;

static float smooth_speed = 0.0f;
static float smooth_rpm   = 0.0f;

static uint16_t prev_disp_speed = 0xFFFFU;
static uint16_t prev_disp_rpm   = 0xFFFFU;
static uint8_t  prev_gear_id    = 0xFFU;
static uint8_t  prev_turn_blink = 0xFFU;

/* Helper: Calculate needle 2-segment line coordinates */
static void update_needle_points(int16_t cx, int16_t cy, float angle_deg,
                                 lv_point_t *body_pts, lv_point_t *tip_pts)
{
    float rad = DEG2RAD(angle_deg);
    float cos_a = cosf(rad);
    float sin_a = sinf(rad);

    int16_t r_start = NEEDLE_LEN_START;
    int16_t r_mid   = NEEDLE_LEN_BODY;
    int16_t r_end   = NEEDLE_LEN_TOTAL;

    body_pts[0].x = cx + (int16_t)(r_start * cos_a);
    body_pts[0].y = cy + (int16_t)(r_start * sin_a);
    body_pts[1].x = cx + (int16_t)(r_mid * cos_a);
    body_pts[1].y = cy + (int16_t)(r_mid * sin_a);

    tip_pts[0].x = body_pts[1].x;
    tip_pts[0].y = body_pts[1].y;
    tip_pts[1].x = cx + (int16_t)(r_end * cos_a);
    tip_pts[1].y = cy + (int16_t)(r_end * sin_a);
}

void BlackCat_LVGL_Init(void)
{
    scr_main = lv_scr_act();
    lv_obj_clear_flag(scr_main, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(scr_main, lv_color_hex(COLOR_BG_VOID), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr_main, LV_OPA_COVER, LV_PART_MAIN);

    /* 1. Photorealistic 480x320 8-Bit Baked Clean Background */
    img_bg = lv_img_create(scr_main);
    lv_img_set_src(img_bg, &dashboard_bg_img);
    lv_obj_set_pos(img_bg, 0, 0);
    lv_obj_clear_flag(img_bg, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    /* 2. Top Status Bar */
    lbl_clock = lv_label_create(scr_main);
    lv_label_set_text(lbl_clock, "12:39");
    lv_obj_set_style_text_font(lbl_clock, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_clock, lv_color_hex(COLOR_TEXT_WHITE), LV_PART_MAIN);
    lv_obj_set_pos(lbl_clock, 222, 14);

    lbl_temp_out = lv_label_create(scr_main);
    lv_label_set_text(lbl_temp_out, "13.5°C");
    lv_obj_set_style_text_font(lbl_temp_out, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_temp_out, lv_color_hex(COLOR_TEXT_SLATE), LV_PART_MAIN);
    lv_obj_set_pos(lbl_temp_out, 266, 14);

    /* Turn Signals (Using ui_img_turn_light_png, placed above dials) */
    img_turn_l = lv_img_create(scr_main);
    lv_img_set_src(img_turn_l, &ui_img_turn_light_png);
    lv_obj_set_pos(img_turn_l, 106, 14);
    lv_obj_set_style_img_recolor_opa(img_turn_l, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_img_recolor(img_turn_l, lv_color_hex(COLOR_LAMP_OFF), LV_PART_MAIN);

    img_turn_r = lv_img_create(scr_main);
    lv_img_set_src(img_turn_r, &ui_img_turn_light_png);
    lv_obj_set_pos(img_turn_r, 350, 14);
    lv_obj_set_style_img_recolor_opa(img_turn_r, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_img_recolor(img_turn_r, lv_color_hex(COLOR_LAMP_OFF), LV_PART_MAIN);

    /* 3. Left Dial: Tachometer Needle Lines (0 RPM = 125 deg, 8000 RPM = 405 deg) */
    line_rpm_body = lv_line_create(scr_main);
    lv_obj_set_style_line_width(line_rpm_body, 3, LV_PART_MAIN);
    lv_obj_set_style_line_color(line_rpm_body, lv_color_hex(COLOR_TEXT_WHITE), LV_PART_MAIN);
    lv_obj_set_style_line_rounded(line_rpm_body, true, LV_PART_MAIN);

    line_rpm_tip = lv_line_create(scr_main);
    lv_obj_set_style_line_width(line_rpm_tip, 2, LV_PART_MAIN);
    lv_obj_set_style_line_color(line_rpm_tip, lv_color_hex(COLOR_VW_RED), LV_PART_MAIN);
    lv_obj_set_style_line_rounded(line_rpm_tip, true, LV_PART_MAIN);

    update_needle_points(DIAL_L_CX, DIAL_L_CY, 125.0f, rpm_body_pts, rpm_tip_pts);
    lv_line_set_points(line_rpm_body, rpm_body_pts, 2);
    lv_line_set_points(line_rpm_tip, rpm_tip_pts, 2);

    /* Left Hub Container: Auto-centering for Gear Character */
    hub_l = lv_obj_create(scr_main);
    lv_obj_set_size(hub_l, 52, 52);
    lv_obj_set_pos(hub_l, DIAL_L_CX - 26, DIAL_L_CY - 26);
    lv_obj_clear_flag(hub_l, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(hub_l, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_opa(hub_l, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_pad_all(hub_l, 0, LV_PART_MAIN);

    lbl_gear_hero = lv_label_create(hub_l);
    lv_obj_set_style_text_font(lbl_gear_hero, &lv_font_montserrat_36, LV_PART_MAIN);
    lv_label_set_text(lbl_gear_hero, "P");
    lv_obj_set_style_text_color(lbl_gear_hero, lv_color_hex(COLOR_TEXT_WHITE), LV_PART_MAIN);
    lv_obj_align(lbl_gear_hero, LV_ALIGN_CENTER, 0, 0);

    /* 4. Right Dial: Speedometer Needle Lines (0 km/h = 125 deg, 260 km/h = 405 deg) */
    line_spd_body = lv_line_create(scr_main);
    lv_obj_set_style_line_width(line_spd_body, 3, LV_PART_MAIN);
    lv_obj_set_style_line_color(line_spd_body, lv_color_hex(COLOR_TEXT_WHITE), LV_PART_MAIN);
    lv_obj_set_style_line_rounded(line_spd_body, true, LV_PART_MAIN);

    line_spd_tip = lv_line_create(scr_main);
    lv_obj_set_style_line_width(line_spd_tip, 2, LV_PART_MAIN);
    lv_obj_set_style_line_color(line_spd_tip, lv_color_hex(COLOR_VW_RED), LV_PART_MAIN);
    lv_obj_set_style_line_rounded(line_spd_tip, true, LV_PART_MAIN);

    update_needle_points(DIAL_R_CX, DIAL_R_CY, 125.0f, spd_body_pts, spd_tip_pts);
    lv_line_set_points(line_spd_body, spd_body_pts, 2);
    lv_line_set_points(line_spd_tip, spd_tip_pts, 2);

    /* Right Hub Container: Auto-centering for Speed Readout & Unit */
    hub_r = lv_obj_create(scr_main);
    lv_obj_set_size(hub_r, 68, 68);
    lv_obj_set_pos(hub_r, DIAL_R_CX - 34, DIAL_R_CY - 34);
    lv_obj_clear_flag(hub_r, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(hub_r, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_opa(hub_r, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_pad_all(hub_r, 0, LV_PART_MAIN);

    lbl_spd_hero = lv_label_create(hub_r);
    lv_obj_set_style_text_font(lbl_spd_hero, &lv_font_montserrat_36, LV_PART_MAIN);
    lv_label_set_text(lbl_spd_hero, "0");
    lv_obj_set_style_text_color(lbl_spd_hero, lv_color_hex(COLOR_TEXT_WHITE), LV_PART_MAIN);
    lv_obj_align(lbl_spd_hero, LV_ALIGN_CENTER, 0, -8);

    lbl_spd_unit = lv_label_create(hub_r);
    lv_obj_set_style_text_font(lbl_spd_unit, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_label_set_text(lbl_spd_unit, "km/h");
    lv_obj_set_style_text_color(lbl_spd_unit, lv_color_hex(COLOR_TEXT_SILVER), LV_PART_MAIN);
    lv_obj_align(lbl_spd_unit, LV_ALIGN_CENTER, 0, 18);

    /* 5. Center Information Area */
    hub_c = lv_obj_create(scr_main);
    lv_obj_set_size(hub_c, 80, 120);
    lv_obj_set_pos(hub_c, 200, 95);
    lv_obj_clear_flag(hub_c, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(hub_c, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_opa(hub_c, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_pad_all(hub_c, 0, LV_PART_MAIN);

    lbl_range_title = lv_label_create(hub_c);
    lv_label_set_text(lbl_range_title, "RANGE");
    lv_obj_set_style_text_font(lbl_range_title, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_range_title, lv_color_hex(COLOR_TEXT_SLATE), LV_PART_MAIN);
    lv_obj_align(lbl_range_title, LV_ALIGN_CENTER, 0, -22);

    lbl_range_val = lv_label_create(hub_c);
    lv_label_set_text(lbl_range_val, "280 km");
    lv_obj_set_style_text_font(lbl_range_val, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_range_val, lv_color_hex(COLOR_TEXT_WHITE), LV_PART_MAIN);
    lv_obj_align(lbl_range_val, LV_ALIGN_CENTER, 0, 6);

    /* Bottom Telltales */
    lbl_autohold = lv_label_create(scr_main);
    lv_label_set_text(lbl_autohold, "(A)");
    lv_obj_set_style_text_font(lbl_autohold, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_autohold, lv_color_hex(COLOR_READY_GREEN), LV_PART_MAIN);
    lv_obj_set_pos(lbl_autohold, 210, 264);

    img_seatbelt = lv_img_create(scr_main);
    lv_img_set_src(img_seatbelt, &ui_img_safety_belt_png);
    lv_obj_set_pos(img_seatbelt, 233, 262);
    lv_obj_set_style_img_recolor_opa(img_seatbelt, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_img_recolor(img_seatbelt, lv_color_hex(COLOR_VW_RED), LV_PART_MAIN);

    lbl_epc = lv_label_create(scr_main);
    lv_label_set_text(lbl_epc, "EPC");
    lv_obj_set_style_text_font(lbl_epc, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_epc, lv_color_hex(COLOR_VW_GOLD), LV_PART_MAIN);
    lv_obj_set_pos(lbl_epc, 255, 264);

    /* 6. Side Submeter Dynamic Active Pills */
    /* Coolant vertical pills: aligned to actual guide slots at x=20 */
    uint16_t c_ys[4] = {196, 184, 172, 160};
    for (int i = 0; i < 4; i++)
    {
        obj_coolant_bars[i] = lv_obj_create(scr_main);
        lv_obj_set_size(obj_coolant_bars[i], 4, 6);
        lv_obj_set_pos(obj_coolant_bars[i], 20, c_ys[i]);
        lv_obj_set_style_radius(obj_coolant_bars[i], 1, LV_PART_MAIN);
        lv_obj_set_style_border_width(obj_coolant_bars[i], 0, LV_PART_MAIN);
        lv_obj_set_style_bg_color(obj_coolant_bars[i], lv_color_hex(COLOR_TEXT_WHITE), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(obj_coolant_bars[i], LV_OPA_COVER, LV_PART_MAIN);
    }

    /* Fuel vertical pills: aligned to actual guide slots at x=466 */
    uint16_t f_ys[4] = {170, 184, 198, 212};
    for (int i = 0; i < 4; i++)
    {
        obj_fuel_bars[i] = lv_obj_create(scr_main);
        lv_obj_set_size(obj_fuel_bars[i], 4, 6);
        lv_obj_set_pos(obj_fuel_bars[i], 466, f_ys[i]);
        lv_obj_set_style_radius(obj_fuel_bars[i], 1, LV_PART_MAIN);
        lv_obj_set_style_border_width(obj_fuel_bars[i], 0, LV_PART_MAIN);
        uint32_t col = (i == 3) ? COLOR_VW_RED : COLOR_TEXT_WHITE;
        lv_obj_set_style_bg_color(obj_fuel_bars[i], lv_color_hex(col), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(obj_fuel_bars[i], LV_OPA_COVER, LV_PART_MAIN);
    }
}


void BlackCat_LVGL_Update(const BlackCat_VehicleState *state, uint32_t now_ms)
{
    float target_speed_f = 0.0f;
    float target_rpm_f   = 800.0f;
    uint8_t gear_id      = 0; /* 0:P, 1:R, 2:N, 3:D, 4:S */

    /* 1. Power-on Needle Sweep Animation (0~2400 ms) */
    if (boot_start_ms == 0) boot_start_ms = now_ms;
    uint32_t elapsed = now_ms - boot_start_ms;

    if (!boot_sweep_done)
    {
        if (elapsed < 1000U)
        {
            float t = (float)elapsed / 1000.0f;
            float p = sinf(t * 1.5707963f);
            target_rpm_f   = p * 8000.0f;
            target_speed_f = p * 260.0f;
            gear_id = 4;
        }
        else if (elapsed < 1300U)
        {
            target_rpm_f   = 7950.0f + sinf((float)now_ms * 0.08f) * 45.0f;
            target_speed_f = 260.0f;
            gear_id = 4;
        }
        else if (elapsed < 2400U)
        {
            float t = (float)(elapsed - 1300U) / 1100.0f;
            float p = 1.0f - t;
            float fall = p * p;
            target_rpm_f   = 800.0f + fall * 7150.0f;
            target_speed_f = fall * 260.0f;
            gear_id = 0;
        }
        else
        {
            boot_sweep_done = 1U;
        }
    }

    /* 2. Operational State (Real-time CAN or Autonomous Driving Simulation) */
    if (boot_sweep_done)
    {
        if (state != NULL && state->can_online)
        {
            target_speed_f = (float)state->speed_kph;
            target_rpm_f   = (float)state->rpm;
            gear_id        = (target_speed_f > 1.5f) ? 3U : 0U;
        }
        else
        {
            /* 14-second dynamic driving cycle */
            uint32_t cycle = (now_ms - 2400U) % 14000U;
            if (cycle < 2000U)
            {
                gear_id = 0; /* P */
                target_speed_f = 0.0f;
                target_rpm_f   = 800.0f + sinf((float)now_ms * 0.01f) * 20.0f;
            }
            else if (cycle < 5000U)
            {
                gear_id = 3; /* D1 */
                float t = (float)(cycle - 2000U) / 3000.0f;
                target_speed_f = t * 58.0f;
                target_rpm_f   = 1200.0f + t * 4500.0f;
            }
            else if (cycle < 8500U)
            {
                gear_id = 3; /* D2 */
                float t = (float)(cycle - 5000U) / 3500.0f;
                target_speed_f = 58.0f + t * 75.0f;
                target_rpm_f   = 2800.0f + t * 4400.0f;
            }
            else if (cycle < 11500U)
            {
                gear_id = 3; /* D3 cruising */
                float t = (float)(cycle - 8500U) / 3000.0f;
                target_speed_f = 133.0f + sinf(t * 3.14f) * 12.0f;
                target_rpm_f   = 3800.0f + sinf(t * 3.14f) * 400.0f;
            }
            else
            {
                float t = (float)(cycle - 11500U) / 2500.0f;
                float p = 1.0f - t;
                target_speed_f = p * 133.0f;
                target_rpm_f   = 800.0f + p * 2800.0f;
                gear_id = (target_speed_f < 2.0f) ? 0U : 3U;
            }
        }
    }

    /* 3. Hardware FPU 50 FPS Smooth Inertia Filtering */
    smooth_speed += (target_speed_f - smooth_speed) * 0.18f;
    smooth_rpm   += (target_rpm_f   - smooth_rpm)   * 0.18f;

    uint16_t disp_speed = (uint16_t)(smooth_speed + 0.5f);
    uint16_t disp_rpm   = (uint16_t)(smooth_rpm + 0.5f);

    /* 4. Update Left Dial (Tachometer Needle & Gear Hero) */
    if (disp_rpm != prev_disp_rpm)
    {
        prev_disp_rpm = disp_rpm;
        /* Angle mapping: 0 RPM = 125 deg (7 o'clock), 8000 RPM = 405 deg (4:30 o'clock), sweep 280 deg */
        float angle_rpm = 125.0f + (smooth_rpm / 8000.0f) * 280.0f;
        update_needle_points(DIAL_L_CX, DIAL_L_CY, angle_rpm, rpm_body_pts, rpm_tip_pts);
        lv_line_set_points(line_rpm_body, rpm_body_pts, 2);
        lv_line_set_points(line_rpm_tip, rpm_tip_pts, 2);
    }

    if (gear_id != prev_gear_id)
    {
        prev_gear_id = gear_id;
        switch (gear_id)
        {
            case 0:  lv_label_set_text(lbl_gear_hero, "P"); break;
            case 1:  lv_label_set_text(lbl_gear_hero, "R"); break;
            case 2:  lv_label_set_text(lbl_gear_hero, "N"); break;
            case 3:  lv_label_set_text(lbl_gear_hero, "D"); break;
            default: lv_label_set_text(lbl_gear_hero, "S"); break;
        }
        lv_obj_align(lbl_gear_hero, LV_ALIGN_CENTER, 0, 0);
    }

    /* 5. Update Right Dial (Speedometer Needle & Digital Speed Hero) */
    if (disp_speed != prev_disp_speed)
    {
        prev_disp_speed = disp_speed;
        /* Authentic VW Virtual Cockpit non-linear scale: 0~100 (125~270 deg), 100~260 (270~405 deg) */
        float angle_spd;
        if (smooth_speed <= 100.0f)
        {
            angle_spd = 125.0f + (smooth_speed / 100.0f) * 145.0f;
        }
        else
        {
            angle_spd = 270.0f + ((smooth_speed - 100.0f) / 160.0f) * 135.0f;
        }
        update_needle_points(DIAL_R_CX, DIAL_R_CY, angle_spd, spd_body_pts, spd_tip_pts);
        lv_line_set_points(line_spd_body, spd_body_pts, 2);
        lv_line_set_points(line_spd_tip, spd_tip_pts, 2);

        char buf[16];
        sprintf(buf, "%u", disp_speed);
        lv_label_set_text(lbl_spd_hero, buf);
        lv_obj_align(lbl_spd_hero, LV_ALIGN_CENTER, 0, -8);
    }

    /* 6. Turn Signals Rhythm (1.5 Hz Authentic Rhythm) */
    uint8_t turn_blink = (uint8_t)((now_ms / 350U) & 1U);
    if (turn_blink != prev_turn_blink)
    {
        prev_turn_blink = turn_blink;
        if (state != NULL && state->can_online && state->turn_signal != 0)
        {
            if (state->turn_signal == 1)
            {
                lv_obj_set_style_img_recolor(img_turn_l, (turn_blink ? lv_color_hex(COLOR_READY_GREEN) : lv_color_hex(COLOR_LAMP_OFF)), LV_PART_MAIN);
                lv_obj_set_style_img_recolor(img_turn_r, lv_color_hex(COLOR_LAMP_OFF), LV_PART_MAIN);
            }
            else if (state->turn_signal == 2)
            {
                lv_obj_set_style_img_recolor(img_turn_l, lv_color_hex(COLOR_LAMP_OFF), LV_PART_MAIN);
                lv_obj_set_style_img_recolor(img_turn_r, (turn_blink ? lv_color_hex(COLOR_READY_GREEN) : lv_color_hex(COLOR_LAMP_OFF)), LV_PART_MAIN);
            }
            else
            {
                lv_obj_set_style_img_recolor(img_turn_l, (turn_blink ? lv_color_hex(COLOR_READY_GREEN) : lv_color_hex(COLOR_LAMP_OFF)), LV_PART_MAIN);
                lv_obj_set_style_img_recolor(img_turn_r, (turn_blink ? lv_color_hex(COLOR_READY_GREEN) : lv_color_hex(COLOR_LAMP_OFF)), LV_PART_MAIN);
            }
        }
        else
        {
            lv_obj_set_style_img_recolor(img_turn_l, lv_color_hex(COLOR_LAMP_OFF), LV_PART_MAIN);
            lv_obj_set_style_img_recolor(img_turn_r, lv_color_hex(COLOR_LAMP_OFF), LV_PART_MAIN);
        }
    }

    /* 7. Seatbelt Warning Status */
    if (state != NULL && state->can_online)
    {
        lv_obj_set_style_img_recolor(img_seatbelt, state->seatbelt ? lv_color_hex(COLOR_VW_RED) : lv_color_hex(COLOR_LAMP_OFF), LV_PART_MAIN);
    }
}

void BlackCat_LVGL_TriggerSalute(void) {}
void BlackCat_LVGL_ToggleMode(void) {}
void BlackCat_LVGL_ToggleSiren(void) {}
