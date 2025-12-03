#ifndef GPS_H
#define GPS_H

#include <stdbool.h>

#define GPRMC_BIT 0x1
#define GPZDA_BIT 0x2
#define VALID_MSK 0x3

typedef struct {
    uint8_t valid_messages;
    uint64_t unix_time;
    int fix_valid;
} gps_data_t;


void gps_timesync(void);

void EXTI7_IRQHandler(void);

#endif // GPS_H