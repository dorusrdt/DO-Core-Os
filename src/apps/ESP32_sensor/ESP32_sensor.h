#ifndef ESP32_SENSOR_H
#define ESP32_SENSOR_H

#include "../../kernel/core/kernel.h"

#define ESP32_SENSOR_APP_ID  11

// API d'enregistrement
SysError_t ESP32_sensor_register_app();
// Obtenir l'ID réel assigné par le système
uint8_t ESP32_sensor_get_app_id(void);

#endif // ESP32_SENSOR_H