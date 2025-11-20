#include <stdlib.h>
#include <stdint.h>
#include "mcu.h"
#include "gpio.h"
#include "timer.h"

// function to execute on button press
void (*button_press_task)(void);
void (*button_release_task)(void);

int init_button(void(*press_task)(void), void(*release_task)(void), uint8_t priority) {
    if (press_task == NULL)
        return -1;
    if (button_press_task != NULL || button_release_task != NULL)
        return -1;
    button_press_task = press_task;
    button_release_task = release_task;

    // Configure pin as input and enable EXTI
    if (release_task == NULL) {
        configure_pin(GPIOC, GPIO_PIN_13, GPIO_MODE_IT_RISING, GPIO_PULLDOWN, GPIO_SPEED_FREQ_VERY_HIGH, 0);
    } else {
        configure_pin(GPIOC, GPIO_PIN_13, GPIO_MODE_IT_RISING_FALLING, GPIO_PULLDOWN, GPIO_SPEED_FREQ_VERY_HIGH, 0);
    }

    // enable EXTI13 interrupt
    NVIC_SetPriority(EXTI13_IRQn, priority);
    NVIC_EnableIRQ(EXTI13_IRQn);

    return 0;
}


void EXTI13_IRQHandler(void) {
    // Read interrupt status bit, clear it and jump to button task
    if (READ_BIT(EXTI->RPR1, EXTI_RPR1_RPIF13)) {
        SET_BIT(EXTI->RPR1, EXTI_RPR1_RPIF13);
        (*button_press_task)();
    }
    if (READ_BIT(EXTI->FPR1, EXTI_FPR1_FPIF13)) {
        SET_BIT(EXTI->FPR1, EXTI_FPR1_FPIF13);
        (*button_release_task)();
    }
}

