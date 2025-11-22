#ifndef ESP32_COM_H
#define ESP32_COM_H

#include "../../kernel/core/kernel.h"

#define ESP32_COM_APP_ID  12

// API d'enregistrement
SysError_t ESP32_com_register_app();
// Obtenir l'ID réel assigné par le système
uint8_t ESP32_com_get_app_id(void);

#endif // ESP32_COM_H