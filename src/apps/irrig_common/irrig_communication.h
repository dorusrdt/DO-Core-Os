#ifndef IRRIG_COMMUNICATION_H
#define IRRIG_COMMUNICATION_H

#include "irrig_ipc.h"
#include "../../kernel/core/kernel.h"
#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

// ===== CONFIGURATION COMMUNICATION =====

typedef struct {
    // Adresses MAC (ESP-NOW)
    uint8_t master_mac[6];   // MAC du Master
    uint8_t slave1_mac[6];   // MAC du Slave 1 (Sensors)
    uint8_t slave2_mac[6];   // MAC du Slave 2 (Relays)

    // MAC locale (pour identifier ce device)
    uint8_t local_mac[6];

    // Timeouts et retry
    uint16_t send_timeout_ms;
    uint8_t retry_count;
    uint16_t retry_delay_ms;

    // Canal WiFi pour ESP-NOW (0 = auto)
    uint8_t wifi_channel;
} IrrigCommConfig_t;

// ===== API COMMUNICATION ESP-NOW =====

// Types de messages ESP-NOW
typedef enum {
    ESPNOW_MSG_SENSOR_DATA = 0x01,      // Slave1 → Master
    ESPNOW_MSG_IRRIGATION_CMD = 0x02,   // Master → Slave2
    ESPNOW_MSG_IRRIGATION_STATUS = 0x03  // Slave2 → Master
} EspNowMessageType_t;

// En-tête de message ESP-NOW
typedef struct {
    EspNowMessageType_t msg_type;
    uint32_t sequence;  // Numéro de séquence pour détecter les doublons
} EspNowHeader_t;

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

// Callbacks pour données reçues (à enregistrer par les apps)
typedef void (*SensorDataCallback_t)(SensorDataPacket_t* data);
typedef void (*IrrigationCommandCallback_t)(IrrigationCommandPacket_t* cmd);
typedef void (*IrrigationStatusCallback_t)(IrrigationStatusPacket_t* status);

// Enregistrer callbacks
void irrig_comm_set_sensor_callback(SensorDataCallback_t callback);
void irrig_comm_set_command_callback(IrrigationCommandCallback_t callback);
void irrig_comm_set_status_callback(IrrigationStatusCallback_t callback);

// Obtenir MAC locale
void irrig_comm_get_local_mac(uint8_t* mac);

#endif // IRRIG_COMMUNICATION_H
