#ifndef ESPNOW_MASTER_H
#define ESPNOW_MASTER_H

#include "../../kernel/core/kernel.h"
#include <Arduino.h>

#define ESPNOW_MASTER_APP_ID  10

// API d'enregistrement
SysError_t espnow_master_register_app();

#endif // ESPNOW_MASTER_H
