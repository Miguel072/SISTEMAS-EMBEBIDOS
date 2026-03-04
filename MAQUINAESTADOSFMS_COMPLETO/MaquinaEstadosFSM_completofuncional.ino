#include <Arduino.h>

#include "Config2.h"
#include "Sensors2.h"
#include "TaskRT2.h"
#include "FmsApp2.h"


// =====================================================
//                        SETUP
// =====================================================
void setup()
{
  Serial.begin(115200);

  // -------------------- GPIO CONFIG --------------------
  pinMode(BTN_PIN, INPUT_PULLUP);

  pinMode(RGB_R_PIN, OUTPUT);
  pinMode(RGB_G_PIN, OUTPUT);
  pinMode(RGB_B_PIN, OUTPUT);

  // Start with RGB OFF
  digitalWrite(RGB_R_PIN, LOW);
  digitalWrite(RGB_G_PIN, LOW);
  digitalWrite(RGB_B_PIN, LOW);

  // -------------------- MODULE INIT --------------------
  Sensors_Init();
  Tasks_Init(); 
  Fsm_Init();

  // Small banner (optional)
  Serial.println("=== ESP32 RT FSM Project Started ===");
}

// =====================================================
//                         LOOP
// =====================================================
void loop()
{
  // 1) Update all sensors (non-blocking, DHT throttled inside)
  Sensors_Update();

  // 2) Update all tasks (blink + timeouts + debug)
  Tasks_UpdateAll();

  // 3) Update state machine
  Fsm_Update();
}