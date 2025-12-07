#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "mcu.h"
#include "gpio.h"
#include "semaphore.h"
#include "ethernet.h"
#include "network.h"


const uint8_t MACAddr[6] = {0x00,0x80,0xE1,0x00,0x00,0x00};

#define BUFFER_SIZE 1524
#define RX_DSC_CNT 8
#define TX_DSC_CNT 8

// Global variables
uint32_t rx_timestamp_sec;
uint32_t rx_timestamp_nsec;
uint32_t base_addend;

// Local (static) variables
__attribute__((section(".eth_rx_buffer")))
static uint8_t eth_rx_buffer[RX_DSC_CNT][BUFFER_SIZE];
__attribute__((section(".eth_tx_buffer")))
static uint8_t eth_tx_buffer[TX_DSC_CNT][BUFFER_SIZE];

volatile static ETH_rx_desc_u dma_rx_desc[RX_DSC_CNT];
volatile static ETH_tx_desc_u dma_tx_desc[TX_DSC_CNT];
static uint32_t current_rx_desc_idx = 0;
static uint32_t current_tx_desc_idx = 0;
static uint32_t current_tx_buffer_idx = 0;

static semaphore_t *rx_semaphore;


// Forward declarations
static inline void init_read_descriptor(volatile ETH_rx_rd_desc_t *desc, uint16_t idx);
void ETH_IO_init(void);
void ETH_PHY_init(void);
void ETH_MAC_init(void);
void ETH_DMA_init(void);
void ETH_PTP_init(void);
void ETH_PPS_init(void);
void ETH_int_init(void);


void ETH_IRQHandler(void) {
    uint32_t int_src = READ_REG(ETH->DMAISR);
    uint32_t isr;
    if (int_src & ETH_DMAISR_DMACIS) { // DMA interrupt
        isr = READ_REG(ETH->DMACSR);
        // Receive frame interrupt
        if (isr & ETH_DMACIER_RIE) c_signal(rx_semaphore);
    }
    else if (int_src & ETH_DMAISR_MACIS) { // MTL interrupt
        isr = READ_REG(ETH->MACISR);
    }
    else if (int_src & ETH_DMAISR_MTLIS) { // MAC interrupt
        isr = READ_REG(ETH->MTLISR);
    }

    // clear all DMA interrupt bits
    // The interrupt is cleared only when all the bits of interrupt status register (ETH_DMAISR) are cleared.
    WRITE_REG(ETH->DMACSR, 0xFFFFFFFF);
}


// Checks for valid RX descriptors
// If one exists, process it before resetting DMA descriptor
uint8_t *ETH_receive_frame(){
    ETH_rx_rd_desc_t *rd_desc = &dma_rx_desc[current_rx_desc_idx].rd;
    ETH_rx_wb_desc_t *wb_desc = &dma_rx_desc[current_rx_desc_idx].wb;

    // Check that DMA has released this descriptor
    if (wb_desc->status & (0x1<<15))
        return NULL;

    // Check for timestamp
    /* uint8_t clear_ctx_desc = 0;
    uint16_t ctx_desc_idx = (current_rx_desc_idx + 1) % RX_DSC_CNT;
    ETH_rx_ctx_desc_t *ctx_desc = &dma_rx_desc[ctx_desc_idx].ctx;
    if ((ctx_desc->ctrl >> 30) == 0x1) { // DMA has released own bit and this descriptor is of context type
        rx_timestamp_sec = ctx_desc->timestamp_high;
        rx_timestamp_nsec = ctx_desc->timestamp_low;
        clear_ctx_desc = 1;
    } */

    // If no errors, return pointer to buffer
    if (!(wb_desc->pkt_len & (0x1<<15))){
        return eth_rx_buffer[current_rx_desc_idx];
    }

    // Else if errors, return buffer to DMA
    ETH_free_rx_buffer();

    return NULL;
}


void ETH_free_rx_buffer() {
    ETH_rx_rd_desc_t *rd_desc = &dma_rx_desc[current_rx_desc_idx].rd;
    // Configure descriptor for receive and release back to DMA
    init_read_descriptor(rd_desc, current_rx_desc_idx);
    // Update current RX descriptor idx
    current_rx_desc_idx = (current_rx_desc_idx + 1) % RX_DSC_CNT;

    // Reconfigure context descriptor if it exists
    /* if (clear_ctx_desc) {
        // Configure descriptor for receive and release back to DMA
        init_read_descriptor((ETH_rx_rd_desc_t *)ctx_desc, current_rx_desc_idx);
        // Update current RX descriptor idx
        current_rx_desc_idx = (current_rx_desc_idx + 1) % RX_DSC_CNT;
    } */

    // Update RX descriptor tail pointer;
    WRITE_REG(ETH->DMACRDTPR, (uint32_t)&dma_rx_desc[current_rx_desc_idx]);
}


void ETH_send_frame(uint8_t *buffer, uint16_t length) {
    // Send an Ethernet frame using DMA.

    // Set up packet descriptor
    volatile ETH_tx_rd_desc_t *desc = &dma_tx_desc[current_tx_desc_idx].rd;
    //volatile ETH_tx_wb_desc_t *wb_desc = &dma_tx_desc[current_tx_desc_idx].wb;
   
    desc->buffer1_addr = (uint32_t)buffer;
    desc->buffer1_len = length & 0x3FFF;
    desc->buffer2_len = 0x1 << 14; // enable timestamp
    desc->ctrl = 0;
    desc->ctrl |= 0x1 << 29 | 0x1 << 28; // first and last descriptor
    desc->ctrl |= 0b10 << 23; // src addr insertion
    desc->ctrl |= 0x1 << 31; // set own bit

    // Update current TX descriptor idx
    current_tx_desc_idx = (current_tx_desc_idx + 1) % TX_DSC_CNT;
    // Update TX descriptor tail pointer;
    WRITE_REG(ETH->DMACTDTPR, (uint32_t)&dma_tx_desc[current_tx_desc_idx]);
}


// Blocking, will return a pointer to the end of the first available TX buffer
uint8_t* ETH_get_tx_buffer() {
    // Check if buffer is free (DMA has released the corresponding descriptor)
    volatile ETH_tx_wb_desc_t *wb_desc = &dma_tx_desc[current_tx_desc_idx].wb;
    if (wb_desc->status & (0x1<<31))
        return NULL;

    uint8_t *buffer = (uint8_t *)eth_tx_buffer[current_tx_buffer_idx] + (BUFFER_SIZE - 1);
    current_tx_buffer_idx = (current_tx_buffer_idx + 1) % TX_DSC_CNT;
    return buffer;
}

// Fills in the correct ethernet header and returns the length of the header
uint16_t ETH_build_header(uint8_t *buffer, uint8_t *dst_mac, uint16_t ethertype) {
    uint16_t header_len = sizeof(eth_header_t);
    eth_header_t *frame = (eth_header_t *)(buffer - header_len);

    memcpy(frame->dest, dst_mac, 6);
    memcpy(frame->src, MACAddr, 6);
    frame->ethertype = htons(ethertype);

    return header_len;
}


void ETH_init(semaphore_t *eth_rx_semaphore) {
    rx_semaphore = eth_rx_semaphore;
    ETH_IO_init();
    ETH_PHY_init();
    ETH_MAC_init();
    ETH_DMA_init();
    ETH_int_init();
    ETH_PTP_init();
    ETH_PPS_init();
    
    // Start DMA transmit and receive
    SET_BIT(ETH->DMACTCR, ETH_DMACTCR_ST);
    SET_BIT(ETH->DMACRCR, ETH_DMACRCR_SR);
    
    // Enable MAC transmitter and receiver
    SET_BIT(ETH->MACCR, ETH_MACCR_TE);
    SET_BIT(ETH->MACCR, ETH_MACCR_RE);
}


void ETH_IO_init(void) {
    // Enable clocks
    SET_BIT(RCC->AHB1ENR, RCC_AHB1ENR_ETHEN);
    (void)READ_BIT(RCC->AHB1ENR, RCC_AHB1ENR_ETHEN);
    SET_BIT(RCC->AHB1ENR, RCC_AHB1ENR_ETHTXEN);
    (void)READ_BIT(RCC->AHB1ENR, RCC_AHB1ENR_ETHTXEN);
    SET_BIT(RCC->AHB1ENR, RCC_AHB1ENR_ETHRXEN);
    (void)READ_BIT(RCC->AHB1ENR, RCC_AHB1ENR_ETHRXEN);

    // Configure GPIO pins connecting MCU to Ethernet PHY
    configure_pin(GPIOC, RMII_MDC_Pin, GPIO_MODE_AF_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_HIGH, GPIO_AF11_ETH);
    configure_pin(GPIOC, RMII_RXD0_Pin, GPIO_MODE_AF_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_HIGH, GPIO_AF11_ETH);
    configure_pin(GPIOC, RMII_RXD1_Pin, GPIO_MODE_AF_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_HIGH, GPIO_AF11_ETH);
    configure_pin(GPIOA, RMII_REF_CLK_Pin, GPIO_MODE_AF_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_HIGH, GPIO_AF11_ETH);
    configure_pin(GPIOA, RMII_MDIO_Pin, GPIO_MODE_AF_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_HIGH, GPIO_AF11_ETH);
    configure_pin(GPIOA, RMII_CRS_DV_Pin, GPIO_MODE_AF_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_HIGH, GPIO_AF11_ETH);
    configure_pin(GPIOB, RMII_TXD1_Pin, GPIO_MODE_AF_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_HIGH, GPIO_AF11_ETH);
    configure_pin(GPIOG, RMII_TXT_EN_Pin, GPIO_MODE_AF_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_HIGH, GPIO_AF11_ETH);
    configure_pin(GPIOG, RMI_TXD0_Pin, GPIO_MODE_AF_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_HIGH, GPIO_AF11_ETH);
}


uint16_t read_PHY_reg(uint8_t phy_addr) {
    MODIFY_REG(ETH->MACMDIOAR, ETH_MACMDIOAR_MOC_RD, 0b11 << 2); // read mode
    MODIFY_REG(ETH->MACMDIOAR, ETH_MACMDIOAR_RDA, (phy_addr & 0x1F) << 16); // reg addr
    SET_BIT(ETH->MACMDIOAR, ETH_MACMDIOAR_MB);

    while (READ_BIT(ETH->MACMDIOAR, ETH_MACMDIOAR_MB));
    return (uint16_t)READ_BIT(ETH->MACMDIODR, ETH_MACMDIODR_MD);
}


void write_PHY_reg(uint8_t phy_addr, uint16_t data) {
    MODIFY_REG(ETH->MACMDIOAR, ETH_MACMDIOAR_MOC_RD, 0b01 << 2); // write mode
    MODIFY_REG(ETH->MACMDIODR, ETH_MACMDIODR_MD, data);
    MODIFY_REG(ETH->MACMDIOAR, ETH_MACMDIOAR_RDA, (phy_addr & 0x1F) << 16); // reg addr
    SET_BIT(ETH->MACMDIOAR, ETH_MACMDIOAR_MB);

    while (READ_BIT(ETH->MACMDIOAR, ETH_MACMDIOAR_MB));
}


void ETH_PHY_init(void) {
    // Configure parameters of physical link to LAN8742A-CZ-TR PHY

    // enable SBS clock
    SET_BIT(RCC->APB3ENR, RCC_APB3ENR_SBSEN);
    (void)READ_BIT(RCC->APB3ENR, RCC_APB3ENR_SBSEN);

    // select RMII PHY interface (change from MII)
    MODIFY_REG(SBS->PMCR, SBS_PMCR_ETH_SEL_PHY, (uint32_t)(SBS_ETH_RMII));
    (void)SBS->PMCR; // dummy read to sync with ETH

    // set MDIO clock
    MODIFY_REG(ETH->MACMDIOAR, ETH_MACMDIOAR_CR, ETH_MACMDIOAR_CR_DIV124);
    // Now it is possible to read PHY registers via MDIO with read_PHY_reg()
}


void ETH_MAC_init(void) {
    // Ethernet Software reset
    SET_BIT(ETH->DMAMR, ETH_DMAMR_SWR);
    while (READ_BIT(ETH->DMAMR, ETH_DMAMR_SWR) != 0) {};

    //////// Configure MAC ////////
    // configure MAC address
    WRITE_REG(ETH->MACA0HR, ((uint32_t)MACAddr[5] << 8 | (uint32_t)MACAddr[4]));
    WRITE_REG(ETH->MACA0LR, ((uint32_t)MACAddr[3] << 24 | (uint32_t)MACAddr[2] << 16 | 
                             (uint32_t)MACAddr[1] << 8 | (uint32_t)MACAddr[0]));

    // configure IPv4 address
    WRITE_REG(ETH->MACARPAR, (IPv4_ADDR[0]<<24 | IPv4_ADDR[1]<<16 | IPv4_ADDR[2]<<8 | IPv4_ADDR[3]));

    // Disable MAC address filtering (Promiscuous Mode)
    WRITE_REG(ETH->MACPFR, ETH_MACPFR_PR);

    // operating mode config
    uint32_t cfg = ETH->MACCR;
    cfg |= ETH_MACCR_ARP; // ARP offloading
    cfg |= ETH_MACCR_SARC_REPADDR0; // automatic source MAC address 1 replacement
    cfg |= ETH_MACCR_IPC; // checksum offload
    cfg |= ETH_MACCR_CST; // CRC stripping for Type packets
    cfg |= ETH_MACCR_ACS; // automatic pad/crc stripping
    cfg |= ETH_MACCR_FES; // 100 Mbps
    cfg |= ETH_MACCR_DM; // full duplex
    WRITE_REG(ETH->MACCR, cfg);

    //////// Configure MTL ////////
    SET_BIT(ETH->MTLRQOMR, ETH_MTLRQOMR_DISTCPEF); // don't drop packets that fail CRC
    SET_BIT(ETH->MTLRQOMR, ETH_MTLRQOMR_FEP); // forward error packets
    SET_BIT(ETH->MTLRQOMR, ETH_MTLRQOMR_RSF); // receive queue store and forward (not cut-through)

    WRITE_REG(ETH->MTLTQOMR, 0x2 << 2); // Enable transmit Queue
}


static inline void init_read_descriptor(volatile ETH_rx_rd_desc_t *desc, uint16_t idx) {
    // Configure descriptor for receive and release back to DMA
    desc->buffer1_addr = (uint32_t)eth_rx_buffer[idx];
    desc->status = 0xC1; // set own bit , enable interrupt, and buffer1_valid
}


void ETH_DMA_init(void) {
    // TX descriptors
    // Set Transmit Descriptor Ring Length
    WRITE_REG(ETH->DMACTDRLR, TX_DSC_CNT-1);
    // Set Transmit Descriptor List Address
    WRITE_REG(ETH->DMACTDLAR, (uint32_t)&dma_tx_desc[0]);

    // RX descriptors
    for (int i = 0; i < RX_DSC_CNT; i++) {
        volatile ETH_rx_rd_desc_t *desc = &dma_rx_desc[i].rd;
        init_read_descriptor(desc, i);
    }
    // Set Receive Buffers Length
    MODIFY_REG(ETH->DMACRCR, ETH_DMACRCR_RBSZ, BUFFER_SIZE << 1);
    // Set Receive Descriptor Ring Length
    WRITE_REG(ETH->DMACRDRLR, RX_DSC_CNT-1);
    // Set Receive Descriptor List Address
    WRITE_REG(ETH->DMACRDLAR, (uint32_t)&dma_rx_desc[0]);
    // Set Receive Descriptor Tail pointer Address
    WRITE_REG(ETH->DMACRDTPR, (uint32_t)&dma_rx_desc[RX_DSC_CNT-1]);
}


void ETH_int_init() {
    // Enable ETH DMA interrupts
    uint32_t dma_ints = 0;
    dma_ints |= ETH_DMACIER_NIE;  // Normal DMA ints bulk-enable
    dma_ints |= ETH_DMACIER_RIE;  // Receive interrupts
    dma_ints |= ETH_DMACIER_TIE;  // Transmit interrupts
    SET_BIT(ETH->DMACIER, dma_ints);

    // enable ETH interrupts in NVIC
    NVIC_SetPriority(ETH_IRQn, 3);
    NVIC_EnableIRQ(ETH_IRQn);
}

void ETH_PTP_init() {
    uint32_t cfg;

    // Mask the Timestamp Trigger interrupt
    CLEAR_BIT(ETH->MACIER, ETH_MACIER_TSIE);
    
    // Enable timestamping
    SET_BIT(ETH->MACTSCR, ETH_MACTSCR_TSENA);

    // set subsecond rollover to 0x3B9AC9FF (999999999 nanosec)
    //SET_BIT(ETH->MACTSCR, ETH_MACTSCR_TSCTRLSSR);
    //const int CLK_PERIOD = 4; // clk_ptp_i period (4ns for 250MHz)
    // configure subsecond increment value
    const uint32_t CLK_PERIOD = 21; // (0x7FFFFFFF / 100000000) should count to 2^31 every second
    SET_BIT(ETH->MACSSIR, CLK_PERIOD << ETH_MACMACSSIR_SSINC_Pos);
    // Configure addend value (high precision frequency mult/div)
    base_addend = ((uint64_t)0xFFFFFFFF * (uint64_t)100000000) / (uint64_t)get_clock_speed();
    WRITE_REG(ETH->MACTSAR, base_addend);
    SET_BIT(ETH->MACTSCR, ETH_MACTSCR_TSADDREG);
    while (READ_BIT(ETH->MACTSCR, ETH_MACTSCR_TSADDREG));

    // Set fine update mode
    SET_BIT(ETH->MACTSCR, ETH_MACTSCR_TSCFUPDT);

    // Update system time
    WRITE_REG(ETH->MACSTSUR, 0);
    WRITE_REG(ETH->MACSTNUR, 0);
    SET_BIT(ETH->MACTSCR, ETH_MACTSCR_TSINIT); // initialize timestamp value
    while (READ_BIT(ETH->MACTSCR, ETH_MACTSCR_TSINIT)); // wait for completion

    // Enable PTP offloading features
#define MASTER
#ifdef MASTER
    /* // Automatic PTP Sync messages
    MODIFY_REG(ETH->MACTSCR, ETH_MACTSCR_SNAPTYPSEL_Msk, 0 << ETH_MACTSCR_SNAPTYPSEL_Pos);
    MODIFY_REG(ETH->MACTSCR, ETH_MACTSCR_TSMSTRENA_Msk, 1 << ETH_MACTSCR_TSMSTRENA_Pos);
    MODIFY_REG(ETH->MACTSCR, ETH_MACTSCR_TSEVNTENA_Msk, 1 << ETH_MACTSCR_TSEVNTENA_Pos);
    MODIFY_REG(ETH->MACTSCR, ETH_MACTSCR_TSIPV4ENA_Msk, 1 << ETH_MACTSCR_TSIPV4ENA_Pos);
    SET_BIT(ETH->MACTSCR, ETH_MACTSCR_TSIPENA);
    SET_BIT(ETH->MACTSCR, ETH_MACTSCR_TSVER2ENA);

    // Enable PTP offloading
    cfg = 0;
    cfg |= ETH_MACPOCR_PTOEN;
    cfg |= ETH_MACPOCR_ASYNCEN;
    //cfg |= domain_num << ETH_MACPOCR_DN_Pos;
    WRITE_REG(ETH->MACPOCR, cfg);

    // Configure Source port identity
    WRITE_REG(ETH->MACSPI0R, 0xdeadbeef);
    WRITE_REG(ETH->MACSPI1R, 0xd066f00d);
    WRITE_REG(ETH->MACSPI2R, 0x1234);

    // Set automatic sync message period
    MODIFY_REG(ETH->MACLMIR, ETH_MACLMIR_LSI_Msk, 0 << ETH_MACLMIR_LSI_Pos); */
#else
    // Automatic PTP Sync messages
    MODIFY_REG(ETH->MACTSCR, ETH_MACTSCR_SNAPTYPSEL_Msk, 0 << ETH_MACTSCR_SNAPTYPSEL_Pos);
    MODIFY_REG(ETH->MACTSCR, ETH_MACTSCR_TSMSTRENA_Msk, 0 << ETH_MACTSCR_TSMSTRENA_Pos);
    MODIFY_REG(ETH->MACTSCR, ETH_MACTSCR_TSEVNTENA_Msk, 1 << ETH_MACTSCR_TSEVNTENA_Pos);
    MODIFY_REG(ETH->MACTSCR, ETH_MACTSCR_TSIPV4ENA_Msk, 1 << ETH_MACTSCR_TSIPV4ENA_Pos);
    SET_BIT(ETH->MACTSCR, ETH_MACTSCR_TSIPENA);
    SET_BIT(ETH->MACTSCR, ETH_MACTSCR_TSVER2ENA);

    // Fine correction method
    WRITE_REG(ETH->MACTSAR, 0);
    SET_BIT(ETH->MACTSCR, ETH_MACTSCR_TSADDREG);
    while (READ_BIT(ETH->MACTSCR, ETH_MACTSCR_TSADDREG));
    //SET_BIT(ETH->MACTSCR, ETH_MACTSCR_TSCFUPDT); // use fine correction

    // Enable PTP offloading
    cfg = 0;
    cfg |= ETH_MACPOCR_PTOEN;
    //cfg |= domain_num << ETH_MACPOCR_DN_Pos;
    WRITE_REG(ETH->MACPOCR, cfg);

    // Configure Source port identity
    WRITE_REG(ETH->MACSPI0R, 0xdeadbeef);
    WRITE_REG(ETH->MACSPI1R, 0xd066f00d);
    WRITE_REG(ETH->MACSPI2R, 0x4321);

    // Number of sync messages received before sending DELAY_REQ
    MODIFY_REG(ETH->MACLMIR, ETH_MACLMIR_DRSYNCR_Msk, 0 << ETH_MACLMIR_DRSYNCR_Pos);
#endif

    // Enable timestamp interrupt
    //SET_BIT(ETH->MACIER, ETH_MACIER_TSIE);
}


// Configure Ethernet PPS to PG8
void ETH_PPS_init(void) {
    // configure PPS pin PG8
    configure_pin(GPIOG, GPIO_PIN_8, GPIO_MODE_AF_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_VERY_HIGH, GPIO_AF11_ETH);

    // Exceeding target time (0 unless set) triggers PPS output
    MODIFY_REG(ETH->MACPPSCR, ETH_MACPPSCR_TRGTMODSEL0_Msk, 0x3 << ETH_MACPPSCR_TRGTMODSEL0_Pos);
    // Freq of PPS
    //MODIFY_REG(ETH->MACPPSCR, ETH_MACPPSCR_PPSCTRL_Msk, 0xF << ETH_MACPPSCR_PPSCTRL_Pos);
    MODIFY_REG(ETH->MACPPSCR, ETH_MACPPSCR_PPSCTRL_Msk, 0x1 << ETH_MACPPSCR_PPSCTRL_Pos);
}


void ETH_update_PTP_TS_oneshot(const int32_t offset_sec, const int32_t offset_nsec) {
    WRITE_REG(ETH->MACSTSUR, offset_sec);
    WRITE_REG(ETH->MACSTNUR, offset_nsec);
    SET_BIT(ETH->MACTSCR, ETH_MACTSCR_TSINIT); // initialize timestamp value
    while (READ_BIT(ETH->MACTSCR, ETH_MACTSCR_TSINIT)); // wait for completion
}


void ETH_update_PTP_TS_fine(const int32_t correction) {
    uint32_t new_addend = base_addend + correction;  
    while (READ_BIT(ETH->MACTSCR, ETH_MACTSCR_TSADDREG));
    WRITE_REG(ETH->MACTSAR, new_addend);
    SET_BIT(ETH->MACTSCR, ETH_MACTSCR_TSADDREG);
    while (READ_BIT(ETH->MACTSCR, ETH_MACTSCR_TSADDREG));
}


void ETH_send_timestamp_frame(uint8_t *data, uint16_t length) {
    // Set up context descriptor
    ETH_tx_ctx_desc_t *ctx_desc = &dma_tx_desc[current_tx_desc_idx].ctx;
    ctx_desc->ctrl = 0;
    ctx_desc->ctrl |= (0x1 << 30); // context type
    //ctx_desc->ctrl |= (0x1 << 27); // one-step correction
    ctx_desc->ctrl |= (0x1 << 31);

    // Update current TX descriptor idx
    current_tx_desc_idx = (current_tx_desc_idx + 1) % TX_DSC_CNT;
    // Update TX descriptor tail pointer;
    WRITE_REG(ETH->DMACTDTPR, (uint32_t)&dma_tx_desc[current_tx_desc_idx]);

    ETH_send_frame(data, length);
}
