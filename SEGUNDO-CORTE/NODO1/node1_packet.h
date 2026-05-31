#pragma once

#include <stdint.h>

// ============================================================
// Paquete compatible con el Nodo 3 FINAL del grupo
// ============================================================
// IMPORTANTE:
// El Nodo 3 que enviaste valida que len == sizeof(datos_t).
// Por eso este struct DEBE tener el mismo orden, tipos y __packed__.
//
// Orden esperado por Nodo 3:
// pulso, spo2, acc_x/y/z, gyr_x/y/z, latitud, longitud,
// altitud, velocidad, movimiento, version.
//
// Para Nodo 1:
// - pulso y spo2 vienen del MAX30100.
// - acc/gyr vienen del QMI8658.
// - latitud/longitud/altitud/velocidad se envian en 0 porque son de Nodo 2.
// - movimiento se envia en 0 porque Nodo 3 calcula movimiento centralizado.

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
} nodo1_packet_t;
