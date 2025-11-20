#ifndef MCU_H
#define MCU_H

#include <stdint.h>

#include "cmsis_gcc.h"
#include "core_armv8mml.h"
#include "stm32h563xx.h"
#include "stm32h563xx_gpio.h"
#include "stm32h563xx_eth.h"

static inline uint32_t get_clock_speed(void) {
    return 250000000;
}

#endif // MCU_H