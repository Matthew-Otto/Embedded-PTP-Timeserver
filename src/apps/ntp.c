#include <stdlib.h>
#include <string.h>
#include "ntp.h"
#include "schedule.h"
#include "ethernet.h"
#include "ip.h"

// only one outstanding ntp req supported for now
static uint8_t pending_req = 0;
static ntp_req_t request;
static ntp_packet_t resp_packet = {
    .li_vn_mode = 0xE4,   // 0xE4 when unsynchronized, 0x24 when GPS lock
    .stratum = 1,
    .poll = 0,
    .precision = -20,
    .root_delay = 0,
    .root_dispersion = 0,
    .ref_id = 0x5350472E, // "GPS"
};

void get_time(uint32_t *sec, uint32_t *frac) {
    // BOZO: ns timer =/= fraction of a second
    *sec = READ_REG(((ETH_TypeDef *)ETH)->MACSTSUR);
    *frac = READ_REG(((ETH_TypeDef *)ETH)->MACSTNUR);
}

void receive_ntp_req(ntp_packet_t *ntp_packet, udp_header_t *udp_header, ipv4_header_t *ip_header, eth_header_t *frame_header) {
    get_time(&request.recv_ts_sec, &request.recv_ts_frac);
    request.orig_ts_sec = ntp_packet->tx_ts_sec;
    request.orig_ts_frac = ntp_packet->tx_ts_frac;
    request.src_port = ntohs(udp_header->src_port);
    request.src_ip = ntohl(ip_header->src_addr);
    memcpy(&request.src_mac, frame_header->src, 6);
    pending_req = 1;
}

void send_ntp_resp() {
    uint8_t *buffer = ETH_get_tx_buffer();
    if (buffer == NULL)
        return;

    resp_packet.orig_ts_sec = request.orig_ts_sec;
    resp_packet.orig_ts_frac = request.orig_ts_frac;
    resp_packet.recv_ts_sec = request.recv_ts_sec;
    resp_packet.recv_ts_frac = request.recv_ts_frac;
    get_time(&resp_packet.tx_ts_sec, &resp_packet.tx_ts_frac);

    uint16_t length = 0;
    length += build_udp_header(buffer, PORT_NTP, request.src_port, (uint8_t *)&resp_packet, sizeof(ntp_packet_t));
    length += build_ipv4_header(buffer - length, pack4byte(IPv4_ADDR), request.src_ip, length, PROTO_UDP, 0);
    length += ETH_build_header(buffer - length, request.src_mac, ETHERTYPE_IPv4);
    ETH_send_frame(buffer - length, length);
}

void ntp_process() {
    while (1) {
        if (pending_req) {
            send_ntp_resp();
            pending_req = 0;
        }
        suspend();
    }
}