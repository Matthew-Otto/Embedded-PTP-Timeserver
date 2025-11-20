#ifndef BUTTON_H
#define BUTTON_H

#include <stdint.h>

int init_button(void(*press_task)(void), void(*release_task)(void), uint8_t priority);

void EXTI13_IRQHandler(void);
void TIM7_IRQHandler(void);

#endif // BUTTON_H
