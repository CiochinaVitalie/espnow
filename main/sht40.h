#pragma once
#include "driver/i2c_master.h"

#define SHT40_ADDR_DEFAULT 0x44 /* SHT40-AD1B; other variants: 0x45 (BD1B), 0x46 (CD1B) */

typedef struct sht40 *sht40_handle_t;

esp_err_t sht40_init(i2c_master_bus_handle_t bus, uint8_t addr, sht40_handle_t *out);
/* High-precision measurement; temperature in °C, humidity in %RH (0..100). */
esp_err_t sht40_read(sht40_handle_t h, float *temp_c, float *rh);
/* 32-bit unique serial number of the currently reachable sensor. */
esp_err_t sht40_read_serial(sht40_handle_t h, uint32_t *serial);
esp_err_t sht40_deinit(sht40_handle_t h);
