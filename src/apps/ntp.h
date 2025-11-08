#ifndef NTP_H
#define NTP_H

#include <stdint.h>
#include "ethernet.h"
#include "ip.h"


#define NTP_TIMESTAMP_DELTA 2208988800UL  // Seconds between 1900 and 1970

typedef struct {
    uint8_t  src_mac[6];
    uint32_t src_ip;
    uint16_t src_port;
    uint32_t orig_ts_sec;
    uint32_t orig_ts_frac;
    uint32_t recv_ts_sec;
    uint32_t recv_ts_frac; 
} ntp_req_t;

typedef struct {
    uint8_t  li_vn_mode;      // Leap Indicator, Version, Mode
    uint8_t  stratum;         // Indicates the distance from the reference clock
    uint8_t  poll;            // Poll interval
    uint8_t  precision;       // Signed log₂(seconds) of system clock precision (e.g., –18 ≈ 1 microsecond).
    uint32_t root_delay;      // Total round-trip delay to the reference clock, in NTP short format.
    uint32_t root_dispersion; // Total dispersion to the reference clock, in NTP short format.
    uint32_t ref_id;          // Identifies the specific server or reference clock; interpretation depends on Stratum.
    uint32_t ref_ts_sec;      // Time when the system clock was last set or corrected, in NTP timestamp format.
    uint32_t ref_ts_frac;     // Time when the system clock was last set or corrected, in NTP timestamp format.
    uint32_t orig_ts_sec;     // Time at the client when the request departed, in NTP timestamp format.
    uint32_t orig_ts_frac;    // Time at the client when the request departed, in NTP timestamp format.
    uint32_t recv_ts_sec;     // The local time, in timestamp format, when the latest NTP message arrived.
    uint32_t recv_ts_frac;    // The local time, in timestamp format, when the latest NTP message arrived.
    uint32_t tx_ts_sec;       // Time at the server when the response left, in NTP timestamp format.
    uint32_t tx_ts_frac;      // Time at the server when the response left, in NTP timestamp format.
} ntp_packet_t;

void receive_ntp_req(ntp_packet_t*, udp_header_t*, ipv4_header_t*, eth_header_t*);
void ntp_process(void);

#endif // NTP_H
