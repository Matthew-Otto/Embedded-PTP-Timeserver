#include "mcu.h"
#include "uart.h"
#include "fifo.h"

static uart_desc_t desc[5];

void init_uart(uint8_t uart_idx, int32_t baudrate, uint16_t fifo_size, uint8_t int_pri) {
    IRQn_Type uart_irq_num;

    // configure peripheral clocks
    switch (uart_idx) {
        case 2:
            desc[uart_idx].uart = USART2;
            uart_irq_num = USART2_IRQn;
            SET_BIT(RCC->APB1LENR, RCC_APB1LENR_USART2EN);
            (void)READ_BIT(RCC->APB1LENR, RCC_APB1LENR_USART2EN);
            break;
        case 3:
            desc[uart_idx].uart = USART3;
            uart_irq_num = USART3_IRQn;
            SET_BIT(RCC->APB1LENR, RCC_APB1LENR_USART3EN);
            (void)READ_BIT(RCC->APB1LENR, RCC_APB1LENR_USART3EN);
            break;
        
        default:
            break;
    }

    // Clear config and disable usart
    WRITE_REG(desc[uart_idx].uart->CR1, 0);
    SET_BIT(desc[uart_idx].uart->CR1, USART_CR1_FIFOEN); // enable FIFO
    SET_BIT(desc[uart_idx].uart->CR1, USART_CR1_TE);     // enable transmit
    SET_BIT(desc[uart_idx].uart->CR1, USART_CR1_RE);     // enable receive
    
    WRITE_REG(desc[uart_idx].uart->BRR, 250000000 / baudrate);

    SET_BIT(desc[uart_idx].uart->CR1, USART_CR1_RXNEIE_RXFNEIE); // RX fifo not empty
    
    SET_BIT(desc[uart_idx].uart->CR1, USART_CR1_UE);     // enable UART

    desc[uart_idx].tx_fifo = fifo8_init(fifo_size);
    desc[uart_idx].rx_fifo = fifo8_init(fifo_size);

    // enable interrupts
    NVIC_SetPriority(uart_irq_num, 5);
    NVIC_EnableIRQ(uart_irq_num);
}

static inline void uart_interrupt_handler(uint8_t uart_idx, USART_TypeDef *uart) {
    uint8_t data;
    // if data in rx hw fifo, put it in software fifo
    while (uart->ISR & USART_ISR_RXNE_RXFNE) {
        data = (0xFF & uart->RDR);
        fifo8_put(desc[uart_idx].rx_fifo, data);
    }

    // while tx hw fifo not full
    while (READ_BIT(uart->ISR, USART_ISR_TXE_TXFNF)) {
        if (fifo8_get(desc[uart_idx].tx_fifo, &data) == -1) {
            // if sw fifo empty, disable hw fifo not full interrupt
            CLEAR_BIT(uart->CR1, USART_CR1_TXFEIE);
            break;
        }
        uart->TDR = data;
    }
}



void uart_in_string(uint8_t uart_idx, char *buff, uint32_t buff_size) {
    uint32_t length = 0;
    uint8_t inchar;
    do {
        fifo8_get_blocking(desc[uart_idx].rx_fifo, &inchar);
        
        if (inchar == '\n')
            continue;
        if (inchar == '\r') {
            break;
        } else if (length < buff_size) {
            *buff++ = inchar;
            length++;
        }
    } while (length < buff_size);
    *buff = '\0';
}

int uart_in_string_nonblocking(uint8_t uart_idx, char *buff, uint32_t buff_size) {
    uint32_t length = 0;
    uint8_t inchar;
    do {
        if (fifo8_get(desc[uart_idx].rx_fifo, &inchar) < 0) {
            *buff = '\0';
            return 1;
        }
        
        if (inchar == '\n')
            continue;
        if (inchar == '\r') {
            break;
        } else if (length < buff_size) {
            *buff++ = inchar;
            length++;
        }
    } while (length < buff_size);
    *buff = '\0';

    return 0;
}


void uart_out_char(uint8_t uart_idx, uint8_t data) {
    // if tx_fifo is empty and hardware fifo is not full
    if (!fifo8_size(desc[uart_idx].tx_fifo) && (desc[uart_idx].uart->ISR & USART_ISR_TXE_TXFNF)) {
        //put directly into hardware TX buffer
        desc[uart_idx].uart->TDR = data;
    } else { 
        // put into software buffer (blocking)
        while (fifo8_put(desc[uart_idx].tx_fifo, data) == -1);
        // enable hw fifo empty interrupt
        SET_BIT(desc[uart_idx].uart->CR1, USART_CR1_TXFEIE);
    }
}

void uart_out_string(uint8_t uart_idx, char *buff) {
    while (*buff) {
        uart_out_char(uart_idx, *buff++);
    }
}


void USART1_IRQHandler(void) {
    uart_interrupt_handler(1, USART1);
}
void USART2_IRQHandler(void) {
    uart_interrupt_handler(2, USART2);
}
void USART3_IRQHandler(void) {
    uart_interrupt_handler(3, USART3);
}
void UART4_IRQHandler(void) {
    uart_interrupt_handler(4, UART4);
}
void UART5_IRQHandler(void) {
    uart_interrupt_handler(5, UART5);
}
