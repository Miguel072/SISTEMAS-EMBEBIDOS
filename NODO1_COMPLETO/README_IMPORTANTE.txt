NODO 1 - ESP32-S3 TOUCH LCD FSM (ZIP CORREGIDO V2)

Cambios de esta version:
- LCD orientada sin espejo horizontal
- UI reubicada para verse mejor en pantalla circular
- Touch estable por interrupcion (no lectura continua)
- Touch ID deshabilitado en sdkconfig.defaults
- QMI8658 movido a components/qmi8658 para no depender de parches en managed_components
- Parche de CMake para esp_driver_i2c incluido
- Parche de M_PI incluido
- Correccion de la FSM: diferencia angular normalizada
- Correccion de la FSM: deteccion de caida menos sensible y con confirmacion

Pasos en VS Code:
1) Abrir esta carpeta como proyecto
2) Usar terminal ESP-IDF
3) Ejecutar:
   idf.py fullclean
   idf.py set-target esp32s3
   idf.py reconfigure
   idf.py build
   idf.py -p COMX flash monitor

Controles touch:
- CAL  : guarda postura de referencia
- RST  : limpia estado de caida
- VIEW : cambia entre pantalla de estado y pantalla de datos

Notas:
- Si la placa arranca quieta, el sistema puede autocalibrar una referencia inicial.
- Para una referencia mejor, deja el nodo quieto en postura normal y toca CAL.
- Esta version busca reducir las falsas CAIDAS cuando el angulo cruza 0/360 grados.


ESP-NOW: esta version recibe BPM del Nodo 2 por broadcast en canal 1 y lo muestra como FC en la LCD.
