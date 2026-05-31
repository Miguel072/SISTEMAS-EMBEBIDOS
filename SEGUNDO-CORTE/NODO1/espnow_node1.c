#include "espnow_node1.h"

#include <string.h>
#include <stdbool.h>

#include "esp_log.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "config.h"

static const char *TAG = "ESPNOW_N1";
static bool s_last_send_ok = false;

static uint8_t s_peer_mac[6] = NODE3_MAC_ADDR;

#if ESPNOW_USE_BROADCAST
static const uint8_t s_broadcast_mac[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
#endif

static void on_data_sent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status)
{
    (void)tx_info;
    s_last_send_ok = (status == ESP_NOW_SEND_SUCCESS);
    ESP_LOGI(TAG, "TX %s", s_last_send_ok ? "OK" : "FAIL");
}

esp_err_t espnow_node1_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

#if NODE1_FORCE_STA_MAC
    uint8_t forced_mac[6] = NODE1_STA_MAC_OVERRIDE;
    esp_err_t mac_ret = esp_wifi_set_mac(WIFI_IF_STA, forced_mac);
    if (mac_ret != ESP_OK) {
        ESP_LOGW(TAG, "Could not force STA MAC: %s", esp_err_to_name(mac_ret));
    } else {
        ESP_LOGI(TAG, "Forced STA MAC for Nodo 3 compatibility: %02X:%02X:%02X:%02X:%02X:%02X",
                 forced_mac[0], forced_mac[1], forced_mac[2], forced_mac[3], forced_mac[4], forced_mac[5]);
    }
#endif

    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));

    uint8_t sta_mac[6] = {0};
    esp_wifi_get_mac(WIFI_IF_STA, sta_mac);
    ESP_LOGI(TAG, "STA MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             sta_mac[0], sta_mac[1], sta_mac[2], sta_mac[3], sta_mac[4], sta_mac[5]);

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(on_data_sent));

    esp_now_peer_info_t peer = {0};
#if ESPNOW_USE_BROADCAST
    memcpy(peer.peer_addr, s_broadcast_mac, 6);
#else
    memcpy(peer.peer_addr, s_peer_mac, 6);
#endif
    peer.channel = ESPNOW_CHANNEL;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;

    ret = esp_now_add_peer(&peer);
    if (ret != ESP_OK && ret != ESP_ERR_ESPNOW_EXIST) {
        ESP_LOGE(TAG, "Cannot add peer: %s", esp_err_to_name(ret));
        return ret;
    }

#if ESPNOW_USE_BROADCAST
    ESP_LOGI(TAG, "ESP-NOW ready. Destination: BROADCAST, channel %d", ESPNOW_CHANNEL);
#else
    ESP_LOGI(TAG, "ESP-NOW ready. Destination: %02X:%02X:%02X:%02X:%02X:%02X, channel %d",
             s_peer_mac[0], s_peer_mac[1], s_peer_mac[2], s_peer_mac[3], s_peer_mac[4], s_peer_mac[5], ESPNOW_CHANNEL);
#endif

    return ESP_OK;
}

esp_err_t espnow_node1_send(const nodo1_packet_t *packet)
{
    if (!packet) {
        return ESP_ERR_INVALID_ARG;
    }

#if ESPNOW_USE_BROADCAST
    return esp_now_send(s_broadcast_mac, (const uint8_t *)packet, sizeof(nodo1_packet_t));
#else
    return esp_now_send(s_peer_mac, (const uint8_t *)packet, sizeof(nodo1_packet_t));
#endif
}

bool espnow_node1_last_send_ok(void)
{
    return s_last_send_ok;
}
