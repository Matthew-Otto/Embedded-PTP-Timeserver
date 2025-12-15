// networking task that listens for PTP/NTP packets from the network and responds accordingly

#include <stdio.h>
#include "timeserver.h"
#include "network.h"
#include "gps.h"
#include "fifo.h"
#include "heap.h"


// 0xE4 when unsynchronized, 0x24 when GPS lock
#define UNLOCKED_LVM    0xE4
#define LOCKED_LVM      0x24
#define STRATUM         1
#define PRECISION       -23
#define ROOT_DELAY      0
#define ROOT_DISPERSION 1
#define REF_ID          0x00535047 // "GPS"

static ntp_packet_t response = {
    .li_vn_mode = LOCKED_LVM,
    .stratum = STRATUM,
    .precision = PRECISION,
    .root_delay = htonl(ROOT_DELAY),
    .root_dispersion = htonl(ROOT_DISPERSION),
    .ref_id = REF_ID,
};

static inline uint64_t utc_to_ntp(uint64_t utc_ts) {
    return utc_ts + NTP_TIMESTAMP_DELTA;
}

void timeserver(void) {
    FIFO_t *socket_buffer = open_socket(IP_PROTO_UDP, PORT_NTP);
    
    udp_socket_t *socket;
    while (true) {
        fifo_get(socket_buffer, &socket);
        uint64_t rx_time = get_time();

        ntp_packet_t *request = (ntp_packet_t *)socket->payload;

        if (timing_lock)
            response.li_vn_mode = LOCKED_LVM;
        else
            response.li_vn_mode = UNLOCKED_LVM;

        response.poll = request->poll;
        response.ref_ts = htonll(utc_to_ntp(last_sync_ts));
        response.orig_ts = request->tx_ts;
        response.rx_ts = htonll(utc_to_ntp(rx_time));
        response.tx_ts = htonll(utc_to_ntp(get_time()));

        // send response
        uint8_t *buffer = ETH_pkt_alloc_tx();
        
        if (buffer != NULL) {
            uint16_t length = 0;
            length += build_udp_header(buffer, PORT_NTP, socket->src_port, (uint8_t *)&response, sizeof(ntp_packet_t));
            length += build_ipv4_header(buffer - length, socket->src_ip, length, IP_PROTO_UDP, 0);
            length += ETH_build_header(buffer - length, socket->src_mac, ETHERTYPE_IPv4);
            ETH_send_frame(buffer - length, length, false);
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
