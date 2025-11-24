#ifndef GPS_H
#define GPS_H

typedef struct {
    uint64_t utc_time;
    int fix_valid;
} gps_data_t;


void gps_timesync(void);

void EXTI7_IRQHandler(void);

#endif // GPS_H