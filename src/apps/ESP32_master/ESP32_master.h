#ifndef ESP32_MASTER_H
#define ESP32_MASTER_H

#include "../../kernel/core/kernel.h"

#define ESP32_MASTER_APP_ID  10

// API d'enregistrement
SysError_t ESP32_master_register_app();
// Obtenir l'ID réel assigné par le système
uint8_t ESP32_master_get_app_id(void);

#endif // ESP32_MASTER_H