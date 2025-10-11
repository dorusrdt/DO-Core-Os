#include "irrig_app_master.h"
#include "../../kernel/core/log_system_optimized.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

// ===== VARIABLES GLOBALES (Code Référence) =====

// Configuration de l'application
static IrrigAppConfig_t app_config;
static bool app_initialized = false;

// Zone Stack et Sensor Stack (exactement comme code référence)
static ZoneSlot ZONE_STACK[4];      // 4 slots physiques
static SensorSlot SENSOR_STACK[12]; // 12 slots physiques
static String assignedZones[4];     // Track assigned zone IDs
static int assignedZoneCount = 0;

// Timers pour les intervalles
static unsigned long lastConfigPoll = 0;
static unsigned long lastSensorRead = 0;
static unsigned long lastDataSend = 0;

// États du système
static bool deviceRegistered = false;
static bool deviceAssigned = false;

// Données environnementales globales
static float globalTemperature = 24.5;
static float globalHumidity = 60.0;
static float globalPressure = 1012.0;

// Simulation des capteurs
static float simulatedMoisture[MAX_SENSORS];
static bool pumpRunning = false;

// Contrôle d'irrigation
static bool isIrrigating = false;
static unsigned long activeIrrigationTimer = 0;

// ===== CALLBACKS DO-CORE APPLICATION =====

SysError_t irrig_app_master_init(void) {
    if (app_initialized) {
        return SYS_ALREADY_INITIALIZED;
    }
    
    kernel_log(LOG_LEVEL_INFO, "IrrigAppMaster: Initializing irrigation system...");
    kernel_log(LOG_LEVEL_INFO, "Device ID: %s", app_config.device_id);
    kernel_log(LOG_LEVEL_INFO, "Server: %s", app_config.server_url);
    
    // Initialiser zone stack (exactement comme JS simulator)
    for (int i = 0; i < 4; i++) {
        ZONE_STACK[i].id = i + 1;
        ZONE_STACK[i].configured = false;
        ZONE_STACK[i].zoneId = "";
        ZONE_STACK[i].waterPerDay = 0;
        ZONE_STACK[i].irrigationTime = "";
        ZONE_STACK[i].humidityThreshold = 0;
    }
    
    // Initialiser sensor stack (exactement comme JS simulator)
    for (int i = 0; i < 12; i++) {
        if (i < 9) {
            SENSOR_STACK[i].id = "s_0" + String(i + 1);
        } else {
            SENSOR_STACK[i].id = "s_" + String(i + 1);
        }
        SENSOR_STACK[i].assigned = false;
        SENSOR_STACK[i].zoneId = "";
    }
    
    // Initialiser hardware
    initializeHardware();
    
    // Initialiser simulation capteurs
    for (int i = 0; i < MAX_SENSORS; i++) {
        simulatedMoisture[i] = 30 + random(0, 30); // 30-60% initial
    }
    
    app_initialized = true;
    kernel_log(LOG_LEVEL_INFO, "IrrigAppMaster: Initialization complete");
    
    return SYS_OK;
}

void irrig_app_master_start(void) {
    kernel_log(LOG_LEVEL_INFO, "IrrigAppMaster: Starting irrigation system...");
    
    if (app_config.simulation_mode) {
        kernel_log(LOG_LEVEL_INFO, "Running in SIMULATION mode");
    } else {
        kernel_log(LOG_LEVEL_INFO, "Running in REAL HARDWARE mode");
    }
    
    // WiFi déjà connecté par DO-Core OS
    if (WiFi.status() == WL_CONNECTED) {
        kernel_log(LOG_LEVEL_INFO, "WiFi connected: %s", WiFi.localIP().toString().c_str());
        
        // Enregistrer le device
        registerDevice();
        
        if (deviceRegistered) {
            kernel_log(LOG_LEVEL_INFO, "Device registered successfully");
        }
    } else {
        kernel_log(LOG_LEVEL_WARN, "WiFi not connected - waiting for connection");
    }
    
    kernel_log(LOG_LEVEL_INFO, "IrrigAppMaster: System ready");
}

void irrig_app_master_loop(void) {
    unsigned long currentTime = millis();
    
    // Vérifier connexion WiFi
    if (WiFi.status() != WL_CONNECTED) {
        // WiFi géré par DO-Core, on attend juste
        return;
    }
    
    // Vérifier timer irrigation
    checkIrrigationTimer();
    
    // Poll configuration toutes les 10 secondes (comme code référence)
    if (currentTime - lastConfigPoll >= (app_config.poll_interval_seconds * 1000)) {
        pollConfiguration();
        lastConfigPoll = currentTime;
    }
    
    // Lire capteurs toutes les 5 secondes
    if (currentTime - lastSensorRead >= (app_config.sensor_read_interval_seconds * 1000)) {
        readAllSensors();
        lastSensorRead = currentTime;
    }
    
    // Envoyer données toutes les 15 secondes
    if (currentTime - lastDataSend >= (app_config.data_send_interval_seconds * 1000)) {
        sendSensorData();
        lastDataSend = currentTime;
    }
    
    // Vérifier planning irrigation toutes les minutes
    if (currentTime % 60000 < 1000) {
        checkIrrigationSchedule();
    }
    
    // Vérifier seuils d'humidité toutes les 30 secondes
    if (currentTime % 30000 < 1000) {
        checkMoistureThresholds();
    }
    
    // Delay géré par app_task_wrapper (10ms)
}

void irrig_app_master_stop(void) {
    kernel_log(LOG_LEVEL_INFO, "IrrigAppMaster: Stopping irrigation system...");
    
    // Arrêter irrigation en cours
    if (isIrrigating) {
        isIrrigating = false;
        activeIrrigationTimer = 0;
        digitalWrite(PUMP_RELAY_PIN, LOW);
        pumpRunning = false;
        kernel_log(LOG_LEVEL_INFO, "Emergency stop: irrigation halted");
    }
    
    kernel_log(LOG_LEVEL_INFO, "IrrigAppMaster: Cleanup completed");
    app_initialized = false;
}

SysError_t register_irrig_app_master(const IrrigAppConfig_t* config) {
    if (!config) {
        kernel_log(LOG_LEVEL_ERROR, "Invalid configuration provided");
        return SYS_INVALID_PARAM;
    }
    
    // Copier la configuration
    memcpy(&app_config, config, sizeof(IrrigAppConfig_t));
    
    // Validation
    if (strlen(app_config.device_id) == 0) {
        kernel_log(LOG_LEVEL_ERROR, "Device ID cannot be empty");
        return SYS_INVALID_PARAM;
    }
    
    if (strlen(app_config.server_url) == 0) {
        kernel_log(LOG_LEVEL_ERROR, "Server URL cannot be empty");
        return SYS_INVALID_PARAM;
    }
    
    // Créer callbacks
    AppCallbacks_t callbacks = {0};
    callbacks.init = irrig_app_master_init;
    callbacks.start = irrig_app_master_start;
    callbacks.stop = irrig_app_master_stop;
    callbacks.loop = irrig_app_master_loop;
    
    uint8_t app_id;
    
    // Enregistrer l'application
    SysError_t result = app_register("IrrigAppMaster",
                                   "Smart Irrigation Control System",
                                   APP_TYPE_USER,
                                   &callbacks,
                                   &app_id);
    
    if (result == SYS_OK) {
        kernel_log(LOG_LEVEL_INFO, "IrrigAppMaster registered with ID %d", app_id);
    } else {
        kernel_log(LOG_LEVEL_ERROR, "Failed to register application (error: %d)", result);
    }
    
    return result;
}

// ===== INITIALISATION HARDWARE =====

void initializeHardware(void) {
    #ifdef USE_REAL_HARDWARE
        kernel_log(LOG_LEVEL_INFO, "Initializing real hardware...");
        initializeRealHardware();
    #else
        kernel_log(LOG_LEVEL_INFO, "Initializing hardware (SIMULATION MODE)...");
    #endif
    
    // Initialiser LED status
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW);
    
    // Initialiser valeurs capteurs
    initializeSimulatedSensors();
    
    #ifdef USE_REAL_HARDWARE
        kernel_log(LOG_LEVEL_INFO, "Initialized 12 real moisture sensors");
        kernel_log(LOG_LEVEL_INFO, "Initialized 4 real zone relays + pump");
    #else
        kernel_log(LOG_LEVEL_INFO, "Initialized 12 simulated moisture sensors");
        kernel_log(LOG_LEVEL_INFO, "Initialized 4 simulated zone relays + pump");
    #endif
}

void initializeRealHardware(void) {
    // Initialiser pins relais en sorties
    pinMode(ZONE_1_RELAY_PIN, OUTPUT);
    pinMode(ZONE_2_RELAY_PIN, OUTPUT);
    pinMode(ZONE_3_RELAY_PIN, OUTPUT);
    pinMode(ZONE_4_RELAY_PIN, OUTPUT);
    pinMode(PUMP_RELAY_PIN, OUTPUT);
    
    // Tous les relais OFF au démarrage
    digitalWrite(ZONE_1_RELAY_PIN, LOW);
    digitalWrite(ZONE_2_RELAY_PIN, LOW);
    digitalWrite(ZONE_3_RELAY_PIN, LOW);
    digitalWrite(ZONE_4_RELAY_PIN, LOW);
    digitalWrite(PUMP_RELAY_PIN, LOW);
    
    kernel_log(LOG_LEVEL_INFO, "Real hardware pins initialized");
    kernel_log(LOG_LEVEL_INFO, "Zone relays: %d, %d, %d, %d", 
               ZONE_1_RELAY_PIN, ZONE_2_RELAY_PIN, ZONE_3_RELAY_PIN, ZONE_4_RELAY_PIN);
    kernel_log(LOG_LEVEL_INFO, "Pump relay: %d", PUMP_RELAY_PIN);
}

void initializeSimulatedSensors(void) {
    kernel_log(LOG_LEVEL_INFO, "Initializing sensor simulation...");
    
    // Initialiser avec valeurs réalistes par zone
    for (int i = 0; i < MAX_SENSORS; i++) {
        int zoneIndex = i / SENSORS_PER_ZONE;
        
        // Différentes bases d'humidité par zone
        switch (zoneIndex) {
            case 0: // Zone 1 - Tomates (besoin eau)
                simulatedMoisture[i] = 35 + random(-5, 10);
                break;
            case 1: // Zone 2 - Laitue (retient humidité)
                simulatedMoisture[i] = 55 + random(-5, 8);
                break;
            case 2: // Zone 3 - Carottes (modéré)
                simulatedMoisture[i] = 45 + random(-8, 12);
                break;
            case 3: // Zone 4 - Mixte
                simulatedMoisture[i] = 50 + random(-10, 10);
                break;
        }
        
        // Contraindre dans plage réaliste
        simulatedMoisture[i] = constrain(simulatedMoisture[i], 15, 85);
        
        kernel_log(LOG_LEVEL_DEBUG, "Sensor %d (Zone %d): %.1f%% moisture", 
                   i+1, zoneIndex+1, simulatedMoisture[i]);
    }
}


// ===== GESTION CONFIGURATION =====

void registerDevice(void) {
    if (WiFi.status() != WL_CONNECTED) {
        kernel_log(LOG_LEVEL_ERROR, "Cannot register: WiFi not connected");
        return;
    }
    
    kernel_log(LOG_LEVEL_INFO, "Registering device with server...");
    
    HTTPClient http;
    http.setTimeout(30000); // 30 secondes timeout
    
    String url = String(app_config.server_url) + "/api/devices/register";
    
    if (!http.begin(url)) {
        kernel_log(LOG_LEVEL_ERROR, "Failed to initialize HTTP client");
        return;
    }
    
    http.addHeader("Content-Type", "application/json");
    
    // Créer payload d'enregistrement
    DynamicJsonDocument doc(1024);
    doc["type"] = "register";
    doc["deviceId"] = app_config.device_id;
    doc["capacity"]["zones"] = MAX_ZONES;
    doc["capacity"]["sensors"] = MAX_SENSORS;
    doc["timestamp"] = getTimestamp();
    
    String payload;
    serializeJson(doc, payload);
    
    // Ajouter signature HMAC
    String timestamp = String(millis());
    String signature = generateHMAC(payload + timestamp);
    http.addHeader("X-Signature", signature);
    http.addHeader("X-Timestamp", timestamp);
    
    kernel_log(LOG_LEVEL_DEBUG, "Sending registration request...");
    
    int httpResponseCode = http.POST(payload);
    
    if (httpResponseCode == 200 || httpResponseCode == 201) {
        String response = http.getString();
        kernel_log(LOG_LEVEL_INFO, "Device registered successfully");
        deviceRegistered = true;
    } else {
        kernel_log(LOG_LEVEL_ERROR, "Registration failed with code: %d", httpResponseCode);
        deviceRegistered = false;
    }
    
    http.end();
}

void pollConfiguration(void) {
    if (!deviceRegistered) {
        kernel_log(LOG_LEVEL_WARN, "Cannot poll config: Device not registered");
        return;
    }
    
    kernel_log(LOG_LEVEL_DEBUG, "Polling configuration...");
    
    HTTPClient http;
    http.setTimeout(10000);
    http.setConnectTimeout(5000);
    
    String url = String(app_config.server_url) + "/api/devices/" + String(app_config.device_id) + "/config";
    
    if (!http.begin(url)) {
        kernel_log(LOG_LEVEL_ERROR, "Failed to initialize HTTP client for config");
        return;
    }
    
    int httpResponseCode = http.GET();
    
    if (httpResponseCode == 200) {
        String response = http.getString();
        kernel_log(LOG_LEVEL_DEBUG, "Config received, parsing...");
        parseConfiguration(response);
    } else if (httpResponseCode == 304) {
        kernel_log(LOG_LEVEL_DEBUG, "Configuration unchanged");
    } else if (httpResponseCode > 0) {
        kernel_log(LOG_LEVEL_ERROR, "Config poll failed: %d", httpResponseCode);
    } else {
        kernel_log(LOG_LEVEL_ERROR, "HTTP request failed");
    }
    
    http.end();
}

void parseConfiguration(String jsonResponse) {
    DynamicJsonDocument doc(4096);
    deserializeJson(doc, jsonResponse);
    
    // Vérifier commande delete_zone directe
    if (doc["type"] == "delete_zone") {
        String zoneId = doc["zoneId"];
        handleZoneDeletion(zoneId);
        return;
    }
    
    // Vérifier commandes en attente
    if (doc.containsKey("commands")) {
        JsonArray commands = doc["commands"];
        for (JsonVariant cmd : commands) {
            String action = cmd["action"];
            if (action == "delete_zone") {
                String zoneId = cmd["zoneId"];
                handleZoneDeletion(zoneId);
            }
        }
    }
    
    if (doc["type"] == "config") {
        JsonArray zonesArray = doc["zones"];
        int newZoneCount = zonesArray.size();
        
        kernel_log(LOG_LEVEL_INFO, "Received configuration for %d zones", newZoneCount);
        
        // Traiter chaque zone
        for (int i = 0; i < newZoneCount && i < 4; i++) {
            JsonObject zone = zonesArray[i];
            String zoneId = zone["zoneId"].as<String>();
            
            // Vérifier si zone existe déjà
            ZoneSlot* existingSlot = nullptr;
            for (int j = 0; j < 4; j++) {
                if (ZONE_STACK[j].configured && ZONE_STACK[j].zoneId == zoneId) {
                    existingSlot = &ZONE_STACK[j];
                    kernel_log(LOG_LEVEL_INFO, "Zone %d: %s (existing - preserving sensors)", 
                               existingSlot->id, zoneId.c_str());
                    break;
                }
            }
            
            if (existingSlot) {
                // Mettre à jour config existante mais préserver capteurs
                existingSlot->waterPerDay = zone["waterPerDay"];
                existingSlot->irrigationTime = zone["irrigationTime"].as<String>();
                existingSlot->humidityThreshold = zone["humidityThreshold"];
                kernel_log(LOG_LEVEL_INFO, "  Water: %dml/day, Time: %s, Threshold: %d%% (updated)", 
                           existingSlot->waterPerDay, existingSlot->irrigationTime.c_str(), 
                           existingSlot->humidityThreshold);
                continue;
            }
            
            // Trouver slot libre pour nouvelle zone
            ZoneSlot* slot = nullptr;
            for (int j = 0; j < 4; j++) {
                if (!ZONE_STACK[j].configured) {
                    slot = &ZONE_STACK[j];
                    break;
                }
            }
            
            if (slot) {
                slot->configured = true;
                slot->zoneId = zoneId;
                slot->waterPerDay = zone["waterPerDay"];
                slot->irrigationTime = zone["irrigationTime"].as<String>();
                slot->humidityThreshold = zone["humidityThreshold"];
                
                // Assigner capteurs à cette zone
                JsonArray sensors = zone["sensors"];
                
                // Calculer baseOffset en comptant capteurs des zones précédentes
                int baseOffset = 0;
                int currentSlotIndex = slot - ZONE_STACK;
                
                for (int z = 0; z < currentSlotIndex; z++) {
                    if (ZONE_STACK[z].configured) {
                        for (int s = 0; s < 12; s++) {
                            if (SENSOR_STACK[s].assigned && SENSOR_STACK[s].zoneId == ZONE_STACK[z].zoneId) {
                                baseOffset++;
                            }
                        }
                    }
                }
                
                for (int k = 0; k < sensors.size() && k < 12; k++) {
                    JsonObject sensor = sensors[k];
                    String configSensorId = sensor["sensorId"].as<String>();
                    
                    // Mapper vers IDs hardware
                    String actualSensorId;
                    if (baseOffset + k + 1 < 10) {
                        actualSensorId = "s_0" + String(baseOffset + k + 1);
                    } else {
                        actualSensorId = "s_" + String(baseOffset + k + 1);
                    }
                    
                    // Trouver slot capteur disponible
                    for (int s = 0; s < 12; s++) {
                        if (!SENSOR_STACK[s].assigned) {
                            SENSOR_STACK[s].assigned = true;
                            SENSOR_STACK[s].id = actualSensorId;
                            SENSOR_STACK[s].zoneId = zoneId;
                            break;
                        }
                    }
                }
                
                assignedZones[assignedZoneCount++] = zoneId;
                
                kernel_log(LOG_LEVEL_INFO, "Zone %d: %s", slot->id, zoneId.c_str());
                kernel_log(LOG_LEVEL_INFO, "  Water: %dml/day, Time: %s, Threshold: %d%%", 
                           slot->waterPerDay, slot->irrigationTime.c_str(), slot->humidityThreshold);
                kernel_log(LOG_LEVEL_INFO, "  Sensors: %d", sensors.size());
            }
        }
        
        // Mettre à jour statut assignation
        deviceAssigned = (assignedZoneCount > 0);
    }
}

void handleZoneDeletion(String zoneId) {
    kernel_log(LOG_LEVEL_INFO, "Processing zone deletion: %s", zoneId.c_str());
    
    // Arrêter irrigation si active pour cette zone
    if (isIrrigating && activeIrrigationTimer > 0) {
        kernel_log(LOG_LEVEL_WARN, "Stopping active irrigation for zone deletion");
        activeIrrigationTimer = 0;
        isIrrigating = false;
        digitalWrite(PUMP_RELAY_PIN, LOW);
        kernel_log(LOG_LEVEL_INFO, "Pump: OFF (Emergency stop)");
    }
    
    // Trouver le slot de zone
    ZoneSlot* zoneSlot = nullptr;
    for (int i = 0; i < 4; i++) {
        if (ZONE_STACK[i].configured && ZONE_STACK[i].zoneId == zoneId) {
            zoneSlot = &ZONE_STACK[i];
            kernel_log(LOG_LEVEL_INFO, "Found zone %s in slot %d", zoneId.c_str(), zoneSlot->id);
            break;
        }
    }
    
    if (zoneSlot) {
        // Libérer capteurs assignés
        for (int i = 0; i < 12; i++) {
            if (SENSOR_STACK[i].assigned && SENSOR_STACK[i].zoneId == zoneId) {
                SENSOR_STACK[i].assigned = false;
                SENSOR_STACK[i].zoneId = "";
                kernel_log(LOG_LEVEL_DEBUG, "Freed sensor %s", SENSOR_STACK[i].id.c_str());
            }
        }
        
        // Nettoyer le slot
        zoneSlot->configured = false;
        zoneSlot->zoneId = "";
        zoneSlot->waterPerDay = 0;
        zoneSlot->irrigationTime = "";
        zoneSlot->humidityThreshold = 0;
        kernel_log(LOG_LEVEL_INFO, "Zone %s removed from device", zoneId.c_str());
        
        // Retirer de assignedZones
        for (int i = 0; i < assignedZoneCount; i++) {
            if (assignedZones[i] == zoneId) {
                for (int j = i; j < assignedZoneCount - 1; j++) {
                    assignedZones[j] = assignedZones[j + 1];
                }
                assignedZoneCount--;
                break;
            }
        }
        
        // Mettre à jour statut
        if (assignedZoneCount == 0) {
            deviceAssigned = false;
            kernel_log(LOG_LEVEL_INFO, "Device no longer assigned to any zones");
        }
    } else {
        kernel_log(LOG_LEVEL_WARN, "Zone %s not found on this device", zoneId.c_str());
    }
}

// ===== GESTION CAPTEURS =====

void readAllSensors(void) {
    #ifdef USE_REAL_SENSORS
        readRealSensors();
    #else
        updateSimulatedSensors();
    #endif
    
    // Afficher lectures seulement pour capteurs assignés
    if (assignedZoneCount > 0) {
        kernel_log(LOG_LEVEL_DEBUG, "Current sensor readings:");
        
        for (int i = 0; i < 4; i++) {
            if (ZONE_STACK[i].configured) {
                int sensorCount = 0;
                for (int s = 0; s < 12; s++) {
                    if (SENSOR_STACK[s].assigned && SENSOR_STACK[s].zoneId == ZONE_STACK[i].zoneId) {
                        sensorCount++;
                        kernel_log(LOG_LEVEL_DEBUG, "  Zone %d, Sensor %d (%s): %.1f%% moisture", 
                                   ZONE_STACK[i].id, sensorCount, SENSOR_STACK[s].id.c_str(), 
                                   simulatedMoisture[s]);
                    }
                }
            }
        }
    }
}

void updateSimulatedSensors(void) {
    // Mettre à jour données environnementales globales
    float tempVariation = (random(-100, 101) / 100.0);
    float humidityVariation = (random(-250, 251) / 100.0);
    float pressureVariation = (random(-500, 501) / 100.0);
    
    globalTemperature = constrain(globalTemperature + tempVariation, 15, 40);
    globalHumidity = constrain(globalHumidity + humidityVariation, 30, 90);
    globalPressure = constrain(globalPressure + pressureVariation, 990, 1030);
    
    // Générer lectures réalistes par capteur
    for (int i = 0; i < MAX_SENSORS; i++) {
        float baseValue = 30 + (i * 2);
        float variation = (random(-500, 501) / 100.0);
        
        simulatedMoisture[i] = constrain(baseValue + variation, 15, 75);
    }
}

void readRealSensors(void) {
    // TODO: Implémenter lecture réelle GPIO ADC
    kernel_log(LOG_LEVEL_WARN, "Real sensor reading not implemented yet");
    
    for (int i = 0; i < 12; i++) {
        simulatedMoisture[i] = 50.0f; // Valeur fixe temporaire
    }
}


// ===== COMMUNICATION SERVEUR =====

void sendSensorData(void) {
    if (WiFi.status() != WL_CONNECTED) return;
    
    HTTPClient http;
    http.setTimeout(30000);
    http.begin(String(app_config.server_url) + "/api/devices/sensor-data");
    http.addHeader("Content-Type", "application/json");
    
    // Créer payload exactement comme JS simulator
    DynamicJsonDocument doc(4096);
    doc["type"] = "data";
    doc["deviceId"] = app_config.device_id;
    doc["timestamp"] = getTimestamp();
    
    // Données environnementales globales
    JsonObject globalData = doc.createNestedObject("globalData");
    globalData["temperature"] = globalTemperature;
    globalData["humidity"] = globalHumidity;
    globalData["pressure"] = globalPressure;
    globalData["batteryLevel"] = 85.0 + random(-10, 16);
    globalData["signalStrength"] = WiFi.RSSI();
    
    // Données par zone
    JsonArray zonesArray = doc.createNestedArray("zonesData");
    
    kernel_log(LOG_LEVEL_DEBUG, "Sending sensor data: %d zones", assignedZoneCount);
    
    if (assignedZoneCount > 0) {
        for (int i = 0; i < 4; i++) {
            if (ZONE_STACK[i].configured) {
                JsonObject zoneObj = zonesArray.createNestedObject();
                zoneObj["zoneId"] = ZONE_STACK[i].zoneId;
                
                JsonArray moistureArray = zoneObj.createNestedArray("soilMoisture");
                
                // Ajouter données capteurs pour cette zone
                for (int s = 0; s < 12; s++) {
                    if (SENSOR_STACK[s].assigned && SENSOR_STACK[s].zoneId == ZONE_STACK[i].zoneId) {
                        JsonObject sensorObj = moistureArray.createNestedObject();
                        sensorObj["sensorId"] = SENSOR_STACK[s].id;
                        sensorObj["value"] = simulatedMoisture[s];
                    }
                }
            }
        }
    } else {
        kernel_log(LOG_LEVEL_DEBUG, "No zones configured - sending only global data");
    }
    
    String payload;
    serializeJson(doc, payload);
    
    // Ajouter signature HMAC
    String timestamp = String(millis());
    String signature = generateHMAC(payload + timestamp);
    http.addHeader("X-Signature", signature);
    http.addHeader("X-Timestamp", timestamp);
    
    int httpResponseCode = http.POST(payload);
    
    if (httpResponseCode == 200) {
        kernel_log(LOG_LEVEL_DEBUG, "Sensor data sent successfully");
    } else {
        kernel_log(LOG_LEVEL_ERROR, "Sensor data send failed: %d", httpResponseCode);
    }
    
    http.end();
}

// ===== GESTION IRRIGATION =====

void checkIrrigationSchedule(void) {
    if (!deviceRegistered || assignedZoneCount == 0) return;
    
    // Obtenir heure actuelle (NTP géré par DO-Core)
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return;
    }
    
    char currentTime[6];
    strftime(currentTime, sizeof(currentTime), "%H:%M", &timeinfo);
    
    for (int i = 0; i < 4; i++) {
        if (ZONE_STACK[i].configured && ZONE_STACK[i].irrigationTime.equals(String(currentTime))) {
            kernel_log(LOG_LEVEL_INFO, "Scheduled irrigation for zone %d", ZONE_STACK[i].id);
            executeIrrigation(ZONE_STACK[i].zoneId, ZONE_STACK[i].waterPerDay / 10);
        }
    }
}

void checkMoistureThresholds(void) {
    for (int i = 0; i < 4; i++) {
        if (!ZONE_STACK[i].configured) continue;
        
        // Calculer moyenne humidité pour cette zone
        float totalMoisture = 0;
        int sensorCount = 0;
        
        for (int s = 0; s < 12; s++) {
            if (SENSOR_STACK[s].assigned && SENSOR_STACK[s].zoneId == ZONE_STACK[i].zoneId) {
                totalMoisture += simulatedMoisture[s];
                sensorCount++;
            }
        }
        
        if (sensorCount > 0) {
            float avgMoisture = totalMoisture / sensorCount;
            
            if (avgMoisture < ZONE_STACK[i].humidityThreshold) {
                kernel_log(LOG_LEVEL_WARN, "Zone %d moisture critical: %.1f%% < %d%%", 
                           ZONE_STACK[i].id, avgMoisture, ZONE_STACK[i].humidityThreshold);
                
                // Irrigation d'urgence
                executeIrrigation(ZONE_STACK[i].zoneId, 60); // 1 minute
            }
        }
    }
}

void executeIrrigation(String zoneId, int durationSeconds) {
    // Trouver le slot de zone
    ZoneSlot* zoneSlot = nullptr;
    for (int i = 0; i < 4; i++) {
        if (ZONE_STACK[i].configured && ZONE_STACK[i].zoneId == zoneId) {
            zoneSlot = &ZONE_STACK[i];
            break;
        }
    }
    
    if (!zoneSlot) {
        kernel_log(LOG_LEVEL_WARN, "Zone %s not found or not configured", zoneId.c_str());
        return;
    }
    
    if (isIrrigating) {
        kernel_log(LOG_LEVEL_WARN, "Irrigation already in progress, queuing command");
        return;
    }
    
    kernel_log(LOG_LEVEL_INFO, "Starting irrigation for zone %s", zoneId.c_str());
    kernel_log(LOG_LEVEL_INFO, "Pump: ON - Duration: %ds", durationSeconds);
    isIrrigating = true;
    
    // Activer pompe
    digitalWrite(PUMP_RELAY_PIN, HIGH);
    pumpRunning = true;
    
    // Définir timer
    activeIrrigationTimer = millis() + (durationSeconds * 1000);
}

void checkIrrigationTimer(void) {
    if (isIrrigating && activeIrrigationTimer > 0 && millis() >= activeIrrigationTimer) {
        // Arrêter irrigation
        isIrrigating = false;
        activeIrrigationTimer = 0;
        digitalWrite(PUMP_RELAY_PIN, LOW);
        pumpRunning = false;
        kernel_log(LOG_LEVEL_INFO, "Irrigation completed");
        kernel_log(LOG_LEVEL_INFO, "Pump: OFF");
    }
}

// ===== UTILITAIRES =====

String generateHMAC(String data) {
    // HMAC-SHA256 simplifié
    // TODO: Implémenter vrai HMAC-SHA256 avec mbedtls
    return "dummy_signature_" + String(millis());
}

String getTimestamp(void) {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return String(millis()); // Fallback
    }
    
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S.000Z", &timeinfo);
    return String(timestamp);
}

