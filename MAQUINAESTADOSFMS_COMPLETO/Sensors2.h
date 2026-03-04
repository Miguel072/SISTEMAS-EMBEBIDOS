#pragma once
#include <Arduino.h>

// =====================================================
//                     SENSOR MODULE
// =====================================================
// - Non-blocking update
// - DHT reads throttled (1s)
// - Light reads fast
// - Flame uses DO digital pin
// =====================================================

void Sensors_Init();
void Sensors_Update();

float Sensors_GetTempC();
float Sensors_GetHum();
int   Sensors_GetLightRaw();
bool  Sensors_IsDark();

// Flame controller (gating)
bool  Sensors_FlameActive();