#include "gpio.h"

#define PIN(bank, num) ((((bank) - 'A') << 8) | (num))
#define PINBANK(pin) (pin >> 8)


void GPIO_clk_en(char port) {
    // port letter A - I
    // convert to uppercase
    if (port >= 'a' && port <= 'z')
        port -= 32;
    if (port < 'A' || port > 'I')
        return;

    uint32_t reg_addr = 0x1UL << (port - 'A');
    SET_BIT(RCC->AHB2ENR, reg_addr);
    (void)READ_BIT(RCC->AHB2ENR, reg_addr);
}


void configure_pin(GPIO_TypeDef *GPIO_bank, uint16_t pin, uint32_t mode, uint32_t pupd, uint32_t ospeed, uint8_t alternate) {
    uint32_t pin_num = 0;
    while ((pin >> pin_num) != 0) pin_num++;
    pin_num -= 1;
    uint32_t pin_mode_ofst = pin_num << 1;
    uint32_t pin_alt_ofst = (pin_num & 0x7) << 2;

    MODIFY_REG(GPIO_bank->MODER, 0x3 << pin_mode_ofst, (mode & GPIO_MODE) << pin_mode_ofst);
    MODIFY_REG(GPIO_bank->OTYPER, 0x1 << pin_num, ((mode & GPIO_OUTPUT_TYPE) >> 4U) << pin_num);
    MODIFY_REG(GPIO_bank->PUPDR, 0x3 << pin_mode_ofst, pupd << pin_mode_ofst);
    MODIFY_REG(GPIO_bank->OSPEEDR, 0x3 << pin_mode_ofst, ospeed << pin_mode_ofst);
    MODIFY_REG(GPIO_bank->AFR[pin_num >> 3], 0xF << pin_alt_ofst, alternate << pin_alt_ofst);

    // TODO update to support all modes on all pins
    if ((mode & EXTI_MODE) == EXTI_MODE) {
        // set rising and enable interrupt
        SET_BIT(EXTI->RTSR1, pin);
        SET_BIT(EXTI->IMR1, pin);

        // set exti7 to pin d7
        uint32_t clr_mask = 0xF << ((pin_num & 0x3) * 8);
        uint32_t set_mask = 0x3 << ((pin_num & 0x3) * 8); // 3 is bank d
        MODIFY_REG(EXTI->EXTICR[pin_num >> 2U], clr_mask, set_mask);

        volatile int  x = READ_REG(EXTI->EXTICR[1]);
        volatile int y = 0;
    }
};


void configure_button(void) {
    GPIO_TypeDef *GPIO_bank = GPIOC;
    uint16_t pin = GPIO_PIN_13;
    uint32_t pin_num = 0;
    while ((pin >> pin_num) != 0) pin_num++;
    pin_num -= 1;
    uint32_t pin_mode_ofst = pin_num << 1;

    MODIFY_REG(GPIO_bank->MODER, 0x3 << pin_mode_ofst, (GPIO_MODE_INPUT & GPIO_MODE) << pin_mode_ofst);
    MODIFY_REG(GPIO_bank->OTYPER, 0x1 << pin_num, ((GPIO_MODE_INPUT & GPIO_OUTPUT_TYPE) >> 4U) << pin_num);
    MODIFY_REG(GPIO_bank->PUPDR, 0x3 << pin_mode_ofst, GPIO_PULLDOWN << pin_mode_ofst);
    MODIFY_REG(GPIO_bank->OSPEEDR, 0x3 << pin_mode_ofst, GPIO_SPEED_FREQ_HIGH << pin_mode_ofst);
}

void enable_LED(enum LED_COLOR color) {
    switch (color) {
        case RED_LED:
            GPIOG->BSRR = (uint32_t)GPIO_PIN_4;
            break;
        case YELLOW_LED:
            GPIOF->BSRR = (uint32_t)GPIO_PIN_4;
            break;
        case GREEN_LED:
            GPIOB->BSRR = (uint32_t)GPIO_PIN_0;
            break;
    }
}

void disable_LED(enum LED_COLOR color) {
    switch (color) {
        case RED_LED:
            GPIOG->BSRR = (uint32_t)GPIO_PIN_4 << 16;
            break;
        case YELLOW_LED:
            GPIOF->BSRR = (uint32_t)GPIO_PIN_4 << 16;
            break;
        case GREEN_LED:
            GPIOB->BSRR = (uint32_t)GPIO_PIN_0 << 16;
            break;
    }
}

void toggle_LED(enum LED_COLOR color) {
    switch (color) {
        case RED_LED:
            if (READ_BIT(GPIOG->ODR, GPIO_PIN_4))
                disable_LED(RED_LED);
            else
                enable_LED(RED_LED);
            break;
        case YELLOW_LED:
            if (READ_BIT(GPIOF->ODR, GPIO_PIN_4))
                disable_LED(YELLOW_LED);
            else
                enable_LED(YELLOW_LED);
            break;
        case GREEN_LED:
            if (READ_BIT(GPIOB->ODR, GPIO_PIN_0))
                disable_LED(GREEN_LED);
            else
                enable_LED(GREEN_LED);
            break;
    }
}


void GPIO_init() {
    static uint8_t once = 0;
    if (once) return;
    once = 1;

    GPIO_clk_en('A');
    GPIO_clk_en('B');
    GPIO_clk_en('C');
    GPIO_clk_en('D');
    GPIO_clk_en('E');
    GPIO_clk_en('F');
    GPIO_clk_en('G');
    GPIO_clk_en('H');
    GPIO_clk_en('I');

    configure_pin(GPIOB, GPIO_PIN_0, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_LOW, 0);
    configure_pin(GPIOF, GPIO_PIN_4, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_LOW, 0);
    configure_pin(GPIOG, GPIO_PIN_4, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_LOW, 0);

    configure_button();
}
