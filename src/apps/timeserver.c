// networking task that listens for PTP/NTP packets from the network and responds accordingly

#include <stdio.h>
#include "timeserver.h"
#include "network.h"
#include "fifo.h"
#include "heap.h"


// 0xE4 when unsynchronized, 0x24 when GPS lock
#define UNLOCKED_LVM    0xE4
#define LOCKED_LVM      0x24
#define STRATUM         1
#define POLL            0
#define PRECISION       -21
#define ROOT_DELAY      0
#define ROOT_DISPERSION 0x1
#define REF_ID          0x00535047 // "GPS"

static ntp_packet_t response = {
    .li_vn_mode = LOCKED_LVM,
    .stratum = STRATUM,
    .poll = POLL,
    .precision = PRECISION,
    .root_delay = ROOT_DELAY,
    .root_dispersion = ROOT_DISPERSION,
    .ref_id = REF_ID,
};

void timeserver(void) {
    mFIFO_t *socket_buffer = open_socket(IP_PROTO_UDP, PORT_NTP);
    
    udp_socket_t *socket;
    while (true) {
        mfifo_get(socket_buffer, &socket);

        ntp_packet_t *request = (ntp_packet_t *)socket->payload;

        response.orig_ts = request->tx_ts;
        response.rx_ts = htonll(get_ntp_time());
        response.tx_ts = htonll(get_ntp_time());

        // send response
        uint8_t *buffer = ETH_get_tx_buffer();
        
        if (buffer != NULL) {
            uint16_t length = 0;
            length += build_udp_header(buffer, PORT_NTP, socket->src_port, (uint8_t *)&response, sizeof(ntp_packet_t));
            length += build_ipv4_header(buffer - length, socket->src_ip, length, IP_PROTO_UDP, 0);
            length += ETH_build_header(buffer - length, socket->src_mac, ETHERTYPE_IPv4);
            ETH_send_frame(buffer - length, length);
        }

        // free socket
        free(socket->payload);
        free(socket);
    }
}

uint64_t get_time(void) {
    uint32_t ns_ts = READ_REG(ETH->MACSTNR);
    uint32_t sec_ts = READ_REG(ETH->MACSTSR);
    uint32_t ns_ts2 = READ_REG(ETH->MACSTNR);
    uint32_t sec_ts2 = READ_REG(ETH->MACSTSR);
    
    // if roll over occurred during read
    bool rollover_occurred = ns_ts2 < ns_ts;
    // if the sec_ts value matches the value of MACSTSR after rollover, it was sampled after rollover
    bool sampled_after_rollover = sec_ts == sec_ts2;

    if (rollover_occurred && sampled_after_rollover) {
        sec_ts -= 1;
    }

    // combine both timestamps into a single q32.32 value
    return ((uint64_t)sec_ts << 32) | (ns_ts << 1);
}

uint64_t get_ntp_time(void) {
    uint64_t unix_ts = get_time();
    return unix_ts + NTP_TIMESTAMP_DELTA;
}
