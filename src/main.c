#include <stdint.h>
#include "mcu.h"
#include "schedule.h"
#include "gpio.h"
#include "gps.h"
#include "ntp.h"

/*
//// TODO
periodically poll PHY for linkup/linkdown
reconfigure MAC when new link autonegotiate finishes
*/

void toggle1(void) {
    while (1) {
        GPIOC->BSRR = GPIO_PIN_8;
        GPIOC->BRR = GPIO_PIN_8;
    }
}
void toggle2(void) {
    while (1) {
        GPIOC->BSRR = GPIO_PIN_9;
        GPIOC->BRR = GPIO_PIN_9;
    }
}
void toggle3(void) {
    while (1) {
        GPIOC->BSRR = GPIO_PIN_10;
        GPIOC->BRR = GPIO_PIN_10;
    }
}
void toggle4(void) {
    while (1) {
        GPIOC->BSRR = GPIO_PIN_11;
        GPIOC->BRR = GPIO_PIN_11;
    }
}


int main(void) {
    configure_pin(GPIOC, GPIO_PIN_8, GPIO_MODE_OUTPUT_PP, GPIO_PULLDOWN, GPIO_SPEED_FREQ_VERY_HIGH, 0);
    configure_pin(GPIOC, GPIO_PIN_9, GPIO_MODE_OUTPUT_PP, GPIO_PULLDOWN, GPIO_SPEED_FREQ_VERY_HIGH, 0);
    configure_pin(GPIOC, GPIO_PIN_10, GPIO_MODE_OUTPUT_PP, GPIO_PULLDOWN, GPIO_SPEED_FREQ_VERY_HIGH, 0);
    configure_pin(GPIOC, GPIO_PIN_11, GPIO_MODE_OUTPUT_PP, GPIO_PULLDOWN, GPIO_SPEED_FREQ_VERY_HIGH, 0);

    //gps_init();   
    //ntp_process();

    add_thread(toggle1, 128, 1);
    add_thread(toggle2, 128, 1);
    add_thread(toggle3, 128, 1);
    add_thread(toggle4, 128, 1);

    init_scheduler(1);

    return 0;
}