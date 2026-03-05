#include "Sensors2.h"
#include "Config2.h"
#include <DHT.h>

// =====================================================
//                      DHT OBJECT
// =====================================================
static DHT dht(DHTPIN, DHT11);

// =====================================================
//                     CACHED VALUES
// =====================================================
static float g_tempC = NAN;
static float g_hum   = NAN;

static int   g_lightRaw = 0;
static bool  g_isDark   = false;

static bool  g_flameActive = false;

static unsigned long g_lastDhtMs = 0;

// =====================================================
//               FLAME DIGITAL READ HELPER
// =====================================================
static inline bool readFlameDO()
{
  int raw = digitalRead(FLAME_DO_PIN);

#if FLAME_ACTIVE_LOW
  return (raw == LOW);
#else
  return (raw == HIGH);
#endif
}

void Sensors_Init()
{
  dht.begin();
  analogReadResolution(12);

  // Flame DO pin: safe pullup
  pinMode(FLAME_DO_PIN, INPUT_PULLUP);
}

void Sensors_Update()
{
  // -------------------- LIGHT (FAST) --------------------
  g_lightRaw = analogRead(LDR_PIN);
  g_isDark = (g_lightRaw < LIGHT_RAW_TH);

  // -------------------- FLAME (FAST) --------------------
  g_flameActive = readFlameDO();

  // -------------------- DHT (SLOW) ----------------------
  unsigned long now = millis();
  if (now - g_lastDhtMs < DHT_READ_PERIOD_MS) return;
  g_lastDhtMs = now;

  float t = dht.readTemperature();
  float h = dht.readHumidity();

  // Update only if valid
  if (!isnan(t)) g_tempC = t;
  if (!isnan(h)) g_hum   = h;
}

float Sensors_GetTempC()    { return g_tempC; }
float Sensors_GetHum()      { return g_hum; }
int   Sensors_GetLightRaw() { return g_lightRaw; }
bool  Sensors_IsDark()      { return g_isDark; }
bool  Sensors_FlameActive() { return g_flameActive; }