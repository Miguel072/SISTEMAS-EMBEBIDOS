#pragma once
#include <Arduino.h>

// ===================== SENSOR API =====================
// Call once in setup
void Sensors_Init();

// Call often in loop (non-blocking)
void Sensors_Update();

// Getters (cached values)
float Sensors_GetTempC();
float Sensors_GetHum();
int   Sensors_GetLightRaw();
bool  Sensors_IsDark();