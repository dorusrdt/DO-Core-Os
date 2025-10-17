#ifndef IRRIG_APP_SLAVE_SENSORS_H
#define IRRIG_APP_SLAVE_SENSORS_H

#include "../../kernel/app/app_manager.h"
#include "../irrig_common/irrig_types.h"
#include "../irrig_common/irrig_ipc.h"
#include <Arduino.h>

// ===== CONFIGURATION SLAVE SENSORS =====

typedef struct {
    bool simulation_mode;          // true = simulation, false = hardware réel
    uint16_t read_interval_ms;     // Intervalle lecture capteurs (ex: 5000ms)
    uint8_t samples_per_read;      // Nombre échantillons pour moyennage (ex: 5)
    bool enable_http_server;       // Exposer serveur HTTP (pour mode multi-device)
    uint16_t http_server_port;     // Port serveur HTTP (ex: 8081)
} IrrigSensorConfig_t;

// ===== CALLBACKS DO-CORE APPLICATION =====

SysError_t irrig_app_slave_sensors_init(void);
void irrig_app_slave_sensors_start(void);
void irrig_app_slave_sensors_loop(void);
void irrig_app_slave_sensors_stop(void);

// Fonction d'enregistrement
SysError_t register_irrig_app_slave_sensors(const IrrigSensorConfig_t* config);

// ===== FONCTIONS PRINCIPALES =====

// Initialisation hardware
void sensors_init_hardware(void);
void sensors_init_real_hardware(void);
void sensors_init_simulation(void);

// Lecture capteurs
void sensors_read_all(SensorDataPacket_t* packet);
void sensors_read_real(float moisture[MAX_SENSORS]);
void sensors_update_simulation(float moisture[MAX_SENSORS]);

// Serveur HTTP (si enable_http_server = true)
void sensors_start_http_server(void);
void sensors_stop_http_server(void);
void sensors_handle_http_requests(void);

#endif // IRRIG_APP_SLAVE_SENSORS_H
