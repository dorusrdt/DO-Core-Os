#ifndef IRRIG_COMMUNICATION_H
#define IRRIG_COMMUNICATION_H

#include "irrig_ipc.h"
#include "../../kernel/core/kernel.h"
#include <Arduino.h>

// ===== CONFIGURATION COMMUNICATION =====

typedef struct {
    // Adresses réseau
    char master_ip[16];      // IP du Master
    uint16_t master_port;    // Port du Master (ex: 8080)
    char slave1_ip[16];      // IP du Slave 1 (Sensors)
    uint16_t slave1_port;    // Port du Slave 1 (ex: 8081)
    char slave2_ip[16];      // IP du Slave 2 (Relays)
    uint16_t slave2_port;    // Port du Slave 2 (ex: 8082)
    
    // Timeouts et retry
    uint16_t http_timeout_ms;
    uint8_t retry_count;
    uint16_t retry_delay_ms;
} IrrigCommConfig_t;

// ===== API COMMUNICATION HTTP =====

// Initialisation
SysError_t irrig_comm_init(IrrigCommConfig_t* config);

// Slave 1 → Master : Publier données capteurs
bool irrig_comm_publish_sensor_data(SensorDataPacket_t* data);

// Master → Slave 2 : Envoyer commande irrigation
bool irrig_comm_send_irrigation_command(IrrigationCommandPacket_t* cmd);

// Slave 2 → Master : Publier statut irrigation (optionnel)
bool irrig_comm_publish_irrigation_status(IrrigationStatusPacket_t* status);

// Obtenir configuration actuelle
IrrigCommConfig_t* irrig_comm_get_config(void);

#endif // IRRIG_COMMUNICATION_H
