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

// Constantes pour les créneaux d'irrigation
#define MAX_SCHEDULES_PER_ZONE 5  // Maximum 5 créneaux par jour

// Structure d'un slot de zone (avec mapping physique flexible)
struct ZoneSlot {
    int id;                    // Index array (0-3)
    int physicalZoneNumber;    // Numéro physique du relais (1-4, défaut: id+1)
    bool configured;           // Zone configurée ou non
    String zoneId;             // ID serveur (ex: "zone_abc123")
    int waterPerDay;           // Volume d'eau par jour (ml)
    String irrigationTimes[MAX_SCHEDULES_PER_ZONE];  // Heures d'irrigation (ex: "08:00", "14:00")
    int scheduleCount;         // Nombre de créneaux configurés (1-5)
    int humidityThreshold;     // Seuil d'urgence (%)
};

// Structure d'un slot de capteur (exactement comme JS simulator)
struct SensorSlot {
    String id;                 // "s_01" à "s_12"
    bool assigned;             // Capteur assigné à une zone
    String zoneId;             // Référence à la zone (vide si non assigné)
};

// Utiliser constantes communes (pas de duplication)
#include "../irrig_common/irrig_types.h"

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

// Gestion capteurs (reçus des Slaves)
void updateGlobalEnvironmentData(void);  // ✅ Génère données globales (simulation)
void updateSensorDataFromSlave(float moisture[MAX_SENSORS], float temp, float hum, float press);

// Communication serveur
void sendSensorData(void);

// Gestion irrigation (commandes vers Slaves)
void checkIrrigationSchedule(void);
void checkMoistureThresholds(void);
void sendIrrigationCommand(String zoneId, int durationSeconds);
void checkIrrigationTimer(void);

// Affichage capteurs par zone
void displaySensorIdsPerZone(void);

// Utilitaires
String generateHMAC(String data);
String getTimestamp(void);



#endif // IRRIG_APP_MASTER_H