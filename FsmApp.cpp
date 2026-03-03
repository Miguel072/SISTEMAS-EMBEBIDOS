#include "FsmApp.h"
#include "Config.h"
#include "StateMachineLib.h"
#include "Sensors.h"
#include "TaskRT.h"

// =====================================================
//                       STATES
// =====================================================
enum State
{
  INICIO = 0,
  MON_TEMP = 1,
  MON_HUMED = 2,
  MON_LUZ = 3,
  ALERTA_BLUE = 4,
  ALARMA_RED = 5
};

// 6 states, 11 transitions
static StateMachine sm(6, 11);

// =====================================================
//               FIX: HUMED UNREACHABLE BUG
// =====================================================
static bool nextTempGoesToHum = true;

// =====================================================
//                    ATTEMPTS LOGIC
// =====================================================
static int attempts = 0;
static bool attemptCountedThisAlert = false;

// =====================================================
//                  BUTTON EDGE (NO VOLATILE)
// =====================================================
static bool Button_PressedEdge()
{
  static bool lastPressed = false;
  bool now = (digitalRead(BTN_PIN) == LOW);
  bool edge = (now && !lastPressed);
  lastPressed = now;
  return edge;
}

// =====================================================
//               STATE ENTER / LEAVE ACTIONS
// =====================================================
static void enterInicio()
{
  attempts = 0;
  nextTempGoesToHum = true;
  Tasks_ClearAllTimeoutFlags();
  Tasks_StartInicioBlink();
}

static void leaveInicio()
{
  Tasks_StopInicioBlink();
}

static void enterMonTemp()
{
  // Start ONLY ONE timeout path (prevents HUMED being skipped)
  if (nextTempGoesToHum)
  {
    Tasks_StopTempToLuz();
    Tasks_StartTempToHum();
  }
  else
  {
    Tasks_StopTempToHum();
    Tasks_StartTempToLuz();
  }
}

static void leaveMonTemp()
{
  Tasks_StopTempToLuz();
  Tasks_StopTempToHum();
}

static void enterMonHumed()
{
  // Stay 3s then back to Temp
  Tasks_StartHumToTemp();
  nextTempGoesToHum = false; // next cycle from Temp goes to Luz
}

static void leaveMonHumed()
{
  Tasks_StopHumToTemp();
}

static void enterMonLuz()
{
  // Stay 5s then back to Temp
  Tasks_StartLuzToTemp();
  nextTempGoesToHum = true; // next cycle from Temp goes to Humed
}

static void leaveMonLuz()
{
  Tasks_StopLuzToTemp();
}

static void enterAlerta()
{
  attemptCountedThisAlert = false;
  Tasks_StartAlertaBlink();
  Tasks_StartAlertaToTemp();
}

static void leaveAlerta()
{
  Tasks_StopAlertaBlink();
  Tasks_StopAlertaToTemp();
}

static void enterAlarma()
{
  Tasks_StartAlarmaBlink();
  Tasks_StartAlarmaToTemp();
}

static void leaveAlarma()
{
  Tasks_StopAlarmaBlink();
  Tasks_StopAlarmaToTemp();
}

// =====================================================
//               TRANSITION CONDITIONS
// =====================================================
static bool trInicioToTemp()
{
  return Button_PressedEdge();
}

static bool trTempToAlerta()
{
  float t = Sensors_GetTempC();
  return (!isnan(t) && t >= TEMP_ALERT_C);
}

static bool trTempToLuz()
{
  if (nextTempGoesToHum) return false;
  return Tasks_PopTempToLuzDone();
}

static bool trTempToHumed()
{
  if (!nextTempGoesToHum) return false;
  return Tasks_PopTempToHumDone();
}

static bool trLuzToAlerta()
{
  return Sensors_IsDark();
}

static bool trLuzToTemp()
{
  return Tasks_PopLuzToTempDone();
}

static bool trHumedToTemp()
{
  return Tasks_PopHumToTempDone();
}

// Optional: allow ALERTA if temp is still high while in HUMED
static bool trHumedToAlerta()
{
  float t = Sensors_GetTempC();
  return (!isnan(t) && t >= TEMP_ALERT_C);
}

static bool trAlertaToTemp()
{
  return Tasks_PopAlertaToTempDone();
}

static bool trAlertaToAlarma()
{
  // Count ONE attempt per alert entry if temp >= 30
  float t = Sensors_GetTempC();
  if (!attemptCountedThisAlert && !isnan(t) && t >= TEMP_ALARM_C)
  {
    attempts++;
    attemptCountedThisAlert = true;
  }
  return (attempts >= 3);
}

static bool trAlarmaToTemp()
{
  return Tasks_PopAlarmaToTempDone();
}

// =====================================================
//                  FSM SETUP (11 transitions)
// =====================================================
static void setupStateMachine()
{
  // 1) INICIO -> MON_TEMP (button)
  sm.AddTransition(INICIO, MON_TEMP, trInicioToTemp);

  // 2) MON_TEMP -> ALERTA (temp >= 25)
  sm.AddTransition(MON_TEMP, ALERTA_BLUE, trTempToAlerta);

  // 3) MON_TEMP -> MON_LUZ (2s path)
  sm.AddTransition(MON_TEMP, MON_LUZ, trTempToLuz);

  // 4) MON_TEMP -> MON_HUMED (4s path)
  sm.AddTransition(MON_TEMP, MON_HUMED, trTempToHumed);

  // 5) MON_LUZ -> ALERTA (dark)
  sm.AddTransition(MON_LUZ, ALERTA_BLUE, trLuzToAlerta);

  // 6) MON_LUZ -> MON_TEMP (5s)
  sm.AddTransition(MON_LUZ, MON_TEMP, trLuzToTemp);

  // 7) MON_HUMED -> ALERTA (optional temp >=25)
  sm.AddTransition(MON_HUMED, ALERTA_BLUE, trHumedToAlerta);

  // 8) MON_HUMED -> MON_TEMP (3s)
  sm.AddTransition(MON_HUMED, MON_TEMP, trHumedToTemp);

  // 9) ALERTA -> ALARMA (3 attempts temp>=30)
  sm.AddTransition(ALERTA_BLUE, ALARMA_RED, trAlertaToAlarma);

  // 10) ALERTA -> MON_TEMP (3s)
  sm.AddTransition(ALERTA_BLUE, MON_TEMP, trAlertaToTemp);

  // 11) ALARMA -> MON_TEMP (5s)
  sm.AddTransition(ALARMA_RED, MON_TEMP, trAlarmaToTemp);

  // On entering/leaving
  sm.SetOnEntering(INICIO, enterInicio);
  sm.SetOnLeaving(INICIO, leaveInicio);

  sm.SetOnEntering(MON_TEMP, enterMonTemp);
  sm.SetOnLeaving(MON_TEMP, leaveMonTemp);

  sm.SetOnEntering(MON_HUMED, enterMonHumed);
  sm.SetOnLeaving(MON_HUMED, leaveMonHumed);

  sm.SetOnEntering(MON_LUZ, enterMonLuz);
  sm.SetOnLeaving(MON_LUZ, leaveMonLuz);

  sm.SetOnEntering(ALERTA_BLUE, enterAlerta);
  sm.SetOnLeaving(ALERTA_BLUE, leaveAlerta);

  sm.SetOnEntering(ALARMA_RED, enterAlarma);
  sm.SetOnLeaving(ALARMA_RED, leaveAlarma);
}

void Fsm_Init()
{
  setupStateMachine();
  sm.SetState(INICIO, false, true);
}

void Fsm_Update()
{
  sm.Update();
}