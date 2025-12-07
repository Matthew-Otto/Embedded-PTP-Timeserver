#ifndef TIMESERVER_H
#define TIMESERVER_H

#include <stdint.h>

#define NTP_TIMESTAMP_DELTA 0x83aa7e8000000000  // Seconds between 1900 and 1970 (left shift 32)

typedef struct {
    uint8_t  li_vn_mode;      // Leap Indicator, Version, Mode
    uint8_t  stratum;         // Indicates the distance from the reference clock
    uint8_t  poll;            // Poll interval
    uint8_t  precision;       // Signed log₂(seconds) of system clock precision (e.g., –18 ≈ 1 microsecond).
    uint32_t root_delay;      // Total round-trip delay to the reference clock, in NTP short format.
    uint32_t root_dispersion; // Total dispersion to the reference clock, in NTP short format.
    uint32_t ref_id;          // Identifies the specific server or reference clock; interpretation depends on Stratum.
    uint64_t ref_ts;          // Time when the system clock was last set or corrected, in NTP timestamp format.
    uint64_t orig_ts;         // Time at the client when the request departed, in NTP timestamp format.
    uint64_t rx_ts;           // The local time, in timestamp format, when the latest NTP message arrived.
    uint64_t tx_ts;           // Time at the server when the response left, in NTP timestamp format.
} ntp_packet_t;

void timeserver(void);
uint64_t get_time(void);
uint64_t get_ntp_time(void);

#endif // TIMESERVER_H