# Embedded GPS PTP Timeserver

A GPS-referenced Stratum 1 network time server with support for Precision Time Protocol (PTP, IEEE 1588) and NTPv4 (RFC 5905).

This implementation is entirely bare metal, all of the code in this repo was written from scratch for the STMicroelectronics NUCLEO-H563ZI platform.


## Dependencies:

* arm-none-eabi-*
* openocd-stm

To build, run `make` from the top level directory.\
To flash, run `make flash` from the top level directory.


## Platform

### Hardware
This project was built for the STMicroelectronics NUCLEO-H563ZI. This board features an Arm Cortex-M33 running at (up-to) 250MHz and 100 Mbps Ethernet with hardware timestamping support (for IEEE 1588 PTP)

The main 32.768 kHz crystal oscillator is quite unstable, making accurate time-keeping over long durations impossible. I plan to replace it with a TCXO at some point in the future.

### Timing

The Ethernet MAC contains a hardware timer used to timestamp PTP packets as they enter/leave the device. This timer can also be modified by software and is used as the authoritative time source when generating NTP packets.

### GPS

UTC time is received via GPS module (I use a ublox LEA-5T) over serial (USART2). The PPS signal triggers a snapshot of the PTP timestamp timers which are used to calculate the current time offset. The frequency divider driving the system timer is then adjusted using a PI controller to smoothly align the system time to UTC time.

![sync ack gif](figures/sync_lock.gif)\
*A demo of the system time synchronizing with GPS. The yellow trace is the PPS reference from GPS. The blue trace is the PPS signal originating in the Ethernet MAC. The components of the PI controller can be seen in the bottom right.*

The frequency drift of the main clock (and maybe also the GPS receiver) is quite noticeable at steady-state.


![steady state drift gif](figures/steady_state.gif)\
*Clock drift at steady state (10x speed)*


This data was collected with a piece of foam covering the MCU and external XO. I think a large amount of the drift is caused by trying to use GPS indoors and not a bad crystal oscillator.

### Networking (Ethernet)

Custom netcode receives packets from the internet and passes them to a dedicated timeserver process.

This process will provide time to network clients over either NTP or Layer3 PTP(TODO)

L2 PTP is handled automatically in the Ethernet MAC of the STM32H563

### RTOS

This app runs on a custom RTOS supporting priority-based round-robin task scheduling.\
Various primitives such as blocking semaphores and multi-thread safe FIFOs are included.\
It also includes custom heap management using buddy allocation.\
Custom netcode allows passing network traffic to multiple separate processes based on protocol/port mappings.\
A command interpreter is exposed on the STLINK-V3EC Virtual COM port (USART3) by default. However, the serial interface used can be easily changed.\
This interpreter exposes various OS statistics and information on the running application(s).

---
---
---

## Implementation details (under construction)

### Configuring System Clock

To use on-board high speed crystal oscillator:
Add solder to bridges SB3 and SB4
remove solder bridge from SB49

TODO:
configure PLL with M,N,P/Q/R as 1,20,2 respectively

### configuring PTP timer snapshots (TODO)

configure gpio pin connected to GPS PPS as event source for timer 3
have timer3 trigger output event to trigger ethernet ptp timestamp

### Tuning PI(D) controller
The proportional component should be as large as possible without causing oscillations. This is fairly easy to tune by disabling all the other components and plotting the error over time. However, I found that I was able to increase the proportional gain after including the integral component.

The integral part should have a range large enough to correct for the steady-state frequency-offset of the fine clock divider. The maximum value shouldn't be set much higher than this in order to minimize wind-up. A good way to find the steady-state offset required is to run the time sync with only the proportional component enabled. While in this mode, the error will converge to some nonzero value. The correction value applied when the error becomes stable is the value the integral component should assume at steady state.

To tune the integral gain, set it to ~1/100 the proportional gain and increase until oscillations occur, then back off by a value of 10 or so.

### Ethernet Notes

The application sends data via Ethernet by assigning the address of an ethernet frame in memory to a DMA descriptor and triggering a transmission by updating the descriptor ring tail pointer.
To receive, the application assigns a pointer to an empty buffer to a DMA descriptor and waits for a packet to arrive via the network.

When a valid packet is received from the network and passes MAC filtering, the DMA transfers the packet to memory referenced by the first available descriptor. the ethernet interrupt is then raised.
The packet need not be copied again. PAckets should be processed in place and the descriptor returned to DMA once the memory can be freed.

The MAC can output a PPS signal used to compare the synchronization between two devices. This function ETH_PPS_OUT can be assigned to pins PB5 and PG8


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

### Configuring Ethernet

#### Configuring Clocks
The Ethernet MAC uses three clocks connected to the AHB1 bus:
ETH
ETHTX
ETHRX

#### Configuring GPIO
The Ethernet MAC on the STM32H563ZI connects to the PHY via RMII interface. This interface includes the following pins which should be configured as high-speed alt-function 11 (Ethernet) GPIO:

ETH_REF_CLK -> PA1 \
ETH_MDC     -> PC1 \
ETH_MDIO    -> PA2 \
ETH_CRS_DV  -> PA7 \
ETH_RXD0    -> PC4 \
ETH_RXD1    -> PC5 \
ETH_TXD0    -> PG13 \
ETH_TXD1    -> PB15 \
ETH_TX_EN   -> PG11


#### Initialize PHY (via MDIO)
The LAN8742A-CZ-TR PHY is set to autonegotiate out of the box and likely doesn't need modification. However, the ETH <-> PHY interface must be configured to use RMII

1. Enable SBS clock (APB3ENR_SBS)
2. Set the PHY interface to RMII in the SBS_PMCR register \
(optional)
3. Set MDIO clock to a reasonable division of the system clock in the MACMDIOAR register


#### Configuring MAC

TODO mac address, IP address\
TODO describe automatic ARP handling\
TODO filtering rx mac address


---


## References / Resources:

[STMicroelectronics/OpenOCD](https://github.com/STMicroelectronics/OpenOCD)

[IEEE 1588-2012 Standard](https://standards.ieee.org/ieee/1588/4355/)

[STM32H563 Reference Manual](https://www.st.com/resource/en/reference_manual/rm0481-stm32h52333xx-stm32h56263xx-and-stm32h573xx-armbased-32bit-mcus-stmicroelectronics.pdf) (RM0481)

[STM32H563 Datasheet](https://www.st.com/resource/en/datasheet/stm32h562ag.pdf) (DS14258)

[PID Without a PhD](https://www.wescottdesign.com/articles/pid/pidWithoutAPhd.pdf)
