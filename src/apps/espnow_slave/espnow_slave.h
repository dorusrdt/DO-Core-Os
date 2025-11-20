#ifndef ESPNOW_SLAVE_H
#define ESPNOW_SLAVE_H

#include "../../kernel/core/kernel.h"
#include <Arduino.h>

#define ESPNOW_SLAVE_APP_ID  11

// API d'enregistrement
SysError_t espnow_slave_register_app();

#endif // ESPNOW_SLAVE_H
