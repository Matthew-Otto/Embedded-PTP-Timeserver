#ifndef TIME_H
#define TIME_H

#include <stdint.h>
#include "mcu.h"

typedef enum {
    SLEEP = 0,
    PERIODIC = 1
} timer_e;

void init_timer(void);
void arm_timer(timer_e alarm_id, uint32_t alarm_value);
uint32_t get_raw_time_ms(void);

void TIM2_IRQHandler(void);

#endif // TIME_H