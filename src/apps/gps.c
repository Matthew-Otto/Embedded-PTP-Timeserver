#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include "gps.h"
#include "semaphore.h"
#include "uart.h"
#include "gpio.h"
#include "ethernet.h"
#include "ip.h"

#include <stdio.h>

#define BUFF_SIZE 256
#define MAX_FIELD_CNT 20

static const int UART_IDX = 2;
static char strbuf[BUFF_SIZE];

static semaphore_t nmea_burst_sync;
static gps_data_t gps_data;
static int64_t pps_ts_q32;

static bool timing_lock = 0;
static int sync_cnt = 0;



// Split NMEA sentence into fields
int split_fields(char *sentence, char *fields[], int max_fields) {
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
    uint32_t unix_time = strtol(fields[1], &end, 10);
    time.tm_hour = unix_time / 10000;
    time.tm_min = (unix_time / 100) % 100;
    time.tm_sec = unix_time % 100;
    time.tm_mday = strtol(fields[2], &end, 10);
    time.tm_mon = strtol(fields[3], &end, 10) - 1;
    time.tm_year = strtol(fields[4], &end, 10) - 1900;

    gps_data.unix_time = mktime(&time);
    gps_data.valid_messages |= GPZDA_BIT;
}

void parse_GPRMC(char *fields[], int field_cnt) {
    if (field_cnt < 10) return;
    gps_data.fix_valid = (fields[2][0] == 'A'); // A=valid, V=invalid
    gps_data.valid_messages |= GPRMC_BIT;
}


void parse_nmea_sentence(const char *sentence) {
    if (sentence[0] != '$') return;

    char *fields[MAX_FIELD_CNT];
    int field_cnt = split_fields(sentence, fields, MAX_FIELD_CNT);

    // UTC time and date
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
    init_semaphore(&nmea_burst_sync, 1);

    enable_LED(RED_LED);

    const int32_t coarse_threshold = 5;
    // PI controller
    const int32_t Kp = 3;
    const int32_t Ki = 12;
    const int64_t integral_max = Ki * 50000000;
    const int64_t integral_min = -Ki * 50000000;
    int64_t integral_state = 0;

    while (1) {
        b_wait(&nmea_burst_sync);
        
        gps_data.valid_messages = 0;
        while((gps_data.valid_messages & VALID_MSK) != VALID_MSK) {
            uart_in_string(UART_IDX, strbuf, BUFF_SIZE);
            parse_nmea_sentence(strbuf);
        }
        disable_LED(RED_LED);

        int64_t ref_time_q32 = gps_data.unix_time << 32;
        int64_t phase_error_q32 = ref_time_q32 - pps_ts_q32;


        // coarse correction
        int32_t phase_error_int = (int32_t)(phase_error_q32 >> 32);
        if (phase_error_q32 < 0) {
            phase_error_int += 1;
        }
        if (abs(phase_error_int) > coarse_threshold) {          
            ETH_update_PTP_TS_oneshot(gps_data.unix_time, 0);
            disable_LED(GREEN_LED);
            enable_LED(YELLOW_LED);
            continue;
        }

        // BOZO DEBUG
        printf("\x1B[2J\x1B[H");
        uint32_t frac = (uint32_t)phase_error_q32;
        double frac_d = (phase_error_q32 > 0) ? frac / 4294967296.0 : (-1 * frac) / 4294967296.0;
        if (phase_error_q32 < 0)
            printf("phase error: -%u.%09u s\r\n", phase_error_int, (unsigned)(frac_d * 1000000000));
        else
            printf("phase error:  %u.%09u s\r\n", phase_error_int, (unsigned)(frac_d * 1000000000));


        int32_t correction = 0;
        // Proportional component
        correction += phase_error_q32 / Kp;

        printf("proportional: %d\r\n", correction); // BOZO DEBUG

        // Integral component
        integral_state += phase_error_q32;
        if (integral_state > integral_max) {
            integral_state = integral_max;
        } else if (integral_state < integral_min) {
            integral_state = integral_min;
        }
        correction += integral_state / Ki;

        // DEBUG BOZO
        int32_t test = integral_state / Ki;
        printf("integral:     %ld\r\n", test);
        printf("correction:   %d\r\n", correction);

        // fine correction
        ETH_update_PTP_TS_fine(correction);
        

        // update LEDs
        if (abs(phase_error_q32) < 430) { // < ~100ns
            if (sync_cnt > 5) {
                timing_lock = true;
                disable_LED(YELLOW_LED);
            } else {
                sync_cnt++;
            }
        } else {
            sync_cnt = 0;
            timing_lock = false;
            enable_LED(YELLOW_LED);
        }
        disable_LED(GREEN_LED);  
    }
}

void EXTI7_IRQHandler(void) {
    uint32_t pps_ns_ts = READ_REG(ETH->MACSTNR);
    uint32_t pps_sec_ts = READ_REG(ETH->MACSTSR);
    uint32_t pps_ns_ts2 = READ_REG(ETH->MACSTNR);
    uint32_t pps_sec_ts2 = READ_REG(ETH->MACSTSR);
    
    // if roll over occurred during read
    bool rollover_occurred = pps_ns_ts2 < pps_ns_ts;
    // if the sec_ts value matches the value of MACSTSR after rollover, it was sampled after rollover
    bool sampled_after_rollover = pps_sec_ts == pps_sec_ts2;

    if (rollover_occurred && sampled_after_rollover) {
        pps_sec_ts -= 1;
    }

    // combine both timestamps into a single q32.32 value
    pps_ts_q32 = ((int64_t)pps_sec_ts << 32) | (pps_ns_ts << 1);

    if (timing_lock) {
        enable_LED(GREEN_LED);
    }

    // signal gps_timesync task to read NMEA data
    b_signal(&nmea_burst_sync);

    // ack interrupt
    SET_BIT(EXTI->RPR1, 0x1 << 7);
}