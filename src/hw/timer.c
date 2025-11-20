#include "mcu.h"
#include "gpio.h"

void init_timer(void) {
    // enable peripheral clock
    SET_BIT(RCC->APB1LENR, RCC_APB1LENR_TIM2EN);
    (void)READ_BIT(RCC->APB1LENR, RCC_APB1LENR_TIM2EN);

    // set timer to count down
    SET_BIT(TIM2->CR1, TIM_CR1_DIR);
    // configure one-pulse mode
    SET_BIT(TIM2->CR1, TIM_CR1_OPM);

    // configure prescaler for 1us
    WRITE_REG(TIM2->PSC, (get_clock_speed() / 1000000U) - 1U);
    
    // enable timer interrupt
    SET_BIT(TIM2->DIER, TIM_DIER_UIE);

    // enable TIM2 interrupts in NVIC
    NVIC_SetPriority(TIM2_IRQn, 5);
    NVIC_EnableIRQ(TIM2_IRQn);
}

void sleep_ms(uint16_t ms) {
    // set timeout time
    WRITE_REG(TIM2->CNT, ms * 1000);
    // enable timer
    SET_BIT(TIM2->CR1, TIM_CR1_CEN);
    // wait for timer interrupt
    __WFI();
}

void TIM2_IRQHandler(void) {
    // ack interrupt
    WRITE_REG(TIM2->SR, 0);
}
