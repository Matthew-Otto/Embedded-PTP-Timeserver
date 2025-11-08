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


static float atofloat(const char *str) {
    int sign = 1;
    float fraction = 0.0;
    float divisor = 1.0;
    float value = 0.0;

    if (*str == '-') {
        sign = -1;
        str++;
    }
    while (*str >= '0' && *str <= '9') {
        value = value * 10.0 + (*str - '0');
        str++;
    }
    if (*str == '.') {
        str++;
        while (*str >= '0' && *str <= '9') {
            fraction = fraction * 10.0 + (*str - '0');
            divisor *= 10.0;
            str++;
        }
        value += fraction / divisor;
    }

    return sign * value;
}

// Convert NMEA ddmm.mmmm -> decimal degrees (float)
static float nmea_to_float(const char *nmea, char hemisphere) {
    if (*nmea == '\0')
        return 0.0f;
    float val = strtof(nmea, NULL);
    int degrees = (int)(val / 100.0f);
    float minutes = val - (float)(degrees * 100);
    float decimal = (float)degrees + minutes / 60.0f;
    if (hemisphere == 'S' || hemisphere == 'W') decimal *= -1.0f;
    return decimal;
}

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
    strncpy(gps_data.utc_time, fields[1], sizeof(gps_data.utc_time));
    gps_data.fix_valid = (fields[2][0] == 'A'); // A=valid, V=invalid
    gps_data.latitude  = nmea_to_float(fields[3], fields[4][0]);
    gps_data.longitude = nmea_to_float(fields[5], fields[6][0]);

    strncpy(gps_data.date, fields[9], sizeof(gps_data.date));
}

static void parse_GPGGA(char *fields[], int field_cnt) {
    if (field_cnt < 10) return;
    strncpy(gps_data.utc_time, fields[1], sizeof(gps_data.utc_time));
    gps_data.latitude  = nmea_to_float(fields[2], fields[3][0]);
    gps_data.longitude = nmea_to_float(fields[4], fields[5][0]);
    gps_data.fix_quality = atoi(fields[6]);
    gps_data.num_satellites = atoi(fields[7]);
    gps_data.hdop = atofloat(fields[8]);
}

void parse_nmea_sentence(const char *sentence) {
    if (sentence[0] != '$') return;

    char *fields[MAX_FIELD_CNT];
    int field_cnt = split_fields(sentence, fields, MAX_FIELD_CNT);

    // Recommended Minimum Navigation Information
    if (strncmp(fields[0], "$GPRMC", 6) == 0)
        parse_GPRMC(fields, field_cnt);
    // Global Positioning System Fix Data
    else if (strncmp(fields[0], "$GPGGA", 6) == 0)
        parse_GPGGA(fields, field_cnt);
    /* // GPS DOP and active satellites
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

    if (gps_data.fix_valid != 0) {
        enable_LED(GREEN_LED);

        uint8_t dst_mac[6] = {0x00,0x00,0xba,0x90,0x38,0xb5};
        uint8_t *buffer = ETH_get_tx_buffer();
        if (buffer == NULL)
            return;
        uint16_t length = 0;
        length += build_udp_header(buffer, 5555, 5555, (uint8_t *)&gps_data, sizeof(gps_data_t));
        length += build_ipv4_header(buffer - length, pack4byte(IPv4_ADDR), 0x0A000458, length, PROTO_UDP, 1);
        length += ETH_build_header(buffer - length, dst_mac, ETHERTYPE_IPv4);
        ETH_send_frame(buffer - length, length);
    } else {
        disable_LED(GREEN_LED);
    }
}

float gps_get_lat(void) {
    return gps_data.latitude;
}

float gps_get_long(void) {
    return gps_data.longitude;
}

void gps_init(void) {
    // configure gpio
    configure_pin(GPIOD, GPIO_PIN_7, GPIO_MODE_IT_RISING, GPIO_NOPULL, GPIO_SPEED_FREQ_VERY_HIGH, 0); // PPS
    configure_pin(GPIOD, GPIO_PIN_6, GPIO_MODE_AF_PP, GPIO_PULLUP, GPIO_SPEED_FREQ_VERY_HIGH, GPIO_AF7_USART2); // RX
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

void EXTI7_IRQHandler(void) {
    // set frac bits here

    
    // if valid gps data, toggle green, else set yellow

    // parse gps and update time

    // TODO disable yellow when the gps says it's locked
    disable_LED(YELLOW_LED);


    toggle_LED(GREEN_LED);
    SET_BIT(EXTI->RPR1, 0x1 << 7);
}