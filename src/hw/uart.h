#ifndef UART_H
#define UART_H

#include <stdint.h>
#include "mcu.h"
#include "fifo.h"

typedef struct {
    FIFO8_t *tx_fifo;
    FIFO8_t *rx_fifo;
    USART_TypeDef *uart;
} uart_desc_t;

void init_uart(uint8_t uart_idx, int32_t baudrate, uint16_t fifo_size, uint8_t int_pri);
void uart_in_string(uint8_t uart_idx, char *buff, uint32_t buff_size);
int uart_in_string_nonblocking(uint8_t uart_idx, char *buff, uint32_t buff_size);
void uart_out_string(uint8_t uart_idx, char *buff);
void uart_out_char(uint8_t uart_idx, uint8_t data);

void USART1_IRQHandler(void);
void USART2_IRQHandler(void);
void USART3_IRQHandler(void);
void UART4_IRQHandler(void);
void UART5_IRQHandler(void);

#endif // UART_H