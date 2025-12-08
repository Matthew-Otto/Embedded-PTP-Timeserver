
#ifndef ETH_H
#define ETH_H

#include <stdint.h>
#include "mcu.h"
#include "semaphore.h"

extern const uint8_t MACAddr[6];

#define DESC_WB_EXT_TS_AVAIL   (0x1 << 14)
#define DESC_WB_EXT_TS_DROPPED (0x1 << 15)
#define DESC_WB_STATUS_LAST    (0x1 << 12)
#define DESC_WB_LEN_ERROR      (0x1 << 15)
#define DESC_CTX_VALID         (0x3 << 30)

typedef struct pkt_buffer_node {
    uint8_t *buffer_ptr;
    struct pkt_buffer_node *next_node;
} pkt_buffer_node;

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
int ETH_receive_frame(uint8_t **frame_ptr, uint64_t *timestamp);

void ETH_send_frame(uint8_t *data, uint16_t length);
uint8_t* ETH_get_tx_buffer();
uint16_t ETH_build_header(uint8_t *buffer, uint8_t *dst_mac, uint16_t ethertype);
void ETH_update_PTP_TS_oneshot(const int32_t offset_sec, const int32_t offset_nsec);
void ETH_update_PTP_TS_fine(const int32_t new_addend);
void ETH_init(semaphore_t *eth_rx_semaphore);

uint8_t *ETH_alloc_pkt_buff(void);
void ETH_free_pkt_buff(uint8_t *ptr);

#endif // ETH_H