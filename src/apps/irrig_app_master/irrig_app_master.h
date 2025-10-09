#ifndef IRRIG_APP_MASTER_H
#define IRRIG_APP_MASTER_H

#include "../../kernel/app/app_manager.h"

// Structure de configuration de l'application d'irrigation
typedef struct {
    // Configuration réseau
    char server_url[128];
    char device_id[64];
    char device_secret[32];

    // Configuration temporelle
    uint16_t poll_interval_seconds;
    uint16_t sensor_read_interval_seconds;
    uint16_t data_send_interval_seconds;

    // Configuration matérielle
    uint8_t max_zones;
    uint8_t max_sensors;

    // Mode de fonctionnement
    bool simulation_mode;
} IrrigAppConfig_t;

// Déclaration des fonctions de l'application
SysError_t irrig_app_master_init(void);
void irrig_app_master_start(void);
void irrig_app_master_loop(void);
void irrig_app_master_stop(void);

// Fonction d'enregistrement de l'application
SysError_t register_irrig_app_master(const IrrigAppConfig_t* config);

#endif // IRRIG_APP_MASTER_H