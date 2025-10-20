#include "irrig_app_slave_sensors.h"
#include "../../kernel/core/log_system_optimized.h"
#include "../irrig_common/irrig_communication.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// ===== VARIABLES GLOBALES =====

static IrrigSensorConfig_t app_config;
static bool app_initialized = false;
static unsigned long last_read_time = 0;

// Données capteurs (UNIQUEMENT humidité sol)
static float simulated_moisture[MAX_SENSORS];

// Serveur HTTP
static WebServer* http_server = nullptr;

// Statistiques
static uint32_t total_reads = 0;
static uint32_t total_publishes = 0;

// ===== CALLBACKS DO-CORE =====

SysError_t irrig_app_slave_sensors_init(void) {
    if (app_initialized) {
        return SYS_ALREADY_INITIALIZED;
    }
    
    kernel_log(LOG_LEVEL_INFO, "IrrigSlaveSensors: Initializing...");
    kernel_log(LOG_LEVEL_INFO, "  Mode: %s", app_config.simulation_mode ? "SIMULATION" : "REAL");
    kernel_log(LOG_LEVEL_INFO, "  Read interval: %dms", app_config.read_interval_ms);
    kernel_log(LOG_LEVEL_INFO, "  Samples per read: %d", app_config.samples_per_read);
    
    // Initialiser hardware
    sensors_init_hardware();
    
    // Initialiser serveur HTTP si activé
    if (app_config.enable_http_server) {
        sensors_start_http_server();
    }
    
    app_initialized = true;
    kernel_log(LOG_LEVEL_INFO, "IrrigSlaveSensors: Initialization complete");
    
    return SYS_OK;
}

void irrig_app_slave_sensors_start(void) {
    kernel_log(LOG_LEVEL_INFO, "IrrigSlaveSensors: Starting...");
    
    if (WiFi.status() == WL_CONNECTED) {
        kernel_log(LOG_LEVEL_INFO, "WiFi connected: %s", WiFi.localIP().toString().c_str());
    } else {
        kernel_log(LOG_LEVEL_WARN, "WiFi not connected");
    }
    
    kernel_log(LOG_LEVEL_INFO, "IrrigSlaveSensors: Ready");
}

void irrig_app_slave_sensors_loop(void) {
    unsigned long current_time = millis();
    
    // Gérer requêtes HTTP
    if (app_config.enable_http_server && http_server) {
        sensors_handle_http_requests();
    }
    
    // Lire capteurs selon intervalle
    if (current_time - last_read_time >= app_config.read_interval_ms) {
        // Créer paquet données
        SensorDataPacket_t packet;
        sensors_read_all(&packet);
        
        total_reads++;
        
        // Publier vers Master via HTTP
        if (irrig_comm_publish_sensor_data(&packet)) {
            total_publishes++;
            kernel_log(LOG_LEVEL_DEBUG, "IrrigSlaveSensors: Data published (reads: %lu, publishes: %lu)", 
                       total_reads, total_publishes);
        } else {
            kernel_log(LOG_LEVEL_WARN, "IrrigSlaveSensors: Failed to publish data");
        }
        
        last_read_time = current_time;
    }
}

void irrig_app_slave_sensors_stop(void) {
    kernel_log(LOG_LEVEL_INFO, "IrrigSlaveSensors: Stopping...");
    
    // Arrêter serveur HTTP
    if (http_server) {
        sensors_stop_http_server();
    }
    
    kernel_log(LOG_LEVEL_INFO, "IrrigSlaveSensors: Statistics:");
    kernel_log(LOG_LEVEL_INFO, "  Total reads: %lu", total_reads);
    kernel_log(LOG_LEVEL_INFO, "  Total publishes: %lu", total_publishes);
    
    app_initialized = false;
}

SysError_t register_irrig_app_slave_sensors(const IrrigSensorConfig_t* config) {
    if (!config) {
        kernel_log(LOG_LEVEL_ERROR, "Invalid configuration");
        return SYS_INVALID_PARAM;
    }
    
    // Copier configuration
    memcpy(&app_config, config, sizeof(IrrigSensorConfig_t));
    
    // Créer callbacks
    AppCallbacks_t callbacks = {0};
    callbacks.init = irrig_app_slave_sensors_init;
    callbacks.start = irrig_app_slave_sensors_start;
    callbacks.stop = irrig_app_slave_sensors_stop;
    callbacks.loop = irrig_app_slave_sensors_loop;
    
    uint8_t app_id;
    
    // Enregistrer application
    SysError_t result = app_register("IrrigSlaveSensors",
                                   "Sensor Acquisition Module",
                                   APP_TYPE_USER,
                                   &callbacks,
                                   &app_id);
    
    if (result == SYS_OK) {
        kernel_log(LOG_LEVEL_INFO, "IrrigSlaveSensors registered with ID %d", app_id);
    } else {
        kernel_log(LOG_LEVEL_ERROR, "Failed to register (error: %d)", result);
    }
    
    return result;
}

// ===== INITIALISATION HARDWARE =====

void sensors_init_hardware(void) {
    if (app_config.simulation_mode) {
        kernel_log(LOG_LEVEL_INFO, "Initializing simulated sensors...");
        sensors_init_simulation();
    } else {
        kernel_log(LOG_LEVEL_INFO, "Initializing real hardware sensors...");
        sensors_init_real_hardware();
        sensors_init_simulation();  // Valeurs initiales
    }
    
    kernel_log(LOG_LEVEL_INFO, "Initialized %d moisture sensors", MAX_SENSORS);
}

void sensors_init_real_hardware(void) {
    // Configurer pins ADC en entrée (déjà par défaut sur ESP32)
    // Les pins ADC n'ont pas besoin de pinMode()
    kernel_log(LOG_LEVEL_INFO, "Real ADC pins configured");
}

void sensors_init_simulation(void) {
    // Initialiser avec valeurs réalistes par zone
    for (int i = 0; i < MAX_SENSORS; i++) {
        int zoneIndex = i / SENSORS_PER_ZONE;
        
        switch (zoneIndex) {
            case 0: // Zone 1 - Tomates
                simulated_moisture[i] = 35 + random(-5, 10);
                break;
            case 1: // Zone 2 - Laitue
                simulated_moisture[i] = 55 + random(-5, 8);
                break;
            case 2: // Zone 3 - Carottes
                simulated_moisture[i] = 45 + random(-8, 12);
                break;
            case 3: // Zone 4 - Mixte
                simulated_moisture[i] = 50 + random(-10, 10);
                break;
        }
        
        simulated_moisture[i] = constrain(simulated_moisture[i], 15, 85);
    }
    
    kernel_log(LOG_LEVEL_INFO, "Simulation initialized with realistic values");
}

// ===== LECTURE CAPTEURS =====

void sensors_read_all(SensorDataPacket_t* packet) {
    if (!packet) return;
    
    packet->timestamp = millis();
    
    // ✅ Lire UNIQUEMENT capteurs d'humidité sol
    if (app_config.simulation_mode) {
        sensors_update_simulation(simulated_moisture);
    } else {
        sensors_read_real(simulated_moisture);
    }
    
    // ✅ Copier UNIQUEMENT données d'humidité
    for (int i = 0; i < MAX_SENSORS; i++) {
        packet->moisture[i] = simulated_moisture[i];
    }
    
    // ✅ Données globales mises à 0 (seront générées par Master)
    packet->temperature = 0.0;
    packet->humidity = 0.0;
    packet->pressure = 0.0;
    packet->battery_level = 0.0;
    packet->signal_strength = 0;
    
    kernel_log(LOG_LEVEL_DEBUG, "Slave1: Read %d moisture sensors", MAX_SENSORS);
}

void sensors_read_real(float moisture[MAX_SENSORS]) {
    const int pins[] = {
        MOISTURE_PIN_1, MOISTURE_PIN_2, MOISTURE_PIN_3, MOISTURE_PIN_4,
        MOISTURE_PIN_5, MOISTURE_PIN_6, MOISTURE_PIN_7, MOISTURE_PIN_8,
        MOISTURE_PIN_9, MOISTURE_PIN_10, MOISTURE_PIN_11, MOISTURE_PIN_12
    };
    
    for (int i = 0; i < MAX_SENSORS; i++) {
        // Moyennage échantillons
        int sum = 0;
        for (int j = 0; j < app_config.samples_per_read; j++) {
            sum += analogRead(pins[i]);
            delay(10);
        }
        int adcValue = sum / app_config.samples_per_read;
        
        // Convertir ADC → % humidité
        // 4095 (sec) → 0%, 0 (mouillé) → 100%
        moisture[i] = map(adcValue, MOISTURE_DRY_VALUE, MOISTURE_WET_VALUE, 0, 100);
        moisture[i] = constrain(moisture[i], 0, 100);
    }
}

void sensors_update_simulation(float moisture[MAX_SENSORS]) {
    // ✅ Mettre à jour UNIQUEMENT capteurs d'humidité sol avec variation réaliste
    for (int i = 0; i < MAX_SENSORS; i++) {
        float variation = (random(-200, 201) / 100.0);
        moisture[i] = constrain(moisture[i] + variation, 15, 85);
    }
}

// ===== SERVEUR HTTP =====

void sensors_start_http_server(void) {
    if (http_server) {
        kernel_log(LOG_LEVEL_WARN, "HTTP server already running");
        return;
    }
    
    http_server = new WebServer(app_config.http_server_port);
    
    // Route GET /api/sensors/status
    http_server->on("/api/sensors/status", HTTP_GET, []() {
        DynamicJsonDocument doc(512);
        doc["device_id"] = "ESP32_SLAVE_SENSORS";
        doc["uptime"] = millis() / 1000;
        doc["last_read"] = last_read_time;
        doc["sensors_count"] = MAX_SENSORS;
        doc["mode"] = app_config.simulation_mode ? "simulation" : "real";
        doc["total_reads"] = total_reads;
        doc["total_publishes"] = total_publishes;
        
        String response;
        serializeJson(doc, response);
        
        http_server->send(200, "application/json", response);
    });
    
    // Route GET /api/sensors/data (lecture immédiate)
    http_server->on("/api/sensors/data", HTTP_GET, []() {
        SensorDataPacket_t packet;
        sensors_read_all(&packet);
        
        DynamicJsonDocument doc(2048);
        doc["timestamp"] = packet.timestamp;
        
        JsonArray moistureArray = doc.createNestedArray("moisture");
        for (int i = 0; i < MAX_SENSORS; i++) {
            moistureArray.add(packet.moisture[i]);
        }
        
        doc["temperature"] = packet.temperature;
        doc["humidity"] = packet.humidity;
        doc["pressure"] = packet.pressure;
        doc["battery_level"] = packet.battery_level;
        doc["signal_strength"] = packet.signal_strength;
        
        String response;
        serializeJson(doc, response);
        
        http_server->send(200, "application/json", response);
    });
    
    http_server->begin();
    kernel_log(LOG_LEVEL_INFO, "HTTP server started on port %d", app_config.http_server_port);
}

void sensors_stop_http_server(void) {
    if (http_server) {
        http_server->stop();
        delete http_server;
        http_server = nullptr;
        kernel_log(LOG_LEVEL_INFO, "HTTP server stopped");
    }
}

void sensors_handle_http_requests(void) {
    if (http_server) {
        http_server->handleClient();
    }
}
