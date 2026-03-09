/**
 * @file TaskRT2.cpp
 * @brief Implementación del módulo de tareas temporizadas cooperativas.
 *
 * @details
 * Este módulo gestiona:
 *
 * - Parpadeos del LED RGB (Inicio, Alerta, Alarma)
 * - Timeouts de transición entre estados de la FSM
 * - Tarea periódica de monitoreo por Serial
 *
 * Utiliza AsyncTaskLib para ejecutar tareas no bloqueantes
 * basadas en temporización por millis().
 *
 * Arquitectura:
 *  - Blink → tareas encadenadas ON/OFF
 *  - Timeouts → tareas one-shot que activan flags
 *  - Display → tarea periódica rearmable
 *
 * Este módulo NO toma decisiones.
 * Solo genera eventos temporales para la FSM.
 */

#include "TaskRT2.h"
#include "Config2.h"
#include "AsyncTaskLib.h"
#include "Sensors2.h"
#include "FmsApp2.h"


/* =====================================================
   RGB HELPERS (BAJO NIVEL)
   ===================================================== */

/**
 * @brief Apaga completamente el LED RGB.
 */
static inline void RGB_Off()
{
  digitalWrite(RGB_R_PIN, LOW);
  digitalWrite(RGB_G_PIN, LOW);
  digitalWrite(RGB_B_PIN, LOW);
}

/**
 * @brief Activa color verde (Estado INICIO).
 */
static inline void RGB_GreenOn()
{
  digitalWrite(RGB_R_PIN, LOW);
  digitalWrite(RGB_G_PIN, HIGH);
  digitalWrite(RGB_B_PIN, LOW);
}

/**
 * @brief Activa color azul (Estado ALERTA).
 */
static inline void RGB_BlueOn()
{
  digitalWrite(RGB_R_PIN, LOW);
  digitalWrite(RGB_G_PIN, LOW);
  digitalWrite(RGB_B_PIN, HIGH);
}

/**
 * @brief Activa color rojo (Estado ALARMA).
 */
static inline void RGB_RedOn()
{
  digitalWrite(RGB_R_PIN, HIGH);
  digitalWrite(RGB_G_PIN, LOW);
  digitalWrite(RGB_B_PIN, LOW);
}


/* =====================================================
   FLAGS DE CONTROL DE PARPADEO
   ===================================================== */

/** @brief Controla si el blink de Inicio está activo. */
static bool blinkInicio = false;

/** @brief Controla si el blink de Alerta está activo. */
static bool blinkAlerta = false;

/** @brief Controla si el blink de Alarma está activo. */
static bool blinkAlarma = false;


/* =====================================================
   TAREAS DE PARPADEO (ENCADENADAS)
   ===================================================== */

/**
 * Cada parpadeo usa dos tareas:
 *  ON  → activa color
 *  OFF → apaga y rearma ON
 */

static AsyncTask tIniOn(INI_ON_MS);
static AsyncTask tIniOff(INI_OFF_MS);

static AsyncTask tAlOn(ALERT_ON_MS);
static AsyncTask tAlOff(ALERT_OFF_MS);

static AsyncTask tArOn(ALARM_ON_MS);
static AsyncTask tArOff(ALARM_OFF_MS);


/* =====================================================
   TAREAS DE TIMEOUT (ONE-SHOT)
   ===================================================== */

static AsyncTask tTempToLuz(T_TEMP_TO_LUZ_MS);
static AsyncTask tTempToHum(T_TEMP_TO_HUM_MS);
static AsyncTask tHumToTemp(T_HUM_TO_TEMP_MS);
static AsyncTask tLuzToTemp(T_LUZ_TO_TEMP_MS);
static AsyncTask tAlertaToTemp(T_ALERTA_TO_TEMP_MS);
static AsyncTask tAlarmaToTemp(T_ALARMA_TOTAL_MS);


/* =====================================================
   FLAGS DE EVENTO (TIMEOUTS)
   ===================================================== */

static bool fTempToLuzDone = false;
static bool fTempToHumDone = false;
static bool fHumToTempDone = false;
static bool fLuzToTempDone = false;
static bool fAlertaToTempDone = false;
static bool fAlarmaToTempDone = false;


/* =====================================================
   TAREA DE MONITOREO SERIAL
   ===================================================== */

/** @brief Indica si el monitoreo está habilitado. */
static bool displayEnabled = false;

/** @brief Tarea periódica de impresión. */
static AsyncTask tDisplay(700);

/**
 * @brief Imprime una línea completa de diagnóstico.
 */
static void Display_PrintLine()
{
  Serial.print("State: ");
  Serial.print(Fsm_GetStateName());

  Serial.print(" | TempC: ");
  float t = Sensors_GetTempC();
  if (isnan(t)) Serial.print("nan"); else Serial.print(t, 1);

  Serial.print(" | Hum: ");
  float h = Sensors_GetHum();
  if (isnan(h)) Serial.print("nan"); else Serial.print(h, 1);

  Serial.print(" | LightRaw: ");
  Serial.print(Sensors_GetLightRaw());

  Serial.print(" | Dark: ");
  Serial.print(Sensors_IsDark() ? "YES" : "NO");

  Serial.print(" | Attempts: ");
  Serial.println(Fsm_GetAttempts());
}


/* =====================================================
   INICIALIZACIÓN DE TAREAS
   ===================================================== */

/**
 * @brief Configura callbacks de todas las tareas.
 *
 * Se ejecuta una vez desde setup().
 */
void Tasks_Init()
{
  /* ---------------- BLINK INICIO ---------------- */

  tIniOn.OnFinish = [](){
    if (!blinkInicio) return;
    RGB_GreenOn();
    tIniOff.Start();
  };

  tIniOff.OnFinish = [](){
    RGB_Off();
    if (blinkInicio) tIniOn.Start();
  };

  /* ---------------- BLINK ALERTA ---------------- */

  tAlOn.OnFinish = [](){
    if (!blinkAlerta) return;
    RGB_BlueOn();
    tAlOff.Start();
  };

  tAlOff.OnFinish = [](){
    RGB_Off();
    if (blinkAlerta) tAlOn.Start();
  };

  /* ---------------- BLINK ALARMA ---------------- */

  tArOn.OnFinish = [](){
    if (!blinkAlarma) return;
    RGB_RedOn();
    tArOff.Start();
  };

  tArOff.OnFinish = [](){
    RGB_Off();
    if (blinkAlarma) tArOn.Start();
  };

  /* ---------------- TIMEOUT FLAGS ---------------- */

  tTempToLuz.OnFinish    = [](){ fTempToLuzDone = true; };
  tTempToHum.OnFinish    = [](){ fTempToHumDone = true; };
  tHumToTemp.OnFinish    = [](){ fHumToTempDone = true; };
  tLuzToTemp.OnFinish    = [](){ fLuzToTempDone = true; };
  tAlertaToTemp.OnFinish = [](){ fAlertaToTempDone = true; };
  tAlarmaToTemp.OnFinish = [](){ fAlarmaToTempDone = true; };

  /* ---------------- DISPLAY ---------------- */

  tDisplay.OnFinish = [](){
    if (!displayEnabled) return;
    Display_PrintLine();
    tDisplay.Start(); // rearmado periódico
  };
}


/* =====================================================
   ACTUALIZACIÓN GLOBAL
   ===================================================== */

/**
 * @brief Actualiza todas las tareas activas.
 *
 * Debe llamarse continuamente desde loop().
 */
void Tasks_UpdateAll()
{
  tIniOn.Update();  tIniOff.Update();
  tAlOn.Update();   tAlOff.Update();
  tArOn.Update();   tArOff.Update();

  tTempToLuz.Update();
  tTempToHum.Update();
  tHumToTemp.Update();
  tLuzToTemp.Update();
  tAlertaToTemp.Update();
  tAlarmaToTemp.Update();

  tDisplay.Update();
}


/* =====================================================
   CONTROL DISPLAY
   ===================================================== */

void Tasks_StartMonitoringDisplay()
{
  displayEnabled = true;
  tDisplay.Start();
}

void Tasks_StopMonitoringDisplay()
{
  displayEnabled = false;
  tDisplay.Stop();
}


/* =====================================================
   CONTROL BLINK
   ===================================================== */

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


/* =====================================================
   CONTROL TIMEOUT
   ===================================================== */

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


/* =====================================================
   POP EVENTS (CONSUMO DE FLAGS)
   ===================================================== */

/**
 * @brief Devuelve y limpia flag de evento.
 */

bool Tasks_PopTempToLuzDone(){ bool v = fTempToLuzDone; fTempToLuzDone = false; return v; }
bool Tasks_PopTempToHumDone(){ bool v = fTempToHumDone; fTempToHumDone = false; return v; }
bool Tasks_PopHumToTempDone(){ bool v = fHumToTempDone; fHumToTempDone = false; return v; }
bool Tasks_PopLuzToTempDone(){ bool v = fLuzToTempDone; fLuzToTempDone = false; return v; }
bool Tasks_PopAlertaToTempDone(){ bool v = fAlertaToTempDone; fAlertaToTempDone = false; return v; }
bool Tasks_PopAlarmaToTempDone(){ bool v = fAlarmaToTempDone; fAlarmaToTempDone = false; return v; }

/**
 * @brief Limpia todos los flags de timeout.
 */
void Tasks_ClearAllTimeoutFlags()
{
  fTempToLuzDone = false;
  fTempToHumDone = false;
  fHumToTempDone = false;
  fLuzToTempDone = false;
  fAlertaToTempDone = false;
  fAlarmaToTempDone = false;
}