#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_mac.h"   // Исправляет ошибку MACSTR
#include "esp_now.h"
#include "esp_log.h"
#include "sensors.h"
#include "sensors_packet.h"

// Роль и MAC приемника задаются в menuconfig: "Sensor node configuration".
#ifdef CONFIG_ESPNOW_ROLE_SENDER
#define ROLE_SENDER
#endif

#ifdef ROLE_SENDER
static const char *TAG = "SENDER";
#else
static const char *TAG = "RECEIVER";
#endif

// Общая функция инициализации Wi-Fi и ESP-NOW
void init_wifi_and_espnow(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_ERROR_CHECK(esp_now_init());
}

#ifdef ROLE_SENDER
// --- ЛОГИКА ПЕРЕДАТЧИКА ---

// Новый формат колбэка отправки для ESP-IDF v5.3+
void on_data_sent(const esp_now_send_info_t *send_info, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
        ESP_LOGI(TAG, "Доставлено успешно на " MACSTR, MAC2STR(send_info->des_addr));
    } else {
        ESP_LOGW(TAG, "Ошибка доставки! (Приемник включен? MAC-адрес верный?)");
    }
}

void app_main(void) {
    uint8_t receiver_mac[6];
    unsigned m[6];
    if (sscanf(CONFIG_ESPNOW_RECEIVER_MAC, "%x:%x:%x:%x:%x:%x", &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]) != 6) {
        ESP_LOGE(TAG, "Неверный формат CONFIG_ESPNOW_RECEIVER_MAC: \"%s\"", CONFIG_ESPNOW_RECEIVER_MAC);
        return;
    }
    for (int i = 0; i < 6; i++) {
        receiver_mac[i] = (uint8_t)m[i];
    }

    init_wifi_and_espnow();
    ESP_LOGI(TAG, "Запуск в режиме ПЕРЕДАТЧИКА");

    // Регистрируем колбэк отправки
    esp_now_register_send_cb(on_data_sent);

    // Настраиваем структуру пира (приемника)
    esp_now_peer_info_t peer_info = {};
    memcpy(peer_info.peer_addr, receiver_mac, 6);
    peer_info.channel = 0; // 0 = текущий канал Wi-Fi
    peer_info.encrypt = false;

    ESP_ERROR_CHECK(esp_now_add_peer(&peer_info)); // при ошибке перезагрузка, а не молчаливый выход

    // Не перезагружаемся, если мультиплексор/датчики не найдены: повторяем попытку.
    esp_err_t err;
    while ((err = sensors_init()) != ESP_OK) {
        ESP_LOGE(TAG, "Инициализация датчиков не удалась: %s. Повтор через 5 с", esp_err_to_name(err));
        vTaskDelay(pdMS_TO_TICKS(5000));
    }

    uint16_t seq = 0;
    sensor_data_t data[SENSORS_COUNT];
    sensors_packet_t pkt;

    while (1) {
        sensors_read_all(data);
        size_t len = sensors_pack(data, seq++, &pkt);

        if (esp_now_send(receiver_mac, (uint8_t *)&pkt, len) == ESP_OK) {
            ESP_LOGI(TAG, "Пакет #%u отправлен (%u байт)", pkt.seq, (unsigned)len);
        } else {
            ESP_LOGE(TAG, "Ошибка вызова esp_now_send");
        }

        vTaskDelay(pdMS_TO_TICKS(CONFIG_SENSORS_POLL_PERIOD_MS));
    }
}

#else
// --- ЛОГИКА ПРИЕМНИКА ---

// Колбэк принимающего устройства для ESP-IDF v5.3+
void on_data_recv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    const sensors_packet_t *pkt = (const sensors_packet_t *)data;

    if (len < 4 || pkt->version != SENSORS_PACKET_VERSION || pkt->count > SENSORS_COUNT ||
        len != (int)SENSORS_PACKET_LEN(pkt->count)) {
        ESP_LOGW(TAG, "Неверный пакет от " MACSTR ", %d байт", MAC2STR(recv_info->src_addr), len);
        return;
    }
    ESP_LOGI(TAG, "Пакет #%u от " MACSTR ", датчиков: %u", pkt->seq, MAC2STR(recv_info->src_addr), pkt->count);
    for (int i = 0; i < pkt->count; i++) {
        const sensor_item_t *it = &pkt->items[i];
        if (it->temp == SENSOR_TEMP_ERR) {
            ESP_LOGW(TAG, "ID %08" PRIX32 " (mux 0x%02X, ch %u): ошибка датчика", it->id, it->mux_addr, it->channel);
        } else if (it->rh == SENSOR_RH_ERR) {
            ESP_LOGW(TAG, "ID %08" PRIX32 " (mux 0x%02X, ch %u): ошибка датчика влажности", it->id, it->mux_addr, it->channel);
        } else {
            ESP_LOGI(TAG, "ID %08" PRIX32 " (mux 0x%02X, ch %u): %.2f C, %.2f %%", it->id, it->mux_addr, it->channel, it->temp / 100.0f, it->rh / 100.0f);
        }
    }
}

void app_main(void) {
    init_wifi_and_espnow();
    ESP_LOGI(TAG, "Запуск в режиме ПРИЕМНИКА");

    // Выводим в лог собственный MAC-адрес платы
    uint8_t my_mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, my_mac);
    ESP_LOGW(TAG, "СКОПИРУЙТЕ ЭТОТ MAC В КОД ПЕРЕДАТЧИКА: " MACSTR, MAC2STR(my_mac));

    // Регистрируем колбэк приема
    esp_now_register_recv_cb(on_data_recv);

    ESP_LOGI(TAG, "Ожидание данных...");
    while (1) {
        vTaskDelay(portMAX_DELAY);
    }
}
#endif
