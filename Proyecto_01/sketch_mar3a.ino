#include <Arduino.h>
#include "Config.h"
#include "Sensors.h"
#include "TaskRT.h"
#include "FsmApp.h"

// =====================================================
//                     SETUP
// =====================================================
void setup()
{
  Serial.begin(115200);

  pinMode(RGB_R_PIN, OUTPUT);
  pinMode(RGB_G_PIN, OUTPUT);
  pinMode(RGB_B_PIN, OUTPUT);

  pinMode(BTN_PIN, INPUT_PULLUP);

  Sensors_Init();
  Tasks_Init();
  Tasks_StartDebug();   // debug periodic prints
  Fsm_Init();           // initial state = INICIO
}

// =====================================================
//                     LOOP
//  RULE: only update modules (no delays)
// =====================================================
void loop()
{
  Sensors_Update();
  Fsm_Update();
  Tasks_UpdateAll();
}