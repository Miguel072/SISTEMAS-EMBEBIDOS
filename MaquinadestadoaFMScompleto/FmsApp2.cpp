#include "FmsApp2.h"
#include "Config2.h"
#include "StateMachineLib.h"
#include "Sensors2.h"
#include "TaskRT2.h"

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

// 6 states, 11 transitions (keep consistent)
static StateMachine sm(6, 11);

// =====================================================
//             FLOW CONTROL (DIAGRAM BUG FIX)
// =====================================================
// We alternate cycles, but MON_LUZ is only allowed if FLAME is active.
static bool nextTempGoesToHum = true;

// =====================================================
//                    ATTEMPTS LOGIC
// =====================================================
static int attempts = 0;
static bool attemptCountedThisAlert = false;

// =====================================================
//                  BUTTON EDGE (NO volatile)
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
  // ---------------------------------------------------
  // KEY IDEA (professor rule):
  // Temp -> Luz (2s) ONLY if flame sensor is ACTIVE.
  // If flame is NOT active, we IGNORE Luz and let
  // Temp -> Hum (4s) happen normally.
  // ---------------------------------------------------

  bool flameOK = Sensors_FlameActive();

  if (nextTempGoesToHum)
  {
    // This cycle goes to HUMED (always)
    Tasks_StopTempToLuz();
    Tasks_StartTempToHum();
  }
  else
  {
    // Intended cycle goes to LUZ, but only if flameOK
    Tasks_StopTempToHum();

    if (flameOK)
    {
      Tasks_StartTempToLuz();
    }
    else
    {
      // Skip Luz: force HUM instead to avoid timing clash
      nextTempGoesToHum = true;
      Tasks_StartTempToHum();
    }
  }
}

static void leaveMonTemp()
{
  Tasks_StopTempToLuz();
  Tasks_StopTempToHum();
}

static void enterMonHumed()
{
  Tasks_StartHumToTemp();
  nextTempGoesToHum = false; // next cycle try Luz (if flame allows)
}

static void leaveMonHumed()
{
  Tasks_StopHumToTemp();
}

static void enterMonLuz()
{
  Tasks_StartLuzToTemp();
  nextTempGoesToHum = true;  // after Luz, next cycle is Hum
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
  // Safety gate: only allow if this cycle is Luz and flame active
  if (nextTempGoesToHum) return false;
  if (!Sensors_FlameActive()) return false;
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
  float t = Sensors_GetTempC();

  // Count attempt only once per ALERTA entry
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
  sm.AddTransition(INICIO, MON_TEMP, trInicioToTemp);

  sm.AddTransition(MON_TEMP, ALERTA_BLUE, trTempToAlerta);
  sm.AddTransition(MON_TEMP, MON_LUZ, trTempToLuz);
  sm.AddTransition(MON_TEMP, MON_HUMED, trTempToHumed);

  sm.AddTransition(MON_LUZ, ALERTA_BLUE, trLuzToAlerta);
  sm.AddTransition(MON_LUZ, MON_TEMP, trLuzToTemp);

  sm.AddTransition(MON_HUMED, ALERTA_BLUE, trHumedToAlerta);
  sm.AddTransition(MON_HUMED, MON_TEMP, trHumedToTemp);

  sm.AddTransition(ALERTA_BLUE, ALARMA_RED, trAlertaToAlarma);
  sm.AddTransition(ALERTA_BLUE, MON_TEMP, trAlertaToTemp);

  sm.AddTransition(ALARMA_RED, MON_TEMP, trAlarmaToTemp);

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