#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "interpreter.h"
#include "gpio.h"
#include "uart.h"
#include "schedule.h"
#include "heap.h"
#include "timeserver.h"
#include "gps.h"

#define HELP_MESSAGE "\
### Commands:\r\n\
help:           print this message\r\n\
time [reset]:   print the current system time\r\n\
stats:          print various OS diagnostics\r\n\
\r\n"

#define BUFF_SIZE 256

static const int UART_IDX = 3;
static char strbuf[BUFF_SIZE];



static inline void print(char *str){
    uart_out_string(UART_IDX, str);
}

void i_help(int argc, char **argv);

extern bool timing_lock;
void i_get_time(int argc, char **argv) {
    while (1) {
        uint64_t ts = get_time();
        uint32_t s = ts >> 32;
        uint32_t ns = ((ts & 0xFFFFFFFF) * 999999999) >> 32;
        char *status = timing_lock ? "locked  " : "unlocked";
        snprintf(strbuf, BUFF_SIZE, "GPS: %s\r\nRaw system time: %u.%09u\r\033[A", status, s, ns);
        print(strbuf);
        sleep(17);
        
        // wait for ctrl-c
        if (uart_search_for_char_nonblocking(UART_IDX, 0x03)) {
            print("\r\n\n");
            break;
        }
    }
}

void i_stats(int argc, char **argv) {
    /* uint32_t cpu0_idle = get_idle_percentage(0);
    uint32_t cpu1_idle = get_idle_percentage(1);
    snprintf(strbuf, BUFF_SIZE, "CPU0 idle percentage: %d.%d\r\nCPU1 idle percentage: %d.%d\r\n",
            cpu0_idle/10, cpu0_idle%10, cpu1_idle/10, cpu1_idle%10);
    print(strbuf);
    // scheduler stats
    scheduler_stats_t sstats;
    scheduler_stats(&sstats);
    snprintf(input_buffer, buffsize, "\r\nRunning Threads: %d\r\nActive Threads: %d\r\nLifetime Threads: %d\r\n", 
        sstats.running_thread_cnt, sstats.active_thread_cnt, sstats.lifetime_thread_cnt);
    print(input_buffer); */
    // heap stats
    heap_stats_t hstats;
    heap_stats(&hstats);
    snprintf(strbuf, BUFF_SIZE, "\r\nHeap size: %d\r\nHeap used: %d\r\nHeap free: %d\r\n", 
        hstats.size, hstats.used, hstats.free);
    print(strbuf);
}

void i_crash(int argc, char **argv) {
    print("jumping to NULL pointer...");
    ((void (*)(void))0x0)();
}


cmd_t command_list[] = {
    {"help", i_help, "print this message"},
    {"time", i_get_time, "print (or reset) the current system time"},
    {"stats", i_stats, "print various OS diagnostics"},
    {"crash", i_crash, "crash the system"},
    {NULL, NULL, NULL} // sentinel
};


void i_help(int argc, char **argv) {
    snprintf(strbuf, BUFF_SIZE, "##### Commands #####\r\n");
    print(strbuf);
    
    for (int i = 0; command_list[i].name != NULL; i++) {
        snprintf(strbuf, BUFF_SIZE, "%s: %s\r\n", command_list[i].name, command_list[i].help);
        print(strbuf);
    }
}


void interpreter(void) {
    configure_pin(GPIOD, GPIO_PIN_8, GPIO_MODE_AF_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_VERY_HIGH, GPIO_AF7_USART3); // RX
    configure_pin(GPIOD, GPIO_PIN_9, GPIO_MODE_AF_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_VERY_HIGH, GPIO_AF7_USART3); // TX
    init_uart(UART_IDX, 1000000, 92, 5);
    
    char *argv[10];
    int argc = 0;
    bool match;

    print("\x1B[2J\x1B[H"); // clear terminal
    print("Enter a command:\r\n");
    while (1) {
        // get input
        print("> ");
        uart_in_string_reflect(UART_IDX, strbuf, BUFF_SIZE);
        print("\r\n");

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
        if (!match) {
            print("Invalid command '");
            print(argv[0]);
            print("'\r\n");
        }
    }
}
