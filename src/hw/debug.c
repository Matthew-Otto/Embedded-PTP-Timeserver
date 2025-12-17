#include "mcu.h"
#include "debug.h"
#include "gpio.h"


static pindef_t pins[8] = {
    {GPIOG, GPIO_PIN_12},
    {GPIOE, GPIO_PIN_9},
    {GPIOE, GPIO_PIN_11},
    {GPIOE, GPIO_PIN_14},
    {GPIOE, GPIO_PIN_13},
    {GPIOG, GPIO_PIN_14},
    {GPIOB, GPIO_PIN_6},
    {GPIOB, GPIO_PIN_7}
};

void init_debug_pins(void) {
    for (int i = 0; i < 8; i++) {
        GPIO_TypeDef *GPIO_bank = pins[i].bank;
        uint16_t pin = pins[i].pin;
        configure_pin(GPIO_bank, pin, GPIO_MODE_OUTPUT_PP, GPIO_PULLDOWN, GPIO_SPEED_FREQ_VERY_HIGH, 0);
    }
}

void debug_toggle(int idx) {
    GPIO_TypeDef *GPIO_bank = pins[idx].bank;
    uint16_t pin = pins[idx].pin;

    if (READ_BIT(GPIO_bank->ODR, pin))
        GPIO_bank->BRR = pin;
    else
        GPIO_bank->BSRR = pin;
}