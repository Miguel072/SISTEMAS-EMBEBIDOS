#include "FmsApp2.h"
#include "Config2.h"
#include "StateMachineLib.h"
#include "Sensors2.h"
#include "TaskRT2.h"


// =====================================================
//        FORWARD DECLARATIONS (FIX COMPILATION ERROR)
// =====================================================
static void enterInicio();
static void leaveInicio();

static void enterMonTemp();
static void leaveMonTemp();

static void enterMonHumed();
static void leaveMonHumed();

static void enterMonLuz();
static void leaveMonLuz();

static void enterAlerta();
static void leaveAlerta();

static void enterAlarma();
static void leaveAlarma();

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
//                 PUBLIC INFO FOR DISPLAY
// =====================================================
static State g_currentState = INICIO;
static int   g_attempts = 0;

const char* Fsm_GetStateName()
{
  switch (g_currentState)
  {
    case INICIO: return "INICIO";
    case MON_TEMP: return "MON_TEMP";
    case MON_HUMED: return "MON_HUMED";
    case MON_LUZ: return "MON_LUZ";
    case ALERTA_BLUE: return "ALERTA";
    case ALARMA_RED: return "ALARMA";
    default: return "UNKNOWN";
  }
}
int Fsm_GetAttempts(){ return g_attempts; }

// =====================================================
//             FLOW CONTROL (simple alternating)
// =====================================================
static bool nextTempGoesToHum = true;

// =====================================================
//                    ATTEMPTS LOGIC
// =====================================================
static bool attemptCountedThisAlert = false;

// =====================================================
//                  BUTTON EDGE
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
//               TRANSITION CONDITIONS
// =====================================================
static bool trInicioToTemp(){ return Button_PressedEdge(); }

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

static bool trLuzToAlerta(){ return Sensors_IsDark(); }
static bool trLuzToTemp(){ return Tasks_PopLuzToTempDone(); }

static bool trHumedToAlerta()
{
  float t = Sensors_GetTempC();
  return (!isnan(t) && t >= TEMP_ALERT_C);
}
static bool trHumedToTemp(){ return Tasks_PopHumToTempDone(); }

static bool trAlertaToTemp(){ return Tasks_PopAlertaToTempDone(); }

static bool trAlertaToAlarma()
{
  float t = Sensors_GetTempC();
  if (!attemptCountedThisAlert && !isnan(t) && t >= TEMP_ALARM_C)
  {
    g_attempts++;
    attemptCountedThisAlert = true;
  }
  return (g_attempts >= 3);
}

static bool trAlarmaToTemp(){ return Tasks_PopAlarmaToTempDone(); }

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

// =====================================================
//               STATE ENTER / LEAVE ACTIONS
// =====================================================

// INICIO
static void enterInicio()
{
  g_currentState = INICIO;
  g_attempts = 0;
  nextTempGoesToHum = true;

  // INICIO: no prints
  Tasks_StopMonitoringDisplay();

  Tasks_ClearAllTimeoutFlags();
  Tasks_StartInicioBlink();
}
static void leaveInicio()
{
  Tasks_StopInicioBlink();
}

// MON_TEMP
static void enterMonTemp()
{
  g_currentState = MON_TEMP;

  // Monitoring: prints ON
  Tasks_StartMonitoringDisplay();

  // Alternate: HUM then LUZ then HUM...
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

// MON_HUMED
static void enterMonHumed()
{
  g_currentState = MON_HUMED;

  // Monitoring: prints ON
  Tasks_StartMonitoringDisplay();

  Tasks_StartHumToTemp();
  nextTempGoesToHum = false;
}
static void leaveMonHumed()
{
  Tasks_StopHumToTemp();
}

// MON_LUZ
static void enterMonLuz()
{
  g_currentState = MON_LUZ;

  // Monitoring: prints ON
  Tasks_StartMonitoringDisplay();

  Tasks_StartLuzToTemp();
  nextTempGoesToHum = true;
}
static void leaveMonLuz()
{
  Tasks_StopLuzToTemp();
}

// ALERTA
static void enterAlerta()
{
  g_currentState = ALERTA_BLUE;

  // ALERTA: prints ON (per your request)
  Tasks_StartMonitoringDisplay();

  attemptCountedThisAlert = false;

  Tasks_StartAlertaBlink();
  Tasks_StartAlertaToTemp();
}
static void leaveAlerta()
{
  Tasks_StopAlertaBlink();
  Tasks_StopAlertaToTemp();
}

// ALARMA
static void enterAlarma()
{
  g_currentState = ALARMA_RED;

  // ALARMA: (default) no prints, change if you want
  Tasks_StopMonitoringDisplay();

  Tasks_StartAlarmaBlink();
  Tasks_StartAlarmaToTemp();
}
static void leaveAlarma()
{
  Tasks_StopAlarmaBlink();
  Tasks_StopAlarmaToTemp();
}

// =====================================================
//                 PUBLIC API
// =====================================================
void Fsm_Init()
{
  setupStateMachine();
  sm.SetState(INICIO, false, true);
}

void Fsm_Update()
{
  sm.Update();
}