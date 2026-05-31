/*
===========================================================
 NODO 3 FINAL COMPLETO
 HELTEC WIFI LORA 32 V3
 ESP32-S3 + OLED + ESPNOW + WIFI + MYSQL
===========================================================

LEDS:

ROJO  -> MOVIMIENTO
VERDE -> PULSO
AZUL  -> SPO2

FUNCIONALIDADES:
- ESPNOW
- WIFI
- HTTP POST
- HTTP GET
- MYSQL
- FREE RTOS
- OLED
- ALERTAS
- TEMPORIZADORES
- MENSAJES SIMULTANEOS
- UMBRALES DINAMICOS
===========================================================
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include <esp_now.h>
#include "esp_wifi.h"

#include <Wire.h>
#include <time.h>

#include "SSD1306Wire.h"
#include "pins_arduino.h"

// =====================================================
// WIFI
// =====================================================

const char* ssid =
"Redmi";

const char* password =
"Realme12";

// =====================================================
// NTP
// =====================================================

const char* ntpServer =
"pool.ntp.org";

const long gmtOffset_sec =
-5 * 3600;

const int daylightOffset_sec =
0;

// =====================================================
// SERVIDOR
// =====================================================

String servidorGuardar =
"http://10.194.135.144/iot/guardar.php";

String servidorUmbrales =
"http://10.194.135.144/iot/obtener_umbrales.php";

// =====================================================
// OLED
// =====================================================

SSD1306Wire display(
  0x3c,
  SDA_OLED,
  SCL_OLED
);

// =====================================================
// LEDS
// =====================================================

#define LED_R 35
#define LED_G 36
#define LED_B 37

// =====================================================
// MACS
// =====================================================

uint8_t mac_nodo1[] = {
  0x68,0x25,0xdd,0x32,0x70,0xcc
};

uint8_t mac_nodo2[] = {
  0x48,0xCA,0x43,0x3C,0x65,0xCC
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

// =====================================================
// VARIABLES
// =====================================================

datos_t nodo1;
datos_t nodo2;

SemaphoreHandle_t xMutex;

bool nodo1Conectado = false;
bool nodo2Conectado = false;

unsigned long ultimoNodo1 = 0;
unsigned long ultimoNodo2 = 0;

// =====================================================
// UMBRALES
// =====================================================

float pulsoMinimo = 60;
float pulsoMaximo = 120;

float spo2Minimo = 92;
float spo2Maximo = 100;

// =====================================================
// ALERTAS
// =====================================================

bool alertaPulso = false;
bool alertaSpo2 = false;
bool alertaMovimiento = false;

// =====================================================
// TEMPORIZADORES
// =====================================================

unsigned long inicioPulso = 0;
unsigned long inicioSpo2 = 0;
unsigned long inicioMovimiento = 0;

unsigned long tiempoPulso = 0;
unsigned long tiempoSpo2 = 0;
unsigned long tiempoMovimiento = 0;

// =====================================================
// CONTADORES NORMALES
// =====================================================

int normalesPulso = 0;
int normalesSpo2 = 0;
int normalesMovimiento = 0;

// =====================================================
// CONTEOS
// =====================================================

bool conteoPulso = false;
bool conteoSpo2 = false;
bool conteoMovimiento = false;

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
// WIFI
// =====================================================

void conectarWiFi() {

  WiFi.mode(WIFI_AP_STA);

  WiFi.begin(
    ssid,
    password
  );

  while(WiFi.status() != WL_CONNECTED) {

    delay(500);
  }
}

// =====================================================
// NTP
// =====================================================

void iniciarHora() {

  configTime(
    gmtOffset_sec,
    daylightOffset_sec,
    ntpServer
  );
}

// =====================================================
// HORA
// =====================================================

String obtenerHora() {

  struct tm timeinfo;

  if(!getLocalTime(&timeinfo)) {

    return "--:--:--";
  }

  char buffer[20];

  strftime(
    buffer,
    sizeof(buffer),
    "%H:%M:%S",
    &timeinfo
  );

  return String(buffer);
}

// =====================================================
// PEERS
// =====================================================

void agregarPeer(uint8_t *mac) {

  esp_now_peer_info_t peerInfo;

  memset(
    &peerInfo,
    0,
    sizeof(peerInfo)
  );

  memcpy(
    peerInfo.peer_addr,
    mac,
    6
  );

  peerInfo.channel = 9;

  peerInfo.encrypt = false;

  esp_now_add_peer(&peerInfo);
}

// =====================================================
// RX
// =====================================================

void OnDataRecv(
  const esp_now_recv_info_t *info,
  const uint8_t *incomingData,
  int len
) {

  if(len != sizeof(datos_t))
    return;

  datos_t rx;

  memcpy(
    &rx,
    incomingData,
    sizeof(rx)
  );

  bool esNodo1 =
    memcmp(
      info->src_addr,
      mac_nodo1,
      6
    ) == 0;

  bool esNodo2 =
    memcmp(
      info->src_addr,
      mac_nodo2,
      6
    ) == 0;

  xSemaphoreTake(
    xMutex,
    portMAX_DELAY
  );

  if(esNodo1) {

    nodo1 = rx;

    nodo1Conectado = true;

    ultimoNodo1 = millis();
  }

  if(esNodo2) {

    nodo2 = rx;

    nodo2Conectado = true;

    ultimoNodo2 = millis();
  }

  xSemaphoreGive(xMutex);
}

// =====================================================
// GET UMBRALES
// =====================================================

void obtenerUmbrales() {

  if(WiFi.status() != WL_CONNECTED)
    return;

  HTTPClient http;

  http.begin(
    servidorUmbrales
  );

  int httpCode =
    http.GET();

  if(httpCode > 0) {

    String payload =
      http.getString();

    DynamicJsonDocument doc(512);

    deserializeJson(
      doc,
      payload
    );

    pulsoMinimo =
      doc["pulso_min"];

    pulsoMaximo =
      doc["pulso_max"];

    spo2Minimo =
      doc["spo2_min"];

    spo2Maximo =
      doc["spo2_max"];
  }

  http.end();
}

// =====================================================
// MOVIMIENTO
// =====================================================

bool detectarMovimiento() {

  if(

    abs(nodo1.gyr_x) > 18000 ||
    abs(nodo1.gyr_y) > 18000 ||
    abs(nodo1.gyr_z) > 18000 ||

    abs(nodo2.gyr_x) > 18000 ||
    abs(nodo2.gyr_y) > 18000 ||
    abs(nodo2.gyr_z) > 18000 ||

    abs(nodo1.acc_x) > 25000 ||
    abs(nodo1.acc_y) > 25000 ||
    abs(nodo1.acc_z) > 25000 ||

    abs(nodo2.acc_x) > 25000 ||
    abs(nodo2.acc_y) > 25000 ||
    abs(nodo2.acc_z) > 25000

  ) {

    return true;
  }

  return false;
}

// =====================================================
// ALERTAS
// =====================================================

void evaluarAlertas() {

  bool pulsoFuera =

  (
    nodo1.pulso > pulsoMaximo
  )

  ||

  (
    nodo1.pulso < pulsoMinimo
  );

  bool spo2Fuera =

  (
    nodo1.spo2 > spo2Maximo
  )

  ||

  (
    nodo1.spo2 < spo2Minimo
  );

  bool movimientoFuera =
    detectarMovimiento();

  // =====================================================
  // PULSO
  // =====================================================

  if(pulsoFuera) {

    normalesPulso = 0;

    if(!conteoPulso) {

      inicioPulso = millis();

      conteoPulso = true;
    }

    tiempoPulso =
      (millis() - inicioPulso) / 1000;

    if(tiempoPulso >= 60) {

      alertaPulso = true;
    }

  } else {

    conteoPulso = false;

    tiempoPulso = 0;

    if(alertaPulso) {

      normalesPulso++;

      if(normalesPulso >= 3) {

        alertaPulso = false;

        normalesPulso = 0;
      }
    }
  }

  // =====================================================
  // SPO2
  // =====================================================

  if(spo2Fuera) {

    normalesSpo2 = 0;

    if(!conteoSpo2) {

      inicioSpo2 = millis();

      conteoSpo2 = true;
    }

    tiempoSpo2 =
      (millis() - inicioSpo2) / 1000;

    if(tiempoSpo2 >= 120) {

      alertaSpo2 = true;
    }

  } else {

    conteoSpo2 = false;

    tiempoSpo2 = 0;

    if(alertaSpo2) {

      normalesSpo2++;

      if(normalesSpo2 >= 5) {

        alertaSpo2 = false;

        normalesSpo2 = 0;
      }
    }
  }

  // =====================================================
  // MOVIMIENTO
  // =====================================================

  if(movimientoFuera) {

    normalesMovimiento = 0;

    if(!conteoMovimiento) {

      inicioMovimiento = millis();

      conteoMovimiento = true;
    }

    tiempoMovimiento =
      (millis() - inicioMovimiento) / 1000;

    if(tiempoMovimiento >= 30) {

      alertaMovimiento = true;
    }

  } else {

    conteoMovimiento = false;

    tiempoMovimiento = 0;

    if(alertaMovimiento) {

      normalesMovimiento++;

      if(normalesMovimiento >= 2) {

        alertaMovimiento = false;

        normalesMovimiento = 0;
      }
    }
  }

  // =====================================================
  // LEDS
  // =====================================================

  digitalWrite(
    LED_G,
    alertaPulso
  );

  digitalWrite(
    LED_B,
    alertaSpo2
  );

  digitalWrite(
    LED_R,
    alertaMovimiento
  );
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

  int y = 0;

  // =====================================================
  // ALERTA PULSO
  // =====================================================

  if(alertaPulso || conteoPulso) {

    display.drawString(
      0,
      y,
      "ALERTA PULSO"
    );

    y += 12;

    display.drawString(
      0,
      y,
      "P:"
      + String(nodo1.pulso,0)
      + " T:"
      + String(tiempoPulso)
      + "s"
    );

    y += 14;
  }

  // =====================================================
  // ALERTA SPO2
  // =====================================================

  if(alertaSpo2 || conteoSpo2) {

    display.drawString(
      0,
      y,
      "ALERTA OXIGENACION"
    );

    y += 12;

    display.drawString(
      0,
      y,
      "S:"
      + String(nodo1.spo2,0)
      + " T:"
      + String(tiempoSpo2)
      + "s"
    );

    y += 14;
  }

  // =====================================================
  // ALERTA MOVIMIENTO
  // =====================================================

  if(alertaMovimiento || conteoMovimiento) {

    display.drawString(
      0,
      y,
      "ALTO NIVEL MOVIM"
    );

    y += 12;

    display.drawString(
      0,
      y,
      "T:"
      + String(tiempoMovimiento)
      + "s"
    );

    y += 14;
  }

  // =====================================================
  // NORMAL
  // =====================================================

  if(

    !alertaPulso &&
    !alertaSpo2 &&
    !alertaMovimiento &&

    !conteoPulso &&
    !conteoSpo2 &&
    !conteoMovimiento

  ) {

    display.drawString(
      0,
      0,
      nodo1Conectado ?
      "RX NODO1" :
      "N1 OFF"
    );

    display.drawString(
      64,
      0,
      nodo2Conectado ?
      "RX NODO2" :
      "N2 OFF"
    );

    display.drawString(
      0,
      12,
      "P:"
      + String(nodo1.pulso,0)
    );

    display.drawString(
      64,
      12,
      "S:"
      + String(nodo1.spo2,0)
    );

    display.drawString(
      0,
      24,
      "LAT:"
    );

    display.drawString(
      30,
      24,
      String(nodo2.latitud,2)
    );

    display.drawString(
      0,
      36,
      "VEL:"
      + String(nodo2.velocidad,1)
    );

    display.drawString(
      0,
      52,
      obtenerHora()
    );
  }

  display.display();
}

// =====================================================
// HTTP POST
// =====================================================

void enviarServidor() {

  if(WiFi.status() != WL_CONNECTED)
    return;

  HTTPClient http;

  http.begin(
    servidorGuardar
  );

  http.addHeader(
    "Content-Type",
    "application/x-www-form-urlencoded"
  );

  String datosPOST =

  "pulso=" + String(nodo1.pulso) +

  "&spo2=" + String(nodo1.spo2) +

  "&acc1_x=" + String(nodo1.acc_x) +
  "&acc1_y=" + String(nodo1.acc_y) +
  "&acc1_z=" + String(nodo1.acc_z) +

  "&gyr1_x=" + String(nodo1.gyr_x) +
  "&gyr1_y=" + String(nodo1.gyr_y) +
  "&gyr1_z=" + String(nodo1.gyr_z) +

  "&acc2_x=" + String(nodo2.acc_x) +
  "&acc2_y=" + String(nodo2.acc_y) +
  "&acc2_z=" + String(nodo2.acc_z) +

  "&gyr2_x=" + String(nodo2.gyr_x) +
  "&gyr2_y=" + String(nodo2.gyr_y) +
  "&gyr2_z=" + String(nodo2.gyr_z) +

  "&latitud=" + String(nodo2.latitud,6) +
  "&longitud=" + String(nodo2.longitud,6) +

  "&altitud=" + String(nodo2.altitud) +

  "&velocidad=" + String(nodo2.velocidad) +

  "&movimiento=" + String(alertaMovimiento);

  http.POST(datosPOST);

  http.end();
}

// =====================================================
// TASKS
// =====================================================

void taskOLED(void *pv) {

  while(1) {

    actualizarOLED();

    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

void taskAlertas(void *pv) {

  while(1) {

    evaluarAlertas();

    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

void taskServidor(void *pv) {

  while(1) {

    enviarServidor();

    vTaskDelay(pdMS_TO_TICKS(10000));
  }
}

void taskUmbrales(void *pv) {

  while(1) {

    obtenerUmbrales();

    vTaskDelay(pdMS_TO_TICKS(30000));
  }
}

// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);

  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);

  digitalWrite(LED_R, LOW);
  digitalWrite(LED_G, LOW);
  digitalWrite(LED_B, LOW);

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
    "NODO 3"
  );

  display.display();

  xMutex =
    xSemaphoreCreateMutex();

  conectarWiFi();

  iniciarHora();

  if(
    esp_now_init() != ESP_OK
  ) {

    return;
  }

  esp_now_register_recv_cb(
    OnDataRecv
  );

  agregarPeer(mac_nodo1);

  agregarPeer(mac_nodo2);

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
    taskAlertas,
    "ALERTAS",
    4096,
    NULL,
    1,
    NULL,
    1
  );

  xTaskCreatePinnedToCore(
    taskServidor,
    "SERVER",
    8192,
    NULL,
    1,
    NULL,
    1
  );

  xTaskCreatePinnedToCore(
    taskUmbrales,
    "UMBRALES",
    8192,
    NULL,
    1,
    NULL,
    1
  );
}

// =====================================================
// LOOP
// =====================================================

void loop() {

  delay(1000);
}
