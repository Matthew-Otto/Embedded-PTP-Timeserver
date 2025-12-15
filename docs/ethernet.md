# Ethernet

## Using Ethernet
The application sends data via Ethernet by assigning the address of an ethernet frame in memory to a DMA descriptor and triggering a transmission by updating the descriptor ring tail pointer.

### Receive

To receive, the application writes a pointer to an empty memory block into a DMA descriptor and waits for a packet to arrive via the network.

When a valid packet is received from the network and passes MAC filtering, the DMA transfers the packet to memory referenced by the first available descriptor. An Ethernet DMA interrupt is then raised. The ISR unblocks the network semaphore, and the application processes the packet.
Once the packet has been processed, the memory block containing the packet should be freed so it can be assigned to a new RX descriptor.

POTENTIAL BUG: is enough packets are received, blocks will be exhausted and new RX descriptors will not be created. There is currently method to recover from this state.

TODO: rx timestamp


### Transmit

Memory for a transmit packet can be allocated with ETH_pkt_alloc_tx() which will return a pointer to the last byte of the memory block. Packets should be constructed back-to-front starting at this pointer.

Packets are sent by passing the pointer to and length of a complete ethernet frame to ETH_send_frame(uint8_t *buffer, uint16_t length).

This function attempts to acquire an available transmit DMA descriptor. If sucessful, it will write the pointer to the frame in this descriptor and pass it to the DMA engine. If unsucessful, this function will instead put the frame pointer in a FIFO that will be serviced by the DMA transmit complete interrupt.
When the DMA notifies that a trasmit is complete, a frame pointer is popped from the FIFO and inserted into the newly freed descriptor.

(TODO: can I have DMA almost out of descriptor interrupts like uart hw fifo?)

TODO: tx timestamp?










## Configuring Ethernet

### Configuring Clocks
The Ethernet MAC uses three clocks connected to the AHB1 bus:
ETH
ETHTX
ETHRX

### Configuring GPIO
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


### Initialize PHY (via MDIO)
The LAN8742A-CZ-TR PHY is set to autonegotiate out of the box and likely doesn't need modification. However, the ETH <-> PHY interface must be configured to use RMII

1. Enable SBS clock (APB3ENR_SBS)
2. Set the PHY interface to RMII in the SBS_PMCR register \
(optional)
3. Set MDIO clock to a reasonable division of the system clock in the MACMDIOAR register


### Configuring MAC

TODO mac address, IP address\
TODO describe automatic ARP handling\
TODO filtering rx mac address