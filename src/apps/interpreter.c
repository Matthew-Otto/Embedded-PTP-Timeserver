#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "interpreter.h"
#include "gpio.h"
#include "uart.h"
#include "ntp.h"

#define HELP_MESSAGE "\
### Commands:\r\n\
help:           print this message\r\n\
time [reset]:   print (or reset) the current system time\r\n\
stats:          print various OS diagnostics\r\n\
\r\n"


const int UART_IDX = 3;
const int buff_size = 256;

void i_help(int argc, char **argv) {
    toggle_GPIO(GPIOG, GPIO_PIN_2);  // BOZO
    uart_out_string(UART_IDX, HELP_MESSAGE);
    toggle_GPIO(GPIOG, GPIO_PIN_2);  // BOZO
}

void i_get_time(int argc, char **argv) {
    const int buff_size = 50;
    char buff[buff_size];
    uint32_t sec;
    uint32_t frac;
    get_time(&sec, &frac);
    snprintf(buff, buff_size, "Raw system time: %u.%u\r\n", sec, frac);
    uart_out_string(UART_IDX, buff);
}

void i_crash(int argc, char **argv) {
    uart_out_string(UART_IDX, "jumping to NULL pointer...");
    ((void (*)(void))0x0)();
}

cmd_t command_list[] = {
    {"help", i_help, "prints the current runtime in ms"},
    {"time", i_get_time, "prints the current runtime in ms"},
    {"crash", i_crash, "prints the current runtime in ms"},
    //{"TEMP", cmd_temp, "Read temperature"},
    {NULL, NULL, NULL} // sentinel
};

void interpreter(void) {
    configure_pin(GPIOD, GPIO_PIN_8, GPIO_MODE_AF_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_VERY_HIGH, GPIO_AF7_USART3); // RX
    configure_pin(GPIOD, GPIO_PIN_9, GPIO_MODE_AF_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_VERY_HIGH, GPIO_AF7_USART3); // TX
    init_uart(UART_IDX, 1000000, 92, 5);
    
    char strbuf[buff_size];
    char *argv[10];
    int argc = 0;
    bool match;

    uart_out_string(UART_IDX, "\x1B[2J\x1B[H");
    uart_out_string(UART_IDX, "Enter a command:\r\n");
    while (1) {
        // get input
        uart_out_string(UART_IDX, "> ");
        uart_in_string_reflect(UART_IDX, strbuf, buff_size);
        uart_out_string(UART_IDX, "\r\n");

        // tokenize input
        char *token = strtok(strbuf, " ");
        while (token != NULL && argc < 10) {
            argv[argc++] = token;
            token = strtok(NULL, " ");
        }
        if (argc == 0) continue;

        // parse input
        match = false;
        for (int i = 0; command_list[i].name != NULL; i++) {
            if (strcmp(argv[0], command_list[i].name) == 0) {
                command_list[i].func(argc, argv);
                match = true;
                break;
            }
        }
        if (!match)
            uart_out_string(UART_IDX, "Invalid command\r\n");
    }
}