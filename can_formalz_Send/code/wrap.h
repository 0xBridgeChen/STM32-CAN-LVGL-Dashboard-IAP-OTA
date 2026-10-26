#ifndef _wrap_h
#define _wrap_h
void can_filter_init(void);
uint32_t can_send_message(uint32_t tx_stdid,uint32_t dlc_len,uint8_t* data);
uint32_t can_get_message(uint32_t* rx_id,uint32_t* rx_len,uint8_t*rx_data);
#endif
