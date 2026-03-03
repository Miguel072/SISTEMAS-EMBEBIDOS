#include "TaskRT.h"
#include "Config.h"
#include "AsyncTaskLib.h"
#include "Sensors.h"

// =====================================================
//                RGB DIGITAL HELPERS
// =====================================================
static inline void RGB_Off()
{
  digitalWrite(RGB_R_PIN, LOW);
  digitalWrite(RGB_G_PIN, LOW);
  digitalWrite(RGB_B_PIN, LOW);
}

static inline void RGB_GreenOn()
{
  digitalWrite(RGB_R_PIN, LOW);
  digitalWrite(RGB_G_PIN, HIGH);
  digitalWrite(RGB_B_PIN, LOW);
}

static inline void RGB_BlueOn()
{
  digitalWrite(RGB_R_PIN, LOW);
  digitalWrite(RGB_G_PIN, LOW);
  digitalWrite(RGB_B_PIN, HIGH);
}

static inline void RGB_RedOn()
{
  digitalWrite(RGB_R_PIN, HIGH);
  digitalWrite(RGB_G_PIN, LOW);
  digitalWrite(RGB_B_PIN, LOW);
}

// =====================================================
//                BLINK FLAGS
// =====================================================
static bool blinkInicio = false;
static bool blinkAlerta = false;
static bool blinkAlarma = false;

// =====================================================
//                BLINK TASKS (CHAINED)
// =====================================================
static AsyncTask tIniOn(INI_ON_MS);
static AsyncTask tIniOff(INI_OFF_MS);

static AsyncTask tAlOn(ALERT_ON_MS);
static AsyncTask tAlOff(ALERT_OFF_MS);

static AsyncTask tArOn(ALARM_ON_MS);
static AsyncTask tArOff(ALARM_OFF_MS);

// =====================================================
//                TIMEOUT TASKS
// =====================================================
static AsyncTask tTempToLuz(T_TEMP_TO_LUZ_MS);
static AsyncTask tTempToHum(T_TEMP_TO_HUM_MS);
static AsyncTask tHumToTemp(T_HUM_TO_TEMP_MS);
static AsyncTask tLuzToTemp(T_LUZ_TO_TEMP_MS);
static AsyncTask tAlertaToTemp(T_ALERTA_TO_TEMP_MS);
static AsyncTask tAlarmaToTemp(T_ALARMA_TOTAL_MS);

// =====================================================
//                TIMEOUT EVENT FLAGS
//   (Fix for missing AsyncTask::IsFinished())
// =====================================================
static bool fTempToLuzDone = false;
static bool fTempToHumDone = false;
static bool fHumToTempDone = false;
static bool fLuzToTempDone = false;
static bool fAlertaToTempDone = false;
static bool fAlarmaToTempDone = false;

// =====================================================
//                DEBUG TASK
// =====================================================
static AsyncTask tDebug(DEBUG_PERIOD_MS);

void Tasks_Init()
{
  // -------------------- INICIO BLINK --------------------
  tIniOn.OnFinish = [](){
    if (!blinkInicio) return;
    RGB_GreenOn();
    tIniOff.Start();
  };
  tIniOff.OnFinish = [](){
    RGB_Off();
    if (blinkInicio) tIniOn.Start();
  };

  // -------------------- ALERTA BLINK --------------------
  tAlOn.OnFinish = [](){
    if (!blinkAlerta) return;
    RGB_BlueOn();
    tAlOff.Start();
  };
  tAlOff.OnFinish = [](){
    RGB_Off();
    if (blinkAlerta) tAlOn.Start();
  };

  // -------------------- ALARMA BLINK --------------------
  tArOn.OnFinish = [](){
    if (!blinkAlarma) return;
    RGB_RedOn();
    tArOff.Start();
  };
  tArOff.OnFinish = [](){
    RGB_Off();
    if (blinkAlarma) tArOn.Start();
  };

  // -------------------- TIMEOUT EVENTS --------------------
  tTempToLuz.OnFinish    = [](){ fTempToLuzDone = true; };
  tTempToHum.OnFinish    = [](){ fTempToHumDone = true; };
  tHumToTemp.OnFinish    = [](){ fHumToTempDone = true; };
  tLuzToTemp.OnFinish    = [](){ fLuzToTempDone = true; };
  tAlertaToTemp.OnFinish = [](){ fAlertaToTempDone = true; };
  tAlarmaToTemp.OnFinish = [](){ fAlarmaToTempDone = true; };

  // -------------------- DEBUG LOOP TASK --------------------
  tDebug.OnFinish = [](){
    Serial.print("[DEBUG] TempC: ");
    float t = Sensors_GetTempC();
    if (isnan(t)) Serial.print("nan"); else Serial.print(t, 1);

    Serial.print(" | Hum: ");
    float h = Sensors_GetHum();
    if (isnan(h)) Serial.print("nan"); else Serial.print(h, 1);

    Serial.print(" | LightRaw: ");
    Serial.print(Sensors_GetLightRaw());

    Serial.print(" | Dark: ");
    Serial.println(Sensors_IsDark() ? "YES" : "NO");

    tDebug.Start();
  };
}

void Tasks_UpdateAll()
{
  // Blink chains
  tIniOn.Update();  tIniOff.Update();
  tAlOn.Update();   tAlOff.Update();
  tArOn.Update();   tArOff.Update();

  // Timeouts
  tTempToLuz.Update();
  tTempToHum.Update();
  tHumToTemp.Update();
  tLuzToTemp.Update();
  tAlertaToTemp.Update();
  tAlarmaToTemp.Update();

  // Debug
  tDebug.Update();
}

// =====================================================
//                    BLINK CONTROL
// =====================================================
void Tasks_StartInicioBlink()
{
  blinkInicio = true; blinkAlerta = false; blinkAlarma = false;
  RGB_Off();
  tIniOn.Start();
}
void Tasks_StopInicioBlink()
{
  blinkInicio = false;
  RGB_Off();
}

void Tasks_StartAlertaBlink()
{
  blinkInicio = false; blinkAlerta = true; blinkAlarma = false;
  RGB_Off();
  tAlOn.Start();
}
void Tasks_StopAlertaBlink()
{
  blinkAlerta = false;
  RGB_Off();
}

void Tasks_StartAlarmaBlink()
{
  blinkInicio = false; blinkAlerta = false; blinkAlarma = true;
  RGB_Off();
  tArOn.Start();
}
void Tasks_StopAlarmaBlink()
{
  blinkAlarma = false;
  RGB_Off();
}

// =====================================================
//                 TIMEOUT START/STOP
// (IMPORTANT: clear flag before Start)
// =====================================================
void Tasks_StartTempToLuz(){ fTempToLuzDone = false; tTempToLuz.Start(); }
void Tasks_StopTempToLuz() { tTempToLuz.Stop(); }

void Tasks_StartTempToHum(){ fTempToHumDone = false; tTempToHum.Start(); }
void Tasks_StopTempToHum() { tTempToHum.Stop(); }

void Tasks_StartHumToTemp(){ fHumToTempDone = false; tHumToTemp.Start(); }
void Tasks_StopHumToTemp() { tHumToTemp.Stop(); }

void Tasks_StartLuzToTemp(){ fLuzToTempDone = false; tLuzToTemp.Start(); }
void Tasks_StopLuzToTemp() { tLuzToTemp.Stop(); }

void Tasks_StartAlertaToTemp(){ fAlertaToTempDone = false; tAlertaToTemp.Start(); }
void Tasks_StopAlertaToTemp() { tAlertaToTemp.Stop(); }

void Tasks_StartAlarmaToTemp(){ fAlarmaToTempDone = false; tAlarmaToTemp.Start(); }
void Tasks_StopAlarmaToTemp() { tAlarmaToTemp.Stop(); }

// =====================================================
//                 TIMEOUT EVENTS (POP FLAGS)
// =====================================================
bool Tasks_PopTempToLuzDone(){ bool v = fTempToLuzDone; fTempToLuzDone = false; return v; }
bool Tasks_PopTempToHumDone(){ bool v = fTempToHumDone; fTempToHumDone = false; return v; }
bool Tasks_PopHumToTempDone(){ bool v = fHumToTempDone; fHumToTempDone = false; return v; }
bool Tasks_PopLuzToTempDone(){ bool v = fLuzToTempDone; fLuzToTempDone = false; return v; }
bool Tasks_PopAlertaToTempDone(){ bool v = fAlertaToTempDone; fAlertaToTempDone = false; return v; }
bool Tasks_PopAlarmaToTempDone(){ bool v = fAlarmaToTempDone; fAlarmaToTempDone = false; return v; }

void Tasks_ClearAllTimeoutFlags()
{
  fTempToLuzDone = false;
  fTempToHumDone = false;
  fHumToTempDone = false;
  fLuzToTempDone = false;
  fAlertaToTempDone = false;
  fAlarmaToTempDone = false;
}

// =====================================================
//                      DEBUG
// =====================================================
void Tasks_StartDebug()
{
  tDebug.Start();
}