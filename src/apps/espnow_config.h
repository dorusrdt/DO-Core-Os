#ifndef ESPNOW_CONFIG_H
#define ESPNOW_CONFIG_H

#include "espnow_common.h"
#include "../kernel/core/kernel.h"

// Charger la config depuis NVS
SysError_t espnow_config_load(DeviceConfig_t* config);

// Sauvegarder la config en NVS
SysError_t espnow_config_save(const DeviceConfig_t* config);

// Changer le master_id
SysError_t espnow_config_set_master_id(uint8_t new_master_id);

// Changer le device_index
SysError_t espnow_config_set_device_index(uint8_t new_index);

// Réinitialiser la config
SysError_t espnow_config_reset();

// Obtenir la config actuelle
const DeviceConfig_t* espnow_config_get();

#endif // ESPNOW_CONFIG_H
