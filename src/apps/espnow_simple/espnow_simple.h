#ifndef ESPNOW_SIMPLE_H
#define ESPNOW_SIMPLE_H

#include "../../kernel/core/kernel.h"
#include <Arduino.h>

#define ESPNOW_SIMPLE_APP_ID  12

// API d'enregistrement
SysError_t espnow_simple_register_app();

#endif // ESPNOW_SIMPLE_H

