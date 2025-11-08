#include <stdint.h>
#include "mcu.h"
#include "ntp.h"

/*
//// TODO
periodically poll PHY for linkup/linkdown
reconfigure MAC when new link autonegotiate finishes
*/

volatile char strbuf[100];

int main(void) {
    // set gps vcc pin high
    GPIOD->BSRR = (uint32_t)GPIO_PIN_3;
    // set gps gnd pin low
    GPIOD->BSRR = (uint32_t)GPIO_PIN_4 << 16;

    
    ntp_process();

    return 0;
}