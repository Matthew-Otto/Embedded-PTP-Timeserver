#ifndef CONFIG_H
#define CONFIG_H

#ifdef BACKUP
static const uint8_t MAC_ADDR[6]  = {0x00,0x80,0xE1,0x00,0x00,0x02};
static const uint8_t IPv4_ADDR[4] = {10, 0, 123, 2};
static const int32_t GPS_BAUD = 9600;
#else
static const uint8_t MAC_ADDR[6]  = {0x00,0x80,0xE1,0x00,0x00,0x01};
static const uint8_t IPv4_ADDR[4] = {10, 0, 123, 3};
static const int32_t GPS_BAUD = 115200;
#endif


#endif // CONFIG_H