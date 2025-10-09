#include "irrig_app_master.h"
#include "../../kernel/core/log_system_optimized.h"
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Variables statiques de l'application
static IrrigAppConfig_t app_config;
static bool app_initialized = false;
static uint32_t loop_counter = 0;

// ===== VARIABLES MODULE CAPTEURS =====
static SensorManager_t sensor_manager;
static unsigned long last_sensor_read = 0;

// ===== VARIABLES MODULE ZONES =====
static ZoneManager_t zone_manager;

// ===== VARIABLES MODULE HTTP =====
static HttpManager_t http_manager;

// Fonctions de l'application
SysError_t irrig_app_master_init(void) {
    if (app_initialized) {
        return SYS_ALREADY_INITIALIZED;
    }

    kernel_log(LOG_LEVEL_INFO, "IrrigAppMaster: Initializing irrigation system...");
    kernel_log(LOG_LEVEL_INFO, "IrrigAppMaster: Device ID: %s", app_config.device_id);
    kernel_log(LOG_LEVEL_INFO, "IrrigAppMaster: Server: %s", app_config.server_url);
    kernel_log(LOG_LEVEL_INFO, "IrrigAppMaster: Max zones: %d, Max sensors: %d",
               app_config.max_zones, app_config.max_sensors);
    kernel_log(LOG_LEVEL_INFO, "IrrigAppMaster: Simulation mode: %s",
               app_config.simulation_mode ? "ENABLED" : "DISABLED");

    // Initialiser le module capteurs
    sensor_manager_init(app_config.simulation_mode);
    last_sensor_read = 0;

    // Initialiser le module zones
    zone_manager_init();

    // Initialiser le module HTTP
    http_manager_init();

    app_initialized = true;
    loop_counter = 0;

    return SYS_OK;
}

void irrig_app_master_start(void) {
    kernel_log(LOG_LEVEL_INFO, "IrrigAppMaster: Starting irrigation system...");

    if (app_config.simulation_mode) {
        kernel_log(LOG_LEVEL_INFO, "IrrigAppMaster: Running in SIMULATION mode");
    } else {
        kernel_log(LOG_LEVEL_INFO, "IrrigAppMaster: Running in REAL HARDWARE mode");
    }

    // Tester la connectivité serveur
    kernel_log(LOG_LEVEL_INFO, "IrrigAppMaster: Testing server connectivity...");
    HttpIrrigError_t connectivity_result = http_test_connectivity();
    if (connectivity_result == HTTP_IRRIG_OK) {
        kernel_log(LOG_LEVEL_INFO, "IrrigAppMaster: Server connectivity OK");

        // Tenter l'enregistrement du device
        kernel_log(LOG_LEVEL_INFO, "IrrigAppMaster: Registering device with server...");
        HttpIrrigError_t register_result = http_register_device();
        if (register_result == HTTP_IRRIG_OK) {
            kernel_log(LOG_LEVEL_INFO, "IrrigAppMaster: Device registration successful");
        } else {
            kernel_log(LOG_LEVEL_WARN, "IrrigAppMaster: Device registration failed - will retry later");
        }
    } else {
        kernel_log(LOG_LEVEL_WARN, "IrrigAppMaster: Server connectivity failed - will retry later");
    }

    kernel_log(LOG_LEVEL_INFO, "IrrigAppMaster: System ready for irrigation control");
}

void irrig_app_master_loop(void) {
    // Boucle principale de l'application
    loop_counter++;
    unsigned long current_time = millis();

    // Lecture des capteurs selon l'intervalle configuré
    if (current_time - last_sensor_read >= (app_config.sensor_read_interval_seconds * 1000)) {
        sensor_read_all_sensors();
        last_sensor_read = current_time;

        // Mettre à jour les moyennes d'humidité des zones
        zone_update_moisture_averages();

        // Log de lecture capteurs
        kernel_log(LOG_LEVEL_DEBUG, "IrrigAppMaster: Sensors read - Total reads: %lu",
                   sensor_manager.total_reads);
    }

    // Log périodique pour montrer que l'application fonctionne
    if (loop_counter % 10 == 0) {  // Tous les 10 cycles (10 secondes)
        kernel_log(LOG_LEVEL_INFO, "IrrigAppMaster: Running... (loop #%lu)", loop_counter);
        sensor_print_values(); // Afficher les valeurs capteurs périodiquement
    }

    // Communication HTTP selon les intervalles configurés
    // Envoi des données capteurs (data_send_interval_seconds)
    if (current_time - http_manager.last_data_send >= (app_config.data_send_interval_seconds * 1000)) {
        HttpIrrigError_t result = http_send_sensor_data();
        if (result == HTTP_IRRIG_OK) {
            kernel_log(LOG_LEVEL_DEBUG, "IrrigAppMaster: Sensor data sent to server");
        } else {
            kernel_log(LOG_LEVEL_WARN, "IrrigAppMaster: Failed to send sensor data - Error: %s",
                       http_get_error_string(result).c_str());
        }
    }

    // Poll de la configuration serveur (poll_interval_seconds)
    if (current_time - http_manager.last_config_poll >= (app_config.poll_interval_seconds * 1000)) {
        HttpIrrigError_t result = http_poll_server_config();
        if (result == HTTP_IRRIG_OK) {
            kernel_log(LOG_LEVEL_DEBUG, "IrrigAppMaster: Server config polled successfully");
        } else {
            kernel_log(LOG_LEVEL_WARN, "IrrigAppMaster: Failed to poll server config - Error: %s",
                       http_get_error_string(result).c_str());
        }
    }

    // Afficher l'état des zones tous les 30 cycles (30 secondes)
    if (loop_counter % 30 == 0) {
        zone_print_status();
    }

    // Afficher les stats HTTP tous les 60 cycles (60 secondes)
    if (loop_counter % 60 == 0) {
        http_print_stats();
    }

    // Simulation d'activité
    if (app_config.simulation_mode && loop_counter % 5 == 0) {
        kernel_log(LOG_LEVEL_DEBUG, "IrrigAppMaster: Simulating sensor readings...");
    }

    // Délai obligatoire pour éviter la surcharge CPU
    vTaskDelay(pdMS_TO_TICKS(1000)); // 1 seconde
}

void irrig_app_master_stop(void) {
    kernel_log(LOG_LEVEL_INFO, "IrrigAppMaster: Stopping irrigation system...");
    kernel_log(LOG_LEVEL_INFO, "IrrigAppMaster: Total loops executed: %lu", loop_counter);
    kernel_log(LOG_LEVEL_INFO, "IrrigAppMaster: Cleanup completed");

    app_initialized = false;
    loop_counter = 0;
}

SysError_t register_irrig_app_master(const IrrigAppConfig_t* config) {
    if (!config) {
        kernel_log(LOG_LEVEL_ERROR, "IrrigAppMaster: Invalid configuration provided");
        return SYS_INVALID_PARAM;
    }

    // Copie de la configuration
    memcpy(&app_config, config, sizeof(IrrigAppConfig_t));

    // Validation de base de la configuration
    if (strlen(app_config.device_id) == 0) {
        kernel_log(LOG_LEVEL_ERROR, "IrrigAppMaster: Device ID cannot be empty");
        return SYS_INVALID_PARAM;
    }

    if (strlen(app_config.server_url) == 0) {
        kernel_log(LOG_LEVEL_ERROR, "IrrigAppMaster: Server URL cannot be empty");
        return SYS_INVALID_PARAM;
    }

    // Création des callbacks de l'application
    AppCallbacks_t callbacks = {0};
    callbacks.init = irrig_app_master_init;
    callbacks.start = irrig_app_master_start;
    callbacks.stop = irrig_app_master_stop;
    callbacks.loop = irrig_app_master_loop;
    // pause et resume sont optionnels, laissés à NULL

    uint8_t app_id;

    // Enregistrement de l'application
    SysError_t result = app_register("IrrigAppMaster",
                                   "Smart Irrigation Control System",
                                   APP_TYPE_USER,
                                   &callbacks,
                                   &app_id);

    if (result == SYS_OK) {
        kernel_log(LOG_LEVEL_INFO, "IrrigAppMaster: Successfully registered with ID %d", app_id);
    } else {
        kernel_log(LOG_LEVEL_ERROR, "IrrigAppMaster: Failed to register application (error: %d)", result);
    }

    return result;
}

// ===== IMPLEMENTATION MODULE CAPTEURS =====

// Initialisation du gestionnaire de capteurs
void sensor_manager_init(bool simulation_mode) {
    sensor_manager.simulation_mode = simulation_mode;
    sensor_manager.last_update = 0;
    sensor_manager.total_reads = 0;

    // Initialiser tous les capteurs
    for (int i = 0; i < 12; i++) {
        sensor_manager.sensors[i].moisture_percent = 0.0f;
        sensor_manager.sensors[i].last_read_time = 0;
        sensor_manager.sensors[i].is_connected = true;
        sensor_manager.sensors[i].temperature = 24.5f;
        sensor_manager.sensors[i].read_count = 0;
    }

    kernel_log(LOG_LEVEL_INFO, "SensorManager: Initialized (%s mode) - 12 sensors ready",
               simulation_mode ? "SIMULATION" : "REAL HARDWARE");
}

// Lecture de tous les capteurs
void sensor_read_all_sensors(void) {
    if (sensor_manager.simulation_mode) {
        sensor_update_simulation();
    } else {
        sensor_read_real_sensors();
    }

    sensor_manager.last_update = millis();
    sensor_manager.total_reads++;

    // Log de debug occasionnel
    if (sensor_manager.total_reads % 10 == 0) {
        kernel_log(LOG_LEVEL_DEBUG, "SensorManager: %lu total sensor reads completed",
                   sensor_manager.total_reads);
    }
}

// Lecture d'un capteur spécifique
float sensor_get_moisture(uint8_t sensor_id) {
    if (sensor_id >= 12) {
        kernel_log(LOG_LEVEL_WARN, "SensorManager: Invalid sensor ID %d (max 11)", sensor_id);
        return 0.0f;
    }

    return sensor_manager.sensors[sensor_id].moisture_percent;
}

// Calcul de la moyenne d'humidité pour une zone (3 capteurs par zone)
float sensor_get_zone_average(uint8_t zone_id) {
    if (zone_id >= 4) { // 4 zones max
        kernel_log(LOG_LEVEL_WARN, "SensorManager: Invalid zone ID %d (max 3)", zone_id);
        return 0.0f;
    }

    // Chaque zone a 3 capteurs (zone 0: capteurs 0,1,2; zone 1: 3,4,5, etc.)
    uint8_t start_sensor = zone_id * 3;
    float total = 0.0f;
    uint8_t valid_sensors = 0;

    for (uint8_t i = 0; i < 3; i++) {
        uint8_t sensor_id = start_sensor + i;
        if (sensor_id < 12 && sensor_manager.sensors[sensor_id].is_connected) {
            total += sensor_manager.sensors[sensor_id].moisture_percent;
            valid_sensors++;
        }
    }

    if (valid_sensors == 0) {
        kernel_log(LOG_LEVEL_WARN, "SensorManager: No valid sensors in zone %d", zone_id);
        return 0.0f;
    }

    float average = total / valid_sensors;
    kernel_log(LOG_LEVEL_DEBUG, "SensorManager: Zone %d average moisture: %.1f%% (%d sensors)",
               zone_id, average, valid_sensors);

    return average;
}

// Mise à jour simulation (basé sur le code de référence)
void sensor_update_simulation(void) {
    // Simulation réaliste comme dans le code de référence
    for (int i = 0; i < 12; i++) {
        // Base différente pour chaque capteur (30-42%)
        float base_value = 30 + (i * 1); // 30% to 41%

        // Variation réaliste (±5%)
        float variation = (random(-500, 501) / 100.0); // -5.00 to +5.00

        // Calculer nouvelle valeur
        float new_value = base_value + variation;

        // Contraindre entre 15% et 75%
        new_value = constrain(new_value, 15.0, 75.0);

        // Mettre à jour le capteur
        sensor_manager.sensors[i].moisture_percent = new_value;
        sensor_manager.sensors[i].last_read_time = millis();
        sensor_manager.sensors[i].read_count++;
        sensor_manager.sensors[i].temperature = 24.5f + (random(-200, 201) / 100.0); // ±2°C
    }

    kernel_log(LOG_LEVEL_DEBUG, "SensorManager: Simulation updated - 12 sensors");
}

// Lecture réelle des capteurs ADC (TODO - à implémenter)
void sensor_read_real_sensors(void) {
    // TODO: Implémenter la lecture réelle des GPIO ADC
    // Pour l'instant, utiliser des valeurs fixes pour test
    kernel_log(LOG_LEVEL_WARN, "SensorManager: Real sensor reading not implemented yet");

    for (int i = 0; i < 12; i++) {
        sensor_manager.sensors[i].moisture_percent = 50.0f; // Valeur fixe temporaire
        sensor_manager.sensors[i].last_read_time = millis();
        sensor_manager.sensors[i].read_count++;
    }
}

// Affichage des valeurs capteurs (debug)
void sensor_print_values(void) {
    kernel_log(LOG_LEVEL_INFO, "=== SENSOR VALUES ===");

    for (int zone = 0; zone < 4; zone++) {
        float zone_avg = sensor_get_zone_average(zone);
        kernel_log(LOG_LEVEL_INFO, "Zone %d: %.1f%% avg", zone, zone_avg);

        // Afficher les 3 capteurs de la zone
        for (int s = 0; s < 3; s++) {
            uint8_t sensor_id = zone * 3 + s;
            if (sensor_id < 12) {
                float moisture = sensor_manager.sensors[sensor_id].moisture_percent;
                kernel_log(LOG_LEVEL_INFO, "  S%d: %.1f%% (reads: %lu)",
                          sensor_id, moisture, sensor_manager.sensors[sensor_id].read_count);
            }
        }
    }

    kernel_log(LOG_LEVEL_INFO, "Total sensor reads: %lu", sensor_manager.total_reads);
}

// Validation d'une valeur d'humidité
bool sensor_validate_moisture(float value) {
    return (value >= 0.0f && value <= 100.0f);
}

// Conversion ADC vers pourcentage (TODO - à implémenter)
float sensor_adc_to_percent(int adc_value) {
    // TODO: Implémenter la conversion ADC vers pourcentage
    // Formule basée sur MOISTURE_DRY_VALUE et MOISTURE_WET_VALUE
    if (adc_value <= MOISTURE_WET_VALUE) return 100.0f;
    if (adc_value >= MOISTURE_DRY_VALUE) return 0.0f;

    // Interpolation linéaire
    float percent = 100.0f - ((adc_value - MOISTURE_WET_VALUE) *
                              100.0f / (MOISTURE_DRY_VALUE - MOISTURE_WET_VALUE));
    return constrain(percent, 0.0f, 100.0f);
}

// ===== IMPLEMENTATION MODULE ZONES =====

// Initialisation du gestionnaire de zones
void zone_manager_init(void) {
    zone_manager.last_config_update = 0;
    zone_manager.total_irrigation_events = 0;
    zone_manager.zones_initialized = false;

    // Initialiser toutes les zones
    for (int i = 0; i < MAX_ZONES; i++) {
        // Configuration par défaut
        zone_manager.zones[i].config.zone_id = i;
        zone_manager.zones[i].config.configured = false;
        strcpy(zone_manager.zones[i].config.zone_name, "Zone X");
        zone_manager.zones[i].config.zone_name[5] = '0' + i; // Zone 0, 1, 2, 3
        zone_manager.zones[i].config.water_per_day_ml = 2000; // 2L par défaut
        strcpy(zone_manager.zones[i].config.irrigation_time, "08:00"); // 8h du matin
        zone_manager.zones[i].config.humidity_threshold = 25; // 25% seuil urgence
        zone_manager.zones[i].config.auto_irrigation_enabled = true;

        // Association automatique des capteurs (3 par zone)
        for (int s = 0; s < SENSORS_PER_ZONE; s++) {
            zone_manager.zones[i].config.sensor_ids[s] = (i * SENSORS_PER_ZONE) + s;
        }

        // État initial
        zone_manager.zones[i].status.state = ZONE_STATE_INACTIVE;
        zone_manager.zones[i].status.last_irrigation_time = 0;
        zone_manager.zones[i].status.total_water_used_ml = 0;
        zone_manager.zones[i].status.irrigation_count = 0;
        zone_manager.zones[i].status.current_moisture_avg = 0.0f;
        zone_manager.zones[i].status.last_sensor_update = 0;
    }

    zone_manager.zones_initialized = true;
    kernel_log(LOG_LEVEL_INFO, "ZoneManager: Initialized - 4 zones ready with default config");
}

// Configuration d'une zone
SysError_t zone_configure(uint8_t zone_id, const ZoneConfig_t* config) {
    if (zone_id >= MAX_ZONES) {
        kernel_log(LOG_LEVEL_ERROR, "ZoneManager: Invalid zone ID %d (max %d)", zone_id, MAX_ZONES-1);
        return SYS_INVALID_PARAM;
    }

    if (!zone_validate_config(config)) {
        kernel_log(LOG_LEVEL_ERROR, "ZoneManager: Invalid configuration for zone %d", zone_id);
        return SYS_INVALID_PARAM;
    }

    // Copier la configuration
    memcpy(&zone_manager.zones[zone_id].config, config, sizeof(ZoneConfig_t));
    zone_manager.zones[zone_id].config.zone_id = zone_id; // Forcer l'ID correct
    zone_manager.last_config_update = millis();

    kernel_log(LOG_LEVEL_INFO, "ZoneManager: Zone %d configured - Name: %s, Water: %dml, Time: %s",
               zone_id, config->zone_name, config->water_per_day_ml, config->irrigation_time);

    return SYS_OK;
}

// Obtenir la configuration d'une zone
SysError_t zone_get_config(uint8_t zone_id, ZoneConfig_t* config) {
    if (zone_id >= MAX_ZONES || !config) {
        return SYS_INVALID_PARAM;
    }

    memcpy(config, &zone_manager.zones[zone_id].config, sizeof(ZoneConfig_t));
    return SYS_OK;
}

// Obtenir l'état d'une zone
SysError_t zone_get_status(uint8_t zone_id, ZoneStatus_t* status) {
    if (zone_id >= MAX_ZONES || !status) {
        return SYS_INVALID_PARAM;
    }

    memcpy(status, &zone_manager.zones[zone_id].status, sizeof(ZoneStatus_t));
    return SYS_OK;
}

// Mettre à jour les moyennes d'humidité des zones
void zone_update_moisture_averages(void) {
    for (int zone_id = 0; zone_id < MAX_ZONES; zone_id++) {
        if (zone_manager.zones[zone_id].config.configured) {
            float avg = sensor_get_zone_average(zone_id);
            zone_manager.zones[zone_id].status.current_moisture_avg = avg;
            zone_manager.zones[zone_id].status.last_sensor_update = millis();
        }
    }

    kernel_log(LOG_LEVEL_DEBUG, "ZoneManager: Moisture averages updated for all zones");
}

// Vérifier si une zone nécessite irrigation
bool zone_needs_irrigation(uint8_t zone_id) {
    if (zone_id >= MAX_ZONES || !zone_manager.zones[zone_id].config.configured) {
        return false;
    }

    ZoneConfig_t* config = &zone_manager.zones[zone_id].config;
    ZoneStatus_t* status = &zone_manager.zones[zone_id].status;

    // Vérifier le seuil d'urgence
    if (status->current_moisture_avg < config->humidity_threshold) {
        kernel_log(LOG_LEVEL_WARN, "ZoneManager: Zone %d needs irrigation - Moisture: %.1f%% < Threshold: %d%%",
                   zone_id, status->current_moisture_avg, config->humidity_threshold);
        return true;
    }

    // TODO: Vérifier l'heure programmée
    // Pour l'instant, seulement le seuil d'urgence
    return false;
}

// Calculer le volume d'eau pour une zone
uint16_t zone_calculate_water_volume(uint8_t zone_id) {
    if (zone_id >= MAX_ZONES || !zone_manager.zones[zone_id].config.configured) {
        return 0;
    }

    return zone_manager.zones[zone_id].config.water_per_day_ml;
}

// Afficher l'état de toutes les zones
void zone_print_status(void) {
    kernel_log(LOG_LEVEL_INFO, "=== ZONE STATUS ===");

    for (int zone_id = 0; zone_id < MAX_ZONES; zone_id++) {
        ZoneConfig_t* config = &zone_manager.zones[zone_id].config;
        ZoneStatus_t* status = &zone_manager.zones[zone_id].status;

        const char* state_str = "UNKNOWN";
        switch (status->state) {
            case ZONE_STATE_INACTIVE: state_str = "INACTIVE"; break;
            case ZONE_STATE_ACTIVE: state_str = "ACTIVE"; break;
            case ZONE_STATE_IRRIGATING: state_str = "IRRIGATING"; break;
            case ZONE_STATE_ERROR: state_str = "ERROR"; break;
        }

        if (config->configured) {
            kernel_log(LOG_LEVEL_INFO, "Zone %d (%s): %s", zone_id, config->zone_name, state_str);
            kernel_log(LOG_LEVEL_INFO, "  Moisture: %.1f%%, Threshold: %d%%, Water/day: %dml",
                       status->current_moisture_avg, config->humidity_threshold, config->water_per_day_ml);
            kernel_log(LOG_LEVEL_INFO, "  Irrigation time: %s, Auto: %s",
                       config->irrigation_time, config->auto_irrigation_enabled ? "ON" : "OFF");
            kernel_log(LOG_LEVEL_INFO, "  Total water: %lu ml, Irrigations: %lu",
                       status->total_water_used_ml, status->irrigation_count);
        } else {
            kernel_log(LOG_LEVEL_INFO, "Zone %d: NOT CONFIGURED", zone_id);
        }
    }

    kernel_log(LOG_LEVEL_INFO, "Total irrigation events: %lu", zone_manager.total_irrigation_events);
}

// Validation d'une configuration de zone
bool zone_validate_config(const ZoneConfig_t* config) {
    if (!config) return false;

    // Vérifier l'ID de zone
    if (config->zone_id >= MAX_ZONES) return false;

    // Vérifier le nom (non vide)
    if (strlen(config->zone_name) == 0) return false;

    // Vérifier le volume d'eau (raisonnable)
    if (config->water_per_day_ml == 0 || config->water_per_day_ml > 10000) return false;

    // Vérifier le seuil d'humidité (0-100%)
    if (config->humidity_threshold > 100) return false;

    // Vérifier l'heure d'irrigation (format HH:MM)
    if (strlen(config->irrigation_time) != 5) return false;
    if (config->irrigation_time[2] != ':') return false;

    // Parser l'heure
    int hour = atoi(config->irrigation_time);
    int minute = atoi(config->irrigation_time + 3);
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59) return false;

    return true;
}

// Réinitialiser une zone
SysError_t zone_reset(uint8_t zone_id) {
    if (zone_id >= MAX_ZONES) {
        return SYS_INVALID_PARAM;
    }

    // Réinitialiser l'état
    zone_manager.zones[zone_id].status.state = ZONE_STATE_INACTIVE;
    zone_manager.zones[zone_id].status.last_irrigation_time = 0;
    zone_manager.zones[zone_id].status.total_water_used_ml = 0;
    zone_manager.zones[zone_id].status.irrigation_count = 0;
    zone_manager.zones[zone_id].status.current_moisture_avg = 0.0f;
    zone_manager.zones[zone_id].status.last_sensor_update = 0;

    kernel_log(LOG_LEVEL_INFO, "ZoneManager: Zone %d reset to initial state", zone_id);
    return SYS_OK;
}

// Vérifier si une zone est configurée
bool zone_is_configured(uint8_t zone_id) {
    if (zone_id >= MAX_ZONES) return false;
    return zone_manager.zones[zone_id].config.configured;
}

// Obtenir le nombre de zones configurées
uint8_t zone_get_configured_count(void) {
    uint8_t count = 0;
    for (int i = 0; i < MAX_ZONES; i++) {
        if (zone_manager.zones[i].config.configured) count++;
    }
    return count;
}

// ===== IMPLEMENTATION MODULE HTTP =====

// Initialisation du gestionnaire HTTP
void http_manager_init(void) {
    http_manager.initialized = false;
    http_manager.last_register_attempt = 0;
    http_manager.last_data_send = 0;
    http_manager.last_config_poll = 0;
    http_manager.register_retry_count = 0;
    http_manager.data_send_count = 0;
    http_manager.config_poll_count = 0;
    http_manager.error_count = 0;
    http_manager.last_error = HTTP_IRRIG_OK;

    http_manager.initialized = true;
    kernel_log(LOG_LEVEL_INFO, "HTTP Manager: Initialized - Ready for server communication");
}

// Enregistrement du device auprès du serveur
HttpIrrigError_t http_register_device(void) {
    if (!http_manager.initialized) {
        return HTTP_IRRIG_ERROR_INIT;
    }

    // Vérifier la connectivité WiFi
    if (WiFi.status() != WL_CONNECTED) {
        kernel_log(LOG_LEVEL_WARN, "HTTP Manager: No WiFi connection for device registration");
        http_manager.last_error = HTTP_IRRIG_ERROR_CONNECT;
        http_manager.error_count++;
        return HTTP_IRRIG_ERROR_CONNECT;
    }

    HTTPClient http;
    http.setTimeout(10000); // 10 secondes timeout

    // Construction de l'URL d'enregistrement
    String register_url = String(app_config.server_url) + "/api/devices/register";

    kernel_log(LOG_LEVEL_INFO, "HTTP Manager: Registering device at %s", register_url.c_str());

    // Préparation des données JSON
    DynamicJsonDocument doc(512);
    doc["device_id"] = app_config.device_id;
    doc["device_secret"] = app_config.device_secret;
    doc["device_type"] = "ESP32_IRRIGATION_MASTER";
    doc["firmware_version"] = "0.3.0";
    doc["capabilities"]["zones"] = 4;
    doc["capabilities"]["sensors"] = 12;
    doc["capabilities"]["simulation_mode"] = app_config.simulation_mode;

    String json_payload;
    serializeJson(doc, json_payload);

    // Génération HMAC pour authentification
    String hmac_signature = http_generate_hmac(json_payload, app_config.device_secret);

    // Configuration de la requête
    http.begin(register_url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-HMAC-Signature", hmac_signature);
    http.addHeader("User-Agent", "ESP32-Irrigation-Master/0.3.0");

    // Envoi de la requête
    int http_code = http.POST(json_payload);

    kernel_log(LOG_LEVEL_DEBUG, "HTTP Manager: Register POST returned code %d", http_code);

    if (http_code == HTTP_CODE_OK || http_code == HTTP_CODE_CREATED) {
        // Succès
        String response = http.getString();
        kernel_log(LOG_LEVEL_INFO, "HTTP Manager: Device registered successfully");

        // Parser la réponse pour récupérer le token si nécessaire
        DynamicJsonDocument response_doc(256);
        DeserializationError error = deserializeJson(response_doc, response);

        if (!error) {
            if (response_doc.containsKey("device_token")) {
                // Stocker le token pour les futures requêtes
                kernel_log(LOG_LEVEL_DEBUG, "HTTP Manager: Device token received");
            }
        }

        http_manager.last_register_attempt = millis();
        http_manager.register_retry_count = 0;
        http.end();
        return HTTP_IRRIG_OK;

    } else {
        // Erreur
        String error_msg = http.errorToString(http_code);
        kernel_log(LOG_LEVEL_ERROR, "HTTP Manager: Device registration failed - Code: %d, Error: %s",
                   http_code, error_msg.c_str());

        http_manager.last_error = HTTP_IRRIG_ERROR_SERVER;
        http_manager.error_count++;
        http_manager.register_retry_count++;

        http.end();
        return HTTP_IRRIG_ERROR_SERVER;
    }
}

// Envoi des données capteurs au serveur
HttpIrrigError_t http_send_sensor_data(void) {
    if (!http_manager.initialized) {
        return HTTP_IRRIG_ERROR_INIT;
    }

    if (WiFi.status() != WL_CONNECTED) {
        kernel_log(LOG_LEVEL_WARN, "HTTP Manager: No WiFi connection for sensor data send");
        http_manager.last_error = HTTP_IRRIG_ERROR_CONNECT;
        http_manager.error_count++;
        return HTTP_IRRIG_ERROR_CONNECT;
    }

    HTTPClient http;
    http.setTimeout(8000); // 8 secondes timeout

    String data_url = String(app_config.server_url) + "/api/sensor-data";

    // Construction du payload
    SensorDataPayload_t payload;
    payload.timestamp = millis();
    payload.zone_count = 4;

    // Remplir les données des zones
    for (int zone_id = 0; zone_id < 4; zone_id++) {
        payload.zones[zone_id].zone_id = zone_id;
        payload.zones[zone_id].moisture_avg = zone_manager.zones[zone_id].status.current_moisture_avg;
        payload.zones[zone_id].sensor_count = 3;

        // Récupérer les valeurs individuelles des capteurs
        for (int s = 0; s < 3; s++) {
            uint8_t sensor_id = (zone_id * 3) + s;
            payload.zones[zone_id].sensor_values[s] = sensor_get_moisture(sensor_id);
        }
    }

    String json_payload = http_build_sensor_json(&payload);

    // Génération HMAC
    String hmac_signature = http_generate_hmac(json_payload, app_config.device_secret);

    // Configuration requête
    http.begin(data_url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-HMAC-Signature", hmac_signature);
    http.addHeader("X-Device-ID", app_config.device_id);
    http.addHeader("User-Agent", "ESP32-Irrigation-Master/0.3.0");

    kernel_log(LOG_LEVEL_DEBUG, "HTTP Manager: Sending sensor data to %s", data_url.c_str());

    int http_code = http.POST(json_payload);

    if (http_code == HTTP_CODE_OK) {
        kernel_log(LOG_LEVEL_INFO, "HTTP Manager: Sensor data sent successfully");
        http_manager.last_data_send = millis();
        http_manager.data_send_count++;
        http.end();
        return HTTP_IRRIG_OK;

    } else {
        String error_msg = http.errorToString(http_code);
        kernel_log(LOG_LEVEL_ERROR, "HTTP Manager: Sensor data send failed - Code: %d, Error: %s",
                   http_code, error_msg.c_str());

        http_manager.last_error = HTTP_IRRIG_ERROR_SERVER;
        http_manager.error_count++;
        http.end();
        return HTTP_IRRIG_ERROR_SERVER;
    }
}

// Récupération de la configuration depuis le serveur
HttpIrrigError_t http_poll_server_config(void) {
    if (!http_manager.initialized) {
        return HTTP_IRRIG_ERROR_INIT;
    }

    if (WiFi.status() != WL_CONNECTED) {
        kernel_log(LOG_LEVEL_WARN, "HTTP Manager: No WiFi connection for config poll");
        http_manager.last_error = HTTP_IRRIG_ERROR_CONNECT;
        http_manager.error_count++;
        return HTTP_IRRIG_ERROR_CONNECT;
    }

    HTTPClient http;
    http.setTimeout(5000); // 5 secondes timeout

    String config_url = String(app_config.server_url) + "/api/devices/" + String(app_config.device_id) + "/config";

    // Génération HMAC pour la requête GET
    String hmac_data = String(app_config.device_id) + String(millis());
    String hmac_signature = http_generate_hmac(hmac_data, app_config.device_secret);

    http.begin(config_url);
    http.addHeader("X-HMAC-Signature", hmac_signature);
    http.addHeader("X-Device-ID", app_config.device_id);
    http.addHeader("User-Agent", "ESP32-Irrigation-Master/0.3.0");

    kernel_log(LOG_LEVEL_DEBUG, "HTTP Manager: Polling config from %s", config_url.c_str());

    int http_code = http.GET();

    if (http_code == HTTP_CODE_OK) {
        String response = http.getString();

        // Parser la réponse JSON
        ServerConfigResponse_t server_config;
        if (http_parse_config_response(response, &server_config)) {

            // Appliquer la configuration reçue
            bool config_changed = false;
            for (int zone_id = 0; zone_id < 4; zone_id++) {
                if (server_config.zones[zone_id].configured) {
                    ZoneConfig_t new_config = zone_manager.zones[zone_id].config;
                    new_config.configured = true;
                    strcpy(new_config.zone_name, server_config.zones[zone_id].zone_name);
                    new_config.water_per_day_ml = server_config.zones[zone_id].water_per_day_ml;
                    strcpy(new_config.irrigation_time, server_config.zones[zone_id].irrigation_time);
                    new_config.humidity_threshold = server_config.zones[zone_id].humidity_threshold;
                    new_config.auto_irrigation_enabled = server_config.zones[zone_id].auto_irrigation_enabled;

                    if (zone_configure(zone_id, &new_config) == SYS_OK) {
                        config_changed = true;
                        kernel_log(LOG_LEVEL_INFO, "HTTP Manager: Zone %d config updated from server", zone_id);
                    }
                }
            }

            if (config_changed) {
                kernel_log(LOG_LEVEL_INFO, "HTTP Manager: Configuration updated from server");
            } else {
                kernel_log(LOG_LEVEL_DEBUG, "HTTP Manager: No config changes from server");
            }

            http_manager.last_config_poll = millis();
            http_manager.config_poll_count++;
            http.end();
            return HTTP_IRRIG_OK;

        } else {
            kernel_log(LOG_LEVEL_ERROR, "HTTP Manager: Failed to parse config response");
            http_manager.last_error = HTTP_IRRIG_ERROR_JSON;
            http_manager.error_count++;
            http.end();
            return HTTP_IRRIG_ERROR_JSON;
        }

    } else {
        String error_msg = http.errorToString(http_code);
        kernel_log(LOG_LEVEL_ERROR, "HTTP Manager: Config poll failed - Code: %d, Error: %s",
                   http_code, error_msg.c_str());

        http_manager.last_error = HTTP_IRRIG_ERROR_SERVER;
        http_manager.error_count++;
        http.end();
        return HTTP_IRRIG_ERROR_SERVER;
    }
}

// Test de connectivité avec le serveur
HttpIrrigError_t http_test_connectivity(void) {
    if (!http_manager.initialized) {
        return HTTP_IRRIG_ERROR_INIT;
    }

    if (WiFi.status() != WL_CONNECTED) {
        return HTTP_IRRIG_ERROR_CONNECT;
    }

    HTTPClient http;
    http.setTimeout(3000); // 3 secondes timeout

    String test_url = String(app_config.server_url) + "/api/health";

    http.begin(test_url);
    http.addHeader("User-Agent", "ESP32-Irrigation-Master/0.3.0");

    kernel_log(LOG_LEVEL_DEBUG, "HTTP Manager: Testing connectivity to %s", test_url.c_str());

    int http_code = http.GET();

    http.end();

    if (http_code == HTTP_CODE_OK) {
        kernel_log(LOG_LEVEL_INFO, "HTTP Manager: Server connectivity OK");
        return HTTP_IRRIG_OK;
    } else {
        kernel_log(LOG_LEVEL_WARN, "HTTP Manager: Server connectivity failed - Code: %d", http_code);
        return HTTP_IRRIG_ERROR_SERVER;
    }
}

// Affichage des statistiques HTTP
void http_print_stats(void) {
    kernel_log(LOG_LEVEL_INFO, "=== HTTP MANAGER STATS ===");
    kernel_log(LOG_LEVEL_INFO, "Initialized: %s", http_manager.initialized ? "YES" : "NO");
    kernel_log(LOG_LEVEL_INFO, "Register attempts: %lu", http_manager.register_retry_count);
    kernel_log(LOG_LEVEL_INFO, "Data sends: %lu", http_manager.data_send_count);
    kernel_log(LOG_LEVEL_INFO, "Config polls: %lu", http_manager.config_poll_count);
    kernel_log(LOG_LEVEL_INFO, "Total errors: %lu", http_manager.error_count);
    kernel_log(LOG_LEVEL_INFO, "Last error: %s", http_get_error_string(http_manager.last_error));

    if (http_manager.last_register_attempt > 0) {
        kernel_log(LOG_LEVEL_INFO, "Last register: %lu ms ago", millis() - http_manager.last_register_attempt);
    }
    if (http_manager.last_data_send > 0) {
        kernel_log(LOG_LEVEL_INFO, "Last data send: %lu ms ago", millis() - http_manager.last_data_send);
    }
    if (http_manager.last_config_poll > 0) {
        kernel_log(LOG_LEVEL_INFO, "Last config poll: %lu ms ago", millis() - http_manager.last_config_poll);
    }
}

// Génération HMAC-SHA256 (simplifié pour l'exemple)
String http_generate_hmac(const String& data, const String& secret) {
    // TODO: Implémenter HMAC-SHA256 réel
    // Pour l'instant, retourner un hash simple pour les tests
    String combined = data + secret;
    uint32_t hash = 0;
    for (char c : combined) {
        hash = hash * 31 + c;
    }

    char hash_str[9];
    sprintf(hash_str, "%08x", hash);
    return String(hash_str);
}

// Construction du JSON pour les données capteurs
String http_build_sensor_json(const SensorDataPayload_t* data) {
    DynamicJsonDocument doc(1024);

    doc["timestamp"] = data->timestamp;
    doc["device_id"] = app_config.device_id;
    doc["zone_count"] = data->zone_count;

    JsonArray zones = doc.createNestedArray("zones");
    for (int i = 0; i < data->zone_count; i++) {
        JsonObject zone = zones.createNestedObject();
        zone["zone_id"] = data->zones[i].zone_id;
        zone["moisture_avg"] = data->zones[i].moisture_avg;
        zone["sensor_count"] = data->zones[i].sensor_count;

        JsonArray sensors = zone.createNestedArray("sensor_values");
        for (int s = 0; s < data->zones[i].sensor_count; s++) {
            sensors.add(data->zones[i].sensor_values[s]);
        }
    }

    String json_string;
    serializeJson(doc, json_string);
    return json_string;
}

// Parsing de la réponse de configuration serveur
bool http_parse_config_response(const String& json_response, ServerConfigResponse_t* config) {
    DynamicJsonDocument doc(1024);

    DeserializationError error = deserializeJson(doc, json_response);
    if (error) {
        kernel_log(LOG_LEVEL_ERROR, "HTTP Manager: JSON parse error: %s", error.c_str());
        return false;
    }

    config->config_updated = doc["config_updated"] | false;
    config->server_timestamp = doc["server_timestamp"] | 0;

    if (doc.containsKey("zones") && doc["zones"].is<JsonArray>()) {
        JsonArray zones_array = doc["zones"];
        int zone_index = 0;

        for (JsonObject zone : zones_array) {
            if (zone_index < 4) {
                config->zones[zone_index].configured = zone["configured"] | false;
                strlcpy(config->zones[zone_index].zone_name, zone["zone_name"] | "Zone X", sizeof(config->zones[zone_index].zone_name));
                config->zones[zone_index].water_per_day_ml = zone["water_per_day_ml"] | 2000;
                strlcpy(config->zones[zone_index].irrigation_time, zone["irrigation_time"] | "08:00", sizeof(config->zones[zone_index].irrigation_time));
                config->zones[zone_index].humidity_threshold = zone["humidity_threshold"] | 25;
                config->zones[zone_index].auto_irrigation_enabled = zone["auto_irrigation_enabled"] | true;
                zone_index++;
            }
        }
    }

    return true;
}

// Conversion erreur en string
String http_get_error_string(HttpIrrigError_t error) {
    switch (error) {
        case HTTP_IRRIG_OK: return "OK";
        case HTTP_IRRIG_ERROR_INIT: return "INIT_ERROR";
        case HTTP_IRRIG_ERROR_CONNECT: return "CONNECT_ERROR";
        case HTTP_IRRIG_ERROR_TIMEOUT: return "TIMEOUT_ERROR";
        case HTTP_IRRIG_ERROR_AUTH: return "AUTH_ERROR";
        case HTTP_IRRIG_ERROR_JSON: return "JSON_ERROR";
        case HTTP_IRRIG_ERROR_SERVER: return "SERVER_ERROR";
        default: return "UNKNOWN_ERROR";
    }
}