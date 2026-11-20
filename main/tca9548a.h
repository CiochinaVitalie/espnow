#pragma once
#include "driver/i2c_master.h"

#define TCA9548A_ADDR_DEFAULT 0x70 /* A0..A2 = GND; address is 0x70..0x77 */

typedef struct tca9548a *tca9548a_handle_t;

/* Attach the mux at `addr` to an existing I2C master bus. */
esp_err_t tca9548a_init(i2c_master_bus_handle_t bus, uint8_t addr, tca9548a_handle_t *out);
/* Bitmask of enabled channels (bit n = channel n); 0 disconnects all. */
esp_err_t tca9548a_set_mask(tca9548a_handle_t h, uint8_t mask);
/* Enable only channel 0..7. */
esp_err_t tca9548a_select(tca9548a_handle_t h, uint8_t channel);
esp_err_t tca9548a_get_mask(tca9548a_handle_t h, uint8_t *mask);
esp_err_t tca9548a_deinit(tca9548a_handle_t h);
