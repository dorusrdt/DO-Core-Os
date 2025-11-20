#ifndef ESPNOW_COMMON_H
#define ESPNOW_COMMON_H

#include <Arduino.h>

// ============ Types de trames ============
enum : uint8_t { 
    FRAME_BEACON = 1,
    FRAME_DATA = 2,
    FRAME_ACK = 3
};

// ============ Configuration du device (stockée en NVS) ============
typedef struct __attribute__((packed)) {
    uint16_t device_id;         // Basé sur MAC (unique, read-only)
    uint8_t master_id;          // ID du master (1-255)
    uint8_t device_index;       // Rôle (0=master, 1+=slave)
    uint32_t config_version;    // Version de la config
} DeviceConfig_t;

// ============ Beacon : Master annonce sa présence ============
typedef struct __attribute__((packed)) {
    uint8_t frame_type;         // FRAME_BEACON
    uint8_t version;            // 1
    uint16_t master_id;         // ID du master (pour filtrage)
    uint16_t device_id;         // ID unique du master
    uint8_t master_mac[6];      // MAC du master
    uint8_t channel;            // Canal WiFi actuel
    uint32_t beacon_id;         // ID unique du beacon (incrémenté)
    uint32_t uptime_ms;         // Uptime du master
} BeaconPacket_t;

// ============ Data : Slave envoie ses données ============
typedef struct __attribute__((packed)) {
    uint8_t frame_type;         // FRAME_DATA
    uint8_t version;            // 1
    uint16_t slave_id;          // ID unique du slave
    uint16_t master_id;         // ID du master (pour vérification)
    uint32_t seq;               // Numéro de séquence
    uint32_t timestamp;         // Timestamp local
    uint16_t humidity[12];      // 12 voies de données
} DataPacket_t;

// ============ ACK : Master confirme réception ============
typedef struct __attribute__((packed)) {
    uint8_t frame_type;         // FRAME_ACK
    uint8_t version;            // 1
    uint16_t slave_id;          // ID du slave
    uint32_t seq;               // Numéro de séquence du data reçu
} AckPacket_t;

#endif // ESPNOW_COMMON_H
