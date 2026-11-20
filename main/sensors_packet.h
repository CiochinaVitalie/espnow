#pragma once
/* Wire format shared by sender and receiver. Little-endian, packed. */
#include <stdint.h>

#define SENSORS_PACKET_VERSION 3
#define SENSORS_COUNT          15
#define SENSOR_TEMP_ERR        INT16_MIN  /* sensor failed to read */
#define SENSOR_RH_ERR          UINT16_MAX

typedef struct __attribute__((packed)) {
    uint32_t id;       /* SHT40 32-bit serial number; 0 = not read yet */
    uint8_t  mux_addr; /* I2C address of the TCA9548A that polled this sensor */
    uint8_t  channel;  /* TCA9548A channel 0..7 */
    int16_t  temp;  /* degC * 100, SENSOR_TEMP_ERR on error */
    uint16_t rh;    /* %RH * 100,  SENSOR_RH_ERR on error */
} sensor_item_t;

typedef struct __attribute__((packed)) {
    uint8_t  version; /* SENSORS_PACKET_VERSION */
    uint8_t  count;   /* number of valid entries in items[] */
    uint16_t seq;     /* increments per packet, detects losses */
    sensor_item_t items[SENSORS_COUNT];
} sensors_packet_t;

/* Number of bytes to send/expect for a packet with `count` items. */
#define SENSORS_PACKET_LEN(count) (4 + (count) * sizeof(sensor_item_t))
