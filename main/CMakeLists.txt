idf_component_register(SRCS "espnow_example_main.c" "tca9548a.c" "sht40.c" "sensors.c"
                    PRIV_REQUIRES esp_driver_i2c nvs_flash esp_event esp_netif esp_wifi
                    INCLUDE_DIRS ".")
