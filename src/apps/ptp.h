#ifndef PTP_H
#define PTP_H

#include <stdint.h>
#include "mcu.h"

#define MSGTYPE_SYNC       0
#define MSGTYPE_DELAY_REQ  1
#define MSGTYPE_DELAY_RESP 9


// IEEE 1588 PTPv2 common header
typedef struct {
    uint8_t  msg_type;
    uint8_t  ptp_version;
    uint8_t  msg_len[2];
    uint8_t  domain_num;
    uint8_t  minorSdoId;
    uint8_t  flag_field[2];
    uint8_t  correction_field[8];
    uint32_t  _;
    uint8_t  source_port_id[10];
    uint8_t  seq_id[2];
    uint8_t  ctrl_field;
    int8_t   log_msg_interval;
    uint8_t  timestamp_sec_upper[2];
    uint8_t  timestamp_sec[4];
    uint8_t  timestamp_nsec[4];
} ptp_header_t;


void process_ptp_message(uint8_t *msg);

#endif // PTP_H
