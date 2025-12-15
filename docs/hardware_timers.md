# Hardware Timers


### Hardware PTP timer architecture

Two timers:
* second counter (32 bits)
* subsecond counter (31 bits)

The second register increments every time the subsecond register rolls over (@ 0x7FFF_FFFF)\
The subsecond register is incremented by the value in MACSSIR.\
The value in MACSSIR determines the maximum possible precision.\
i.e.: for a theoretical precision of 10ns, the subsecond register should be incremented at 100MHz.\
In this case, the value of MACSSIR should be: 0x7FFFFFFF / 100000000 = ~21

To adjust for clock skew, the subsecond register is incremented using a high precision clock divider/multiplier implemented as another 32-bit timer.\
This timer increments the subsecond register when it rolls over. The value added to this timer at every cycle is contained in the MACTSAR register. \
The proper value for MACTSAR can be calculated with: 0xFFFFFFFF / (clock_speed / subsec_incr_freq)
i.e.: If the subsecond register is configured to increment at 100MHz and the system clock operates at 250MHz, MACTSAR should be set to 0xFFFFFFFF / (250M / 100M) = 1717986918.\
This register should be updated using the true value of the system clock as it drifts away from 250MHz.


### configuring PTP timer snapshots (TODO)

configure gpio pin connected to GPS PPS as event source for timer 3
have timer3 trigger output event to trigger ethernet ptp timestamp


### Experimental Verification

The MAC can output a PPS signal used to compare the synchronization between two devices. This function ETH_PPS_OUT can be assigned to pin PB5 or PG8

TODO