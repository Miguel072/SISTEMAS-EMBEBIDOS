/*
===========================================================
 NODO 2 FINAL CORREGIDO
 HELTEC WIFI LORA 32 V3
 ESP32-S3 + IMU RAW + GPS + OLED + ESP-NOW
===========================================================

FUNCIONES:
- IMU RAW
- GPS NEO6M
- OLED integrada
- ESP-NOW
- FreeRTOS
- Envio cada 10 segundos
- SOLO mide y transmite
===========================================================

PINES

OLED INTERNA
GPIO17 -> SDA
GPIO18 -> SCL

IMU
GPIO41 -> SDA
GPIO42 -> SCL

GPS
GPIO34 -> RX
GPIO33 -> TX
===========================================================
*/

#include <WiFi.h>
#include <esp_now.h>
#include "esp_wifi.h"

#include <Wire.h>

#include <TinyGPS++.h>
#include <HardwareSerial.h>

#include "SSD1306Wire.h"
#include "pins_arduino.h"

// =====================================================
// OLED
// =====================================================

SSD1306Wire display(
  0x3c,
  SDA_OLED,
  SCL_OLED
);

// =====================================================
// I2C IMU
// =====================================================

TwoWire MPU_I2C = TwoWire(1);

// =====================================================
// IMU RAW
// =====================================================

#define MPU_ADDR 0x68

#define WHO_AM_I     0x75
#define PWR_MGMT_1   0x6B
#define ACCEL_XOUT_H 0x3B

int16_t AcX;
int16_t AcY;
int16_t AcZ;

int16_t GyX;
int16_t GyY;
int16_t GyZ;

// =====================================================
// GPS
// =====================================================

TinyGPSPlus gps;

HardwareSerial gpsSerial(1);

#define GPS_RX 34
#define GPS_TX 33
#define GPS_BAUD 9600

bool gpsFix = false;

// =====================================================
// MAC NODO 3
// =====================================================

uint8_t macNodo3[] = {
  0xF4,0x12,0xFA,0x9F,0x3B,0x58
};

// =====================================================
// ESTRUCTURA
// =====================================================

typedef struct __attribute__((packed)) {

  float pulso;
  float spo2;

  float acc_x;
  float acc_y;
  float acc_z;

  float gyr_x;
  float gyr_y;
  float gyr_z;

  float latitud;
  float longitud;

  float altitud;
  float velocidad;

  uint8_t movimiento;
  uint8_t version;

} datos_t;

datos_t datos;

// =====================================================
// VARIABLES
// =====================================================

bool envioOK = false;
bool mpuOK = false;

// =====================================================
// OLED POWER
// =====================================================

void VextON() {

  pinMode(Vext, OUTPUT);

  digitalWrite(Vext, LOW);
}

void displayReset() {

  pinMode(RST_OLED, OUTPUT);

  digitalWrite(RST_OLED, HIGH);

  delay(1);

  digitalWrite(RST_OLED, LOW);

  delay(1);

  digitalWrite(RST_OLED, HIGH);

  delay(1);
}

// =====================================================
// CALLBACK ESPNOW
// =====================================================

void OnDataSent(
  const wifi_tx_info_t *info,
  esp_now_send_status_t status
) {

  if(status == ESP_NOW_SEND_SUCCESS) {

    envioOK = true;

    Serial.println("ESP NOW OK");
  }
  else {

    envioOK = false;

    Serial.println("ESP NOW ERROR");
  }
}

// =====================================================
// ESPNOW
// =====================================================

void iniciarESPNow() {

  WiFi.mode(WIFI_STA);

  esp_wifi_set_channel(
    13,
    WIFI_SECOND_CHAN_NONE
  );

  if(esp_now_init() != ESP_OK) {

    Serial.println("ESP NOW ERROR");

    while(true);
  }

  esp_now_register_send_cb(
    OnDataSent
  );

  esp_now_peer_info_t peerInfo;

  memset(
    &peerInfo,
    0,
    sizeof(peerInfo)
  );

  memcpy(
    peerInfo.peer_addr,
    macNodo3,
    6
  );

  peerInfo.channel = 13;

  peerInfo.encrypt = false;

  esp_now_add_peer(&peerInfo);

  Serial.println("ESP NOW LISTO");
}

// =====================================================
// LEER REGISTRO
// =====================================================

uint8_t leerRegistro(uint8_t reg) {

  MPU_I2C.beginTransmission(MPU_ADDR);

  MPU_I2C.write(reg);

  MPU_I2C.endTransmission(false);

  MPU_I2C.requestFrom(MPU_ADDR, 1);

  if(MPU_I2C.available()) {

    return MPU_I2C.read();
  }

  return 0xFF;
}

// =====================================================
// ESCRIBIR REGISTRO
// =====================================================

void escribirRegistro(
  uint8_t reg,
  uint8_t valor
) {

  MPU_I2C.beginTransmission(MPU_ADDR);

  MPU_I2C.write(reg);

  MPU_I2C.write(valor);

  MPU_I2C.endTransmission();
}

// =====================================================
// LEER IMU
// =====================================================

void leerMPU() {

  MPU_I2C.beginTransmission(MPU_ADDR);

  MPU_I2C.write(ACCEL_XOUT_H);

  MPU_I2C.endTransmission(false);

  MPU_I2C.requestFrom(MPU_ADDR, 14);

  if(MPU_I2C.available() >= 14) {

    AcX =
      MPU_I2C.read() << 8 |
      MPU_I2C.read();

    AcY =
      MPU_I2C.read() << 8 |
      MPU_I2C.read();

    AcZ =
      MPU_I2C.read() << 8 |
      MPU_I2C.read();

    MPU_I2C.read();
    MPU_I2C.read();

    GyX =
      MPU_I2C.read() << 8 |
      MPU_I2C.read();

    GyY =
      MPU_I2C.read() << 8 |
      MPU_I2C.read();

    GyZ =
      MPU_I2C.read() << 8 |
      MPU_I2C.read();
  }
}

// =====================================================
// OLED
// =====================================================

void actualizarOLED() {

  display.clear();

  display.setTextAlignment(
    TEXT_ALIGN_LEFT
  );

  display.setFont(
    ArialMT_Plain_10
  );

  // ===============================================
  // GPS
  // ===============================================

  if(gpsFix) {

    display.drawString(
      0,
      0,
      "LAT:"
      + String(datos.latitud,6)
    );

    display.drawString(
      0,
      10,
      "LON:"
      + String(datos.longitud,6)
    );
  }
  else {

    display.drawString(
      0,
      0,
      "GPS SIN FIX"
    );
  }

  // ===============================================
  // ACELEROMETRO
  // ===============================================

  display.drawString(
    0,
    22,
    "A:"
    + String(datos.acc_x,0)
    + " "
    + String(datos.acc_y,0)
    + " "
    + String(datos.acc_z,0)
  );

  // ===============================================
  // GIROSCOPIO
  // ===============================================

  display.drawString(
    0,
    32,
    "G:"
    + String(datos.gyr_x,0)
    + " "
    + String(datos.gyr_y,0)
    + " "
    + String(datos.gyr_z,0)
  );

  // ===============================================
  // ALTITUD
  // ===============================================

  display.drawString(
    0,
    44,
    "ALT:"
    + String(datos.altitud,1)
  );

  // ===============================================
  // VELOCIDAD
  // ===============================================

  display.drawString(
    64,
    44,
    "VEL:"
    + String(datos.velocidad,1)
  );

  // ===============================================
  // ESPNOW
  // ===============================================

  display.drawString(
    0,
    54,
    envioOK ?
    "TX OK" :
    "TX..."
  );

  // ===============================================
  // GPS STATUS
  // ===============================================

  display.drawString(
    64,
    54,
    gpsFix ?
    "GPS OK" :
    "NO GPS"
  );

  display.display();
}

// =====================================================
// TASK IMU
// =====================================================

void taskMPU(void *pv) {

  while(1) {

    if(mpuOK) {

      leerMPU();

      datos.acc_x = AcX;
      datos.acc_y = AcY;
      datos.acc_z = AcZ;

      datos.gyr_x = GyX;
      datos.gyr_y = GyY;
      datos.gyr_z = GyZ;

      // Movimiento ahora se procesa en Nodo 3
      datos.movimiento = 0;
    }

    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

// =====================================================
// TASK GPS
// =====================================================

void taskGPS(void *pv) {

  while(1) {

    while(gpsSerial.available()) {

      gps.encode(
        gpsSerial.read()
      );
    }

    if(gps.location.isValid()) {

      gpsFix = true;

      datos.latitud =
        gps.location.lat();

      datos.longitud =
        gps.location.lng();

      datos.altitud =
        gps.altitude.meters();

      datos.velocidad =
        gps.speed.kmph();
    }
    else {

      gpsFix = false;
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// =====================================================
// TASK OLED
// =====================================================

void taskOLED(void *pv) {

  while(1) {

    actualizarOLED();

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// =====================================================
// TASK ESPNOW
// =====================================================

void taskESPNow(void *pv) {

  while(1) {

    datos.version = 1;

    esp_now_send(
      macNodo3,
      (uint8_t*)&datos,
      sizeof(datos)
    );

    Serial.println("DATOS ENVIADOS");

    vTaskDelay(pdMS_TO_TICKS(10000));
  }
}

// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);

  Serial.println();
  Serial.println("SETUP INICIO");

  // OLED
  VextON();

  delay(500);

  displayReset();

  delay(500);

  display.init();

  display.flipScreenVertically();

  display.setFont(
    ArialMT_Plain_10
  );

  display.clear();

  display.drawString(
    0,
    0,
    "NODO 2"
  );

  display.display();

  Serial.println("OLED OK");

  // IMU
  MPU_I2C.begin(41,42);

  Serial.println("Bus IMU OK");

  uint8_t whoami =
    leerRegistro(WHO_AM_I);

  Serial.print("WHO_AM_I: 0x");

  Serial.println(whoami, HEX);

  escribirRegistro(
    PWR_MGMT_1,
    0x00
  );

  delay(100);

  uint8_t pwr =
    leerRegistro(PWR_MGMT_1);

  Serial.print("PWR_MGMT_1 = 0x");

  Serial.println(pwr, HEX);

  mpuOK = true;

  Serial.println("IMU RAW OK");

  // GPS
  gpsSerial.begin(
    GPS_BAUD,
    SERIAL_8N1,
    GPS_RX,
    GPS_TX
  );

  Serial.println("GPS OK");

  // ESP NOW
  iniciarESPNow();

  // TASKS
  xTaskCreatePinnedToCore(
    taskMPU,
    "MPU",
    4096,
    NULL,
    1,
    NULL,
    1
  );

  xTaskCreatePinnedToCore(
    taskGPS,
    "GPS",
    4096,
    NULL,
    1,
    NULL,
    1
  );

  xTaskCreatePinnedToCore(
    taskOLED,
    "OLED",
    4096,
    NULL,
    1,
    NULL,
    1
  );

  xTaskCreatePinnedToCore(
    taskESPNow,
    "ESPNOW",
    4096,
    NULL,
    1,
    NULL,
    1
  );

  Serial.println("NODO 2 LISTO");
}

// =====================================================
// LOOP
// =====================================================

void loop() {

  delay(1000);
}
