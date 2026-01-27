# Ethernet

## Using Ethernet
The application sends data via Ethernet by assigning the address of an ethernet frame in memory to a DMA descriptor and triggering a transmission by updating the descriptor ring tail pointer.

### Receive

To receive, the application writes a pointer to an empty memory block into a DMA descriptor and waits for a packet to arrive via the network.

When a valid packet is received from the network and passes MAC filtering, the DMA transfers the packet to memory referenced by the first available descriptor. An Ethernet DMA interrupt is then raised. The ISR unblocks the network semaphore, and the application processes the packet.
Once the packet has been processed, the memory block containing the packet should be freed so it can be assigned to a new RX descriptor.

POTENTIAL BUG: if enough packets are received, blocks will be exhausted and new RX descriptors will not be created. There is currently method to recover from this state.

### Transmit

Memory for a transmit packet can be allocated with ETH_pkt_alloc_tx() which will return a pointer to the last byte of the memory block. Packets should be constructed back-to-front starting at this pointer.

Packets are sent by passing the pointer to and length of a complete ethernet frame to ETH_send_frame(uint8_t *buffer, uint16_t length).

This function attempts to acquire an available transmit DMA descriptor. If sucessful, it will write the pointer to the frame in this descriptor and pass it to the DMA engine. If unsucessful, this function will instead put the frame pointer in a FIFO that will be serviced by the DMA transmit complete interrupt.
When the DMA notifies that a trasmit is complete, a frame pointer is popped from the FIFO and inserted into the newly freed descriptor.


## Configuring Ethernet

### Configuring Clocks
The Ethernet MAC uses three clocks connected to the AHB1 bus:
* ETH
* ETHTX
* ETHRX

They can be enabled by setting bits `RCC_AHB1ENR_ETHEN`, `RCC_AHB1ENR_ETHTXEN`, and `RCC_AHB1ENR_ETHRXEN` respectively in the `RCC_AHB1ENR` register.

### Configuring GPIO
The Ethernet MAC on the STM32H563ZI connects to the PHY via RMII interface. This interface includes the following pins which should be configured as high-speed alt-function 11 (Ethernet) GPIO:
```
ETH_REF_CLK -> PA1
ETH_MDC     -> PC1
ETH_MDIO    -> PA2
ETH_CRS_DV  -> PA7
ETH_RXD0    -> PC4
ETH_RXD1    -> PC5
ETH_TXD0    -> PG13
ETH_TXD1    -> PB15
ETH_TX_EN   -> PG11
```


### Initialize PHY (via MDIO)
The LAN8742A-CZ-TR PHY is set to autonegotiate out of the box and likely doesn't need modification. However, the ETH <-> PHY interface must be configured to use RMII

1. Enable SBS clock (`APB3ENR_SBS`)
2. Set the PHY interface to RMII in the SBS_PMCR register
3. (optional) Set MDIO clock to a reasonable division of the system clock in the `ETH_MACMDIOAR` register

The PHY can be communicated with by writing mode, address, and data to the `ETH_MACMDIOAR` register and waiting for the operation to complete.

### Configuring MAC

#### Configuring MAC address

The MAC address can be configured by writing the address to ETH_MACA0HR and ETH_MACA0LR. This serves primarily three functions:

1. When the IPv4 Address is also configured in `ETH_MACARPAR`, ARP packet can be offloaded to the MAC
2. The MAC can be configured to automatically insert the address into frames as they are transmitted, removing the need to write the source address in every frame that is constructed in software.
2. The MAC can be configured to filter out any packets not destined for this MAC address, lowering netcode pressure in a noisy network.

#### ARP offloading

When the `ARPEN` bit is set in the `ETH_MACCR` register, the MAC will recognize and service incomming ARP requests automatically. ARP packets can then be ignored by enabling broadcast packet filtering in the MAC or by checking for 0b011 in the LT field of the write-back descriptor.

#### Layer 3 filtering

The MAC can be configured to reject any packet not destined for a particular IP address by setting the `ETH_MACPFR_IPFE` bit in `ETH_MACPFR` to enable L3/L4 filtering, and then configuring L3 destination match.

For IPv4, set the `ETH_MACL3L4CR_L3DAM` bit in `ETH_MACL3L4CxR` and then write the IPv4 address of the device to `ETH_MACL3A1RxR`.

For IPv6, set the `ETH_MACL3L4CR_L3DAM` and `ETH_MACL3L4CR_L3PEN` bits in `ETH_MACL3L4CxR`, and then write the 128bit IPv6 address to registers `ETH_MACL3A0RxR, ETH_MACL3A1RxR, ETH_MACL3A2RxR, ETH_MACL3A3RxR` (little endian).