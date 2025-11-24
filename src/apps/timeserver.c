// networking task that listens for PTP/NTP packets from teh network and responds accordingly

#include "heap.h"
#include "semaphore.h"
#include "ethernet.h"


void timeserver(void) {
    semaphore_t eth_rx_semaphore;
    init_semaphore(&eth_rx_semaphore, 0);

    ETH_init(&eth_rx_semaphore);
    
    while (1) {
        // blocking semaphore on network packets
        // counting semaphore, ethernet frame buffer acts like fifo

        c_wait(&eth_rx_semaphore);

        ETH_receive_frame();
    }
}