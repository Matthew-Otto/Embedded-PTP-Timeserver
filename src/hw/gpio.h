#ifndef GPIO_H
#define GPIO_H

#include <stdint.h>
#include "mcu.h"

enum LED_COLOR{
    RED_LED,
    YELLOW_LED,
    GREEN_LED
};

void GPIO_init();
void GPIO_clk_en(char);
void configure_pin(GPIO_TypeDef *GPIO_bank, uint16_t pin, uint32_t mode, uint32_t pupd, uint32_t ospeed, uint8_t alternate);
void toggle_GPIO(GPIO_TypeDef *GPIO_bank, uint16_t pin);

void toggle_LED(enum LED_COLOR color);
void enable_LED(enum LED_COLOR color);
void disable_LED(enum LED_COLOR color);

#endif // GPIO_H