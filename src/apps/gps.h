#ifndef GPS_H
#define GPS_H

typedef struct {
    char utc_time[16];
    char date[8];
    int num_satellites;
    int fix_valid;
    int fix_quality;
} gps_data_t;


void EXTI7_IRQHandler(void);
void gps_init(void);
void parse_nmea_sentence(const char *sentence);
void process_gps(void);

#endif // GPS_H