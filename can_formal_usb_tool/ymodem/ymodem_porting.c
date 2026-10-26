#include "ymodem_porting.h"
#include "usart.h"



/**
  * @brief  Print a string on the HyperTerminal
  * @param  p_string: The string to be printed
  * @retval None
  */
void Serial_PutString(uint8_t *p_string)
{
  uint16_t length = 0;

  while (p_string[length] != '\0')
  {
    length++;
  }
  HAL_UART_Transmit(&huart2, p_string, length, TX_TIMEOUT);
}

/**
  * @brief  Transmit a byte to the HyperTerminal
  * @param  param The byte to be sent
  * @retval HAL_StatusTypeDef HAL_OK if OK
  */
HAL_StatusTypeDef Serial_PutByte( uint8_t param )
{
  return HAL_UART_Transmit(&huart2, &param, 1, TX_TIMEOUT);
}

HAL_StatusTypeDef Serial_Recv_data(uint8_t *pData, uint16_t Size, uint32_t Timeout)
{
    return HAL_UART_Receive(&huart2, pData, Size, Timeout);
}
