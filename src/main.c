#include <stdint.h>
#include "mcu.h"
#include "gps.h"
#include "ntp.h"

/*
//// TODO
periodically poll PHY for linkup/linkdown
reconfigure MAC when new link autonegotiate finishes
*/


int main(void) {
    gps_init();   
    ntp_process();

    return 0;
}