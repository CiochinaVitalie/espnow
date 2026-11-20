#include "freertos/FreeRTOS.h"
#include <math.h>
#include <stdbool.h>
#include "sensors.h"
#include "tca9548a.h"
#include "sht40.h"

#define MUX_COUNT 2

static const uint8_t s_mux_addr[MUX_COUNT] = { 0x70, 0x71 };
static tca9548a_handle_t s_mux[MUX_COUNT];
/* One handle for all sensors: they share address 0x44 and are told apart by the mux channel. */
static sht40_handle_t s_sht;
static uint32_t s_serial[SENSORS_COUNT]; /* cached; retried while 0 */

esp_err_t sensors_init(void)
{
    i2c_master_bus_config_t bc = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = CONFIG_SENSORS_I2C_SDA,
        .scl_io_num = CONFIG_SENSORS_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus;
    esp_err_t err = i2c_new_master_bus(&bc, &bus);
    if (err != ESP_OK) {
        return err;
    }
    for (int m = 0; m < MUX_COUNT; m++) {
        err = tca9548a_init(bus, s_mux_addr[m], &s_mux[m]);
        if (err != ESP_OK) {
            return err;
        }
        tca9548a_set_mask(s_mux[m], 0);
    }
    /* init does a soft reset, so a sensor must be reachable: use the first channel that answers. */
    for (int i = 0; i < SENSORS_COUNT; i++) {
        tca9548a_handle_t mux = s_mux[i / 8];
        if (tca9548a_select(mux, i % 8) == ESP_OK &&
            sht40_init(bus, SHT40_ADDR_DEFAULT, &s_sht) == ESP_OK) {
            return ESP_OK;
        }
        tca9548a_set_mask(mux, 0);
    }
    return ESP_ERR_NOT_FOUND;
}

void sensors_read_all(sensor_data_t out[SENSORS_COUNT])
{
    for (int i = 0; i < SENSORS_COUNT; i++) {
        int m = i / 8;
        /* Only one channel on the whole bus may be open, or two sensors answer at once. */
        out[i].err = tca9548a_set_mask(s_mux[1 - m], 0);
        if (out[i].err == ESP_OK) {
            out[i].err = tca9548a_select(s_mux[m], i % 8);
        }
        if (out[i].err == ESP_OK && s_serial[i] == 0) {
            sht40_read_serial(s_sht, &s_serial[i]);
        }
        out[i].serial = s_serial[i];
        if (out[i].err == ESP_OK) {
            out[i].err = sht40_read(s_sht, &out[i].temp_c, &out[i].rh);
        }
    }
    tca9548a_set_mask(s_mux[0], 0);
    tca9548a_set_mask(s_mux[1], 0);
}

size_t sensors_pack(const sensor_data_t d[SENSORS_COUNT], uint16_t seq, sensors_packet_t *pkt)
{
    pkt->version = SENSORS_PACKET_VERSION;
    pkt->count = SENSORS_COUNT;
    pkt->seq = seq;
    for (int i = 0; i < SENSORS_COUNT; i++) {
        pkt->items[i].id = d[i].serial;
        pkt->items[i].mux_addr = s_mux_addr[i / 8];
        pkt->items[i].channel = i % 8;
        bool ok = d[i].err == ESP_OK;
        pkt->items[i].temp = ok ? (int16_t)lroundf(d[i].temp_c * 100) : SENSOR_TEMP_ERR;
        pkt->items[i].rh = ok ? (uint16_t)lroundf(d[i].rh * 100) : SENSOR_RH_ERR;
    }
    return SENSORS_PACKET_LEN(SENSORS_COUNT);
}
