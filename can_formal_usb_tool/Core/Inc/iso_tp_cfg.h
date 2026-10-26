#include "main.h"

#ifndef __ISO_TP_CFG_H__
#define __ISO_TP_CFG_H__

uint8_t iso_tp_init(void);
void iso_tp_server(void);

void iso_tp_send_ota_data(uint32_t pack_offset,  uint8_t *data, uint32_t pack_size);

void iso_tp_send_test(void);
void iso_tp_test(void);

#endif /* __ISO_TP_CFG_H__ */
