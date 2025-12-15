#include <stdint.h>
#include "mcu.h"
#include "schedule.h"
#include "interpreter.h"
#include "timeserver.h"
#include "gps.h"
#include "gpio.h"

/*
//// TODO
periodically poll PHY for linkup/linkdown
reconfigure MAC when new link autonegotiate finishes
*/



int main(void) {
    configure_pin(GPIOC, GPIO_PIN_8, GPIO_MODE_OUTPUT_PP, GPIO_PULLDOWN, GPIO_SPEED_FREQ_VERY_HIGH, 0);
    configure_pin(GPIOC, GPIO_PIN_9, GPIO_MODE_OUTPUT_PP, GPIO_PULLDOWN, GPIO_SPEED_FREQ_VERY_HIGH, 0);
    configure_pin(GPIOC, GPIO_PIN_10, GPIO_MODE_OUTPUT_PP, GPIO_PULLDOWN, GPIO_SPEED_FREQ_VERY_HIGH, 0);
    configure_pin(GPIOC, GPIO_PIN_11, GPIO_MODE_OUTPUT_PP, GPIO_PULLDOWN, GPIO_SPEED_FREQ_VERY_HIGH, 0);


    add_thread(gps_timesync, 1024, 1);
    add_thread(timeserver, 512, 1);
    add_thread(interpreter, 512, 1);


    init_scheduler(1);

    return 0;
}
