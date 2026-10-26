/*********************************************************************************
 * @file    ui_Screen1.c
 * @brief   汽车方向盘双表盘液晶主界面定义 (SquareLine Studio + 电赛级深度调优)
 * 
 * ================================================================
 * 【双表盘几何动力学与角度映射模型】:
 * 
 * 1. 屏幕分辨率与空间布局 (480 × 320 物理横屏):
 *    - 根屏幕容器     : ui_Screen1 (纯黑背景 #000000，消除边缘漏光)
 *    - 左表盘 (车速表) : ui_ImageSpeedBg (233×233 像素, 偏左 X=-118, Y=18)
 *    - 右表盘 (转速表) : ui_ImageRpmBg   (233×233 像素, 偏右 X=118, Y=18)
 *    - 指针规格       : ui_ImageSpeedNeedle / ui_ImageRpmNeedle (22×86 像素发光细针)
 * 
 * 2. 旋转轴心 (Pivot) 几何定位:
 *    - lv_img_set_pivot(needle, 11, 72)
 *    - ★ 关键参数解析：
 *      * 指针宽度 22 像素 -> 水平中心 X = 22 / 2 = 11；
 *      * 指针长度 86 像素 -> 针柄圆心轴孔位于 Y = 72 (距针尖 72px，距针底 14px)；
 *      * ⚠️ 致命避坑：若漏配或写成图片正中心 (11, 43)，指针旋转时将绕腰部胡乱翻滚，
 *        彻底脱离表盘刻度中心！
 * 
 * 3. 角度运动学与线性映射方程 (LVGL 角度基准单位为 0.1°):
 *    - 初始零刻度 (7点钟方向) : 225.0° (寄存器值 = 2250)
 *    - 满量程终止刻度 (5点钟方向) : 135.0° (顺时针旋转总行程 = 270.0° -> 2700 个单位)
 *    - 车速转换方程 (0 ~ 180 km/h):
 *      Angle = 2250 + (Speed * 2700 / 180) = 2250 + (Speed * 15)  (当超过 3600 时 LVGL 自动模 3600)
 *    - 转速转换方程 (0 ~ 8000 RPM):
 *      Angle = 2250 + (RPM * 2700 / 8000)
 * 
 * 4. 底部状态指示灯阵列 (Telltales):
 *    - 左转向灯 ui_IconTurnLeft  : X=-170, Y=130 (绿光箭头)
 *    - 安全带报警 ui_IconSeatbelt : X=-90,  Y=130 (红色警报)
 *    - 水温报警   ui_IconTemp     : X=90,   Y=130 (冷/热状态)
 *    - 近光灯指示 ui_IconLowBeam  : X=170,  Y=130 (蓝绿照明)
 * ================================================================
 *********************************************************************************/
#include "../ui.h"

void ui_Screen1_screen_init(void)
{
    // ================================================================
    // 1. 根画布创建与背景色设定 (深邃纯黑 #000000 降噪抗眩晕)
    // ================================================================
    ui_Screen1 = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_Screen1, LV_OBJ_FLAG_SCROLLABLE);      /// 禁止画布滑动
    lv_obj_set_style_bg_color(ui_Screen1, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Screen1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    // ================================================================
    // 2. 右侧转速表盘背景图 (233x233, TRUE_COLOR 省去 Alpha 节约 Flash)
    // ================================================================
    ui_ImageRpmBg = lv_img_create(ui_Screen1);
    lv_img_set_src(ui_ImageRpmBg, &ui_img_110083888);
    lv_obj_set_width(ui_ImageRpmBg, LV_SIZE_CONTENT);   /// 233
    lv_obj_set_height(ui_ImageRpmBg, LV_SIZE_CONTENT);    /// 233
    lv_obj_set_x(ui_ImageRpmBg, 118);
    lv_obj_set_y(ui_ImageRpmBg, 18);
    lv_obj_set_align(ui_ImageRpmBg, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_ImageRpmBg, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_clear_flag(ui_ImageRpmBg, LV_OBJ_FLAG_SCROLLABLE);

    // ================================================================
    // 3. 左侧车速表盘背景图 (233x233, 偏左对称布局)
    // ================================================================
    ui_ImageSpeedBg = lv_img_create(ui_Screen1);
    lv_img_set_src(ui_ImageSpeedBg, &ui_img_942215904);
    lv_obj_set_width(ui_ImageSpeedBg, LV_SIZE_CONTENT);   /// 233
    lv_obj_set_height(ui_ImageSpeedBg, LV_SIZE_CONTENT);    /// 233
    lv_obj_set_x(ui_ImageSpeedBg, -118);
    lv_obj_set_y(ui_ImageSpeedBg, 18);
    lv_obj_set_align(ui_ImageSpeedBg, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_ImageSpeedBg, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_clear_flag(ui_ImageSpeedBg, LV_OBJ_FLAG_SCROLLABLE);

    // ================================================================
    // 4. 车速表发光动态指针 (Pivot 锚点精准锁定 (11, 72)，初始角 2250 = 225.0°)
    // ================================================================
    ui_ImageSpeedNeedle = lv_img_create(ui_Screen1);
    lv_img_set_src(ui_ImageSpeedNeedle, &ui_img_1601502596);
    lv_obj_set_width(ui_ImageSpeedNeedle, LV_SIZE_CONTENT);   /// 22
    lv_obj_set_height(ui_ImageSpeedNeedle, LV_SIZE_CONTENT);    /// 86
    lv_obj_set_x(ui_ImageSpeedNeedle, -118);
    lv_obj_set_y(ui_ImageSpeedNeedle, -11);
    lv_obj_set_align(ui_ImageSpeedNeedle, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_ImageSpeedNeedle, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_clear_flag(ui_ImageSpeedNeedle, LV_OBJ_FLAG_SCROLLABLE);
    lv_img_set_pivot(ui_ImageSpeedNeedle, 11, 72);  // ★ 核心物理轴心坐标
    lv_img_set_angle(ui_ImageSpeedNeedle, 2250);   // 7点钟初始零位 (225°)

    // ================================================================
    // 5. 转速表发光动态指针 (Pivot 锚点精准锁定 (11, 72)，初始角 2250 = 225.0°)
    // ================================================================
    ui_ImageRpmNeedle = lv_img_create(ui_Screen1);
    lv_img_set_src(ui_ImageRpmNeedle, &ui_img_1601502596);
    lv_obj_set_width(ui_ImageRpmNeedle, LV_SIZE_CONTENT);   /// 22
    lv_obj_set_height(ui_ImageRpmNeedle, LV_SIZE_CONTENT);    /// 86
    lv_obj_set_x(ui_ImageRpmNeedle, 118);
    lv_obj_set_y(ui_ImageRpmNeedle, -11);
    lv_obj_set_align(ui_ImageRpmNeedle, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_ImageRpmNeedle, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_clear_flag(ui_ImageRpmNeedle, LV_OBJ_FLAG_SCROLLABLE);
    lv_img_set_pivot(ui_ImageRpmNeedle, 11, 72);   // ★ 核心物理轴心坐标
    lv_img_set_angle(ui_ImageRpmNeedle, 2250);     // 7点钟初始零位 (225°)

    // ================================================================
    // 6. 状态警示指示灯阵列 (水平对称分布于仪表下方 Y=130)
    // ================================================================
    // [近光灯]
    ui_IconLowBeam = lv_img_create(ui_Screen1);
    lv_img_set_src(ui_IconLowBeam, &ui_img_light_png);
    lv_obj_set_width(ui_IconLowBeam, LV_SIZE_CONTENT);   /// 32
    lv_obj_set_height(ui_IconLowBeam, LV_SIZE_CONTENT);    /// 32
    lv_obj_set_x(ui_IconLowBeam, 170);
    lv_obj_set_y(ui_IconLowBeam, 130);
    lv_obj_set_align(ui_IconLowBeam, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_IconLowBeam, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_clear_flag(ui_IconLowBeam, LV_OBJ_FLAG_SCROLLABLE);

    // [安全带]
	
    ui_IconSeatbelt = lv_img_create(ui_Screen1);
lv_obj_set_style_img_recolor(ui_IconSeatbelt, lv_color_hex(0x00F374), 0);
lv_obj_set_style_img_recolor_opa(ui_IconSeatbelt, 255, 0);
    lv_img_set_src(ui_IconSeatbelt, &ui_img_safety_belt_png);
    lv_obj_set_width(ui_IconSeatbelt, LV_SIZE_CONTENT);   /// 32
    lv_obj_set_height(ui_IconSeatbelt, LV_SIZE_CONTENT);    /// 32
    lv_obj_set_x(ui_IconSeatbelt, -90);
    lv_obj_set_y(ui_IconSeatbelt, 130);
    lv_obj_set_align(ui_IconSeatbelt, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_IconSeatbelt, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_clear_flag(ui_IconSeatbelt, LV_OBJ_FLAG_SCROLLABLE);

    // [水温]
    ui_IconTemp = lv_img_create(ui_Screen1);
	lv_obj_set_style_img_recolor(ui_IconTemp, lv_color_hex(0x00F374), 0);
    lv_obj_set_style_img_recolor_opa(ui_IconTemp, 255, 0);
    lv_img_set_src(ui_IconTemp, &ui_img_temp_gray_png);
    lv_obj_set_width(ui_IconTemp, LV_SIZE_CONTENT);   /// 32
    lv_obj_set_height(ui_IconTemp, LV_SIZE_CONTENT);    /// 32
    lv_obj_set_x(ui_IconTemp, 90);
    lv_obj_set_y(ui_IconTemp, 130);
    lv_obj_set_align(ui_IconTemp, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_IconTemp, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_clear_flag(ui_IconTemp, LV_OBJ_FLAG_SCROLLABLE);

    // [左转向灯]
	
    ui_IconTurnLeft = lv_img_create(ui_Screen1);
    lv_img_set_src(ui_IconTurnLeft, &ui_img_turn_light_png);
    lv_obj_set_width(ui_IconTurnLeft, LV_SIZE_CONTENT);   /// 32
    lv_obj_set_height(ui_IconTurnLeft, LV_SIZE_CONTENT);    /// 32
    lv_obj_set_x(ui_IconTurnLeft, -170);
    lv_obj_set_y(ui_IconTurnLeft, 130);
    lv_obj_set_align(ui_IconTurnLeft, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_IconTurnLeft, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_clear_flag(ui_IconTurnLeft, LV_OBJ_FLAG_SCROLLABLE);

    // ================================================================
    // 7. 顶部日期与时间显示控件 (居中顶置)
    // ================================================================
    ui_data = lv_label_create(ui_Screen1);
    lv_obj_set_width(ui_data, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_data, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_data, 0);
    lv_obj_set_y(ui_data, -142);
    lv_obj_set_align(ui_data, LV_ALIGN_CENTER);
    lv_label_set_text(ui_data, "2026-10-16");
    lv_obj_set_style_text_color(ui_data, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_data, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_data, &lv_font_montserrat_26, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_time = lv_label_create(ui_Screen1);
    lv_obj_set_width(ui_time, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_time, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_time, 0);
    lv_obj_set_y(ui_time, -100);
    lv_obj_set_align(ui_time, LV_ALIGN_CENTER);
    lv_label_set_text(ui_time, "22:15");
    lv_obj_set_style_text_color(ui_time, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_time, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_time, &lv_font_montserrat_40, LV_PART_MAIN | LV_STATE_DEFAULT);
}
