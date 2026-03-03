#include "Sensors.h"
#include "Config.h"
#include <DHT.h>

// DHT object (C++ -> must be in .cpp, not .c)
static DHT dht(DHTPIN, DHT11);

static float g_tempC = NAN;
static float g_hum   = NAN;
static int   g_lightRaw = 0;
static bool  g_isDark = false;

static unsigned long g_lastDhtMs = 0;

void Sensors_Init()
{
  dht.begin();
  analogReadResolution(12);
}

void Sensors_Update()
{
  // Light is cheap -> read always
  g_lightRaw = analogRead(LDR_PIN);
  g_isDark = (g_lightRaw < LIGHT_RAW_TH);

  // DHT is slow -> read safely every 1s
  unsigned long now = millis();
  if (now - g_lastDhtMs < DHT_READ_PERIOD_MS) return;
  g_lastDhtMs = now;

  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (!isnan(t)) g_tempC = t;
  if (!isnan(h)) g_hum = h;
}

float Sensors_GetTempC()    { return g_tempC; }
float Sensors_GetHum()      { return g_hum; }
int   Sensors_GetLightRaw() { return g_lightRaw; }
bool  Sensors_IsDark()      { return g_isDark; }