#ifndef TIME_H
#define TIME_H

#include <stdint.h>
#include "mcu.h"

void init_timer(void);
void sleep_ms(uint16_t ms);

void TIM2_IRQHandler(void);

#endif // TIME_H