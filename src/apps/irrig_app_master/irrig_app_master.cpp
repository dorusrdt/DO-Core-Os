#include "irrig_app_master.h"
#include "../../kernel/core/log_system_optimized.h"

// Variables statiques de l'application
static IrrigAppConfig_t app_config;
static bool app_initialized = false;
static uint32_t loop_counter = 0;

// ===== VARIABLES MODULE CAPTEURS =====
static SensorManager_t sensor_manager;
static unsigned long last_sensor_read = 0;

// ===== VARIABLES MODULE ZONES =====
static ZoneManager_t zone_manager;

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

    // Afficher l'état des zones tous les 30 cycles (30 secondes)
    if (loop_counter % 30 == 0) {
        zone_print_status();
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