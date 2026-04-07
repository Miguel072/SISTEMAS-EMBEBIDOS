cmake_minimum_required(VERSION 3.16)

# ============================================================
# Proyecto : Nodo Cardíaco ADL — XD-58C
# Materia  : Sistemas Embebidos y Tiempo Real
# Chip     : ESP32 (devkit)
# Sensor   : XD-58C (analógico — fotopletismografía verde 515 nm)
# Framework: ESP-IDF v5.x + FreeRTOS
# ============================================================

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(nodo_cardiaco)
