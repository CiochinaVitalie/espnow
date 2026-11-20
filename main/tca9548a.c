#include <stdlib.h>
#include "tca9548a.h"

struct tca9548a {
    i2c_master_dev_handle_t dev;
};

#define TCA_TIMEOUT_MS 100

esp_err_t tca9548a_init(i2c_master_bus_handle_t bus, uint8_t addr, tca9548a_handle_t *out)
{
    if (!bus || !out) {
        return ESP_ERR_INVALID_ARG;
    }
    struct tca9548a *h = calloc(1, sizeof(*h));
    if (!h) {
        return ESP_ERR_NO_MEM;
    }
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = 400000,
    };
    esp_err_t err = i2c_master_bus_add_device(bus, &cfg, &h->dev);
    if (err != ESP_OK) {
        free(h);
        return err;
    }
    *out = h;
    return ESP_OK;
}

esp_err_t tca9548a_set_mask(tca9548a_handle_t h, uint8_t mask)
{
    return i2c_master_transmit(h->dev, &mask, 1, TCA_TIMEOUT_MS);
}

esp_err_t tca9548a_select(tca9548a_handle_t h, uint8_t channel)
{
    if (channel > 7) {
        return ESP_ERR_INVALID_ARG;
    }
    return tca9548a_set_mask(h, 1 << channel);
}

esp_err_t tca9548a_get_mask(tca9548a_handle_t h, uint8_t *mask)
{
    return i2c_master_receive(h->dev, mask, 1, TCA_TIMEOUT_MS);
}

esp_err_t tca9548a_deinit(tca9548a_handle_t h)
{
    esp_err_t err = i2c_master_bus_rm_device(h->dev);
    free(h);
    return err;
}
