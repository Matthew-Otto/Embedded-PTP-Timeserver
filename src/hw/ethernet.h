
#ifndef ETH_H
#define ETH_H

#include <stdint.h>
#include "mcu.h"
#include "semaphore.h"
#include "fifo.h"

#define RX_DSC_CNT 8
#define TX_DSC_CNT 8

// Bit defines
#define DESC_OWN                 1 << 31

#define DESC_TX_BUFFER2_IOC      1 << 15
#define DESC_TX_BUFFER2_TTSE     1 << 14
#define DESC_TX_FIRST_DESC       1 << 29
#define DESC_TX_LAST_DESC        1 << 28
#define DESC_TX_REPLACE_SRC_ADDR 0b10 << 23

#define DESC_WB_EXT_TS_AVAIL     1 << 14
#define DESC_WB_EXT_TS_DROPPED   1 << 15
#define DESC_WB_STATUS_LAST      1 << 12
#define DESC_WB_LEN_ERROR        1 << 15
#define DESC_CTX_VALID           3 << 30

#define LT_LENGTH     0b000  // The packet is a length packet
#define LT_TYPE       0b001  // The packet is a type packet.
#define LT_ARP        0b011  // The packet is a ARP Request packet type
#define LT_TYP_VLAN   0b100  // The packet is a type packet with VLAN Tag
#define LT_TYP_2VLAN  0b101  // The packet is a type packet with Double VLAN tag
#define LT_MAC        0b110  // The packet is a MAC Control packet type
#define LT_OAM        0b111  // The packet is a OAM packet type

typedef struct {
    volatile uint32_t buffer1_addr;
    volatile uint32_t buffer2_addr;
    volatile uint16_t buffer1_len;
    volatile uint16_t buffer2_len;
    volatile uint32_t ctrl;
} ETH_tx_rd_desc_t;

typedef struct {
    volatile uint32_t timestamp_low;
    volatile uint32_t timestamp_high;
    volatile uint32_t reserved;
    volatile uint32_t status;
} ETH_tx_wb_desc_t;

typedef struct {
    volatile uint32_t timestamp_low;
    volatile uint32_t timestamp_high;
    volatile uint16_t max_seg_size;
    volatile uint16_t inner_vlan_tag;
    volatile uint16_t vlan_tag;
    volatile uint16_t ctrl;
} ETH_tx_ctx_desc_t;

typedef union {
    ETH_tx_rd_desc_t rd;
    ETH_tx_wb_desc_t wb;
    ETH_tx_ctx_desc_t ctx;
} ETH_tx_desc_u;

typedef struct {
    volatile uint32_t buffer1_addr;
    volatile uint32_t reserved1;
    volatile uint32_t buffer2_addr;
    volatile uint8_t  reserved2[3];
    volatile uint8_t  status;
} ETH_rx_rd_desc_t;

typedef struct {
    volatile uint16_t outer_vlan_tag;
    volatile uint16_t inner_vlan_tag;
    volatile uint16_t ext_stat;
    volatile uint16_t mac_ctrl_op;
    volatile uint16_t vlan_filter_status;
    volatile uint16_t mac_filter_stat;
    volatile uint16_t pkt_len;
    volatile uint16_t status;
} ETH_rx_wb_desc_t;

typedef struct {
    volatile uint32_t timestamp_low;
    volatile uint32_t timestamp_high;
    volatile uint32_t _;
    volatile uint32_t ctrl;
} ETH_rx_ctx_desc_t;

typedef union {
    ETH_rx_rd_desc_t rd;
    ETH_rx_wb_desc_t wb;
    ETH_rx_ctx_desc_t ctx;
} ETH_rx_desc_u;


void ETH_IRQHandler(void);
void ETH_receive_frame(uint8_t **frame_ptr, uint64_t *timestamp);

void init_tx_descriptor(uint8_t *buffer, uint16_t length, bool ptp);
uint16_t ETH_build_header(uint8_t *buffer, uint8_t *dst_mac, uint16_t ethertype);
void ETH_update_PTP_TS_oneshot(const int32_t offset_sec, const int32_t offset_nsec);
void ETH_update_PTP_TS_fine(const int32_t new_addend);
void ETH_init(semaphore_t *eth_rx_semaphore, semaphore_t *eth_tx_semaphore);

#endif // ETH_H