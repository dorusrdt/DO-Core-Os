#include "ESP32_sensor.h"
#include "MoistureSensor.h"
#include "../../kernel/app/app_manager.h"
#include "../../kernel/core/log_system_optimized.h"
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <NewPing.h>
#include <cstring>

#define SENSOR_LOG(level, fmt, ...) \
    kernel_log(level, "[ESP32_sensor] " fmt, ##__VA_ARGS__)

// Configuration AP du Master
static const char* master_ap_ssid = "ESP32_MASTER";
static const char* master_ap_pass = "12345678";
static const char* master_ip = "192.168.4.1";
static const uint16_t master_port = 81;

// Hardware Configuration
#define MAX_SENSORS 12

// Moisture sensor pins (exactly like versio)
#define MOISTURE_PIN_1  32
#define MOISTURE_PIN_2  33
#define MOISTURE_PIN_3  34
#define MOISTURE_PIN_4  35
#define MOISTURE_PIN_5  39
#define MOISTURE_PIN_6  36
#define MOISTURE_PIN_7  25
#define MOISTURE_PIN_8  26
#define MOISTURE_PIN_9  27
#define MOISTURE_PIN_10 14
#define MOISTURE_PIN_11 12
#define MOISTURE_PIN_12 13

// Ultrasonic sensor for tank level (using available pins)
// GPIO 2: available output pin for trigger
// GPIO 34: input-only pin perfect for echo
#define ULTRASONIC_TRIGGER_PIN  2
#define ULTRASONIC_ECHO_PIN     15
#define MAX_DISTANCE           200

static const int moisturePins[MAX_SENSORS] = {
    MOISTURE_PIN_1, MOISTURE_PIN_2, MOISTURE_PIN_3, MOISTURE_PIN_4,
    MOISTURE_PIN_5, MOISTURE_PIN_6, MOISTURE_PIN_7, MOISTURE_PIN_8,
    MOISTURE_PIN_9, MOISTURE_PIN_10, MOISTURE_PIN_11, MOISTURE_PIN_12
};

static WebSocketsClient* webSocket = nullptr;
static bool app_running = false;

// Ultrasonic sensor (global instance)
static NewPing* sonar = nullptr;

// Sensor readings storage
static float sensorReadings[MAX_SENSORS];
static unsigned long lastSensorRead = 0;

// MoistureSensor instances for each sensor
// Calibration: V_MIN = 1.50V (humide), V_MAX = 3.15V (sec)
static MoistureSensor* moistureSensors[MAX_SENSORS] = {nullptr};

// Measure tank level using ultrasonic sensor with simulation fallback
static float measureTankLevel() {
    static bool ultrasonicReady = (sonar != nullptr);
    static bool loggedStatus = false;

    // Log sensor status once at startup
    if (!loggedStatus) {
        if (ultrasonicReady) {
            SENSOR_LOG(LOG_LEVEL_INFO, "Ultrasonic sensor initialized - GPIO %d (trigger), %d (echo)",
                      ULTRASONIC_TRIGGER_PIN, ULTRASONIC_ECHO_PIN);
        } else {
            SENSOR_LOG(LOG_LEVEL_WARN, "Ultrasonic sensor not available - using simulation mode");
        }
        loggedStatus = true;
    }

    // Check if ultrasonic sensor is available
    if (ultrasonicReady) {
        // Try ultrasonic sensor first
        unsigned int uS = sonar->ping_median(5); // Median of 5 readings for stability
        if (uS > 0) {
            float distance = uS / US_ROUNDTRIP_CM; // Convert to cm
            // Log water level after each acquisition
            SENSOR_LOG(LOG_LEVEL_INFO, "Water level: %.1f cm (real ultrasonic sensor)", distance);
            return distance;
        } else {
            // Sensor failed this reading - fallback to simulation
            static int failureCount = 0;
            failureCount++;
            if (failureCount % 50 == 0) { // Log every 50 failures to avoid spam
                SENSOR_LOG(LOG_LEVEL_WARN, "Ultrasonic sensor reading failed - using simulation");
            }
        }
    }

    // Sensor not available or failed - use simulation
    static float simulatedLevel = 50.0;
    static bool decreasing = true;

    // Simulate gradual tank level changes
    if (decreasing) {
        simulatedLevel -= 0.5;
        if (simulatedLevel <= 20.0) decreasing = false;
    } else {
        simulatedLevel += 0.5;
        if (simulatedLevel >= 90.0) decreasing = true;
    }

    // Log simulated water level after each acquisition
    SENSOR_LOG(LOG_LEVEL_INFO, "Water level: %.1f cm (simulation mode)", simulatedLevel);

    return simulatedLevel;
}

// Read all moisture sensors using MoistureSensor class
static void readAllSensors() {
    for (int i = 0; i < MAX_SENSORS; i++) {
        if (moistureSensors[i] == nullptr) {
            SENSOR_LOG(LOG_LEVEL_ERROR, "Sensor %d not initialized!", i + 1);
            continue;
        }

        // Read filtered humidity value (met à jour le cache automatiquement)
        double humidity = moistureSensors[i]->readHumidity();
        sensorReadings[i] = (float)humidity;

        // Log avec détails pour debug (utilise les valeurs en cache)
        double voltage = moistureSensors[i]->getLastVoltage();
        int raw = moistureSensors[i]->getLastRaw();
        SENSOR_LOG(LOG_LEVEL_DEBUG, "Sensor %d (pin %d): raw=%d, voltage=%.3fV, humidity=%.1f%%",
                  i + 1, moisturePins[i], raw, voltage, sensorReadings[i]);
    }
}

// Send sensor data to master via WebSocket
static void sendSensorData() {
    if (!webSocket || !webSocket->isConnected()) {
        SENSOR_LOG(LOG_LEVEL_WARN, "Cannot send sensor data: WebSocket not connected");
        return;
    }

    // Create JSON payload with sensor readings (optimisé)
    // 12 capteurs (s01-s12) + timestamp + deviceType ≈ 300 bytes max
    // On alloue 512 bytes pour marge de sécurité
    DynamicJsonDocument doc(512);

    // Add sensor readings with IDs s01-s12
    for (int i = 0; i < MAX_SENSORS; i++) {
        String sensorId;
        if (i < 9) {
            sensorId = "s0" + String(i + 1);
        } else {
            sensorId = "s" + String(i + 1);
        }
        doc[sensorId] = sensorReadings[i];
    }

    // NEW: Add tank level measurement
    doc["tankLevel"] = measureTankLevel();

    doc["timestamp"] = millis();
    doc["deviceType"] = "sensor";

    String payload;
    serializeJson(doc, payload);

    webSocket->sendTXT(payload);

    SENSOR_LOG(LOG_LEVEL_INFO, "Sensor data sent to master (len=%u bytes)", payload.length());
    SENSOR_LOG(LOG_LEVEL_DEBUG, "Payload content: %s", payload.c_str());
}

// Gestionnaire d'événements WebSocket
static void onWebSocketEvent_sensor(WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            SENSOR_LOG(LOG_LEVEL_WARN, "WebSocket disconnected from master");
            break;
        case WStype_CONNECTED:
            SENSOR_LOG(LOG_LEVEL_INFO, "WebSocket connected to master");
            break;
        case WStype_TEXT:
            SENSOR_LOG(LOG_LEVEL_DEBUG, "[Master → Sensor] %.*s", length, payload);
            break;
        case WStype_BIN:
            SENSOR_LOG(LOG_LEVEL_DEBUG, "WebSocket binary message received");
            break;
        case WStype_ERROR:
            SENSOR_LOG(LOG_LEVEL_ERROR, "WebSocket error");
            break;
        default:
            break;
    }
}

// Démarrage de l'application ESP32_sensor
static void ESP32_sensor_app_start(void) {
    if (app_running) {
        return;
    }

    SENSOR_LOG(LOG_LEVEL_INFO, "Starting ESP32 Sensor (12 moisture sensors)...");

    // Initialize sensor pins (analog inputs don't need pinMode, but we can set resolution)
    // ESP32 ADC resolution: 12-bit (0-4095)
    analogSetWidth(12);
    analogSetAttenuation(ADC_11db); // 0-3.3V range

    SENSOR_LOG(LOG_LEVEL_INFO, "Initialized %d moisture sensor pins", MAX_SENSORS);

    // Create MoistureSensor instances with calibration
    // V_MIN = 1.50V (sol très humide), V_MAX = 3.15V (sol très sec)
    for (int i = 0; i < MAX_SENSORS; i++) {
        moistureSensors[i] = new MoistureSensor(moisturePins[i], 1.50, 3.15);
        if (moistureSensors[i]) {
            moistureSensors[i]->begin();
            SENSOR_LOG(LOG_LEVEL_DEBUG, "  Sensor %d: GPIO %d (calibrated: 1.50V-3.15V)",
                      i + 1, moisturePins[i]);
        } else {
            SENSOR_LOG(LOG_LEVEL_ERROR, "Failed to create MoistureSensor %d", i + 1);
        }
    }

    // NEW: Initialize ultrasonic sensor
    sonar = new NewPing(ULTRASONIC_TRIGGER_PIN, ULTRASONIC_ECHO_PIN, MAX_DISTANCE);
    if (sonar == nullptr) {
        SENSOR_LOG(LOG_LEVEL_ERROR, "Failed to create ultrasonic sensor object");
    } else {
        // Test ultrasonic sensor at boot
        unsigned int testPing = sonar->ping();
        if (testPing == 0) {
            SENSOR_LOG(LOG_LEVEL_WARN, "Ultrasonic sensor not detected - will use simulation");
        } else {
            SENSOR_LOG(LOG_LEVEL_INFO, "Ultrasonic sensor ready for tank level measurement");
        }
    }

    // Connexion au réseau AP du Master
    SENSOR_LOG(LOG_LEVEL_INFO, "Connecting to master AP: %s", master_ap_ssid);
    WiFi.begin(master_ap_ssid, master_ap_pass);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
        delay(500);
        SENSOR_LOG(LOG_LEVEL_DEBUG, "Waiting for AP connection... status=%d", WiFi.status());
    }

    if (WiFi.status() != WL_CONNECTED) {
        SENSOR_LOG(LOG_LEVEL_ERROR, "Failed to connect to master AP");
        return;
    }

    SENSOR_LOG(LOG_LEVEL_INFO, "Connected to master AP!");
    SENSOR_LOG(LOG_LEVEL_INFO, "Sensor device IP: %s", WiFi.localIP().toString().c_str());

    // WebSocket client vers le Master
    webSocket = new WebSocketsClient();
    if (!webSocket) {
        SENSOR_LOG(LOG_LEVEL_ERROR, "Failed to create WebSocket client");
        return;
    }

    webSocket->begin(master_ip, master_port, "/");
    webSocket->onEvent(onWebSocketEvent_sensor);
    webSocket->setReconnectInterval(5000); // Reconnexion automatique
    webSocket->enableHeartbeat(30000, 10000, 3); // Ping every 30s, timeout 10s, 3 retries

    SENSOR_LOG(LOG_LEVEL_INFO, "WebSocket client started, connecting to %s:%d", master_ip, master_port);

    // Initialize sensor readings (will be updated on first read)
    for (int i = 0; i < MAX_SENSORS; i++) {
        sensorReadings[i] = 50.0; // Default value
    }

    // Perform initial sensor read to populate cache
    readAllSensors();

    app_running = true;
}

// Arrêt de l'application ESP32_sensor
static void ESP32_sensor_app_stop(void) {
    if (!app_running) {
        return;
    }

    SENSOR_LOG(LOG_LEVEL_INFO, "Stopping ESP32 Sensor...");

    // Cleanup MoistureSensor instances
    for (int i = 0; i < MAX_SENSORS; i++) {
        if (moistureSensors[i]) {
            delete moistureSensors[i];
            moistureSensors[i] = nullptr;
        }
    }

    if (webSocket) {
        webSocket->disconnect();
        delete webSocket;
        webSocket = nullptr;
    }

    WiFi.disconnect(true);

    app_running = false;
}

// Boucle principale de l'application
static void ESP32_sensor_app_loop(void) {
    if (!app_running || !webSocket) {
        return;
    }

    webSocket->loop();

    unsigned long currentTime = millis();

    // Read sensors every 2 seconds
    if (currentTime - lastSensorRead >= 2000) {
        readAllSensors();
        lastSensorRead = currentTime;
    }

    // Send sensor data to master every 2 seconds
    static unsigned long lastSend = 0;
    if (currentTime - lastSend >= 2000) {
        lastSend = currentTime;

        if (webSocket->isConnected()) {
            sendSensorData();
        } else {
            SENSOR_LOG(LOG_LEVEL_WARN, "Cannot send sensor data: WebSocket disconnected");
        }
    }
}

// ID réel assigné par le système (séquence, pas forcément 11)
static uint8_t esp32_sensor_real_app_id = 0;

// Enregistrement de l'application
SysError_t ESP32_sensor_register_app() {
    AppCallbacks_t callbacks = {
        .start = ESP32_sensor_app_start,
        .stop = ESP32_sensor_app_stop,
        .loop = ESP32_sensor_app_loop
    };

    uint8_t app_id;
    SysError_t result = app_register("ESP32_sensor", "Moisture Sensor Application",
                       APP_TYPE_USER, &callbacks, &app_id);
    if (result == SYS_OK) {
        esp32_sensor_real_app_id = app_id;
    }
    return result;
}

// Obtenir l'ID réel assigné par le système
uint8_t ESP32_sensor_get_app_id(void) {
    return esp32_sensor_real_app_id;
}
