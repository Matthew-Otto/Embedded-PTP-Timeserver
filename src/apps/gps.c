#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include "gps.h"
#include "uart.h"
#include "gpio.h"
#include "ethernet.h"
#include "ip.h"


#define BUFF_SIZE 256
#define MAX_FIELD_CNT 20

static const int UART_IDX = 2;
static char strbuf[BUFF_SIZE];

static gps_data_t gps_data;


// Split NMEA sentence into fields
int split_fields(const char *sentence, char *fields[], int max_fields) {
    int count = 0;
    char *token = strtok(sentence, ",");
    while (token && count < max_fields) {
        fields[count++] = token;
        token = strtok(NULL, ",");
    }
    return count;
}

void parse_GPZDA(char *fields[], int field_cnt) {
    if (field_cnt < 7) return;

    char *end;
    struct tm time = {0};
    uint32_t utc_time = strtol(fields[1], &end, 10);
    time.tm_hour = utc_time / 10000;
    time.tm_min = (utc_time / 100) % 100;
    time.tm_sec = utc_time % 100;
    time.tm_mday = strtol(fields[2], &end, 10);
    time.tm_mon = strtol(fields[3], &end, 10) - 1;
    time.tm_year = strtol(fields[4], &end, 10) - 1900;

    gps_data.utc_time = mktime(&time);
}

void parse_GPRMC(char *fields[], int field_cnt) {
    if (field_cnt < 10) return;
    gps_data.fix_valid = (fields[2][0] == 'A'); // A=valid, V=invalid
}


void parse_nmea_sentence(const char *sentence) {
    if (sentence[0] != '$') return;

    char *fields[MAX_FIELD_CNT];
    int field_cnt = split_fields(sentence, fields, MAX_FIELD_CNT);

    if (strncmp(fields[0], "$GPZDA", 6) == 0)
        parse_GPZDA(fields, field_cnt);
    // Recommended Minimum Navigation Information
    else if (strncmp(fields[0], "$GPRMC", 6) == 0)
        parse_GPRMC(fields, field_cnt);
    /* // Global Positioning System Fix Data
    else if (strncmp(fields[0], "$GPGGA", 6) == 0)
        parse_GPGGA(fields, field_cnt);
    // GPS DOP and active satellites
    else if (strncmp(fields[0], "$GPGSA", 6) == 0)
        parse_GPGSA(fields, field_cnt);
    // Satellites in view
    else if (strncmp(fields[0], "$GPGSV", 6) == 0)
        parse_GPGSV(fields, field_cnt);
    // Geographic Position - Latitude/Longitude
    else if (strncmp(fields[0], "$GPGLL", 6) == 0)
        parse_GPGLL(fields, field_cnt); */
}



void gps_init(void) {
    // configure gpio
    configure_pin(GPIOD, GPIO_PIN_7, GPIO_MODE_IT_RISING, GPIO_NOPULL, GPIO_SPEED_FREQ_VERY_HIGH, 0); // PPS
    configure_pin(GPIOD, GPIO_PIN_6, GPIO_MODE_AF_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_VERY_HIGH, GPIO_AF7_USART2); // RX
    configure_pin(GPIOD, GPIO_PIN_5, GPIO_MODE_AF_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_VERY_HIGH, GPIO_AF7_USART2); // TX
    configure_pin(GPIOD, GPIO_PIN_4, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_LOW, 0); // GND
    configure_pin(GPIOD, GPIO_PIN_3, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_LOW, 0); // VCC


    // power on gps via gpio pins
    //GPIOD->BSRR = (uint32_t)GPIO_PIN_4 << 16; // set gps gnd pin low
    //GPIOD->BSRR = (uint32_t)GPIO_PIN_3; // set gps vcc pin high
    

    // configure pps interrupt
    NVIC_SetPriority(EXTI7_IRQn, 0);
    NVIC_EnableIRQ(EXTI7_IRQn);
}

void gps_timesync(void) {
    gps_init();
    init_uart(UART_IDX, 115200, 256, 3);

    enable_LED(RED_LED);

    while(1) {
        uart_in_string(UART_IDX, strbuf, BUFF_SIZE);
        parse_nmea_sentence(strbuf);

        CLEAR_BIT(ETH->MACTSCR, ETH_MACTSCR_TSCFUPDT);
        // set time frac bits to zero
        WRITE_REG(ETH->MACSTSUR, gps_data.utc_time);
        // Update and wait for completion
        SET_BIT(ETH->MACTSCR, ETH_MACTSCR_TSUPDT);
        while (READ_BIT(ETH->MACTSCR, ETH_MACTSCR_TSUPDT));
    }
}

// TODO: capture time at interrupt and calculate adjustment 
void EXTI7_IRQHandler(void) {
    toggle_LED(YELLOW_LED);
    /* CLEAR_BIT(ETH->MACTSCR, ETH_MACTSCR_TSCFUPDT);
    // set time frac bits to zero
    WRITE_REG(ETH->MACSTNUR, 0);
    // Update and wait for completion
    SET_BIT(ETH->MACTSCR, ETH_MACTSCR_TSUPDT);
    while (READ_BIT(ETH->MACTSCR, ETH_MACTSCR_TSUPDT));

    process_gps();

    if (gps_data.fix_valid) {
        //WRITE_REG(ETH->MACSTSUR, offset_sec);
        toggle_LED(GREEN_LED);
        disable_LED(YELLOW_LED);
    } else {
        disable_LED(GREEN_LED);
        enable_LED(YELLOW_LED);
    } */

    // ack interrupt
    SET_BIT(EXTI->RPR1, 0x1 << 7);
}