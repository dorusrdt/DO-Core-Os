#ifndef ESP32_SENSOR_H
#define ESP32_SENSOR_H

#include "../../kernel/core/kernel.h"

#define ESP32_SENSOR_APP_ID  11

// API d'enregistrement
SysError_t ESP32_sensor_register_app();

#endif // ESP32_SENSOR_H