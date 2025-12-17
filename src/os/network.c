#include <stdint.h>
#include <string.h>
#include "mcu.h"
#include "network.h"
#include "schedule.h"
#include "ethernet.h"
#include "fifo.h"
#include "malloc.h"

#include "debug.h"

#ifdef BACKUP
const uint8_t IPv4_ADDR[4] = {10, 0, 123, 2};
#else
const uint8_t IPv4_ADDR[4] = {10, 0, 123, 3};
#endif

static uint32_t SRC_IPv4_ADDR;

static semaphore_t eth_rx_semaphore;
static semaphore_t eth_tx_semaphore;
static FIFO_t *tx_fifo;
static FIFO_t *tx_pending;

static FIFO_t *upd_port_map[1024] = {NULL};

// packet buffer
__attribute__((section(".pkt_buffer_pool")))
static uint8_t pkt_buffer_pool[PKT_BUFFER_CNT][BUFFER_SIZE];

static uint8_t *free_block_root;
static semaphore_t pkt_buff_mutex;

static uint16_t rx_alloc_cnt = 0;
static uint16_t tx_alloc_cnt = 0;



// forward declaration
static uint16_t checksum16(const void *data, uint16_t len);


FIFO_t *open_socket(ip_protocol_t proto, int port) {
    FIFO_t *socket_fifo = fifo_init(8, sizeof(udp_socket_t *));
    if (socket_fifo == NULL) return NULL;
    if (proto = IP_PROTO_UDP)
        upd_port_map[port] = socket_fifo;
    return socket_fifo;
}


void network_init(int priority) {
    SRC_IPv4_ADDR = pack4byte(IPv4_ADDR);

    pkt_buff_init();
    init_semaphore(&eth_rx_semaphore, 0);
    init_semaphore(&eth_tx_semaphore, 0);
    tx_fifo = fifo_init(TX_PKT_MAX, sizeof(tx_pkt_ctx_t));
    tx_pending = fifo_init(TX_DSC_CNT, sizeof(tx_pkt_ctx_t));
    ETH_init(&eth_rx_semaphore, &eth_tx_semaphore);

    // Add network processes to task schedule
    add_thread(network_receive_task, 64, priority);
    add_thread(network_send_task, 64, priority);
    add_thread(network_send_complete_task, 64, priority);
}


///////////////////////////////////////////////////////////////////////////////
//// Receive //////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

static uint64_t timestamp;

void network_receive_task(void) {
    uint8_t *frame_ptr;
    while (1) {
        // counting semaphore, receive packets until no more are ready
        c_wait(&eth_rx_semaphore);
        debug_toggle(1);

        int rc = 0;
        ETH_receive_frame(&frame_ptr, &timestamp);
        if (frame_ptr == NULL) continue;

        rc = process_frame(frame_ptr);
        // free packet buffer
        pkt_free_rx(frame_ptr);
    }
}


int process_frame(uint8_t *frame) {
    eth_header_t *header = (eth_header_t *)frame;
    uint16_t ethertype = ntohs(header->ethertype);
    uint8_t *payload = ((uint8_t *)frame + sizeof(eth_header_t));

    switch (ethertype) {
        case ETHERTYPE_IPv4:
            return process_ipv4(payload, header);
        //case ETHERTYPE_IPv6:
        //case ETHERTYPE_PTP:
        //    return process_ptp_message(payload);
    }
    return -1;
}


int process_ipv4(uint8_t *packet, eth_header_t *frame_header) {
    ipv4_header_t *ip_header = (ipv4_header_t *)packet;

    if ((ip_header->version_ihl >> 4) != 4)
        return -1; // Not IPv4

    uint16_t ip_header_len = (ip_header->version_ihl & 0x0F) * 4;
    uint16_t payload_len = ntohs(ip_header->total_length) - ip_header_len;
    uint8_t *payload = ((uint8_t *)ip_header + ip_header_len);

    switch (ip_header->protocol) {
        case IP_PROTO_ICMP:
            process_icmp((icmp_header_t *)payload, ip_header, frame_header, payload_len);
            break;
        case IP_PROTO_TCP:
            break;
        case IP_PROTO_UDP:
            process_udp((udp_header_t *)payload, ip_header, frame_header, payload_len);
            break;
    }
}


int process_icmp(icmp_header_t *icmp, ipv4_header_t *ip_pkt, eth_header_t *frame_header, uint16_t pkt_len) {
    if (icmp->type != 8)
        return -1; // Not Echo Request

    // respond to ping
    uint8_t *buffer = pkt_alloc_tx();
    if (buffer == NULL)
        return -1;

    uint16_t length = 0;
    length += build_icmp_reply(buffer, ntohs(icmp->id), ntohs(icmp->seq), icmp->data, (pkt_len - sizeof(icmp_header_t)));
    length += build_ipv4_header(buffer - length, ntohl(ip_pkt->src_addr), length, ip_pkt->protocol, ntohs(ip_pkt->id));
    length += ETH_build_header(buffer - length, frame_header->src, ntohs(frame_header->ethertype));

    send_frame(buffer - length, length, NULL);
}


void process_udp(udp_header_t *packet, ipv4_header_t *ip_header, eth_header_t *frame_header, uint16_t pkt_len) {
    uint16_t port = ntohs(packet->dst_port);

    if (port > 1024) return;

    FIFO_t *socket_buffer = upd_port_map[port];

    if (socket_buffer) {
        udp_socket_t *socket = (udp_socket_t *)malloc(sizeof(udp_socket_t));
        if (socket == NULL) return;
        socket->payload = (uint8_t *)malloc(packet->length);
        if (socket->payload == NULL) return;
        
        memcpy(socket->src_mac, frame_header->src, 6);
        socket->src_ip = ntohl(ip_header->src_addr);
        socket->src_port = ntohs(packet->src_port);
        memcpy(socket->payload, packet->data, ntohs(packet->length));
        socket->timestamp = timestamp;

        debug_toggle(2);

        fifo_put(socket_buffer, &socket);
    }
}


///////////////////////////////////////////////////////////////////////////////
//// Transmit /////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


void network_send_task(void) {
    tx_pkt_ctx_t pkt;
    while (1) {
        fifo_get(tx_fifo, &pkt); // wait for a packet
        fifo_put(tx_pending, &pkt); // put packet into in-flight queue (blocks if no descriptors available)
        init_tx_descriptor(pkt.buffer, pkt.length, pkt.ptp);
    }
}

void network_send_complete_task(void) {
    tx_pkt_ctx_t complete_pkt;
    while (1) {
        // counting semaphore, processes complete transmissions until no more are ready
        c_wait(&eth_tx_semaphore);
        // get pkt ctx for packet that was just transmitted, release DMA descriptor
        fifo_get(tx_pending, &complete_pkt);

        debug_toggle(7);

        // release block back to pool
        pkt_free_tx(complete_pkt.buffer);
        // if ptp, response to calling process with eh timestamp from DMA
        if (complete_pkt.ptp) {
            // TODO get mailbox from pkt ctx,
            // TODO get timestamp from dma descriptor
            // TODO put timestamp in mailbox
        }
        // TODO release DMA descriptor
    }
}

void send_frame(uint8_t *buffer, uint16_t length, uint64_t *timestamp) {
    tx_pkt_ctx_t pkt;
    pkt.buffer = buffer;
    pkt.length = length;
    if (timestamp != NULL) {
        pkt.ptp = true;
        // init mailbox
        // TODO put mailbox in pkt ctx struct
        fifo_put(tx_fifo, &pkt);
        // TODO wait for mailbox to unblock
        // TODO return mailbox value
    } else {
        pkt.ptp = false;
        fifo_put(tx_fifo, &pkt);
    }
    debug_toggle(4);
}

uint16_t build_ipv4_header(uint8_t *buffer, uint32_t dst_ip, uint16_t payload_len, 
                           uint8_t protocol, uint16_t id) {

    uint16_t header_len = sizeof(ipv4_header_t);
    ipv4_header_t *ip = (ipv4_header_t *)(buffer - header_len);

    ip->version_ihl = (4 << 4) | 5; // IPv4, 5*4 = 20 bytes
    ip->tos = 0;
    ip->total_length = htons(header_len + payload_len);
    ip->id = htons(id);
    ip->flags_fragment = 0x0040; // Don't Fragment
    ip->ttl = 64;
    ip->protocol = protocol;
    ip->checksum = 0;
    ip->src_addr = SRC_IPv4_ADDR;
    ip->dst_addr = htonl(dst_ip);
    ip->checksum = checksum16((uint8_t*)ip, header_len);

    return header_len;
}

uint16_t build_icmp_reply(uint8_t *buffer, uint16_t id, uint16_t seq_num, 
                          uint8_t *payload, uint16_t payload_len) {
    uint16_t pkt_len = sizeof(icmp_header_t) + payload_len;
    icmp_header_t *icmp = (icmp_header_t *)(buffer - pkt_len);

    icmp->type = ICMP_ECHO_REPLY;
    icmp->code = 0;
    icmp->checksum = 0;
    icmp->id = htons(id);
    icmp->seq = htons(seq_num);
    memcpy(icmp->data, payload, payload_len);
    icmp->checksum = checksum16((uint8_t*)icmp, pkt_len);

    return pkt_len;
}

uint16_t build_udp_header(uint8_t *buffer, uint16_t src_port, uint16_t dst_port, 
                          uint8_t *data, uint16_t data_len) {
    uint16_t dg_len = data_len + sizeof(udp_header_t);
    udp_header_t *udp = (udp_header_t *)(buffer - dg_len);

    udp->src_port = htons(src_port);
    udp->dst_port = htons(dst_port);
    udp->length = htons(dg_len);
    udp->checksum = 0;
    memcpy(udp->data, data, data_len);

    return dg_len;
}

// compute 16-bit Internet checksum
static uint16_t checksum16(const void *data, uint16_t len) {
    uint32_t sum = 0;
    const uint16_t *ptr = data;

    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }
    if (len > 0)
        sum += *((uint8_t *)ptr);

    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);

    return (uint16_t)(~sum);
}



///////////////////////////////////////////////////////////////////////////////
//// Packet Buffer Management /////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


void pkt_buff_init(void) {
    init_semaphore(&pkt_buff_mutex, 0);

    free_block_root = pkt_buffer_pool[0];
    for (int i = 0; i < PKT_BUFFER_CNT - 1; i++) {
        *(uintptr_t *)pkt_buffer_pool[i] = (uintptr_t)pkt_buffer_pool[i+1];
    }
    *(uintptr_t *)pkt_buffer_pool[PKT_BUFFER_CNT-1] = (uintptr_t)NULL;
}

// allocates a block and returns a pointer to the first byte
uint8_t *pkt_alloc_rx(void) {
    b_wait(&pkt_buff_mutex);

    if (rx_alloc_cnt >= RX_PKT_MAX) {
        b_signal(&pkt_buff_mutex);
        return NULL;
    }

    uint8_t *block = free_block_root;
    free_block_root = (uint8_t *)*(uintptr_t *)free_block_root;
    rx_alloc_cnt += 1;

    b_signal(&pkt_buff_mutex);

    return block;
}

// allocates a block and returns a pointer to the last byte
uint8_t *pkt_alloc_tx(void) {
    b_wait(&pkt_buff_mutex);

    if (tx_alloc_cnt >= TX_PKT_MAX) {
        b_signal(&pkt_buff_mutex);
        return NULL;
    }

    uint8_t *block = free_block_root;
    free_block_root = (uint8_t *)*(uintptr_t *)free_block_root;
    tx_alloc_cnt += 1;

    b_signal(&pkt_buff_mutex);

    if (block)
        return block + (BUFFER_SIZE - 1);
    else
        return NULL;
}

// frees the block at pointer
void pkt_free_rx(uint8_t *ptr) {
    b_wait(&pkt_buff_mutex);

    *(uintptr_t *)ptr = (uintptr_t)free_block_root;
    free_block_root = ptr;
    rx_alloc_cnt -= 1;

    b_signal(&pkt_buff_mutex);
}

// frees the block that contains the supplied pointer
void pkt_free_tx(uint8_t *ptr) {
    b_wait(&pkt_buff_mutex);

    // round ptr address down to the first byte of the block
    ptr = ((ptr - (uint8_t *)pkt_buffer_pool) / BUFFER_SIZE) * BUFFER_SIZE + (uint8_t *)pkt_buffer_pool;
    *(uintptr_t *)ptr = (uintptr_t)free_block_root;
    free_block_root = ptr;
    tx_alloc_cnt -= 1;

    b_signal(&pkt_buff_mutex);
}
