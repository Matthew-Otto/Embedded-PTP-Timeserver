#include <stdint.h>
#include "mcu.h"
#include "schedule.h"
#include "interpreter.h"
#include "timeserver.h"
#include "gps.h"

/*
//// TODO
periodically poll PHY for linkup/linkdown
reconfigure MAC when new link autonegotiate finishes
*/

#include "debug.h"

int main(void) {
    init_debug_pins();

    add_thread(gps_timesync, 1024, 1);
    add_thread(timeserver, 512, 1);
    add_thread(interpreter, 512, 1);


    init_scheduler(1);

    return 0;
}
