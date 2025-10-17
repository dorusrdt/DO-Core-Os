#ifndef IRRIG_IPC_H
#define IRRIG_IPC_H

#include "irrig_types.h"
#include <Arduino.h>

// ===== STRUCTURES DE DONNÉES IPC =====

// Paquet données capteurs (Slave 1 → Master)
typedef struct {
    uint32_t timestamp;
    float moisture[MAX_SENSORS];  // % humidité (0-100)
    float temperature;            // °C
    float humidity;               // %
    float pressure;               // hPa
    float battery_level;          // %
    int8_t signal_strength;       // dBm (RSSI)
} SensorDataPacket_t;

// Paquet commande irrigation (Master → Slave 2)
typedef struct {
    IrrigationCommand_t command;
    uint8_t zone_id;              // 1-4
    uint16_t duration_seconds;
    char zone_server_id[64];      // "zone_abc123"
    uint32_t timestamp;
} IrrigationCommandPacket_t;

// Paquet statut irrigation (Slave 2 → Master, optionnel)
typedef struct {
    uint8_t zone_id;              // 0 si aucune irrigation
    bool is_irrigating;
    uint32_t remaining_seconds;
    bool pump_running;
    bool relay_states[MAX_ZONES]; // État des 4 relais
    uint32_t timestamp;
} IrrigationStatusPacket_t;

#endif // IRRIG_IPC_H
