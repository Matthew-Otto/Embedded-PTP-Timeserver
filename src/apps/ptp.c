// IEEE 1588 Precision Time Protocol App

#include <stdint.h>
#include <stdlib.h>
#include "mcu.h"
#include "ethernet.h"
#include "ip.h"
#include "ptp.h"
#include "time.h"


static uint8_t sync_valid = 0;
static uint8_t delay_valid = 0;

static uint64_t last_master_time = 0;
static uint64_t last_slave_time = 0;
static int32_t    last_freq_comp = 1;

static int32_t sync_orig_sec;
static int32_t sync_orig_nsec;
static int32_t sync_recv_sec;
static int32_t sync_recv_nsec;
static int32_t delay_orig_sec;
static int32_t delay_orig_nsec;
static int32_t delay_recv_sec;
static int32_t delay_recv_nsec;

void process_ptp_message(uint8_t *buffer) {
    ptp_header_t *msg = (ptp_header_t *)buffer;
    ETH_TypeDef *eth = ETH;

    if (msg->msg_type == MSGTYPE_SYNC) {
        sync_orig_sec = pack4byte(msg->timestamp_sec);
        sync_orig_nsec = pack4byte(msg->timestamp_nsec);
        sync_recv_sec = rx_timestamp_sec;
        sync_recv_nsec = rx_timestamp_nsec;
        sync_valid = 1;
    } else if (msg->msg_type == MSGTYPE_DELAY_RESP) {
        delay_orig_sec = READ_REG(eth->MACTTSSSR);
        delay_orig_nsec = READ_REG(eth->MACTTSSNR);
        delay_recv_sec = pack4byte(msg->timestamp_sec);
        delay_recv_nsec = pack4byte(msg->timestamp_nsec);
        delay_valid = 1;
    }

    // Update clock
    if (sync_valid && delay_valid) {
        volatile uint64_t master_time;
        volatile uint64_t slave_time;
        int32_t offset_sec = ((delay_recv_sec - delay_orig_sec) - (sync_recv_sec - sync_orig_sec)) / 2;
        int32_t offset_nsec = ((delay_recv_nsec - delay_orig_nsec) - (sync_recv_nsec - sync_orig_nsec)) / 2;
        
        // Set coarse correction
        if (abs(offset_sec) > 2) {
            ETH_update_PTP_TS_coarse(offset_sec, offset_nsec);
            TIME_update(offset_nsec);
        }
    
        // compensate for drift (very buggy, nonfunctional)
        else if (last_master_time || last_slave_time) {
            master_time = sync_orig_sec * 1e9 + sync_orig_nsec;
            slave_time = sync_recv_sec * 1e9 + sync_recv_nsec;

            volatile uint32_t master_delta = master_time - last_master_time;
            volatile uint32_t slave_delta = slave_time - last_slave_time;
            
            volatile int64_t clock_dif = master_time - slave_time;

            volatile int32_t freq_scale_factor = (master_delta + clock_dif) / slave_delta;
            int32_t freq_comp = freq_scale_factor * last_freq_comp;
            last_freq_comp = freq_comp;
            ETH_update_PTP_drift_comp(freq_comp);
        }

        last_master_time = master_time;
        last_slave_time = slave_time;

        sync_valid = 0;
        delay_valid = 0;
    }
}
