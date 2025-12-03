# Embedded GPS PTP Timeserver -- UNDER CONSTRUCTION --

A GPS referenced Stratum 1 network time server with support for IEEE 1588 Precision Time Protocol (PTP).

Bare-metal implementation written for the STMicroelectronics NUCLEO-H563ZI platform



## Dependencies:

* arm-none-eabi-*
* openocd-stm

To build, run `make` from the top level directory.\
To flash, run `make flash` from the top level directory.


## RTOS

This app runs on a custom RTOS supporting priority-based round-robin task scheduling.\
It supports blocking semaphores and task sleeping.\
It utilizes a basic buddy allocation scheme for heap management.\
A command interpreter is exposed on the STLINK-V3EC Virtual COM port (USART3) by default. However, the serial interface used can be easily changed.


## Networking (Ethernet)

A dedicated task (timeserver.c) listens for network traffic. Once a packet is received, it is parsed and a response is generated and sent back to the network.\
Some features can be modified via CLI (MAC/IP address)


## GPS

ublox LEA-5T

---
---
---
## Ethernet Notes

When a valid packet is received from the network and passes MAC filtering, the DMA transfers the packet to memory referenced by the first available descriptor. the ethernet interrupt is then raised.

~~
The application sends data via Ethernet by assigning the address of an ethernet frame in memory to a DMA descriptor and triggering a transmission by updating the descriptor ring tail pointer.
To receive, the application assigns a pointer to an empty buffer to a DMA descriptor and waits for a packet to arrive via the network.

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
This timer increments the subsecond register when it rolls over. The value added to this timer every cycle is contained in the MACTSAR register. \
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


#### Configuring DMA
Memory dedicated for Rx DMA descriptors:
1524 bytes * 4 descriptors = 6096 -> 8192 bytes
TODO configure descriptors
* Enable DMA transmit and receive functions

#### Configuring MAC

TODO mac address \
TODO filtering rx mac address
* Enable MTL transmit and receive functions
* Enable MAC transmit and receive functions



---


## References / Resources:

[STMicroelectronics/OpenOCD](https://github.com/STMicroelectronics/OpenOCD)

[IEEE 1588-2012 Standard](https://standards.ieee.org/ieee/1588/4355/)

[STM32H563 Reference Manual](https://www.st.com/resource/en/reference_manual/rm0481-stm32h52333xx-stm32h56263xx-and-stm32h573xx-armbased-32bit-mcus-stmicroelectronics.pdf) (RM0481)

[STM32H563 Datasheet](https://www.st.com/resource/en/datasheet/stm32h562ag.pdf) (DS14258)
