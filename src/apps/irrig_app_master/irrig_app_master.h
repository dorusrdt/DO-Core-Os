#ifndef IRRIG_APP_MASTER_H
#define IRRIG_APP_MASTER_H

#include "../../kernel/app/app_manager.h"
#include <Arduino.h>

// Structure de configuration de l'application d'irrigation
typedef struct {
    // Configuration réseau
    char server_url[128];
    char device_id[64];
    char device_secret[32];

    // Configuration temporelle
    uint16_t poll_interval_seconds;          // Poll config serveur (10s recommandé)
    uint16_t sensor_read_interval_seconds;   // Lecture capteurs (5s)
    uint16_t data_send_interval_seconds;     // Envoi données (15s)

    // Configuration matérielle
    uint8_t max_zones;                       // 4 zones max
    uint8_t max_sensors;                     // 12 capteurs max

    // Mode de fonctionnement
    bool simulation_mode;                    // true = simulation, false = hardware réel
} IrrigAppConfig_t;

// ===== ARCHITECTURE ZONE_STACK / SENSOR_STACK (Code Référence) =====

// Structure d'un slot de zone (exactement comme JS simulator)
struct ZoneSlot {
    int id;                    // 1-4 (ID physique du slot)
    bool configured;           // Zone configurée ou non
    String zoneId;             // ID serveur (ex: "zone_abc123")
    int waterPerDay;           // Volume d'eau par jour (ml)
    String irrigationTime;     // Heure irrigation "HH:MM"
    int humidityThreshold;     // Seuil d'urgence (%)
};

// Structure d'un slot de capteur (exactement comme JS simulator)
struct SensorSlot {
    String id;                 // "s_01" à "s_12"
    bool assigned;             // Capteur assigné à une zone
    String zoneId;             // Référence à la zone (vide si non assigné)
};

// Constantes pour les capteurs
#define MAX_ZONES 4
#define MAX_SENSORS 12
#define SENSORS_PER_ZONE 10        // Maximum capteurs par zone
#define MOISTURE_DRY_VALUE 4095    // ADC sec (0% humidité)
#define MOISTURE_WET_VALUE 0       // ADC mouillé (100% humidité)
#define MOISTURE_SAMPLES 5         // Nombre d'échantillons pour moyennage

// ===== HARDWARE PINS =====

// Pins relais (contrôle irrigation)
#define ZONE_1_RELAY_PIN 2
#define ZONE_2_RELAY_PIN 4
#define ZONE_3_RELAY_PIN 16
#define ZONE_4_RELAY_PIN 17
#define PUMP_RELAY_PIN   5

// Pins indicateurs
#define STATUS_LED_PIN   18
#define BUZZER_PIN       19

// Pins ADC pour les capteurs d'humidité
#define MOISTURE_PIN_1  32
#define MOISTURE_PIN_2  33
#define MOISTURE_PIN_3  34
#define MOISTURE_PIN_4  35
#define MOISTURE_PIN_5  36
#define MOISTURE_PIN_6  39
#define MOISTURE_PIN_7  25
#define MOISTURE_PIN_8  26
#define MOISTURE_PIN_9  27
#define MOISTURE_PIN_10 14
#define MOISTURE_PIN_11 12
#define MOISTURE_PIN_12 13

// Seuils d'urgence
#define CRITICAL_MOISTURE_THRESHOLD 15  // Irrigation d'urgence si < 15%
#define HIGH_TEMPERATURE_THRESHOLD  40  // Alerte si > 40°C

// ===== CALLBACKS DO-CORE APPLICATION =====
SysError_t irrig_app_master_init(void);
void irrig_app_master_start(void);
void irrig_app_master_loop(void);
void irrig_app_master_stop(void);

// Fonction d'enregistrement de l'application
SysError_t register_irrig_app_master(const IrrigAppConfig_t* config);

// ===== FONCTIONS PRINCIPALES (Code Référence) =====

// Initialisation hardware
void initializeHardware(void);
void initializeRealHardware(void);
void initializeSimulatedSensors(void);

// Gestion configuration
void registerDevice(void);
void pollConfiguration(void);
void parseConfiguration(String jsonResponse);
void handleZoneDeletion(String zoneId);

// Gestion capteurs
void readAllSensors(void);
void updateSimulatedSensors(void);
void readRealSensors(void);

// Communication serveur
void sendSensorData(void);

// Gestion irrigation
void checkIrrigationSchedule(void);
void checkMoistureThresholds(void);
void executeIrrigation(String zoneId, int durationSeconds);
void checkIrrigationTimer(void);

// Utilitaires
String generateHMAC(String data);
String getTimestamp(void);



#endif // IRRIG_APP_MASTER_H