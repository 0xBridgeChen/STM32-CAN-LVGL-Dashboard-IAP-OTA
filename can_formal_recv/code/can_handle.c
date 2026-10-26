/**
 * @file    can_handle.c
 * @brief   接收端仪表 (APP1) CAN 总线矩阵解析与“立红旗”事件分发模块
 *
 * 文件结构：
 *   1. 全局业务状态变量与 UI 刷新红旗标志
 *   2. can_recv_handle() — 零等待抽取 CAN1 FIFO0 报文，解码 0x0B3(OTA) 与 0x0B4(仪表业务)
 */

#include "can_handle.h"
#include "wrap.h"
#include "rtc.h"

// ================================================================
// 1. 车辆业务数据变量（由 CAN 报文更新，供 main.c 的 UI 状态机消费）
// ================================================================
uint8_t Rpm = 0;         // 发动机转速原始值 (单位：×100 RPM，范围 0~80 对应 0~8000 RPM)
uint8_t speed = 0;       // 车辆时速 (单位：km/h，范围 0~180)
uint8_t temp = 0;        // 水温报警状态 (1=红灯高温报警，0=绿灯正常)
uint8_t belt = 0;        // 安全带状态   (1=红灯未系报警，0=绿灯已系)
uint8_t light = 0;       // 车灯状态     (1=近光黄，2=远光红，0=正常绿)
uint8_t turn_light = 0;  // 转向灯状态   (1=开启闪烁绿，0=关闭暗灰)

// ================================================================
// 2. UI 按需刷新标志位（★“立红旗”机制：收到哪类报文才重绘哪个控件，避免全屏无效重绘）
// ================================================================
uint8_t flag_power = 0;      // 车速/转速指针更新红旗
uint8_t flag_temp = 0;       // 水温指示灯更新红旗
uint8_t flag_belt = 0;       // 安全带指示灯更新红旗
uint8_t flag_light = 0;      // 车灯指示灯更新红旗
uint8_t flag_turn_light = 0; // 转向灯指示灯更新红旗
uint8_t flag_ota = 0;        // ★ 远程 OTA 升级唤醒红旗 (收到 0x0B3 置 1)

// ================================================================
// 3. CAN 报文非阻塞解包函数 (在 main.c 主循环中每 5ms 调用一次)
//
//    通信矩阵定义：
//      ① CAN ID = 0x0B3 (CAN_ID_CALL_OTA，8字节)：
//         - 报文内容：BE AD BE EF 01 AA AA AA
//         - 触发动作：置位 flag_ota = 1，通知主循环写入 Sector 3 升级标志并软复位回 Bootloader
//
//      ② CAN ID = 0x0B4 (CAN_ID_UI，8字节，帧头 Byte0 = 0xAF)：
//         - Byte1 = 0x01 (动力)：Byte2 = Rpm(×100), Byte3 = speed(km/h)
//         - Byte1 = 0x02 (水温)：Byte2 = 0/1
//         - Byte1 = 0x03 (安全带)：Byte2 = 0/1
//         - Byte1 = 0x04 (车灯)：Byte2 = 0/1/2
//         - Byte1 = 0x05 (转向灯)：Byte2 = 0/1
//         - Byte1 = 0x06 (日期校准)：Byte3 = 年(如26代表2026), Byte4 = 月, Byte5 = 日
//         - Byte1 = 0x07 (时间校准)：Byte2 = 时, Byte3 = 分, Byte4 = 秒
// ================================================================
void can_recv_handle(void)
{
	uint32_t r_id = 0;
	uint8_t get = 0;
	uint32_t get_len = 0;
	uint8_t get_data[8] = {0};

	// ------- 1. 从硬件 FIFO0 抽取一帧 8 字节标准帧 -------
	get = can_get_message(&r_id, &get_len, get_data);
	if((get == 0) && (get_len == 8))
	{
		// ---- 分支 A：收到网关发来的 0x0B3 远程 OTA 升级唤醒指令 ----
		if(r_id == CAN_ID_CALL_OTA)
		{
			if((get_data[0] == 0xBE) &&
			   (get_data[1] == 0xAD) &&
			   (get_data[2] == 0xBE) &&
			   (get_data[3] == 0xEF) &&
			   (get_data[4] == 0x01))
			{
				flag_ota = 1; // ★ 立下升级红旗，交由主循环写 Flash 并复位
			}
		}
		// ---- 分支 B：收到方向盘/发送板发来的 0x0B4 仪表控制报文 (帧头 0xAF) ----
		else if(get_data[0]==CAN_MSG_HEAD)
		{
			if(r_id==CAN_ID_UI)
			{
				if(get_data[1]==E_UI_TYPE_RPM)
				{
					Rpm = get_data[2];
					speed = get_data[3];
					flag_power = 1; // 动力指针立旗
				}
				else if(get_data[1]==E_UI_TYPE_TEMP)
				{
					temp = get_data[2];
					flag_temp = 1;  // 水温灯立旗
				}
				else if(get_data[1]==E_UI_TYPE_BELT)
				{
					belt = get_data[2];
					flag_belt = 1;  // 安全带立旗
				}
				else if(get_data[1]==E_UI_TYPE_LIGHT)
				{
					light = get_data[2];
					flag_light = 1; // 车灯立旗
				}
				else if(get_data[1]==E_UI_TYPE_TURN_LIGHT)
				{
					turn_light = get_data[2];
					flag_turn_light = 1; // 转向灯立旗
				}
				else if(get_data[1]==E_UI_TYPE_DATE) // 日期校准报文
				{
					RTC_DateTypeDef sDate = {0};
					sDate.Year = get_data[3];    // 年低位 (例如 26 代表 2026 年)
					sDate.Month = get_data[4];   // 月份 (1~12)
					sDate.Date = get_data[5];    // 日期 (1~31)
					sDate.WeekDay = RTC_WEEKDAY_MONDAY; // 星期 (必须 1~7，默认填周一)
					HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
				}
				else if(get_data[1]==E_UI_TYPE_TIME) // 时间校准报文
				{
					RTC_TimeTypeDef sTime = {0};
					sTime.Hours = get_data[2];   // 时 (0~23)
					sTime.Minutes = get_data[3]; // 分 (0~59)
					sTime.Seconds = get_data[4]; // 秒 (0~59)
					HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
				}
			}
		}
	}
}
