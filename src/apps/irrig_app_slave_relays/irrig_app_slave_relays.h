#ifndef IRRIG_APP_SLAVE_RELAYS_H
#define IRRIG_APP_SLAVE_RELAYS_H

#include "../../kernel/app/app_manager.h"
#include "../irrig_common/irrig_types.h"
#include "../irrig_common/irrig_ipc.h"
#include <Arduino.h>

// ===== CONFIGURATION SLAVE RELAYS =====

typedef struct {
    uint32_t safety_timeout_ms;    // Timeout sécurité irrigation (ex: 300000ms = 5min)
    bool enable_http_server;       // Exposer serveur HTTP
    uint16_t http_server_port;     // Port serveur HTTP (ex: 8082)
    uint16_t status_publish_interval_ms;  // Intervalle publication statut (ex: 10000ms)
} IrrigRelayConfig_t;

// ===== CALLBACKS DO-CORE APPLICATION =====

SysError_t irrig_app_slave_relays_init(void);
void irrig_app_slave_relays_start(void);
void irrig_app_slave_relays_loop(void);
void irrig_app_slave_relays_stop(void);

// Fonction d'enregistrement
SysError_t register_irrig_app_slave_relays(const IrrigRelayConfig_t* config);

// ===== FONCTIONS PRINCIPALES =====

// Initialisation hardware
void relays_init_hardware(void);

// Exécution commandes
void relays_execute_command(IrrigationCommandPacket_t* cmd);
void relays_start_irrigation(uint8_t zone_id, uint16_t duration_seconds);
void relays_stop_irrigation(void);
void relays_emergency_stop(void);

// Gestion timers
void relays_check_irrigation_timer(void);

// Contrôle GPIO
void relays_set_zone(uint8_t zone_id, bool state);
void relays_set_pump(bool state);
void relays_set_all_off(void);

// Statut
void relays_get_status(IrrigationStatusPacket_t* status);

// Serveur HTTP
void relays_start_http_server(void);
void relays_stop_http_server(void);
void relays_handle_http_requests(void);

#endif // IRRIG_APP_SLAVE_RELAYS_H
