#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "xd58c.h"

static const char *TAG = "NODE2_HR";
static xd58c_handle_t s_sensor = {0};

#define ESPNOW_CHANNEL 1
#define HEART_PACKET_VERSION 1

typedef struct __attribute__((packed)) {
    uint8_t version;
    uint8_t node_id;
    uint8_t finger_ok;
    uint8_t beat_flag;
    uint8_t quality;
    uint16_t bpm_x10;
    uint16_t ibi_ms;
    uint16_t raw_signal;
    uint32_t timestamp_ms;
} heart_packet_t;

static uint8_t s_broadcast_mac[ESP_NOW_ETH_ALEN] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

static bool bpm_is_valid(const xd58c_vitals_t *v)
{
    return v->finger_ok && (v->bpm >= XD58C_BPM_MIN) && (v->bpm <= XD58C_BPM_MAX);
}

static void wifi_init_for_espnow(void)
{
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
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    uint8_t mac[6] = {0};
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, mac));
    ESP_LOGI(TAG, "MAC nodo 2: %02X:%02X:%02X:%02X:%02X:%02X | Canal ESP-NOW=%d",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], ESPNOW_CHANNEL);
}

static void espnow_send_cb(const wifi_tx_info_t *tx_info, esp_now_send_status_t status)
{
    (void)tx_info;
    if (status != ESP_NOW_SEND_SUCCESS) {
        ESP_LOGW(TAG, "ESP-NOW send fallo");
    }
}

static void espnow_init_sender(void)
{
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));

    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, s_broadcast_mac, ESP_NOW_ETH_ALEN);
    peer.channel = ESPNOW_CHANNEL;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));

    ESP_LOGI(TAG, "ESP-NOW sender listo (broadcast)");
}

static void send_heart_packet(const xd58c_vitals_t *v)
{
    heart_packet_t pkt = {
        .version = HEART_PACKET_VERSION,
        .node_id = 2,
        .finger_ok = v->finger_ok ? 1 : 0,
        .beat_flag = v->beat_flag ? 1 : 0,
        .quality = v->bpm_quality,
        .bpm_x10 = (uint16_t)((v->bpm < 0.0f) ? 0 : (v->bpm * 10.0f + 0.5f)),
        .ibi_ms = (uint16_t)((v->ibi_ms > 65535U) ? 65535U : v->ibi_ms),
        .raw_signal = (uint16_t)((v->raw_signal < 0) ? 0 : (v->raw_signal > 4095 ? 4095 : v->raw_signal)),
        .timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000ULL),
    };
    esp_err_t ret = esp_now_send(s_broadcast_mac, (const uint8_t *)&pkt, sizeof(pkt));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Error enviando ESP-NOW: %s", esp_err_to_name(ret));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "=============================================");
    ESP_LOGI(TAG, "Nodo 2 - XD-58C SOLO PULSO + ESP-NOW");
    ESP_LOGI(TAG, "Conecte el sensor: VCC->3V3, GND->GND, SIG->GPIO34");
    ESP_LOGI(TAG, "Nodo 2 envia BPM al Nodo 1 por ESP-NOW");
    ESP_LOGI(TAG, "=============================================");

    wifi_init_for_espnow();
    espnow_init_sender();
    ESP_ERROR_CHECK(xd58c_init(&s_sensor));

    xd58c_vitals_t vitals = {0};
    uint32_t sample_count = 0;

    while (1) {
        xd58c_get_vitals(&s_sensor, &vitals);
        send_heart_packet(&vitals);

        if (bpm_is_valid(&vitals)) {
            ESP_LOGI(TAG,
                     "PULSO | BPM=%.1f | IBI=%" PRIu32 " ms | RAW=%d | Q=%u | DEDO=SI | LATIDO=%s | ESPNOW=OK",
                     vitals.bpm,
                     vitals.ibi_ms,
                     vitals.raw_signal,
                     vitals.bpm_quality,
                     vitals.beat_flag ? "SI" : "NO");
        } else {
            if ((sample_count % 4) == 0) {
                ESP_LOGI(TAG,
                         "PULSO | Esperando senal valida | RAW=%d | DEDO=%s | BPM=%.1f | ESPNOW=OK",
                         vitals.raw_signal,
                         vitals.finger_ok ? "SI" : "NO",
                         vitals.bpm);
            }
        }

        sample_count++;
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}
