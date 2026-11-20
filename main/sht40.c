#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sht40.h"

#define SHT40_CMD_MEASURE_HIGH 0xFD
#define SHT40_CMD_SERIAL       0x89
#define SHT40_CMD_SOFT_RESET   0x94
#define SHT40_TIMEOUT_MS       100

struct sht40 {
    i2c_master_dev_handle_t dev;
};

/* CRC-8, poly 0x31, init 0xFF (Sensirion). */
static uint8_t crc8(const uint8_t *d, int n)
{
    uint8_t crc = 0xFF;
    while (n--) {
        crc ^= *d++;
        for (int i = 0; i < 8; i++) {
            crc = (crc & 0x80) ? (crc << 1) ^ 0x31 : crc << 1;
        }
    }
    return crc;
}

esp_err_t sht40_init(i2c_master_bus_handle_t bus, uint8_t addr, sht40_handle_t *out)
{
    if (!bus || !out) {
        return ESP_ERR_INVALID_ARG;
    }
    struct sht40 *h = calloc(1, sizeof(*h));
    if (!h) {
        return ESP_ERR_NO_MEM;
    }
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = 400000,
    };
    esp_err_t err = i2c_master_bus_add_device(bus, &cfg, &h->dev);
    if (err == ESP_OK) {
        uint8_t cmd = SHT40_CMD_SOFT_RESET;
        err = i2c_master_transmit(h->dev, &cmd, 1, SHT40_TIMEOUT_MS);
        vTaskDelay(pdMS_TO_TICKS(2)); /* reset takes <1 ms */
        if (err != ESP_OK) {
            i2c_master_bus_rm_device(h->dev);
        }
    }
    if (err != ESP_OK) {
        free(h);
        return err;
    }
    *out = h;
    return ESP_OK;
}

esp_err_t sht40_read(sht40_handle_t h, float *temp_c, float *rh)
{
    uint8_t cmd = SHT40_CMD_MEASURE_HIGH, buf[6];
    esp_err_t err = i2c_master_transmit(h->dev, &cmd, 1, SHT40_TIMEOUT_MS);
    if (err != ESP_OK) {
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(10)); /* high precision: max 8.3 ms */
    err = i2c_master_receive(h->dev, buf, sizeof(buf), SHT40_TIMEOUT_MS);
    if (err != ESP_OK) {
        return err;
    }
    if (crc8(buf, 2) != buf[2] || crc8(buf + 3, 2) != buf[5]) {
        return ESP_ERR_INVALID_CRC;
    }
    float t = -45.0f + 175.0f * ((buf[0] << 8) | buf[1]) / 65535.0f;
    float r = -6.0f + 125.0f * ((buf[3] << 8) | buf[4]) / 65535.0f;
    if (temp_c) *temp_c = t;
    if (rh) *rh = r < 0 ? 0 : (r > 100 ? 100 : r);
    return ESP_OK;
}

esp_err_t sht40_read_serial(sht40_handle_t h, uint32_t *serial)
{
    uint8_t cmd = SHT40_CMD_SERIAL, buf[6];
    esp_err_t err = i2c_master_transmit(h->dev, &cmd, 1, SHT40_TIMEOUT_MS);
    if (err != ESP_OK) {
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(2));
    err = i2c_master_receive(h->dev, buf, sizeof(buf), SHT40_TIMEOUT_MS);
    if (err != ESP_OK) {
        return err;
    }
    if (crc8(buf, 2) != buf[2] || crc8(buf + 3, 2) != buf[5]) {
        return ESP_ERR_INVALID_CRC;
    }
    *serial = (uint32_t)buf[0] << 24 | buf[1] << 16 | buf[3] << 8 | buf[4];
    return ESP_OK;
}

esp_err_t sht40_deinit(sht40_handle_t h)
{
    esp_err_t err = i2c_master_bus_rm_device(h->dev);
    free(h);
    return err;
}
