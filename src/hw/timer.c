#include "mcu.h"
#include "timer.h"
#include "schedule.h"


// initialize main system timer (TIM2)
void init_timer(void) {
    // enable peripheral clock
    SET_BIT(RCC->APB1LENR, RCC_APB1LENR_TIM2EN);
    (void)READ_BIT(RCC->APB1LENR, RCC_APB1LENR_TIM2EN);

    // configure prescaler for 1us
    WRITE_REG(TIM2->PSC, (get_clock_speed() / 1000000U) - 1U);
    // trigger update event to load prescalar
    SET_BIT(TIM2->EGR, TIM_EGR_UG);
    WRITE_REG(TIM2->SR, 0);
    
    // enable timer
    SET_BIT(TIM2->CR1, TIM_CR1_CEN);

    // enable TIM2 interrupts in NVIC
    NVIC_SetPriority(TIM2_IRQn, 5);
    NVIC_EnableIRQ(TIM2_IRQn);
}

void arm_timer(timer_e alarm_id, uint32_t alarm_value) {
    // set desired alarm time and enable interrupt

    TIM2->CCR1 = alarm_value * 1000U;
    SET_BIT(TIM2->DIER, TIM_DIER_CC1IE);
}

// return raw system time (ms)
uint32_t get_raw_time_ms(void) {
    return TIM2->CNT / 1000U;
}

void TIM2_IRQHandler(void) {
    if (TIM2->SR & TIM_SR_CC1IF) {
        CLEAR_BIT(TIM2->DIER, TIM_DIER_CC1IE); // disable CC1 interrupt
        unsleep();
    }
    WRITE_REG(TIM2->SR, 0); // ack interrupt
}
