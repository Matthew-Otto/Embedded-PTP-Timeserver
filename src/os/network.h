#ifndef NETWORK_H
#define NETWORK_H

#include <stdint.h>
#include "mcu.h"
#include "ethernet.h"
#include "fifo.h"


extern const uint8_t IPv4_ADDR[4];

#define htons(x) __builtin_bswap16(x)
#define htonl(x) __builtin_bswap32(x)
#define htonll(x) __builtin_bswap64(x)
#define ntohs(x) __builtin_bswap16(x)
#define ntohl(x) __builtin_bswap32(x)
#define ntohll(x) __builtin_bswap64(x)

static inline uint32_t pack4byte (const uint8_t bytes[4]) {
    uint32_t v;
    __builtin_memcpy(&v, bytes, sizeof(v));
    return v;
}


typedef enum {
    ETHERTYPE_IPv4  = 0x0800,
    ETHERTYPE_ARP   = 0x0806,
    ETHERTYPE_IPv6  = 0x86DD,
    ETHERTYPE_PTP   = 0x88F7
} ethertype_t;

typedef enum {
    IP_PROTO_ICMP        = 1,
    IP_PROTO_IPv4        = 4,
    IP_PROTO_TCP         = 6,
    IP_PROTO_UDP         = 17,
    IP_PROTO_IPv6        = 41,
    IP_PROTO_ICMPV6      = 58,
    IP_PROTO_NONE        = 59,
    IP_PROTO_RAW         = 255
} ip_protocol_t;

typedef enum {
    PORT_NONE          = 0,
    PORT_SSH           = 22,
    PORT_TELNET        = 23,
    PORT_NTP           = 123,
    PORT_PTP_EVENT     = 319,
    PORT_PTP_GENERAL   = 320
} ip_port_t;

typedef enum {
    ICMP_ECHO_REPLY               = 0,
    ICMP_DEST_UNREACHABLE         = 3,
    ICMP_ECHO_REQUEST             = 8,
    ICMP_ROUTER_ADVERTISEMENT     = 9,
    ICMP_ROUTER_SOLICITATION      = 10
} icmp_type_t;


typedef struct {
    uint8_t  dest[6];
    uint8_t  src[6];
    uint16_t ethertype;
} eth_header_t;

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
    uint8_t  data[];
} icmp_header_t;

typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum;
    uint8_t  data[];
} udp_header_t;


typedef struct {
    uint8_t src_mac[6];
    uint32_t src_ip;
    uint16_t src_port;
    uint8_t *payload;
    uint64_t timestamp;
} udp_socket_t;


typedef struct {
    uint8_t *buffer;
    uint16_t length;
    bool ptp;
    // TODO mailbox
} tx_pkt_ctx_t;



void network_init(int priority);
FIFO_t *open_socket(ip_protocol_t proto, int port);
void send_frame(uint8_t *buffer, uint16_t length, uint64_t *timestamp);

// RX functions
int process_frame(uint8_t *frame);
int process_ipv4(uint8_t *packet, eth_header_t *frame_header);
int process_icmp(icmp_header_t*, ipv4_header_t*, eth_header_t*, uint16_t);
void process_udp(udp_header_t*, ipv4_header_t*, eth_header_t*, uint16_t);

// TX functions
uint16_t build_ipv4_header(uint8_t *buffer, uint32_t dst_ip, uint16_t payload_len, 
                           uint8_t protocol, uint16_t id);
uint16_t build_icmp_reply(uint8_t *buffer, uint16_t id, uint16_t seq_num, 
                          uint8_t *payload, uint16_t payload_len);
uint16_t build_udp_header(uint8_t *buffer, uint16_t src_port, uint16_t dst_port, 
                          uint8_t *data, uint16_t data_len);

// Tasks
void network_receive_task(void);
void network_send_task(void);
void network_send_complete_task(void);

// memory block allocation
#define BUFFER_SIZE 1536
#define RX_PKT_MAX 16
#define TX_PKT_MAX 16
#define PKT_BUFFER_CNT 20

void pkt_buff_init(void);
uint8_t *pkt_alloc_rx(void);
uint8_t *pkt_alloc_tx(void);
void pkt_free_rx(uint8_t *ptr);
void pkt_free_tx(uint8_t *ptr);

#endif // NETWORK_H