#include "main.h"
#include "canif.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "isotp_porting.h"
#include "common.h"

/* user implemented, print debug message */
void isotp_user_debug(const char *message, ...)
{   
    printf("[isotp]");
    va_list args;
    va_start(args, message);
    
    // 打印调试信息
    vprintf(message, args);
    
    va_end(args);
}

/* user implemented, send can message. should return ISOTP_RET_OK when success.
 */
int isotp_user_send_can(const uint32_t arbitration_id,
                        const uint8_t *data, const uint8_t size)
{
    uint8_t ret = CAN1_Send_Msg(arbitration_id, (uint8_t *)data, size);


    // dump_hex(data, size, 16);
    return ret;
}

/* user implemented, get millisecond */
uint32_t isotp_user_get_ms(void)
{
    return HAL_GetTick();
}

