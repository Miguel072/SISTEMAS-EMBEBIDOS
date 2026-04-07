# Nodo 2 XD-58C SOLO PULSO

Esta version solo mide frecuencia cardiaca por serial.

## Conexion
- VCC -> 3V3
- GND -> GND
- SIG -> GPIO34

## Comandos
```
idf.py fullclean
idf.py set-target esp32
idf.py reconfigure
idf.py build
idf.py -p COMX flash monitor
```

## Salida esperada
- Esperando senal valida
- BPM, IBI, RAW, calidad y dedo detectado


ESP-NOW: esta version envia BPM en broadcast por canal 1 hacia el Nodo 1.
