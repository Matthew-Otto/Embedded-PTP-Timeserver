#ifndef IP_H
#define IP_H

#include <stdint.h>
#include "mcu.h"
#include "ethernet.h"


#define PROTO_ICMP 1
#define PROTO_TCP  6
#define PROTO_UDP  17

#define ICMP_ECHO_REQ  8
#define ICMP_ECHO_RPLY 0

#define PORT_PTP_EVENT   319
#define PORT_PTP_GENERAL 320
#define PORT_NTP         123

extern const uint8_t IPv4_ADDR[4];

typedef struct {
    uint8_t  version_ihl;
    uint8_t  tos;
    uint16_t total_length;
    uint16_t id;
    uint16_t flags_fragment;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t checksum;
    uint32_t src_addr;
    uint32_t dst_addr;
} ipv4_header_t;

typedef struct {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint16_t id;
    uint16_t seq;
    uint8_t  data[]; // variable length
} icmp_header_t;

typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum;
    uint8_t  data[];
} udp_header_t;

void process_packet(uint8_t*, eth_header_t*);
void process_icmp(icmp_header_t*, ipv4_header_t*, eth_header_t*, uint16_t);
void process_udp(udp_header_t*, ipv4_header_t*, eth_header_t*, uint16_t);

uint16_t build_ipv4_header(uint8_t *buffer, uint32_t src_ip, 
                           uint32_t dst_ip, uint16_t payload_len, 
                           uint8_t protocol, uint16_t id);
uint16_t build_icmp_reply(uint8_t *buffer, uint16_t id, uint16_t seq_num, 
                          uint8_t *payload, uint16_t payload_len);
uint16_t build_udp_header(uint8_t *buffer, uint16_t src_port, uint16_t dst_port, 
                          uint8_t *data, uint16_t data_len);

#endif // IP_H