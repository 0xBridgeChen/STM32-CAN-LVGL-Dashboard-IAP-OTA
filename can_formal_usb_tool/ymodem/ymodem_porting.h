#include "main.h"
#include <stdio.h>
#include <string.h>

#ifndef __YMODEM_PORTING_H__
#define __YMODEM_PORTING_H__

#ifdef __cplusplus
extern "C"
{
#endif

/* Constants used by Serial Command Line Mode */
#define TX_TIMEOUT          ((uint32_t)100)
#define RX_TIMEOUT          HAL_MAX_DELAY

void Serial_PutString(uint8_t * p_string);
HAL_StatusTypeDef Serial_PutByte(uint8_t param);
HAL_StatusTypeDef Serial_Recv_data(uint8_t *pData, uint16_t Size, uint32_t Timeout);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __YMODEM_PORTING_H__ */
