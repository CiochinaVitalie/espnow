#pragma once
#include "esp_err.h"
#include "sensors_packet.h"

/* Sensor i sits on mux i/8, channel i%8. */

typedef struct {
    uint32_t serial; /* SHT40 serial, 0 if unknown */
    esp_err_t err; /* ESP_OK if this sensor was read successfully */
    float temp_c;
    float rh;
} sensor_data_t;

/* Creates the I2C bus, the two TCA9548A (0x70, 0x71) and the SHT40. */
esp_err_t sensors_init(void);
/* Reads all SENSORS_COUNT sensors in turn; a failing sensor does not stop the others. */
void sensors_read_all(sensor_data_t out[SENSORS_COUNT]);
/* Fills `pkt` from `d`; returns the number of bytes to send. */
size_t sensors_pack(const sensor_data_t d[SENSORS_COUNT], uint16_t seq, sensors_packet_t *pkt);
