#pragma once

// ============================================================
// Configuracion general Nodo 1
// ============================================================

#define NODE_ID                         1
#define DATA_VERSION                    1

// La practica pide actualizar pantalla y enviar cada 10 segundos.
#define SAMPLE_FAST_PERIOD_MS           100
#define DISPLAY_SEND_PERIOD_MS          10000
#define DISPLAY_REFRESH_PERIOD_MS       900

// ============================================================
// ESP-NOW
// ============================================================
// Este canal debe coincidir con el canal WiFi real que imprime el Nodo 3.
// En esta version el Nodo 1 envia por canal 13.
// IMPORTANTE: el Nodo 3 tambien debe estar en canal 13 para recibir.
#define ESPNOW_CHANNEL                  13
#define ESPNOW_USE_BROADCAST            0

// MAC real del Nodo 3 destino.
#define NODE3_MAC_ADDR                  { 0xF4, 0x12, 0xFA, 0x9F, 0x3B, 0x58 }

// ============================================================
// MAC origen del Nodo 1
// ============================================================
// El Nodo 3 que enviaste reconoce Nodo 1 comparando la MAC origen contra:
// 68:25:DD:32:70:CC.
// Para que lo reconozca sin cambiar Nodo 3, esta version fuerza la MAC STA.
// Si prefieres usar la MAC real de la placa, pon esto en 0 y cambia mac_nodo1 en Nodo 3.
#define NODE1_FORCE_STA_MAC             1
#define NODE1_STA_MAC_OVERRIDE          { 0x68, 0x25, 0xDD, 0x32, 0x70, 0xCC }

// ============================================================
// MAX3010X
// ============================================================
#define MAX30100_ENABLE                 1
#define MAX30100_MIN_VALID_BPM          35.0f
#define MAX30100_MAX_VALID_BPM          220.0f

// MAX3010X externo conectado al arnes del Nodo 1.
// Conexion acordada: SDA=GPIO15, SCL=GPIO16.
#define MAX30100_I2C_SPEED_HZ           100000

// Corriente LEDs rojo/IR. 0xFF es fuerte para verificar que el sensor queda activo.
// Si se calienta mucho o satura, bajar a 0x77.
#define MAX30100_LED_CONFIG_VALUE       0xFF

#define MAX30100_WARMUP_MS              700
#define SHOW_MAX30100_ERROR_ON_LCD      1

// ============================================================
// Calibracion empirica SpO2 MAX3010X
// ============================================================
// El sensor se probo con RED=0x05 e IR=0x30. Como el IR queda
// mucho mas fuerte que el rojo, se normaliza la relacion RED/IR
// con las corrientes LED antes de estimar SpO2.
// Si SpO2 sigue pegado en 100, sube SPO2_CAL_SLOPE o baja SPO2_CAL_OFFSET.
// Si SpO2 queda muy bajo, baja SPO2_CAL_SLOPE o sube SPO2_CAL_OFFSET.
#define SPO2_CAL_OFFSET                104.0f
#define SPO2_CAL_SLOPE                 6.0f
#define SPO2_CAL_MIN                   80.0f
#define SPO2_CAL_MAX                   100.0f
#define SPO2_FILTER_ALPHA              0.85f
