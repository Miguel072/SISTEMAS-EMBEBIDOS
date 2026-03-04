#pragma once
#include <Arduino.h>

// =====================================================
//                 PIN MAPPING (ESP32)
// =====================================================
#define DHTPIN      4
#define LDR_PIN     34
#define BTN_PIN     23

#define RGB_R_PIN   25
#define RGB_G_PIN   26
#define RGB_B_PIN   27

// -------------------- FLAME SENSOR --------------------
// Use DO pin (digital output)
#define FLAME_DO_PIN  19

// Most flame modules output LOW when flame is detected
#define FLAME_ACTIVE_LOW  1

// =====================================================
//                 THRESHOLDS / RULES
// =====================================================
#define TEMP_ALERT_C     25.0
#define TEMP_ALARM_C     30.0
#define LIGHT_RAW_TH     50   // dark if raw < 500

// =====================================================
//              SAFE SENSOR READ PERIODS
// =====================================================
#define DHT_READ_PERIOD_MS   1000

// =====================================================
//                 DIAGRAM TIMEOUTS
// =====================================================
#define T_TEMP_TO_LUZ_MS     2000
#define T_TEMP_TO_HUM_MS     4000
#define T_HUM_TO_TEMP_MS     3000
#define T_LUZ_TO_TEMP_MS     5000
#define T_ALERTA_TO_TEMP_MS  3000
#define T_ALARMA_TOTAL_MS    5000

// =====================================================
//                 BLINK PATTERNS
// =====================================================
#define INI_ON_MS      200
#define INI_OFF_MS     200

#define ALERT_ON_MS    500
#define ALERT_OFF_MS   900

#define ALARM_ON_MS    100
#define ALARM_OFF_MS   300

// =====================================================
//                 DEBUG
// =====================================================
#define DEBUG_PERIOD_MS 1000