#ifndef can_handle_h
#define can_handle_h

#include "main.h" 

#define CAN_ID_CALL_OTA 0x0B3
#define CAN_ID_UI    0x0B4
#define CAN_MSG_HEAD 0xAF

typedef enum
{
	E_UI_TYPE_RPM = 0X01,
	E_UI_TYPE_TEMP,
	E_UI_TYPE_BELT,
	E_UI_TYPE_LIGHT,
	E_UI_TYPE_TURN_LIGHT,
	E_UI_TYPE_DATE,
	E_UI_TYPE_TIME,
} E_UI_TYPE; 

// 业务数据变量
extern uint8_t Rpm;
extern uint8_t speed;
extern uint8_t temp;
extern uint8_t belt;
extern uint8_t light;
extern uint8_t turn_light;

// 刷新标志位 (立红旗机制)
extern uint8_t flag_power;
extern uint8_t flag_temp;
extern uint8_t flag_belt;
extern uint8_t flag_light;
extern uint8_t flag_turn_light;
extern uint8_t flag_ota;

void can_recv_handle(void);

#endif
