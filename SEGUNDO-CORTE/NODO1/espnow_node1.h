#pragma once

#include "esp_err.h"
#include "node1_packet.h"

esp_err_t espnow_node1_init(void);
esp_err_t espnow_node1_send(const nodo1_packet_t *packet);
bool espnow_node1_last_send_ok(void);
