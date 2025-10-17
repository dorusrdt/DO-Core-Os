#ifndef IRRIG_APP_MASTER_HTTP_H
#define IRRIG_APP_MASTER_HTTP_H

#include "../../kernel/core/kernel.h"
#include "../irrig_common/irrig_ipc.h"
#include <Arduino.h>

// ===== EXTENSION HTTP POUR MASTER =====

// Initialiser serveur HTTP du Master (pour recevoir données Slave 1)
SysError_t master_http_init(uint16_t port);

// Arrêter serveur HTTP
void master_http_stop(void);

// Gérer requêtes HTTP (à appeler dans loop)
void master_http_handle_requests(void);

// Callbacks pour données reçues
typedef void (*SensorDataCallback_t)(SensorDataPacket_t* data);
typedef void (*IrrigationStatusCallback_t)(IrrigationStatusPacket_t* status);

// Enregistrer callbacks
void master_http_set_sensor_callback(SensorDataCallback_t callback);
void master_http_set_status_callback(IrrigationStatusCallback_t callback);

#endif // IRRIG_APP_MASTER_HTTP_H
