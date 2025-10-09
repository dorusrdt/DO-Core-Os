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

// ===== MODULE CAPTEURS =====

// Structure pour un capteur individuel
typedef struct {
    float moisture_percent;    // Humidité actuelle (0-100%)
    uint32_t last_read_time;   // Timestamp dernière lecture
    bool is_connected;         // État de connexion du capteur
    float temperature;         // Température ambiante (bonus)
    uint32_t read_count;       // Nombre de lectures effectuées
} SensorData_t;

// Structure pour le gestionnaire de capteurs
typedef struct {
    SensorData_t sensors[12];  // 12 capteurs max (comme dans config.h)
    uint32_t last_update;      // Timestamp dernière mise à jour globale
    bool simulation_mode;      // Mode simulation actif
    uint32_t total_reads;      // Compteur total de lectures
} SensorManager_t;

// Constantes pour les capteurs (comme dans config.h)
#define MOISTURE_DRY_VALUE    4095  // ADC sec (0% humidité)
#define MOISTURE_WET_VALUE    0     // ADC mouillé (100% humidité)
#define MOISTURE_SAMPLES      5     // Nombre d'échantillons pour moyennage

// ===== MODULE ZONES =====

// Constantes pour les zones
#define MAX_ZONES              4     // 4 zones maximum
#define SENSORS_PER_ZONE       3     // 3 capteurs par zone
#define MAX_ZONE_NAME_LENGTH   16    // Longueur max nom zone
#define IRRIGATION_TIME_LENGTH 6     // Format "HH:MM" + null

// États d'une zone
typedef enum {
    ZONE_STATE_INACTIVE = 0,    // Zone désactivée
    ZONE_STATE_ACTIVE,          // Zone active, attente irrigation
    ZONE_STATE_IRRIGATING,      // En cours d'irrigation
    ZONE_STATE_ERROR            // Erreur détectée
} ZoneState_t;

// Structure de configuration d'une zone
typedef struct {
    uint8_t zone_id;                    // 0-3
    bool configured;                   // Zone configurée
    char zone_name[MAX_ZONE_NAME_LENGTH]; // Nom de la zone
    uint16_t water_per_day_ml;         // Volume d'eau par jour (ml)
    char irrigation_time[IRRIGATION_TIME_LENGTH]; // Heure d'irrigation "HH:MM"
    uint8_t humidity_threshold;        // Seuil d'urgence humidité (%)
    uint8_t sensor_ids[SENSORS_PER_ZONE]; // IDs des capteurs associés
    bool auto_irrigation_enabled;      // Irrigation automatique activée
} ZoneConfig_t;

// Structure d'état runtime d'une zone
typedef struct {
    ZoneState_t state;                 // État actuel
    uint32_t last_irrigation_time;     // Timestamp dernière irrigation
    uint32_t total_water_used_ml;      // Eau totale utilisée (ml)
    uint32_t irrigation_count;         // Nombre d'irrigations
    float current_moisture_avg;        // Moyenne humidité actuelle
    uint32_t last_sensor_update;       // Dernière MAJ capteurs
} ZoneStatus_t;

// Structure complète d'une zone
typedef struct {
    ZoneConfig_t config;               // Configuration
    ZoneStatus_t status;               // État runtime
} Zone_t;

// Gestionnaire des zones
typedef struct {
    Zone_t zones[MAX_ZONES];           // 4 zones
    uint32_t last_config_update;       // Dernière MAJ config
    uint32_t total_irrigation_events;  // Total événements irrigation
    bool zones_initialized;            // Zones initialisées
} ZoneManager_t;

// Pins ADC pour les capteurs (comme dans config.h)
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

// Déclaration des fonctions de l'application
SysError_t irrig_app_master_init(void);
void irrig_app_master_start(void);
void irrig_app_master_loop(void);
void irrig_app_master_stop(void);

// ===== FONCTIONS MODULE CAPTEURS =====

// Initialisation du gestionnaire de capteurs
void sensor_manager_init(bool simulation_mode);

// Lecture de tous les capteurs
void sensor_read_all_sensors(void);

// Lecture d'un capteur spécifique
float sensor_get_moisture(uint8_t sensor_id);

// Calcul de la moyenne d'humidité pour une zone
float sensor_get_zone_average(uint8_t zone_id);

// Mise à jour simulation (comme dans le code de référence)
void sensor_update_simulation(void);

// Lecture réelle des capteurs ADC (TODO)
void sensor_read_real_sensors(void);

// Affichage des valeurs capteurs (debug)
void sensor_print_values(void);

// Validation d'une valeur d'humidité
bool sensor_validate_moisture(float value);

// Conversion ADC vers pourcentage (TODO)
float sensor_adc_to_percent(int adc_value);

// ===== FONCTIONS MODULE ZONES =====

// Initialisation du gestionnaire de zones
void zone_manager_init(void);

// Configuration d'une zone
SysError_t zone_configure(uint8_t zone_id, const ZoneConfig_t* config);

// Obtenir la configuration d'une zone
SysError_t zone_get_config(uint8_t zone_id, ZoneConfig_t* config);

// Obtenir l'état d'une zone
SysError_t zone_get_status(uint8_t zone_id, ZoneStatus_t* status);

// Mettre à jour les moyennes d'humidité des zones
void zone_update_moisture_averages(void);

// Vérifier si une zone nécessite irrigation
bool zone_needs_irrigation(uint8_t zone_id);

// Calculer le volume d'eau pour une zone
uint16_t zone_calculate_water_volume(uint8_t zone_id);

// Afficher l'état de toutes les zones
void zone_print_status(void);

// Validation d'une configuration de zone
bool zone_validate_config(const ZoneConfig_t* config);

// Réinitialiser une zone
SysError_t zone_reset(uint8_t zone_id);

// Vérifier si une zone est configurée
bool zone_is_configured(uint8_t zone_id);

// Obtenir le nombre de zones configurées
uint8_t zone_get_configured_count(void);

// ===== MODULE HTTP COMMUNICATION =====

// Codes d'erreur HTTP spécifiques
typedef enum {
    HTTP_IRRIG_OK = 0,
    HTTP_IRRIG_ERROR_INIT,
    HTTP_IRRIG_ERROR_CONNECT,
    HTTP_IRRIG_ERROR_TIMEOUT,
    HTTP_IRRIG_ERROR_AUTH,
    HTTP_IRRIG_ERROR_JSON,
    HTTP_IRRIG_ERROR_SERVER
} HttpIrrigError_t;

// Structure pour les données capteurs à envoyer
typedef struct {
    uint32_t timestamp;
    uint8_t zone_count;
    struct {
        uint8_t zone_id;
        float moisture_avg;
        uint8_t sensor_count;
        float sensor_values[3];  // 3 capteurs par zone
    } zones[4];
} SensorDataPayload_t;

// Structure pour la réponse de configuration serveur
typedef struct {
    bool config_updated;
    uint32_t server_timestamp;
    struct {
        bool configured;
        char zone_name[16];
        uint16_t water_per_day_ml;
        char irrigation_time[6];
        uint8_t humidity_threshold;
        bool auto_irrigation_enabled;
    } zones[4];
} ServerConfigResponse_t;

// Gestionnaire HTTP
typedef struct {
    bool initialized;
    uint32_t last_register_attempt;
    uint32_t last_data_send;
    uint32_t last_config_poll;
    uint32_t register_retry_count;
    uint32_t data_send_count;
    uint32_t config_poll_count;
    uint32_t error_count;
    HttpIrrigError_t last_error;
} HttpManager_t;

// ===== FONCTIONS MODULE HTTP =====

// Initialisation du gestionnaire HTTP
void http_manager_init(void);

// Enregistrement du device auprès du serveur
HttpIrrigError_t http_register_device(void);

// Envoi des données capteurs au serveur
HttpIrrigError_t http_send_sensor_data(void);

// Récupération de la configuration depuis le serveur
HttpIrrigError_t http_poll_server_config(void);

// Test de connectivité avec le serveur
HttpIrrigError_t http_test_connectivity(void);

// Affichage des statistiques HTTP
void http_print_stats(void);

// Fonctions utilitaires
String http_generate_hmac(const String& data, const String& secret);
String http_build_sensor_json(const SensorDataPayload_t* data);
bool http_parse_config_response(const String& json_response, ServerConfigResponse_t* config);
String http_get_error_string(HttpIrrigError_t error);

// Fonction d'enregistrement de l'application
SysError_t register_irrig_app_master(const IrrigAppConfig_t* config);

#endif // IRRIG_APP_MASTER_H