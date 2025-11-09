#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "gps.h"
#include "uart.h"
#include "gpio.h"
#include "ethernet.h"
#include "ip.h"


#define MAX_FIELD_CNT 20
#define MAX_SENTENCE_LEN 84

static char strbuf[MAX_SENTENCE_LEN];
static gps_data_t gps_data;


// Split NMEA sentence into fields
static int split_fields(const char *sentence, char *fields[], int max_fields) {
    int count = 0;
    char *token = strtok(sentence, ",");
    while (token && count < max_fields) {
        fields[count++] = token;
        token = strtok(NULL, ",");
    }
    return count;
}

static void parse_GPRMC(char *fields[], int field_cnt) {
    if (field_cnt < 10) return;
    gps_data.fix_valid = (fields[2][0] == 'A'); // A=valid, V=invalid
    strncpy(gps_data.utc_time, fields[1], sizeof(gps_data.utc_time));
    strncpy(gps_data.date, fields[9], sizeof(gps_data.date));
}


void parse_nmea_sentence(const char *sentence) {
    if (sentence[0] != '$') return;

    char *fields[MAX_FIELD_CNT];
    int field_cnt = split_fields(sentence, fields, MAX_FIELD_CNT);

    // Recommended Minimum Navigation Information
    if (strncmp(fields[0], "$GPRMC", 6) == 0)
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

// Call this function regularly to update GPS state
void process_gps(void) {
    int uart_empty;
    do {
        uart_empty = uart_in_string_nonblocking(strbuf, MAX_SENTENCE_LEN);
        parse_nmea_sentence(strbuf);
    } while (!uart_empty);
}

void gps_init(void) {
    // configure gpio
    configure_pin(GPIOD, GPIO_PIN_7, GPIO_MODE_IT_RISING, GPIO_NOPULL, GPIO_SPEED_FREQ_VERY_HIGH, 0); // PPS
    configure_pin(GPIOD, GPIO_PIN_6, GPIO_MODE_AF_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_VERY_HIGH, GPIO_AF7_USART2); // RX
    configure_pin(GPIOD, GPIO_PIN_5, GPIO_MODE_AF_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_VERY_HIGH, GPIO_AF7_USART2); // TX
    configure_pin(GPIOD, GPIO_PIN_4, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_LOW, 0); // GND
    configure_pin(GPIOD, GPIO_PIN_3, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_LOW, 0); // VCC

    // configure uart
    init_uart(2, 9600);
    
    // power on gps via gpio pins
    GPIOD->BSRR = (uint32_t)GPIO_PIN_4 << 16; // set gps gnd pin low
    GPIOD->BSRR = (uint32_t)GPIO_PIN_3; // set gps vcc pin high
    
    enable_LED(RED_LED);

    // configure pps interrupt
    NVIC_SetPriority(EXTI7_IRQn, 3);
    NVIC_EnableIRQ(EXTI7_IRQn);
}


// TODO: capture time at interrupt and calculate adjustment 
void EXTI7_IRQHandler(void) {
    CLEAR_BIT(ETH->MACTSCR, ETH_MACTSCR_TSCFUPDT);
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
    }

    // ack interrupt
    SET_BIT(EXTI->RPR1, 0x1 << 7);
}