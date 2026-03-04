#include <Arduino.h>
#include "AsyncTaskLib.h"
#include "DHT.h"

// ===================== PINES (ESP32) =====================
#define DHTPIN      4
#define DHTTYPE     DHT11

#define LDR_PIN     34      // S -> GPIO34, + -> 3V3, - -> GND

#define RGB_R_PIN   25
#define RGB_G_PIN   26
#define RGB_B_PIN   27

// Si tu RGB es ÁNODO COMÚN pon 1 (invierte PWM)
#define RGB_COMMON_ANODE 0

// PWM (ESP32 core 2.x)
#define PWM_FREQ 5000
#define PWM_RES  8          // 0..255

// Canales PWM (core 2.x)
#define CH_R 0
#define CH_G 1
#define CH_B 2

// ===================== OBJETOS =====================
DHT dht(DHTPIN, DHTTYPE);

// ===================== GLOBALES =====================
float g_tempC = NAN;
float g_hum   = NAN;
int   g_lightPct = -1;      // 0..100
bool  g_alarmEnabled = false;

// Duraciones en microsegundos (micros)
volatile uint32_t g_t1_us = 0, g_t2_us = 0, g_t3_us = 0, g_t4_us = 0, g_t5_on_us = 0, g_t5_off_us = 0;

// ===================== HELPERS =====================
static inline void rgbWrite(uint8_t r, uint8_t g, uint8_t b)
{
#if RGB_COMMON_ANODE
  r = 255 - r; g = 255 - g; b = 255 - b;
#endif
  // ESP32 core 2.x: ledcWrite(channel, duty)
  ledcWrite(CH_R, r);
  ledcWrite(CH_G, g);
  ledcWrite(CH_B, b);
}

static inline int readLightPercent()
{
  int raw = analogRead(LDR_PIN);

  // Si te queda invertido, usa:
  // int pct = map(raw, 4095, 0, 0, 100);

  int pct = map(raw, 0, 4095, 0, 100);
  return constrain(pct, 0, 100);
}

static inline void updateAlarmCondition()
{
  g_alarmEnabled = (!isnan(g_tempC) &&
                    g_lightPct >= 0 &&
                    (g_tempC > 25.0f) &&
                    (g_lightPct < 60));
}

// ===================== CONTEXTO PARA TAREAS =====================
struct Context { };
// FIX: fuerza alineación a 4 bytes para evitar warning del linker
Context ctx __attribute__((aligned(4)));

// Tareas void con parámetro
void taskReadTemp(void* p);
void taskReadLight(void* p);
void taskReadHum(void* p);
void taskSerialDisplay(void* p);
void taskAlarmOn(void* p);
void taskAlarmOff(void* p);

// Wrappers (AsyncTaskLib usa callbacks sin parámetros)
void wTaskReadTemp()      { taskReadTemp(&ctx); }
void wTaskReadLight()     { taskReadLight(&ctx); }
void wTaskReadHum()       { taskReadHum(&ctx); }
void wTaskSerialDisplay() { taskSerialDisplay(&ctx); }
void wTaskAlarmOn()       { taskAlarmOn(&ctx); }
void wTaskAlarmOff()      { taskAlarmOff(&ctx); }

// ===================== ASYNC TASKS (5 tareas) =====================
AsyncTask T1_temp(1500, wTaskReadTemp);        // 1.5 s
AsyncTask T2_light(1000, wTaskReadLight);      // 1.0 s
AsyncTask T3_hum(1800, wTaskReadHum);          // 1.8 s
AsyncTask T4_print(7000, wTaskSerialDisplay);  // 7.0 s

AsyncTask T5_alarmOn(500,  wTaskAlarmOn);      // ON 500 ms
AsyncTask T5_alarmOff(900, wTaskAlarmOff);     // OFF 900 ms

// ===================== IMPLEMENTACIÓN DE TAREAS =====================
void taskReadTemp(void* p)
{
  (void)p;
  uint32_t t0 = micros();

  float t = dht.readTemperature();
  if (!isnan(t)) g_tempC = t;

  updateAlarmCondition();

  g_t1_us = micros() - t0;
  T1_temp.Start();  // vuelve periódica
}

void taskReadLight(void* p)
{
  (void)p;
  uint32_t t0 = micros();

  g_lightPct = readLightPercent();
  updateAlarmCondition();

  g_t2_us = micros() - t0;
  T2_light.Start();
}

void taskReadHum(void* p)
{
  (void)p;
  uint32_t t0 = micros();

  float h = dht.readHumidity();
  if (!isnan(h)) g_hum = h;

  g_t3_us = micros() - t0;
  T3_hum.Start();
}

void taskSerialDisplay(void* p)
{
  (void)p;
  uint32_t t0 = micros();

  Serial.println("------ Estado (cada 7s) ------");
  Serial.print("Temp (C): "); Serial.println(g_tempC, 1);
  Serial.print("Hum  (%): "); Serial.println(g_hum, 1);
  Serial.print("Luz  (%): "); Serial.println(g_lightPct);

  Serial.print("Duraciones (us) | T1:");
  Serial.print(g_t1_us); Serial.print(" T2:");
  Serial.print(g_t2_us); Serial.print(" T3:");
  Serial.print(g_t3_us); Serial.print(" T4:");
  Serial.println(g_t4_us);

  Serial.print("Alarma: ");
  Serial.println(g_alarmEnabled ? "ACTIVA" : "INACTIVA");

  g_t4_us = micros() - t0;
  T4_print.Start();
}

void taskAlarmOn(void* p)
{
  (void)p;
  uint32_t t0 = micros();

  if (g_alarmEnabled)
  {
    rgbWrite(255, 0, 0);   // rojo
    g_t5_on_us = micros() - t0;
    T5_alarmOff.Start();   // encadena OFF
  }
  else
  {
    rgbWrite(0, 0, 0);
    g_t5_on_us = micros() - t0;
  }
}

void taskAlarmOff(void* p)
{
  (void)p;
  uint32_t t0 = micros();

  rgbWrite(0, 0, 0);
  g_t5_off_us = micros() - t0;

  if (g_alarmEnabled)
  {
    T5_alarmOn.Start();
  }
}

// ===================== SETUP / LOOP =====================
void setup()
{
  Serial.begin(115200);
  delay(200);

  analogReadResolution(12);
  dht.begin();

  // ESP32 core 2.x: configurar canales y adjuntar pines
  ledcSetup(CH_R, PWM_FREQ, PWM_RES);
  ledcSetup(CH_G, PWM_FREQ, PWM_RES);
  ledcSetup(CH_B, PWM_FREQ, PWM_RES);

  ledcAttachPin(RGB_R_PIN, CH_R);
  ledcAttachPin(RGB_G_PIN, CH_G);
  ledcAttachPin(RGB_B_PIN, CH_B);

  rgbWrite(0, 0, 0);

  Serial.println("AsyncTaskLib + ESP32 (core 2.0.17) OK");

  T1_temp.Start();
  T2_light.Start();
  T3_hum.Start();
  T4_print.Start();
}

void loop()
{
  // SOLO updates
  T1_temp.Update();
  T2_light.Update();
  T3_hum.Update();
  T4_print.Update();

  T5_alarmOn.Update();
  T5_alarmOff.Update();

  // dispara alarma si se activa y estaba detenida
  if (g_alarmEnabled && !T5_alarmOn.IsActive() && !T5_alarmOff.IsActive())
  {
    T5_alarmOn.Start();
  }
}