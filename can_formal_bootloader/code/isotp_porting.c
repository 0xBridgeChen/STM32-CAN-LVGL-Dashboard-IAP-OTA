#include "main.h"
#include "canif.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "isotp_porting.h"

void isotp_user_debug(const char *message, ...)
{
    printf("[isotp]");
    va_list args;
    va_start(args, message);
    vprintf(message, args);
    va_end(args);
}

int isotp_user_send_can(const uint32_t arbitration_id,
                        const uint8_t *data, const uint8_t size)
{
    uint8_t ret = CAN1_Send_Msg(arbitration_id, (uint8_t *)data, size);
    return ret;
}

uint32_t isotp_user_get_ms(void)
{
    return HAL_GetTick();
}
