#include "irrig_app_master.h"
#include "irrig_app_master_http.h"
#include "../irrig_common/irrig_communication.h"
#include "../../kernel/core/log_system_optimized.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <Adafruit_Sensor.h>

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

// Hash de configuration pour optimisation polling (désactivé temporairement)
static String lastConfigHash = ""; // TODO: réactiver pour optimisation production

// ✅ Capteur BME280
static Adafruit_BME280 bme;
static bool bmeInitialized = false;

// ✅ Données environnementales globales (LUES DU BME280)
static float globalTemperature = 0.0;
static float globalHumidity = 0.0;
static float globalPressure = 0.0;
static float globalBatteryLevel = 85.0; // Simulé (pas de batterie sur BME280)

// Simulation des capteurs
static float simulatedMoisture[MAX_SENSORS];
static bool pumpRunning = false;

// Contrôle d'irrigation
static bool isIrrigating = false;
static unsigned long activeIrrigationTimer = 0;

// ===== PROTOTYPES FONCTIONS LOCALES =====

void initializeBME280(void);
void readBME280Data(void);

// ===== CALLBACKS DO-CORE APPLICATION =====

SysError_t irrig_app_master_init(void) {
    if (app_initialized) {
        return SYS_ALREADY_INITIALIZED;
    }

    kernel_log(LOG_LEVEL_INFO, "IrrigAppMaster: Initializing irrigation system...");
    kernel_log(LOG_LEVEL_INFO, "Device ID: %s", app_config.device_id);
    kernel_log(LOG_LEVEL_INFO, "Server: %s", app_config.server_url);

    // Initialiser zone stack avec mapping physique par défaut
    for (int i = 0; i < 4; i++) {
        ZONE_STACK[i].id = i;
        ZONE_STACK[i].physicalZoneNumber = i + 1;  // Par défaut: slot 0 → zone 1, slot 1 → zone 2, etc.
        ZONE_STACK[i].configured = false;
        ZONE_STACK[i].zoneId = "";
        ZONE_STACK[i].waterPerDay = 0;
        for (int s = 0; s < MAX_SCHEDULES_PER_ZONE; s++) {
            ZONE_STACK[i].irrigationTimes[s] = "";
        }
        ZONE_STACK[i].scheduleCount = 0;
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

    // Initialiser hardware (inclut BME280)
    initializeHardware();

    // Vérifier que BME280 est bien initialisé
    if (bmeInitialized) {
        kernel_log(LOG_LEVEL_INFO, "✅ BME280 sensor initialized successfully during app registration");
    } else {
        kernel_log(LOG_LEVEL_WARN, "⚠️  BME280 sensor not found - using SIMULATION mode");
        kernel_log(LOG_LEVEL_WARN, "   Connect BME280 sensor for real environmental data");
    }

    app_initialized = true;
    kernel_log(LOG_LEVEL_INFO, "IrrigAppMaster: Initialization complete");

    return SYS_OK;
}

void irrig_app_master_start(void) {
    kernel_log(LOG_LEVEL_INFO, "IrrigAppMaster: Starting irrigation system...");

    if (bmeInitialized) {
        kernel_log(LOG_LEVEL_INFO, "✅ Running in REAL HARDWARE mode with BME280 sensor");
        kernel_log(LOG_LEVEL_INFO, "✅ Environmental data from REAL BME280 sensor");
    } else {
        kernel_log(LOG_LEVEL_INFO, "⚠️  Running in SIMULATION mode (BME280 not found)");
        kernel_log(LOG_LEVEL_INFO, "⚠️  Environmental data SIMULATED until sensor connected");
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

    // Les capteurs sont lus automatiquement par Slave1 et reçus via HTTP
    // Pas besoin de readAllSensors() ici

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

    // Envoyer arrêt d'urgence à Slave2
    if (isIrrigating) {
        IrrigationCommandPacket_t cmd;
        cmd.command = CMD_EMERGENCY_STOP;
        cmd.zone_id = 0;
        cmd.duration_seconds = 0;
        cmd.zone_server_id[0] = '\0';
        cmd.timestamp = millis();

        irrig_comm_send_irrigation_command(&cmd);

        isIrrigating = false;
        activeIrrigationTimer = 0;
        kernel_log(LOG_LEVEL_INFO, "Emergency stop sent to Slave2");
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

    // Initialiser valeurs par défaut (sera écrasé par BME280)
    globalTemperature = 20.0;
    globalHumidity = 50.0;
    globalPressure = 1013.0;

    #ifdef USE_REAL_HARDWARE
        kernel_log(LOG_LEVEL_INFO, "Initialized 12 real moisture sensors");
        kernel_log(LOG_LEVEL_INFO, "Initialized 4 real zone relays + pump");
    #else
        kernel_log(LOG_LEVEL_INFO, "Initialized 12 simulated moisture sensors");
        kernel_log(LOG_LEVEL_INFO, "Initialized 4 simulated zone relays + pump");
    #endif
}

void initializeRealHardware(void) {
    // Hardware géré par Slaves, pas besoin d'initialiser ici
    kernel_log(LOG_LEVEL_INFO, "Hardware managed by Slave devices");
    kernel_log(LOG_LEVEL_INFO, "  Slave1: 12 moisture sensors (ADC)");
    kernel_log(LOG_LEVEL_INFO, "  Slave2: 4 zone relays + pump relay");

    // Initialiser le capteur BME280 (OBLIGATOIRE pour Master)
    kernel_log(LOG_LEVEL_INFO, "🔧 Initializing BME280 sensor (REQUIRED for Master)...");
    initializeBME280();
}


void initializeBME280(void) {
    kernel_log(LOG_LEVEL_INFO, "🔧 Initializing BME280 sensor...");

    // Utiliser des pins I2C différents du RTC (qui utilise 21/22)
    // BME280 utilisera GPIO 18 (SDA) et GPIO 19 (SCL)
    const uint8_t BME_SDA = 18;
    const uint8_t BME_SCL = 19;

    kernel_log(LOG_LEVEL_INFO, "   Using I2C pins: SDA=GPIO%d, SCL=GPIO%d", BME_SDA, BME_SCL);

    // Initialiser I2C sur des pins dédiés au BME280
    Wire.begin(BME_SDA, BME_SCL);

    // Essayer d'abord l'adresse 0x76, puis 0x77
    kernel_log(LOG_LEVEL_INFO, "   Trying BME280 at address 0x76...");
    bool status = bme.begin(0x76);

    if (!status) {
        kernel_log(LOG_LEVEL_INFO, "   Trying BME280 at address 0x77...");
        status = bme.begin(0x77);
        if (status) {
            kernel_log(LOG_LEVEL_INFO, "   ✅ BME280 found at address 0x77");
        }
    } else {
        kernel_log(LOG_LEVEL_INFO, "   ✅ BME280 found at address 0x76");
    }

    if (!status) {
        kernel_log(LOG_LEVEL_WARN, "   ❌ BME280 not found at 0x76 or 0x77 - falling back to simulation");
        kernel_log(LOG_LEVEL_WARN, "   To enable real sensor data, connect BME280:");
        kernel_log(LOG_LEVEL_WARN, "     - SDA → GPIO18, SCL → GPIO19");
        kernel_log(LOG_LEVEL_WARN, "     - VCC → 3.3V, GND → GND");
        kernel_log(LOG_LEVEL_WARN, "     - SDO → GND (addr 0x76) or VCC (addr 0x77)");
        kernel_log(LOG_LEVEL_WARN, "   Then restart the device");
        bmeInitialized = false;

        // Fallback vers simulation pour permettre les tests
        kernel_log(LOG_LEVEL_INFO, "   🔄 Using SIMULATED environmental data for testing");
        return;
    }

    bmeInitialized = true;
    kernel_log(LOG_LEVEL_INFO, "   ✅ BME280 sensor initialized successfully at 0x76");

    // Test de lecture simple (comme dans l'exemple)
    kernel_log(LOG_LEVEL_INFO, "   Testing sensor readings...");
    float temp = bme.readTemperature();
    float hum = bme.readHumidity();
    float press = bme.readPressure() / 100.0F;

    if (isnan(temp) || isnan(hum) || isnan(press)) {
        kernel_log(LOG_LEVEL_ERROR, "   ❌ Sensor readings failed - using simulation");
        bmeInitialized = false;
    } else {
        kernel_log(LOG_LEVEL_INFO, "   ✅ Sensor working: T=%.1f°C, H=%.1f%%, P=%.1fhPa",
                   temp, hum, press);
    }
}

void readBME280Data(void) {
    if (!bmeInitialized) {
        kernel_log(LOG_LEVEL_ERROR, "CRITICAL: BME280 not initialized, cannot read data");
        return;
    }

    // Lire les valeurs du capteur
    float temperature = bme.readTemperature();
    float humidity = bme.readHumidity();
    float pressure = bme.readPressure() / 100.0F; // Convertir Pa en hPa

    // Vérifier si les lectures sont valides
    if (isnan(temperature) || isnan(humidity) || isnan(pressure)) {
        kernel_log(LOG_LEVEL_ERROR, "CRITICAL: BME280 read error - invalid sensor data!");
        kernel_log(LOG_LEVEL_ERROR, "Check sensor connections and power supply");
        // Ne pas désactiver le capteur, continuer à essayer
        return;
    }

    // Mettre à jour les variables globales
    globalTemperature = temperature;
    globalHumidity = humidity;
    globalPressure = pressure;

    // Simuler le niveau de batterie (pas disponible sur BME280)
    float batteryVariation = (random(-50, 51) / 100.0);
    globalBatteryLevel = constrain(globalBatteryLevel + batteryVariation, 70.0, 100.0);

    kernel_log(LOG_LEVEL_DEBUG, "Master: REAL BME280 data - T=%.1f°C, H=%.1f%%, P=%.1fhPa, Bat=%.1f%% (simulated)",
               globalTemperature, globalHumidity, globalPressure, globalBatteryLevel);
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

    // Add hash header for change detection (désactivé temporairement)
    // TODO: réactiver pour optimisation production
    // if (lastConfigHash.length() > 0) {
    //     http.addHeader("X-Last-Config-Hash", lastConfigHash);
    // }

    int httpResponseCode = http.GET();

    if (httpResponseCode == 200) {
        String response = http.getString();
        kernel_log(LOG_LEVEL_DEBUG, "Config received, parsing...");

        // Update config hash from response headers (désactivé temporairement)
        // TODO: réactiver pour optimisation production
        // if (http.hasHeader("X-Config-Hash")) {
        //     lastConfigHash = http.header("X-Config-Hash");
        //     kernel_log(LOG_LEVEL_DEBUG, "New config hash: %s", lastConfigHash.c_str());
        // }

        parseConfiguration(response);
    } else if (httpResponseCode == 304) {
        kernel_log(LOG_LEVEL_DEBUG, "Configuration unchanged");
    } else if (httpResponseCode > 0) {
        String response = http.getString();
        kernel_log(LOG_LEVEL_ERROR, "Config poll failed: %d - %s", httpResponseCode, response.c_str());
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
                // Update physical zone number if provided
                if (zone.containsKey("physicalZoneNumber")) {
                    existingSlot->physicalZoneNumber = zone["physicalZoneNumber"];
                }

                existingSlot->waterPerDay = zone["waterPerDay"];
                existingSlot->humidityThreshold = zone["humidityThreshold"];

                // Parser les créneaux d'irrigation (supporte String unique ou Array)
                if (zone["irrigationTime"].is<String>()) {
                    // Format ancien : "08:00"
                    existingSlot->irrigationTimes[0] = zone["irrigationTime"].as<String>();
                    existingSlot->scheduleCount = 1;
                } else if (zone["irrigationTimes"].is<JsonArray>()) {
                    // Format nouveau : ["08:00", "14:00", "18:00"]
                    JsonArray times = zone["irrigationTimes"];
                    existingSlot->scheduleCount = min((int)times.size(), MAX_SCHEDULES_PER_ZONE);
                    for (int t = 0; t < existingSlot->scheduleCount; t++) {
                        existingSlot->irrigationTimes[t] = times[t].as<String>();
                    }
                }

                // IMPORTANT: Clear old sensors for this zone and reassign new ones
                kernel_log(LOG_LEVEL_INFO, "  🔄 Updating sensors for existing zone %s", zoneId.c_str());
                for (int s = 0; s < 12; s++) {
                    if (SENSOR_STACK[s].assigned && SENSOR_STACK[s].zoneId == zoneId) {
                        SENSOR_STACK[s].assigned = false;
                        SENSOR_STACK[s].zoneId = "";
                        SENSOR_STACK[s].id = "";
                    }
                }

                // Reassign new sensors from server config
                JsonArray sensors = zone["sensors"];
                for (int k = 0; k < sensors.size(); k++) {
                    JsonObject sensor = sensors[k];
                    String configSensorId = sensor["sensorId"].as<String>();

                    kernel_log(LOG_LEVEL_DEBUG, "    Reassigning sensor: %s", configSensorId.c_str());

                    // Find an available sensor slot and assign it with the real ID
                    for (int s = 0; s < 12; s++) {
                        if (!SENSOR_STACK[s].assigned) {
                            SENSOR_STACK[s].assigned = true;
                            SENSOR_STACK[s].id = configSensorId; // Use real ID from server
                            SENSOR_STACK[s].zoneId = zoneId;
                            kernel_log(LOG_LEVEL_DEBUG, "    ✅ Sensor %s assigned to slot %d for zone %s",
                                      configSensorId.c_str(), s, zoneId.c_str());
                            break;
                        }
                    }
                }

                kernel_log(LOG_LEVEL_INFO, "  Water: %dml/day, Schedules: %d, Threshold: %d%% (updated)",
                           existingSlot->waterPerDay, existingSlot->scheduleCount, existingSlot->humidityThreshold);
                for (int t = 0; t < existingSlot->scheduleCount; t++) {
                    kernel_log(LOG_LEVEL_INFO, "    Schedule %d/%d: %s",
                               t + 1, existingSlot->scheduleCount, existingSlot->irrigationTimes[t].c_str());
                }
                continue;
            }

            // Get physical zone number from server configuration
            int physicalZoneNumber = zone["physicalZoneNumber"] | (i + 1); // Default to sequential if not specified
            int zoneIndex = physicalZoneNumber - 1; // Convert to 0-based index

            // Validate zone index
            if (zoneIndex < 0 || zoneIndex >= 4) {
                kernel_log(LOG_LEVEL_WARN, "  ⚠️ Invalid physical zone number %d, skipping", physicalZoneNumber);
                continue;
            }

            // Use the specified physical zone slot
            ZoneSlot* slot = &ZONE_STACK[zoneIndex];

            if (slot->configured && slot->zoneId != zoneId) {
                kernel_log(LOG_LEVEL_WARN, "  ⚠️ Physical zone %d already occupied by %s, skipping %s",
                          physicalZoneNumber, slot->zoneId.c_str(), zoneId.c_str());
                continue;
            }

            slot->configured = true;
            slot->zoneId = zoneId;
            slot->physicalZoneNumber = physicalZoneNumber;  // Store physical zone number
            slot->waterPerDay = zone["waterPerDay"];
            slot->humidityThreshold = zone["humidityThreshold"];

            // Parser les créneaux d'irrigation (supporte String unique ou Array)
            if (zone["irrigationTime"].is<String>()) {
                // Format ancien : "08:00"
                slot->irrigationTimes[0] = zone["irrigationTime"].as<String>();
                slot->scheduleCount = 1;
            } else if (zone["irrigationTimes"].is<JsonArray>()) {
                // Format nouveau : ["08:00", "14:00", "18:00"]
                JsonArray times = zone["irrigationTimes"];
                slot->scheduleCount = min((int)times.size(), MAX_SCHEDULES_PER_ZONE);
                for (int t = 0; t < slot->scheduleCount; t++) {
                    slot->irrigationTimes[t] = times[t].as<String>();
                }
            }

            // Assign sensors to this zone with proper sequential sensor ID mapping
            JsonArray sensors = zone["sensors"];

            // Calculate baseOffset by counting sensors from all previously configured zones
            int baseOffset = 0;
            int currentSlotIndex = slot - ZONE_STACK;

            // Count sensors from zones that are already configured in earlier slots
            for (int z = 0; z < currentSlotIndex; z++) {
                if (ZONE_STACK[z].configured) {
                    // Count actual sensors assigned to this zone
                    for (int s = 0; s < 12; s++) {
                        if (SENSOR_STACK[s].assigned && SENSOR_STACK[s].zoneId == ZONE_STACK[z].zoneId) {
                            baseOffset++;
                        }
                    }
                }
            }

            for (int k = 0; k < sensors.size(); k++) {
                JsonObject sensor = sensors[k];
                String configSensorId = sensor["sensorId"].as<String>();

                // Use the actual sensor ID from server configuration (s04, s05, etc.)
                kernel_log(LOG_LEVEL_DEBUG, "    Assigning sensor: %s", configSensorId.c_str());

                // Find an available sensor slot and assign it with the real ID
                for (int s = 0; s < 12; s++) {
                    if (!SENSOR_STACK[s].assigned) {
                        SENSOR_STACK[s].assigned = true;
                        SENSOR_STACK[s].id = configSensorId; // Use real ID from server
                        SENSOR_STACK[s].zoneId = zoneId;
                        kernel_log(LOG_LEVEL_DEBUG, "    ✅ Sensor %s assigned to slot %d for zone %s",
                                  configSensorId.c_str(), s, zoneId.c_str());
                        break;
                    }
                }
            }

            assignedZones[assignedZoneCount++] = zoneId;

            kernel_log(LOG_LEVEL_INFO, "Zone %d (Physical #%d): %s", slot->id, physicalZoneNumber, zoneId.c_str());
            kernel_log(LOG_LEVEL_INFO, "  Water: %dml/day, Schedules: %d, Threshold: %d%%",
                       slot->waterPerDay, slot->scheduleCount, slot->humidityThreshold);
            for (int t = 0; t < slot->scheduleCount; t++) {
                kernel_log(LOG_LEVEL_INFO, "    Schedule %d/%d: %s",
                           t + 1, slot->scheduleCount, slot->irrigationTimes[t].c_str());
            }
            kernel_log(LOG_LEVEL_INFO, "  Sensors: %d", sensors.size());
        }

        // Mettre à jour statut assignation
        deviceAssigned = (assignedZoneCount > 0);

        // Afficher les capteurs utilisés par chaque zone
        displaySensorIdsPerZone();
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
        for (int s = 0; s < MAX_SCHEDULES_PER_ZONE; s++) {
            zoneSlot->irrigationTimes[s] = "";
        }
        zoneSlot->scheduleCount = 0;
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

// ===== GESTION CAPTEURS (Reçus de Slave1 via HTTP) =====

void updateGlobalEnvironmentData(void) {
    if (!bmeInitialized) {
        kernel_log(LOG_LEVEL_ERROR, "CRITICAL: BME280 not initialized - cannot read environmental data!");
        kernel_log(LOG_LEVEL_ERROR, "System requires BME280 sensor to be connected and working");
        // Ne pas continuer - les données seraient invalides
        return;
    }

    // ✅ LIRE UNIQUEMENT données réelles du capteur BME280
    readBME280Data();
}

void updateSensorDataFromSlave(float moisture[MAX_SENSORS], float temp, float hum, float press) {
    // ✅ Mettre à jour UNIQUEMENT données d'humidité reçues de Slave1
    for (int i = 0; i < MAX_SENSORS; i++) {
        simulatedMoisture[i] = moisture[i];
    }

    // ✅ LIRE données environnementales du BME280 (obligatoire)
    updateGlobalEnvironmentData();

    // Afficher résumé des données
    kernel_log(LOG_LEVEL_INFO, "📊 Master: Data updated");
    kernel_log(LOG_LEVEL_INFO, "   Moisture: %d sensors from Slave1", MAX_SENSORS);
    kernel_log(LOG_LEVEL_INFO, "   Environment: T=%.1f°C, H=%.1f%%, P=%.1fhPa (BME280), Bat=%.1f%% (simulated)",
               globalTemperature, globalHumidity, globalPressure, globalBatteryLevel);
    kernel_log(LOG_LEVEL_INFO, "   WiFi RSSI: %d dBm (Master)", WiFi.RSSI());

    // Afficher détails par zone (DEBUG uniquement)
    if (assignedZoneCount > 0) {
        // Clean up orphaned sensors first (sensors assigned to deleted zones)
        for (int s = 0; s < 12; s++) {
            if (SENSOR_STACK[s].assigned) {
                bool zoneExists = false;
                for (int i = 0; i < 4; i++) {
                    if (ZONE_STACK[i].configured && ZONE_STACK[i].zoneId == SENSOR_STACK[s].zoneId) {
                        zoneExists = true;
                        break;
                    }
                }

                if (!zoneExists) {
                    kernel_log(LOG_LEVEL_INFO, "🧹 Cleaning orphaned sensor %s (zone no longer exists)", SENSOR_STACK[s].id.c_str());
                    SENSOR_STACK[s].assigned = false;
                    SENSOR_STACK[s].zoneId = "";
                }
            }
        }

        // Display sensors using physical zone numbers
for (int i = 0; i < 4; i++) {
    if (ZONE_STACK[i].configured) {
        int sensorCount = 0;
        float avgMoisture = 0.0;

        for (int s = 0; s < 12; s++) {
            if (SENSOR_STACK[s].assigned && SENSOR_STACK[s].zoneId == ZONE_STACK[i].zoneId) {
                sensorCount++;
                // Correction inversion capteurs pour affichage cohérent
                float correctedMoisture = 100.0f - simulatedMoisture[s];
                avgMoisture += correctedMoisture;
                kernel_log(LOG_LEVEL_DEBUG, "  Zone %d, Sensor %d (%s): %.1f%% (raw: %.1f%%)",
                           ZONE_STACK[i].physicalZoneNumber, sensorCount, SENSOR_STACK[s].id.c_str(),
                           correctedMoisture, simulatedMoisture[s]);
            }
        }

        if (sensorCount > 0) {
            avgMoisture /= sensorCount;
            kernel_log(LOG_LEVEL_INFO, "   Zone %d (%s): %.1f%% avg (%d sensors)",
                       ZONE_STACK[i].physicalZoneNumber, ZONE_STACK[i].zoneId.c_str(), avgMoisture, sensorCount);
        }
    }
}
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

    // ✅ Données environnementales globales
    if (bmeInitialized) {
        kernel_log(LOG_LEVEL_DEBUG, "📤 Sending REAL BME280 data to server: T=%.1f°C, H=%.1f%%, P=%.1fhPa",
                   globalTemperature, globalHumidity, globalPressure);
    } else {
        kernel_log(LOG_LEVEL_DEBUG, "📤 Sending SIMULATED data to server: T=%.1f°C, H=%.1f%%, P=%.1fhPa",
                   globalTemperature, globalHumidity, globalPressure);
    }

    JsonObject globalData = doc.createNestedObject("globalData");
    globalData["temperature"] = globalTemperature;      // ✅ RÉEL (BME280 obligatoire)
    globalData["humidity"] = globalHumidity;            // ✅ RÉEL (BME280 obligatoire)
    globalData["pressure"] = globalPressure;            // ✅ RÉEL (BME280 obligatoire)
    globalData["batteryLevel"] = globalBatteryLevel;    // Simulé (pas sur BME280)
    globalData["signalStrength"] = WiFi.RSSI();         // ✅ RÉEL

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
        kernel_log(LOG_LEVEL_DEBUG, "⏰ Master: Cannot get local time for schedule check");
        return;
    }

    // ✅ FLAG ANTI-SPAM : Vérifier seulement au changement de minute
    static uint8_t last_checked_minute = 255;
    if (timeinfo.tm_min == last_checked_minute) {
        return;  // Déjà vérifié cette minute
    }
    last_checked_minute = timeinfo.tm_min;

    char currentTime[6];
    strftime(currentTime, sizeof(currentTime), "%H:%M", &timeinfo);

    kernel_log(LOG_LEVEL_DEBUG, "⏰ Master: Checking irrigation schedule (current time: %s)", currentTime);

    // Parcourir toutes les zones configurées
    for (int i = 0; i < 4; i++) {
        if (!ZONE_STACK[i].configured) continue;

        // Vérifier chaque créneau de cette zone
        bool matchFound = false;
        int matchedSchedule = -1;

        for (int s = 0; s < ZONE_STACK[i].scheduleCount; s++) {
            if (ZONE_STACK[i].irrigationTimes[s].equals(String(currentTime))) {
                matchFound = true;
                matchedSchedule = s;
                break;
            }
        }

        if (matchFound) {
            kernel_log(LOG_LEVEL_INFO, "🎯 Master: SCHEDULED IRRIGATION TRIGGERED!");
            kernel_log(LOG_LEVEL_INFO, "   Zone: %s (slot %d)", ZONE_STACK[i].zoneId.c_str(), ZONE_STACK[i].id);
            kernel_log(LOG_LEVEL_INFO, "   Schedule: %d/%d at %s (MATCH!)",
                       matchedSchedule + 1, ZONE_STACK[i].scheduleCount, currentTime);
            kernel_log(LOG_LEVEL_INFO, "   Duration: %ds (based on %dml/day)",
                       ZONE_STACK[i].waterPerDay / 10, ZONE_STACK[i].waterPerDay);
            sendIrrigationCommand(ZONE_STACK[i].zoneId, ZONE_STACK[i].waterPerDay / 10);
        } else {
            kernel_log(LOG_LEVEL_DEBUG, "   Zone %s: %d schedules, current=%s (no match)",
                       ZONE_STACK[i].zoneId.c_str(), ZONE_STACK[i].scheduleCount, currentTime);
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
                // Correction inversion capteurs: 0% = très humide, 100% = très sec
                float correctedMoisture = 100.0f - simulatedMoisture[s];
                totalMoisture += correctedMoisture;
                sensorCount++;
            }
        }

        if (sensorCount > 0) {
            float avgMoisture = totalMoisture / sensorCount;

            if (avgMoisture < ZONE_STACK[i].humidityThreshold) {
                kernel_log(LOG_LEVEL_WARN, "Zone %d moisture critical: %.1f%% < %d%%",
                           ZONE_STACK[i].id, avgMoisture, ZONE_STACK[i].humidityThreshold);

                // Irrigation d'urgence
                sendIrrigationCommand(ZONE_STACK[i].zoneId, 60); // 1 minute
            }
        }
    }
}

void sendIrrigationCommand(String zoneId, int durationSeconds) {
    kernel_log(LOG_LEVEL_INFO, "📤 Master: Preparing irrigation command...");
    kernel_log(LOG_LEVEL_INFO, "   Target zone: %s", zoneId.c_str());
    kernel_log(LOG_LEVEL_INFO, "   Duration: %ds", durationSeconds);

    // Trouver le slot de zone
    ZoneSlot* zoneSlot = nullptr;
    for (int i = 0; i < 4; i++) {
        if (ZONE_STACK[i].configured && ZONE_STACK[i].zoneId == zoneId) {
            zoneSlot = &ZONE_STACK[i];
            kernel_log(LOG_LEVEL_INFO, "   Found in slot %d (physical zone %d)", i, zoneSlot->physicalZoneNumber);
            break;
        }
    }

    if (!zoneSlot) {
        kernel_log(LOG_LEVEL_ERROR, "❌ Master: Zone %s not found or not configured", zoneId.c_str());
        return;
    }

    if (isIrrigating) {
        kernel_log(LOG_LEVEL_WARN, "⚠️  Master: Irrigation already in progress, command rejected");
        return;
    }

    // Créer commande pour Slave2
    IrrigationCommandPacket_t cmd;
    cmd.command = CMD_START_IRRIGATION;
    cmd.zone_id = zoneSlot->physicalZoneNumber - 1;  // Convertir numéro physique 1-4 vers 0-3
    cmd.duration_seconds = durationSeconds;
    strncpy(cmd.zone_server_id, zoneId.c_str(), 63);
    cmd.zone_server_id[63] = '\0';
    cmd.timestamp = millis();

    kernel_log(LOG_LEVEL_INFO, "📤 Master → Slave2: Sending irrigation command");
    kernel_log(LOG_LEVEL_INFO, "   Command: START_IRRIGATION");
    kernel_log(LOG_LEVEL_INFO, "   Zone ID (hardware): %d", cmd.zone_id);
    kernel_log(LOG_LEVEL_INFO, "   Zone ID (server): %s", cmd.zone_server_id);
    kernel_log(LOG_LEVEL_INFO, "   Duration: %ds", cmd.duration_seconds);

    // Envoyer commande via HTTP
    if (irrig_comm_send_irrigation_command(&cmd)) {
        kernel_log(LOG_LEVEL_INFO, "✅ Master → Slave2: Command sent successfully!");
        isIrrigating = true;
        activeIrrigationTimer = millis() + (durationSeconds * 1000);
        kernel_log(LOG_LEVEL_INFO, "   Irrigation timer set: %lus", activeIrrigationTimer / 1000);
    } else {
        kernel_log(LOG_LEVEL_ERROR, "❌ Master → Slave2: Failed to send command");
    }
}

void checkIrrigationTimer(void) {
    if (isIrrigating && activeIrrigationTimer > 0 && millis() >= activeIrrigationTimer) {
        // Timer expiré, irrigation devrait être terminée
        kernel_log(LOG_LEVEL_INFO, "⏱️  Master: Irrigation timer expired");
        kernel_log(LOG_LEVEL_INFO, "   Irrigation should be complete on Slave2");
        isIrrigating = false;
        activeIrrigationTimer = 0;
    }
}

// ===== AFFICHAGE CAPTEURS PAR ZONE =====

void displaySensorIdsPerZone(void) {
    kernel_log(LOG_LEVEL_INFO, "=== SENSOR IDs USED FOR EACH CONFIGURED ZONE ===");

    bool hasConfiguredZones = false;

    for (int i = 0; i < 4; i++) {
        if (ZONE_STACK[i].configured) {
            hasConfiguredZones = true;
            String sensorIds = "";

            for (int s = 0; s < 12; s++) {
                if (SENSOR_STACK[s].assigned && SENSOR_STACK[s].zoneId == ZONE_STACK[i].zoneId) {
                    if (sensorIds.length() > 0) {
                        sensorIds += ", ";
                    }
                    sensorIds += SENSOR_STACK[s].id;
                }
            }

            kernel_log(LOG_LEVEL_INFO, "Zone %d (Physical #%d, ID: %s): Sensors [%s]",
                       ZONE_STACK[i].id, ZONE_STACK[i].physicalZoneNumber,
                       ZONE_STACK[i].zoneId.c_str(), sensorIds.c_str());
        }
    }

    if (!hasConfiguredZones) {
        kernel_log(LOG_LEVEL_INFO, "No zones configured - no sensors assigned");
    }

    kernel_log(LOG_LEVEL_INFO, "=================================================");
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

