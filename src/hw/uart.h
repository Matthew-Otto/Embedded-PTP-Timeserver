#ifndef UART_H
#define UART_H

#include <stdint.h>
#include "mcu.h"

void USART2_IRQHandler(void);
void init_uart(uint8_t uart_idx, int32_t baudrate);
void uart_in_string(char *buff, uint32_t buff_size);
int uart_in_string_nonblocking(char *buff, uint32_t buff_size);

#endif // UART_H