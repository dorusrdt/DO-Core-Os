#include "ESP32_master.h"
#include "../../kernel/app/app_manager.h"
#include "../../kernel/core/log_system_optimized.h"
#include <WiFi.h>
#include <WebSocketsServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <mbedtls/md.h>
#include <time.h>
#include <cstring>

// Configuration AP pour les clients (ESP32_sensor et ESP32_com)
const char* ap_ssid = "ESP32_MASTER";
const char* ap_pass = "12345678";

// Configuration serveur externe
static const char* serverURL = "http://192.168.1.72:8000";
static const char* deviceId = "ESP32_IRRIGATION_11100454456464674";
static const char* deviceSecret = "esp32-secure-key-2024";

// Device Location
#define DEVICE_LATITUDE 35.6695
#define DEVICE_LONGITUDE -5.7857

// Hardware Configuration
#define MAX_ZONES 4
#define MAX_SENSORS 12
#define SENSORS_PER_ZONE 10

// I2C Configuration
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22
#define BME280_I2C_ADDR 0x76

#define MASTER_LOG(level, fmt, ...) \
    kernel_log(level, "[ESP32_master] " fmt, ##__VA_ARGS__)

// Zone Stack Structure (exactly like versio)
struct ZoneSlot {
    int id;  // Array index (0-3)
    int physicalZoneNumber;  // Physical relay/zone number from server (1-4)
    bool configured;
    String zoneId;
    int waterPerDay;
    String irrigationTime;
    int humidityThreshold;
};

// Sensor Stack Structure (exactly like versio)
struct SensorSlot {
    String id;
    bool assigned;
    String zoneId;
};

// Global Variables
static WebSocketsServer* webSocket = nullptr;
static bool app_running = false;

static Adafruit_BME280 bmeSensor;
static bool bme_ready = false;
static bool bme_simulated = false;

typedef struct {
    float temperature_c = NAN;
    float humidity_pct = NAN;
    float pressure_hpa = NAN;
    unsigned long last_sample_ms = 0;
} BmeTelemetry_t;

static BmeTelemetry_t bme_data;

// Irrigation system state (exactly like versio)
static ZoneSlot ZONE_STACK[MAX_ZONES];
static SensorSlot SENSOR_STACK[MAX_SENSORS];
static String assignedZones[MAX_ZONES];
static int assignedZoneCount = 0;

static unsigned long lastConfigPoll = 0;
static unsigned long lastSensorRead = 0;
static unsigned long lastDataSend = 0;
static bool deviceRegistered = false;
static bool deviceAssigned = false;

// Global sensor data for environmental conditions (exactly like versio)
static float globalTemperature = 24.5;
static float globalHumidity = 60.0;
static float globalPressure = 1012.0;

// Irrigation Control Variables (exactly like versio)
static bool isIrrigating = false;
static unsigned long activeIrrigationTimer = 0;
static String activeZoneId = "";

// Collecte des données slaves (sensor readings from ESP32_sensor)
// Buffer pour recevoir les données de l'ESP32_sensor
// Taille optimisée : 12 capteurs (s01-s12) + timestamp + deviceType ≈ 300 bytes max
// On alloue 1024 bytes pour marge de sécurité
// IMPORTANT: Ce document stocke directement le JSON parsé des données sensor
static DynamicJsonDocument slaveSensorData(1024);
static unsigned long slaveSensorDataLastUpdate = 0;

// Forward declarations
static bool init_bme280();
static void refresh_bme280();
static void simulate_bme280();
static void registerDevice();
static void pollConfiguration();
static void parseConfiguration(String jsonResponse);
static void handleZoneDeletion(String zoneId);
static void sendSensorData();
static void checkIrrigationSchedule();
static void checkMoistureThresholds();
static void executeIrrigation(String zoneId, int durationSeconds);
static void checkIrrigationTimer();
static String generateHMAC(String data);
static String getTimestamp();
static void updateSlaveSensorData(uint8_t slaveId, String data);

// Initialize BME280
static bool init_bme280() {
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    MASTER_LOG(LOG_LEVEL_INFO, "Initializing BME280 on SDA=%d SCL=%d addr=0x%02X",
               I2C_SDA_PIN, I2C_SCL_PIN, BME280_I2C_ADDR);

    if (!bmeSensor.begin(BME280_I2C_ADDR)) {
        MASTER_LOG(LOG_LEVEL_WARN, "BME280 not detected. Will simulate values.");
        bme_simulated = true;
        return false;
    }

    bmeSensor.setSampling(
        Adafruit_BME280::MODE_NORMAL,
        Adafruit_BME280::SAMPLING_X2,   // temperature
        Adafruit_BME280::SAMPLING_X16,  // pressure
        Adafruit_BME280::SAMPLING_X1,   // humidity
        Adafruit_BME280::FILTER_X4,
        Adafruit_BME280::STANDBY_MS_500
    );

    MASTER_LOG(LOG_LEVEL_INFO, "BME280 ready");
    bme_simulated = false;
    return true;
}

// Refresh BME280 or simulate
static void refresh_bme280() {
    const unsigned long now = millis();
    if ((now - bme_data.last_sample_ms) < 5000) {
        return;
    }

    if (bme_ready && !bme_simulated) {
        bme_data.temperature_c = bmeSensor.readTemperature();
        bme_data.humidity_pct = bmeSensor.readHumidity();
        bme_data.pressure_hpa = bmeSensor.readPressure() / 100.0F;
    } else {
        simulate_bme280();
    }

    bme_data.last_sample_ms = now;

    MASTER_LOG(LOG_LEVEL_DEBUG,
               "BME280 sample T=%.2f°C H=%.2f%% P=%.2fhPa %s",
               bme_data.temperature_c,
               bme_data.humidity_pct,
               bme_data.pressure_hpa,
               bme_simulated ? "(simulated)" : "");
}

// Simulate BME280 values (exactly like versio logic)
static void simulate_bme280() {
    float tempVariation = (random(-100, 101) / 100.0); // ±1°C
    float humidityVariation = (random(-250, 251) / 100.0); // ±2.5%
    float pressureVariation = (random(-500, 501) / 100.0); // ±5 hPa

    globalTemperature = constrain(globalTemperature + tempVariation, 15, 40);
    globalHumidity = constrain(globalHumidity + humidityVariation, 30, 90);
    globalPressure = constrain(globalPressure + pressureVariation, 990, 1030);

    bme_data.temperature_c = globalTemperature;
    bme_data.humidity_pct = globalHumidity;
    bme_data.pressure_hpa = globalPressure;
}

// Update sensor data from ESP32_sensor slave
// IMPORTANT: Parse directement le JSON reçu et stocke-le dans slaveSensorData
static void updateSlaveSensorData(uint8_t slaveId, String data) {
    // Parser directement le JSON reçu dans slaveSensorData
    DeserializationError error = deserializeJson(slaveSensorData, data);

    if (error) {
        MASTER_LOG(LOG_LEVEL_WARN, "Failed to parse sensor data from slave_%u: %s", slaveId, error.c_str());
        MASTER_LOG(LOG_LEVEL_DEBUG, "Raw data: %s", data.c_str());
        return;
    }

    slaveSensorDataLastUpdate = millis();
    MASTER_LOG(LOG_LEVEL_INFO, "Received sensor data from slave_%u | bytes=%u", slaveId, data.length());
    MASTER_LOG(LOG_LEVEL_DEBUG, "Sensor data parsed successfully, sensors: %d", slaveSensorData.size());

    // Log des valeurs pour debug
    for (int i = 1; i <= MAX_SENSORS; i++) {
        String sensorId = (i < 10) ? "s0" + String(i) : "s" + String(i);
        if (slaveSensorData.containsKey(sensorId)) {
            float value = slaveSensorData[sensorId];
            MASTER_LOG(LOG_LEVEL_DEBUG, "  %s = %.1f%%", sensorId.c_str(), value);
        }
    }
}

// Generate HMAC signature (exactly like versio - simplified for now)
static String generateHMAC(String data) {
    // Simple HMAC-SHA256 implementation placeholder
    // In production, use proper HMAC library
    return "dummy_signature_" + String(millis());
}

// Get timestamp (exactly like versio)
static String getTimestamp() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return String(millis()); // Fallback to millis
    }

    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S.000Z", &timeinfo);
    return String(timestamp);
}

// Register device with server (exactly like versio)
static void registerDevice() {
    if (WiFi.status() != WL_CONNECTED) {
        MASTER_LOG(LOG_LEVEL_WARN, "Cannot register: WiFi not connected");
        return;
    }

    MASTER_LOG(LOG_LEVEL_INFO, "Registering device with server...");
    MASTER_LOG(LOG_LEVEL_DEBUG, "Server URL: %s", serverURL);
    MASTER_LOG(LOG_LEVEL_DEBUG, "Device ID: %s", deviceId);

    HTTPClient http;
    http.setTimeout(30000); // 30 second timeout

    String url = String(serverURL) + "/api/devices/register";
    MASTER_LOG(LOG_LEVEL_DEBUG, "POST URL: %s", url.c_str());

    if (!http.begin(url)) {
        MASTER_LOG(LOG_LEVEL_ERROR, "Failed to initialize HTTP client");
        return;
    }

    http.addHeader("Content-Type", "application/json");

    // Create registration payload (exactly like versio)
    DynamicJsonDocument doc(1024);
    doc["type"] = "register";
    doc["deviceId"] = deviceId;
    doc["capacity"]["zones"] = MAX_ZONES;
    doc["capacity"]["sensors"] = MAX_SENSORS;
    doc["timestamp"] = getTimestamp();
    doc["latitude"] = DEVICE_LATITUDE;
    doc["longitude"] = DEVICE_LONGITUDE;

    String payload;
    serializeJson(doc, payload);
    MASTER_LOG(LOG_LEVEL_DEBUG, "Registration payload: %s", payload.c_str());

    // Add HMAC signature (exactly like versio)
    String timestamp = String(millis());
    String signature = generateHMAC(payload + timestamp);
    http.addHeader("X-Signature", signature);
    http.addHeader("X-Timestamp", timestamp);

    MASTER_LOG(LOG_LEVEL_INFO, "Sending registration request...");

    int retryCount = 0;
    int maxRetries = 3;
    int httpResponseCode = -1;

    while (retryCount < maxRetries && httpResponseCode <= 0) {
        if (retryCount > 0) {
            MASTER_LOG(LOG_LEVEL_DEBUG, "Retry attempt %d/%d...", retryCount + 1, maxRetries);
            delay(5000);
        }

        httpResponseCode = http.POST(payload);
        MASTER_LOG(LOG_LEVEL_DEBUG, "Response code: %d", httpResponseCode);

        if (httpResponseCode > 0) {
            String response = http.getString();
            MASTER_LOG(LOG_LEVEL_DEBUG, "Response: %s", response.c_str());

            if (httpResponseCode == 200 || httpResponseCode == 201) {
                MASTER_LOG(LOG_LEVEL_INFO, "Device registered successfully");
                deviceRegistered = true;
                break;
            } else {
                MASTER_LOG(LOG_LEVEL_ERROR, "Registration failed with code: %d", httpResponseCode);
                deviceRegistered = false;
            }
        } else {
            MASTER_LOG(LOG_LEVEL_ERROR, "HTTP request failed: %s", http.errorToString(httpResponseCode).c_str());
            retryCount++;
        }
    }

    if (!deviceRegistered) {
        MASTER_LOG(LOG_LEVEL_ERROR, "Registration failed after %d attempts", maxRetries);
    }

    http.end();
}

// Poll configuration from server (exactly like versio)
static void pollConfiguration() {
    if (WiFi.status() != WL_CONNECTED) {
        MASTER_LOG(LOG_LEVEL_WARN, "Cannot poll config: WiFi not connected");
        return;
    }

    if (!deviceRegistered) {
        MASTER_LOG(LOG_LEVEL_WARN, "Cannot poll config: Device not registered");
        return;
    }

    MASTER_LOG(LOG_LEVEL_DEBUG, "Polling configuration...");

    HTTPClient http;
    http.setTimeout(10000);
    http.setConnectTimeout(5000);

    String url = String(serverURL) + "/api/devices/" + String(deviceId) + "/config";
    MASTER_LOG(LOG_LEVEL_DEBUG, "GET URL: %s", url.c_str());

    if (!http.begin(url)) {
        MASTER_LOG(LOG_LEVEL_ERROR, "Failed to initialize HTTP client for config");
        return;
    }

    // Add hash header for change detection (exactly like versio)
    static String lastConfigHash = "";
    if (lastConfigHash.length() > 0) {
        http.addHeader("X-Last-Config-Hash", lastConfigHash);
    }

    int httpResponseCode = http.GET();
    MASTER_LOG(LOG_LEVEL_DEBUG, "Config response code: %d", httpResponseCode);

    if (httpResponseCode == 200) {
        String response = http.getString();
        MASTER_LOG(LOG_LEVEL_DEBUG, "Config response: %s", response.c_str());

        // Update config hash from response headers (exactly like versio)
        if (http.hasHeader("X-Config-Hash")) {
            lastConfigHash = http.header("X-Config-Hash");
            MASTER_LOG(LOG_LEVEL_DEBUG, "New config hash: %s", lastConfigHash.c_str());
        }

        parseConfiguration(response);
    } else if (httpResponseCode == 304) {
        MASTER_LOG(LOG_LEVEL_DEBUG, "Configuration unchanged");
    } else if (httpResponseCode > 0) {
        String response = http.getString();
        MASTER_LOG(LOG_LEVEL_ERROR, "Config poll failed: %d - %s", httpResponseCode, response.c_str());
    } else {
        MASTER_LOG(LOG_LEVEL_ERROR, "HTTP request failed: %s", http.errorToString(httpResponseCode).c_str());
    }

    http.end();
}

// Parse configuration (exactly like versio - preserving all logic)
static void parseConfiguration(String jsonResponse) {
    // Buffer optimisé : config peut contenir jusqu'à 4 zones avec leurs capteurs
    // Estimation : ~2-3 KB max pour une config complète
    DynamicJsonDocument doc(4096);
    deserializeJson(doc, jsonResponse);

    // Check for direct delete_zone command (exactly like versio)
    if (doc["type"] == "delete_zone") {
        String zoneId = doc["zoneId"];
        handleZoneDeletion(zoneId);
        return;
    }

    // Check for pending commands first (exactly like versio)
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

        MASTER_LOG(LOG_LEVEL_INFO, "Received configuration for %d zones", newZoneCount);

        // Process each zone in the configuration (exactly like versio)
        for (int i = 0; i < newZoneCount && i < MAX_ZONES; i++) {
            JsonObject zone = zonesArray[i];
            String zoneId = zone["zoneId"].as<String>();

            // Check if this zone already exists (preserve existing zones)
            ZoneSlot* existingSlot = nullptr;
            for (int j = 0; j < MAX_ZONES; j++) {
                if (ZONE_STACK[j].configured && ZONE_STACK[j].zoneId == zoneId) {
                    existingSlot = &ZONE_STACK[j];
                    MASTER_LOG(LOG_LEVEL_INFO, "Zone %d: %s (existing - preserving sensors)", existingSlot->id, zoneId.c_str());
                    break;
                }
            }

            // If zone exists, update config AND reassign sensors (exactly like versio)
            if (existingSlot) {
                existingSlot->waterPerDay = zone["waterPerDay"];
                existingSlot->irrigationTime = zone["irrigationTime"].as<String>();
                existingSlot->humidityThreshold = zone["humidityThreshold"];
                MASTER_LOG(LOG_LEVEL_INFO, "Water: %dml/day, Time: %s, Threshold: %d%% (updated)",
                         existingSlot->waterPerDay, existingSlot->irrigationTime.c_str(), existingSlot->humidityThreshold);

                // IMPORTANT: Clear old sensors for this zone and reassign new ones (exactly like versio)
                MASTER_LOG(LOG_LEVEL_DEBUG, "Updating sensors for existing zone %s", zoneId.c_str());
                for (int s = 0; s < MAX_SENSORS; s++) {
                    if (SENSOR_STACK[s].assigned && SENSOR_STACK[s].zoneId == zoneId) {
                        SENSOR_STACK[s].assigned = false;
                        SENSOR_STACK[s].zoneId = "";
                        SENSOR_STACK[s].id = "";
                    }
                }

                // Reassign new sensors from server config (exactly like versio)
                JsonArray sensors = zone["sensors"];
                for (int k = 0; k < sensors.size(); k++) {
                    JsonObject sensor = sensors[k];
                    String configSensorId = sensor["sensorId"].as<String>();

                    MASTER_LOG(LOG_LEVEL_DEBUG, "Reassigning sensor: %s", configSensorId.c_str());

                    // Find an available sensor slot and assign it with the real ID
                    for (int s = 0; s < MAX_SENSORS; s++) {
                        if (!SENSOR_STACK[s].assigned) {
                            SENSOR_STACK[s].assigned = true;
                            SENSOR_STACK[s].id = configSensorId;
                            SENSOR_STACK[s].zoneId = zoneId;
                            MASTER_LOG(LOG_LEVEL_DEBUG, "Sensor %s assigned to slot %d for zone %s",
                                     configSensorId.c_str(), s, zoneId.c_str());
                            break;
                        }
                    }
                }

                // Build sensor list string for logging with sensor data if available
                String sensorList = "";
                String sensorDataList = "";
                int sensorCount = 0;

                // Utiliser directement slaveSensorData (déjà parsé)
                bool hasSensorData = (slaveSensorDataLastUpdate > 0 &&
                                     (millis() - slaveSensorDataLastUpdate) < 10000); // Données récentes (< 10s)
                DynamicJsonDocument& sensorDocUpdate = slaveSensorData; // Référence directe

                for (int s = 0; s < MAX_SENSORS; s++) {
                    if (SENSOR_STACK[s].assigned && SENSOR_STACK[s].zoneId == zoneId) {
                        if (sensorCount > 0) {
                            sensorList += ", ";
                            sensorDataList += ", ";
                        }
                        sensorList += SENSOR_STACK[s].id;

                        // Add sensor value if available
                        if (hasSensorData && sensorDocUpdate.containsKey(SENSOR_STACK[s].id)) {
                            float sensorValue = sensorDocUpdate[SENSOR_STACK[s].id];
                            sensorDataList += SENSOR_STACK[s].id;
                            sensorDataList += "=";
                            sensorDataList += String(sensorValue, 1);
                            sensorDataList += "%";
                        } else {
                            sensorDataList += SENSOR_STACK[s].id;
                            sensorDataList += "=N/A";
                        }
                        sensorCount++;
                    }
                }

                if (hasSensorData) {
                    MASTER_LOG(LOG_LEVEL_INFO, "Zone %d: %s updated with %d sensors: [%s] | Values: [%s]",
                             existingSlot->id, zoneId.c_str(), sensors.size(), sensorList.c_str(), sensorDataList.c_str());
                } else {
                    MASTER_LOG(LOG_LEVEL_INFO, "Zone %d: %s updated with %d sensors: [%s]",
                             existingSlot->id, zoneId.c_str(), sensors.size(), sensorList.c_str());
                }

                continue; // Skip new zone creation
            }

            // Get physical zone number from server configuration (exactly like versio)
            int physicalZoneNumber = zone["physicalZoneNumber"] | 1;
            int zoneIndex = physicalZoneNumber - 1;

            // Validate zone index
            if (zoneIndex < 0 || zoneIndex >= MAX_ZONES) {
                MASTER_LOG(LOG_LEVEL_WARN, "Invalid physical zone number %d, skipping", physicalZoneNumber);
                continue;
            }

            // Use the specified physical zone slot
            ZoneSlot* slot = &ZONE_STACK[zoneIndex];

            if (slot->configured && slot->zoneId != zoneId) {
                MASTER_LOG(LOG_LEVEL_WARN, "Physical zone %d already occupied by %s, skipping %s",
                         physicalZoneNumber, slot->zoneId.c_str(), zoneId.c_str());
                continue;
            }

            slot->configured = true;
            slot->zoneId = zoneId;
            slot->physicalZoneNumber = physicalZoneNumber;
            slot->waterPerDay = zone["waterPerDay"];
            slot->irrigationTime = zone["irrigationTime"].as<String>();
            slot->humidityThreshold = zone["humidityThreshold"];

            MASTER_LOG(LOG_LEVEL_INFO, "Zone %d (Physical #%d): %s", slot->id, physicalZoneNumber, zoneId.c_str());
            MASTER_LOG(LOG_LEVEL_INFO, "Water: %dml/day, Time: %s, Threshold: %d%%",
                     slot->waterPerDay, slot->irrigationTime.c_str(), slot->humidityThreshold);

            // Assign sensors to this zone (exactly like versio)
            JsonArray sensors = zone["sensors"];

            // Calculate baseOffset by counting sensors from all previously configured zones
            int baseOffset = 0;
            int currentSlotIndex = slot - ZONE_STACK;

            for (int z = 0; z < currentSlotIndex; z++) {
                if (ZONE_STACK[z].configured) {
                    for (int s = 0; s < MAX_SENSORS; s++) {
                        if (SENSOR_STACK[s].assigned && SENSOR_STACK[s].zoneId == ZONE_STACK[z].zoneId) {
                            baseOffset++;
                        }
                    }
                }
            }

            for (int k = 0; k < sensors.size(); k++) {
                JsonObject sensor = sensors[k];
                String configSensorId = sensor["sensorId"].as<String>();

                MASTER_LOG(LOG_LEVEL_DEBUG, "Assigning sensor: %s", configSensorId.c_str());

                // Find an available sensor slot and assign it with the real ID
                for (int s = 0; s < MAX_SENSORS; s++) {
                    if (!SENSOR_STACK[s].assigned) {
                        SENSOR_STACK[s].assigned = true;
                        SENSOR_STACK[s].id = configSensorId;
                        SENSOR_STACK[s].zoneId = zoneId;
                        MASTER_LOG(LOG_LEVEL_DEBUG, "Sensor %s assigned to slot %d for zone %s",
                                 configSensorId.c_str(), s, zoneId.c_str());
                        break;
                    }
                }
            }

            assignedZones[assignedZoneCount++] = zoneId;

            // Build sensor list string for logging with sensor data if available
            String sensorList = "";
            String sensorDataList = "";
            int sensorCount = 0;

            // Utiliser directement slaveSensorData (déjà parsé)
            bool hasSensorData = (slaveSensorDataLastUpdate > 0 &&
                                 (millis() - slaveSensorDataLastUpdate) < 10000); // Données récentes (< 10s)
            DynamicJsonDocument& sensorDoc = slaveSensorData; // Référence directe

            for (int s = 0; s < MAX_SENSORS; s++) {
                if (SENSOR_STACK[s].assigned && SENSOR_STACK[s].zoneId == zoneId) {
                    if (sensorCount > 0) {
                        sensorList += ", ";
                        sensorDataList += ", ";
                    }
                    sensorList += SENSOR_STACK[s].id;

                    // Add sensor value if available
                    if (hasSensorData && sensorDoc.containsKey(SENSOR_STACK[s].id)) {
                        float sensorValue = sensorDoc[SENSOR_STACK[s].id];
                        sensorDataList += SENSOR_STACK[s].id;
                        sensorDataList += "=";
                        sensorDataList += String(sensorValue, 1);
                        sensorDataList += "%";
                    } else {
                        sensorDataList += SENSOR_STACK[s].id;
                        sensorDataList += "=N/A";
                    }
                    sensorCount++;
                }
            }

            if (hasSensorData) {
                MASTER_LOG(LOG_LEVEL_INFO, "Zone %d: %s configured with %d sensors: [%s] | Values: [%s]",
                         slot->id, zoneId.c_str(), sensors.size(), sensorList.c_str(), sensorDataList.c_str());
            } else {
                MASTER_LOG(LOG_LEVEL_INFO, "Zone %d: %s configured with %d sensors: [%s]",
                         slot->id, zoneId.c_str(), sensors.size(), sensorList.c_str());
            }
        }

        // Update device assignment status (exactly like versio)
        deviceAssigned = (assignedZoneCount > 0);
    }
}

// Handle zone deletion (exactly like versio)
static void handleZoneDeletion(String zoneId) {
    MASTER_LOG(LOG_LEVEL_INFO, "Processing zone deletion: %s", zoneId.c_str());

    // Stop any active irrigation immediately if it's for this zone (exactly like versio)
    if (isIrrigating && activeIrrigationTimer > 0 && activeZoneId == zoneId) {
        MASTER_LOG(LOG_LEVEL_WARN, "Stopping active irrigation for zone deletion");
        activeIrrigationTimer = 0;
        isIrrigating = false;
        activeZoneId = "";
        // Send stop command to ESP32_com via WebSocket
        if (webSocket) {
            String stopCmd = "{\"action\":\"stop_irrigation\",\"zoneId\":\"" + zoneId + "\"}";
            webSocket->broadcastTXT(stopCmd);
        }
    }

    // Find the zone slot to remove (exactly like versio)
    ZoneSlot* zoneSlot = nullptr;
    for (int i = 0; i < MAX_ZONES; i++) {
        if (ZONE_STACK[i].configured && ZONE_STACK[i].zoneId == zoneId) {
            zoneSlot = &ZONE_STACK[i];
            MASTER_LOG(LOG_LEVEL_DEBUG, "Found zone %s in slot %d", zoneId.c_str(), zoneSlot->id);
            break;
        }
    }

    if (zoneSlot) {
        // Free sensors assigned to this zone (exactly like versio)
        for (int i = 0; i < MAX_SENSORS; i++) {
            if (SENSOR_STACK[i].assigned && SENSOR_STACK[i].zoneId == zoneId) {
                SENSOR_STACK[i].assigned = false;
                SENSOR_STACK[i].zoneId = "";
                MASTER_LOG(LOG_LEVEL_DEBUG, "Freed sensor %s", SENSOR_STACK[i].id.c_str());
            }
        }

        // Clear the zone slot (exactly like versio)
        zoneSlot->configured = false;
        zoneSlot->zoneId = "";
        zoneSlot->waterPerDay = 0;
        zoneSlot->irrigationTime = "";
        zoneSlot->humidityThreshold = 0;
        MASTER_LOG(LOG_LEVEL_INFO, "Zone %s removed from device", zoneId.c_str());

        // Remove from assignedZones array (exactly like versio)
        for (int i = 0; i < assignedZoneCount; i++) {
            if (assignedZones[i] == zoneId) {
                for (int j = i; j < assignedZoneCount - 1; j++) {
                    assignedZones[j] = assignedZones[j + 1];
                }
                assignedZoneCount--;
                MASTER_LOG(LOG_LEVEL_DEBUG, "Zone %s removed from assigned zones list", zoneId.c_str());
                break;
            }
        }

        // Update device assignment status (exactly like versio)
        if (assignedZoneCount == 0) {
            deviceAssigned = false;
            MASTER_LOG(LOG_LEVEL_INFO, "Device no longer assigned to any zones");
        }
    } else {
        MASTER_LOG(LOG_LEVEL_WARN, "Zone %s not found on this device", zoneId.c_str());
    }
}

// Send sensor data to server (exactly like versio)
static void sendSensorData() {
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    HTTPClient http;
    http.setTimeout(30000);
    http.begin(String(serverURL) + "/api/devices/sensor-data");
    http.addHeader("Content-Type", "application/json");

    // Create sensor data payload exactly like versio
    // Buffer optimisé pour sendSensorData
    // Structure : type + deviceId + timestamp + globalData (5 champs) + zonesData
    // Pour 4 zones avec 3 capteurs chacune : ~1.5 KB max
    // On alloue 3 KB pour marge de sécurité
    DynamicJsonDocument doc(3072);

    // Check if document is valid
    if (doc.capacity() == 0) {
        MASTER_LOG(LOG_LEVEL_ERROR, "Failed to create JSON document - insufficient memory!");
        return;
    }

    doc["type"] = "data";
    doc["deviceId"] = deviceId;
    doc["timestamp"] = getTimestamp();

    // Verify basic fields are set
    if (!doc.containsKey("type") || !doc.containsKey("deviceId")) {
        MASTER_LOG(LOG_LEVEL_ERROR, "Failed to set basic JSON fields!");
        return;
    }

    // Global environmental data (exactly like versio)
    JsonObject globalData = doc.createNestedObject("globalData");
    if (!globalData.isNull()) {
        if (bme_ready || bme_simulated) {
            globalData["temperature"] = bme_data.temperature_c;
            globalData["humidity"] = bme_data.humidity_pct;
            globalData["pressure"] = bme_data.pressure_hpa;
        } else {
            globalData["temperature"] = globalTemperature;
            globalData["humidity"] = globalHumidity;
            globalData["pressure"] = globalPressure;
        }
        globalData["batteryLevel"] = 85.0 + random(-10, 16);
        globalData["signalStrength"] = WiFi.RSSI();
    } else {
        MASTER_LOG(LOG_LEVEL_ERROR, "Failed to create globalData object!");
        return;
    }

    // Zones array with sensor data (exactly like versio)
    JsonArray zonesArray = doc.createNestedArray("zonesData");
    if (zonesArray.isNull()) {
        MASTER_LOG(LOG_LEVEL_ERROR, "Failed to create zonesData array!");
        return;
    }

    MASTER_LOG(LOG_LEVEL_DEBUG, "Sending sensor data: %d zones", assignedZoneCount);

    // Only send zone data if zones are actually configured
    if (assignedZoneCount > 0) {
        // Utiliser directement slaveSensorData (déjà parsé)
        bool hasRecentData = (slaveSensorDataLastUpdate > 0 &&
                             (millis() - slaveSensorDataLastUpdate) < 10000); // Données récentes (< 10s)
        DynamicJsonDocument& sensorDoc = slaveSensorData; // Référence directe

        if (!hasRecentData) {
            MASTER_LOG(LOG_LEVEL_WARN, "No recent sensor data from ESP32_sensor (last update: %lu ms ago)",
                      slaveSensorDataLastUpdate > 0 ? (millis() - slaveSensorDataLastUpdate) : 0);
        } else {
            MASTER_LOG(LOG_LEVEL_DEBUG, "Using sensor data (last update: %lu ms ago)",
                      millis() - slaveSensorDataLastUpdate);
        }

        // Send data for each configured zone (exactly like versio)
        for (int i = 0; i < MAX_ZONES; i++) {
            if (ZONE_STACK[i].configured) {
                JsonObject zoneObj = zonesArray.createNestedObject();
                zoneObj["zoneId"] = ZONE_STACK[i].zoneId;

                JsonArray moistureArray = zoneObj.createNestedArray("soilMoisture");

                // Add sensor data for this zone from ESP32_sensor
                for (int s = 0; s < MAX_SENSORS; s++) {
                    if (SENSOR_STACK[s].assigned && SENSOR_STACK[s].zoneId == ZONE_STACK[i].zoneId) {
                        JsonObject sensorObj = moistureArray.createNestedObject();
                        sensorObj["sensorId"] = SENSOR_STACK[s].id;

                        // Get value from ESP32_sensor data if available
                        if (hasRecentData && sensorDoc.containsKey(SENSOR_STACK[s].id)) {
                            float sensorValue = sensorDoc[SENSOR_STACK[s].id];
                            sensorObj["value"] = sensorValue;
                            MASTER_LOG(LOG_LEVEL_DEBUG, "Zone %s sensor %s: %.1f%%",
                                     ZONE_STACK[i].zoneId.c_str(), SENSOR_STACK[s].id.c_str(), sensorValue);
                        } else {
                            sensorObj["value"] = 50.0; // Default fallback
                            MASTER_LOG(LOG_LEVEL_WARN, "Zone %s sensor %s: using default 50.0%% (data not available)",
                                     ZONE_STACK[i].zoneId.c_str(), SENSOR_STACK[s].id.c_str());
                        }
                    }
                }
            }
        }
    } else {
        MASTER_LOG(LOG_LEVEL_DEBUG, "No zones configured - sending only global environmental data");
    }

    // Verify that "type" field is present BEFORE serialization
    if (!doc.containsKey("type")) {
        MASTER_LOG(LOG_LEVEL_ERROR, "CRITICAL: 'type' field missing in JSON document BEFORE serialization!");
        return;
    }

    // Check document memory usage
    size_t memoryUsed = doc.memoryUsage();
    size_t capacity = doc.capacity();
    MASTER_LOG(LOG_LEVEL_INFO, "JSON document: %u/%u bytes used, zones: %d", memoryUsed, capacity, assignedZoneCount);

    // Verify key fields are present
    if (!doc.containsKey("type") || !doc.containsKey("deviceId") || !doc.containsKey("globalData")) {
        MASTER_LOG(LOG_LEVEL_ERROR, "CRITICAL: Required fields missing! type=%d, deviceId=%d, globalData=%d",
                   doc.containsKey("type"), doc.containsKey("deviceId"), doc.containsKey("globalData"));
        return;
    }

    if (memoryUsed >= capacity * 0.95) {
        MASTER_LOG(LOG_LEVEL_WARN, "JSON document nearly full (%u/%u bytes)", memoryUsed, capacity);
    }

    String payload;
    payload.reserve(1024); // Pre-allocate to avoid fragmentation
    size_t serializedSize = serializeJson(doc, payload);

    if (serializedSize == 0) {
        MASTER_LOG(LOG_LEVEL_ERROR, "CRITICAL: serializeJson returned 0 - serialization failed!");
        return;
    }

    if (payload.length() < 10) {
        MASTER_LOG(LOG_LEVEL_ERROR, "CRITICAL: Payload too small (%u bytes) - JSON is empty or corrupted!", payload.length());
        MASTER_LOG(LOG_LEVEL_ERROR, "Payload content: '%s'", payload.c_str());
        return;
    }

    MASTER_LOG(LOG_LEVEL_INFO, "Sending sensor data to server (payload size: %u bytes, serialized: %u)", payload.length(), serializedSize);
    MASTER_LOG(LOG_LEVEL_DEBUG, "Full payload: %s", payload.c_str());

    // Add HMAC signature (exactly like versio)
    String timestamp = String(millis());
    String signature = generateHMAC(payload + timestamp);
    http.addHeader("X-Signature", signature);
    http.addHeader("X-Timestamp", timestamp);

    int httpResponseCode = http.POST(payload);

    if (httpResponseCode == 200) {
        MASTER_LOG(LOG_LEVEL_INFO, "Sensor data sent successfully");
    } else {
        String response = http.getString();
        MASTER_LOG(LOG_LEVEL_ERROR, "Sensor data send failed: %d - %s", httpResponseCode, response.c_str());
    }

    http.end();
}

// Check irrigation schedule (exactly like versio)
static void checkIrrigationSchedule() {
    if (!deviceRegistered || assignedZoneCount == 0) return;

    // Get current time
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return;
    }

    char currentTime[6];
    strftime(currentTime, sizeof(currentTime), "%H:%M", &timeinfo);

    for (int i = 0; i < MAX_ZONES; i++) {
        if (ZONE_STACK[i].configured && ZONE_STACK[i].irrigationTime.equals(String(currentTime))) {
            MASTER_LOG(LOG_LEVEL_INFO, "Scheduled irrigation for zone %d", ZONE_STACK[i].id);
            executeIrrigation(ZONE_STACK[i].zoneId, ZONE_STACK[i].waterPerDay / 10); // Convert ml to seconds
        }
    }
}

// Check moisture thresholds (exactly like versio)
static void checkMoistureThresholds() {
    for (int i = 0; i < MAX_ZONES; i++) {
        if (!ZONE_STACK[i].configured) continue;

        // Utiliser directement slaveSensorData (déjà parsé)
        DynamicJsonDocument& sensorDoc = slaveSensorData; // Référence directe

        // Check average moisture for this zone (exactly like versio)
        float totalMoisture = 0;
        int sensorCount = 0;
        bool hasRecentData = (slaveSensorDataLastUpdate > 0 &&
                             (millis() - slaveSensorDataLastUpdate) < 10000); // Données récentes (< 10s)

        for (int s = 0; s < MAX_SENSORS; s++) {
            if (SENSOR_STACK[s].assigned && SENSOR_STACK[s].zoneId == ZONE_STACK[i].zoneId) {
                float moisture = 50.0; // Default
                if (hasRecentData && sensorDoc.containsKey(SENSOR_STACK[s].id)) {
                    moisture = sensorDoc[SENSOR_STACK[s].id];
                } else {
                    MASTER_LOG(LOG_LEVEL_DEBUG, "Zone %s sensor %s: using default 50.0%% (hasRecentData=%d, containsKey=%d)",
                             ZONE_STACK[i].zoneId.c_str(), SENSOR_STACK[s].id.c_str(),
                             hasRecentData, sensorDoc.containsKey(SENSOR_STACK[s].id));
                }
                totalMoisture += moisture;
                sensorCount++;
            }
        }

        if (sensorCount > 0) {
            float avgMoisture = totalMoisture / sensorCount;

            if (avgMoisture < ZONE_STACK[i].humidityThreshold) {
                MASTER_LOG(LOG_LEVEL_WARN, "Zone %d moisture critical: %.1f%% < %d%%",
                          ZONE_STACK[i].id, avgMoisture, ZONE_STACK[i].humidityThreshold);

                // Emergency irrigation - 10 seconds
                executeIrrigation(ZONE_STACK[i].zoneId, 10); // 10 seconds emergency irrigation
            }
        }
    }
}

// Check irrigation timer (exactly like versio)
static void checkIrrigationTimer() {
    if (isIrrigating && activeIrrigationTimer > 0 && millis() >= activeIrrigationTimer) {
        // Stop irrigation
        isIrrigating = false;
        activeIrrigationTimer = 0;
        String zoneId = activeZoneId;
        activeZoneId = "";

        MASTER_LOG(LOG_LEVEL_INFO, "Irrigation completed for zone %s", zoneId.c_str());

        // Send stop command to ESP32_com
        if (webSocket) {
            String stopCmd = "{\"action\":\"stop_irrigation\",\"zoneId\":\"" + zoneId + "\"}";
            webSocket->broadcastTXT(stopCmd);
        }
    }
}

// Execute irrigation (exactly like versio logic)
static void executeIrrigation(String zoneId, int durationSeconds) {
    // Find the zone slot
    ZoneSlot* zoneSlot = nullptr;
    for (int i = 0; i < MAX_ZONES; i++) {
        if (ZONE_STACK[i].configured && ZONE_STACK[i].zoneId == zoneId) {
            zoneSlot = &ZONE_STACK[i];
            break;
        }
    }

    if (!zoneSlot) {
        MASTER_LOG(LOG_LEVEL_WARN, "Zone %s not found or not configured", zoneId.c_str());
        return;
    }

    if (isIrrigating) {
        MASTER_LOG(LOG_LEVEL_WARN, "Irrigation already in progress, queuing command");
        return;
    }

    MASTER_LOG(LOG_LEVEL_INFO, "Starting irrigation for zone %s (Physical #%d)", zoneId.c_str(), zoneSlot->physicalZoneNumber);
    MASTER_LOG(LOG_LEVEL_INFO, "Duration: %ds", durationSeconds);

    isIrrigating = true;
    activeZoneId = zoneId;
    activeIrrigationTimer = millis() + (durationSeconds * 1000);

    // Send irrigation command to ESP32_com via WebSocket
    if (webSocket) {
        DynamicJsonDocument cmd(256);
        cmd["action"] = "start_irrigation";
        cmd["zoneId"] = zoneId;
        cmd["physicalZoneNumber"] = zoneSlot->physicalZoneNumber;
        cmd["durationSeconds"] = durationSeconds;

        String cmdStr;
        serializeJson(cmd, cmdStr);
        webSocket->broadcastTXT(cmdStr);

        MASTER_LOG(LOG_LEVEL_INFO, "Irrigation command sent to ESP32_com: %s", cmdStr.c_str());
    }
}

// Gestionnaire d'événements WebSocket (serveur pour clients)
void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            MASTER_LOG(LOG_LEVEL_INFO, "WebSocket client #%u disconnected", num);
            break;
        case WStype_CONNECTED: {
            IPAddress ip = webSocket->remoteIP(num);
            MASTER_LOG(LOG_LEVEL_INFO, "WebSocket client #%u connected from %d.%d.%d.%d",
                      num, ip[0], ip[1], ip[2], ip[3]);
            break;
        }
        case WStype_TEXT: {
            MASTER_LOG(LOG_LEVEL_DEBUG, "[Client #%u → Master] %.*s", num, length, payload);
            String clientMessage = String((char*)payload, length);

            // Check if this is sensor data from ESP32_sensor (client 0 typically)
            if (num == 0) {
                updateSlaveSensorData(num, clientMessage);
            }
            break;
        }
        case WStype_BIN:
            MASTER_LOG(LOG_LEVEL_DEBUG, "WebSocket binary message received");
            break;
        case WStype_ERROR:
            MASTER_LOG(LOG_LEVEL_ERROR, "WebSocket error");
            break;
        default:
            break;
    }
}

// Démarrage de l'application ESP32_master
static void ESP32_master_app_start(void) {
    if (app_running) {
        return;
    }

    MASTER_LOG(LOG_LEVEL_INFO, "Starting ESP32 Master (Irrigation System)...");
    MASTER_LOG(LOG_LEVEL_DEBUG, "Server URL: %s", serverURL);
    MASTER_LOG(LOG_LEVEL_DEBUG, "Device ID: %s", deviceId);

    // Initialize zone stack (exactly like versio)
    for (int i = 0; i < MAX_ZONES; i++) {
        ZONE_STACK[i].id = i + 1;
        ZONE_STACK[i].configured = false;
        ZONE_STACK[i].zoneId = "";
        ZONE_STACK[i].waterPerDay = 0;
        ZONE_STACK[i].irrigationTime = "";
        ZONE_STACK[i].humidityThreshold = 0;
        ZONE_STACK[i].physicalZoneNumber = i + 1;
    }

    // Initialize sensor stack (exactly like versio)
    for (int i = 0; i < MAX_SENSORS; i++) {
        if (i < 9) {
            SENSOR_STACK[i].id = "s0" + String(i + 1);
        } else {
            SENSOR_STACK[i].id = "s" + String(i + 1);
        }
        SENSOR_STACK[i].assigned = false;
        SENSOR_STACK[i].zoneId = "";
    }

    // === AP Mode pour les clients (ESP32_sensor et ESP32_com) ===
    if (WiFi.softAP(ap_ssid, ap_pass)) {
        MASTER_LOG(LOG_LEVEL_INFO, "AP started: %s", ap_ssid);
        MASTER_LOG(LOG_LEVEL_INFO, "AP IP: %s", WiFi.softAPIP().toString().c_str());
    } else {
        MASTER_LOG(LOG_LEVEL_ERROR, "Failed to start AP");
        return;
    }

    // Initialize BME280
    bme_ready = init_bme280();
    if (!bme_ready) {
        // Initialize simulated values
        bme_data.temperature_c = globalTemperature;
        bme_data.humidity_pct = globalHumidity;
        bme_data.pressure_hpa = globalPressure;
        bme_data.last_sample_ms = millis();
    }

    // === WebSocket Server ===
    webSocket = new WebSocketsServer(81);
    if (!webSocket) {
        MASTER_LOG(LOG_LEVEL_ERROR, "Failed to create WebSocket server");
        return;
    }

    webSocket->begin();
    webSocket->onEvent(onWebSocketEvent);

    MASTER_LOG(LOG_LEVEL_INFO, "WebSocket server started on port 81");
    MASTER_LOG(LOG_LEVEL_DEBUG, "Ready to accept ESP32_sensor & ESP32_com clients");

    // Register device with server if WiFi is connected
    if (WiFi.status() == WL_CONNECTED) {
        registerDevice();
    }

    app_running = true;
}

// Arrêt de l'application ESP32_master
static void ESP32_master_app_stop(void) {
    if (!app_running) {
        return;
    }

    MASTER_LOG(LOG_LEVEL_INFO, "Stopping ESP32 Master...");

    // Stop any active irrigation
    if (isIrrigating) {
        if (webSocket) {
            String stopCmd = "{\"action\":\"stop_irrigation\",\"zoneId\":\"" + activeZoneId + "\"}";
            webSocket->broadcastTXT(stopCmd);
        }
        isIrrigating = false;
        activeIrrigationTimer = 0;
        activeZoneId = "";
    }

    if (webSocket) {
        webSocket->close();
        delete webSocket;
        webSocket = nullptr;
    }

    WiFi.softAPdisconnect(true);

    app_running = false;
}

// Boucle principale de l'application
static void ESP32_master_app_loop(void) {
    if (!app_running || !webSocket) {
        return;
    }

    webSocket->loop();

    unsigned long currentTime = millis();

    // Refresh BME280 data
    refresh_bme280();

    // Check irrigation timer
    checkIrrigationTimer();

    // Poll configuration every 10 seconds (exactly like versio)
    if (currentTime - lastConfigPoll >= 10000) {
        pollConfiguration();
        lastConfigPoll = currentTime;
    }

    // Send sensor data every 15 seconds (exactly like versio)
    if (currentTime - lastDataSend >= 15000) {
        sendSensorData();
        lastDataSend = currentTime;
    }

    // Check irrigation schedule every minute (exactly like versio)
    if (currentTime % 60000 < 1000) {
        checkIrrigationSchedule();
    }

    // Check moisture thresholds every 30 seconds (exactly like versio)
    if (currentTime % 30000 < 1000) {
        checkMoistureThresholds();
    }

    // Re-register if WiFi reconnected and not registered
    if (WiFi.status() == WL_CONNECTED && !deviceRegistered) {
        static unsigned long lastRegAttempt = 0;
        if (currentTime - lastRegAttempt > 30000) {
            lastRegAttempt = currentTime;
            registerDevice();
        }
    }
}

// ID réel assigné par le système (séquence, pas forcément 10)
static uint8_t esp32_master_real_app_id = 0;

// Enregistrement de l'application
SysError_t ESP32_master_register_app() {
    AppCallbacks_t callbacks = {
        .start = ESP32_master_app_start,
        .stop = ESP32_master_app_stop,
        .loop = ESP32_master_app_loop
    };

    uint8_t app_id;
    SysError_t result = app_register("ESP32_master", "Irrigation Master Application",
                                    APP_TYPE_USER, &callbacks, &app_id);
    if (result == SYS_OK) {
        esp32_master_real_app_id = app_id;
    }
    return result;
}

// Obtenir l'ID réel assigné par le système
uint8_t ESP32_master_get_app_id(void) {
    return esp32_master_real_app_id;
}
