#include "ESP32_com.h"
#include "../../kernel/app/app_manager.h"
#include "../../kernel/core/log_system_optimized.h"
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <cstring>

#define COM_LOG(level, fmt, ...) \
    kernel_log(level, "[ESP32_com] " fmt, ##__VA_ARGS__)

// Configuration connexion au Master
static const char* master_ap_ssid = "ESP32_MASTER";
static const char* master_ap_pass = "12345678";
static const char* master_ip = "192.168.4.1";
static const uint16_t master_port = 81;

// Hardware Configuration
#define MAX_ZONES 4

// Relay control pins (exactly like versio - active LOW logic)
#define ZONE_1_RELAY_PIN 23   // Zone 1 irrigation relay
#define ZONE_2_RELAY_PIN 4    // Zone 2 irrigation relay
#define ZONE_3_RELAY_PIN 18   // Zone 3 irrigation relay
#define ZONE_4_RELAY_PIN 19   // Zone 4 irrigation relay
#define PUMP_RELAY_PIN   5    // Main water pump relay

static const int zoneRelayPins[MAX_ZONES] = {
    ZONE_1_RELAY_PIN, ZONE_2_RELAY_PIN, ZONE_3_RELAY_PIN, ZONE_4_RELAY_PIN
};

static WebSocketsClient* webSocket = nullptr;
static bool app_running = false;

// Irrigation control state
static bool isIrrigating = false;
static int activeZoneNumber = 0; // 1-4, 0 = none
static unsigned long irrigationEndTime = 0;

// Forward declarations
static void startIrrigation(int zoneNumber, int durationSeconds);
static void stopIrrigation();
static void checkIrrigationTimer();

// Gestionnaire d'événements WebSocket
static void onWebSocketEvent_com(WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            COM_LOG(LOG_LEVEL_WARN, "WebSocket disconnected from master");
            break;
        case WStype_CONNECTED:
            COM_LOG(LOG_LEVEL_INFO, "WebSocket connected to master");
            break;
        case WStype_TEXT: {
            COM_LOG(LOG_LEVEL_DEBUG, "[Master → ESP32_com] %.*s", length, payload);

            String message = String((char*)payload, length);

            // Parse JSON command from master
            DynamicJsonDocument doc(512);
            DeserializationError error = deserializeJson(doc, message);

            if (error) {
                COM_LOG(LOG_LEVEL_WARN, "Failed to parse command JSON: %s", error.c_str());
                break;
            }

            String action = doc["action"].as<String>();

            if (action == "start_irrigation") {
                String zoneId = doc["zoneId"].as<String>();
                int physicalZoneNumber = doc["physicalZoneNumber"] | 1;
                int durationSeconds = doc["durationSeconds"] | 60;

                COM_LOG(LOG_LEVEL_INFO, "Received irrigation command: zoneId=%s, zone=%d, duration=%ds",
                       zoneId.c_str(), physicalZoneNumber, durationSeconds);

                startIrrigation(physicalZoneNumber, durationSeconds);

            } else if (action == "stop_irrigation") {
                String zoneId = doc["zoneId"].as<String>();

                COM_LOG(LOG_LEVEL_INFO, "Received stop irrigation command: zoneId=%s", zoneId.c_str());

                stopIrrigation();
            }
            break;
        }
        case WStype_BIN:
            COM_LOG(LOG_LEVEL_DEBUG, "WebSocket binary message received");
            break;
        case WStype_ERROR:
            COM_LOG(LOG_LEVEL_ERROR, "WebSocket error");
            break;
        default:
            break;
    }
}

// Start irrigation (exactly like versio logic - active LOW)
static void startIrrigation(int zoneNumber, int durationSeconds) {
    // Validate zone number
    if (zoneNumber < 1 || zoneNumber > MAX_ZONES) {
        COM_LOG(LOG_LEVEL_ERROR, "Invalid zone number: %d", zoneNumber);
        return;
    }

    // Stop any active irrigation first
    if (isIrrigating) {
        COM_LOG(LOG_LEVEL_WARN, "Stopping previous irrigation before starting new one");
        stopIrrigation();
        delay(500); // Brief delay
    }

    COM_LOG(LOG_LEVEL_INFO, "Starting irrigation: Zone %d, Duration: %ds", zoneNumber, durationSeconds);

    // Turn on pump first (active LOW - set to LOW to activate)
    digitalWrite(PUMP_RELAY_PIN, LOW);
    COM_LOG(LOG_LEVEL_INFO, "Pump: ON (GPIO %d = LOW)", PUMP_RELAY_PIN);

    // Turn on zone relay (active LOW - set to LOW to activate)
    int zoneIndex = zoneNumber - 1;
    digitalWrite(zoneRelayPins[zoneIndex], LOW);
    COM_LOG(LOG_LEVEL_INFO, "Zone %d relay: ON (GPIO %d = LOW)", zoneNumber, zoneRelayPins[zoneIndex]);

    isIrrigating = true;
    activeZoneNumber = zoneNumber;
    irrigationEndTime = millis() + (durationSeconds * 1000);

    COM_LOG(LOG_LEVEL_INFO, "Irrigation started - will stop in %d seconds", durationSeconds);
}

// Stop irrigation (exactly like versio logic - active LOW)
static void stopIrrigation() {
    if (!isIrrigating) {
        COM_LOG(LOG_LEVEL_DEBUG, "No active irrigation to stop");
        return;
    }

    COM_LOG(LOG_LEVEL_INFO, "Stopping irrigation: Zone %d", activeZoneNumber);

    // Turn off zone relay (active LOW - set to HIGH to deactivate)
    if (activeZoneNumber > 0 && activeZoneNumber <= MAX_ZONES) {
        int zoneIndex = activeZoneNumber - 1;
        digitalWrite(zoneRelayPins[zoneIndex], HIGH);
        COM_LOG(LOG_LEVEL_INFO, "Zone %d relay: OFF (GPIO %d = HIGH)", activeZoneNumber, zoneRelayPins[zoneIndex]);
    }

    // Turn off pump (active LOW - set to HIGH to deactivate)
    digitalWrite(PUMP_RELAY_PIN, HIGH);
    COM_LOG(LOG_LEVEL_INFO, "Pump: OFF (GPIO %d = HIGH)", PUMP_RELAY_PIN);

    isIrrigating = false;
    activeZoneNumber = 0;
    irrigationEndTime = 0;

    COM_LOG(LOG_LEVEL_INFO, "Irrigation stopped");
}

// Check irrigation timer (exactly like versio)
static void checkIrrigationTimer() {
    if (isIrrigating && irrigationEndTime > 0 && millis() >= irrigationEndTime) {
        COM_LOG(LOG_LEVEL_INFO, "Irrigation timer expired - stopping");
        stopIrrigation();
    }
}

// Démarrage de l'application ESP32_com
static void ESP32_com_app_start(void) {
    if (app_running) {
        return;
    }

    COM_LOG(LOG_LEVEL_INFO, "Starting ESP32 Communication Client (Relay Control)...");

    // SECURITY: Initialize relay pins as outputs FIRST and set to HIGH (OFF) immediately
    // This ensures relays are disabled at boot for safety (active LOW logic)
    pinMode(ZONE_1_RELAY_PIN, OUTPUT);
    digitalWrite(ZONE_1_RELAY_PIN, HIGH); // OFF (active LOW)

    pinMode(ZONE_2_RELAY_PIN, OUTPUT);
    digitalWrite(ZONE_2_RELAY_PIN, HIGH); // OFF (active LOW)

    pinMode(ZONE_3_RELAY_PIN, OUTPUT);
    digitalWrite(ZONE_3_RELAY_PIN, HIGH); // OFF (active LOW)

    pinMode(ZONE_4_RELAY_PIN, OUTPUT);
    digitalWrite(ZONE_4_RELAY_PIN, HIGH); // OFF (active LOW)

    pinMode(PUMP_RELAY_PIN, OUTPUT);
    digitalWrite(PUMP_RELAY_PIN, HIGH); // OFF (active LOW)

    // Verify all relays are OFF (safety check)
    COM_LOG(LOG_LEVEL_INFO, "✅ Security: All relay pins initialized and set to OFF (HIGH state)");
    COM_LOG(LOG_LEVEL_INFO, "  Zone relays: GPIO %d, %d, %d, %d = HIGH (OFF)",
           ZONE_1_RELAY_PIN, ZONE_2_RELAY_PIN, ZONE_3_RELAY_PIN, ZONE_4_RELAY_PIN);
    COM_LOG(LOG_LEVEL_INFO, "  Pump relay: GPIO %d = HIGH (OFF)", PUMP_RELAY_PIN);
    COM_LOG(LOG_LEVEL_INFO, "  Active LOW logic: LOW = ON, HIGH = OFF");

    // Connexion au réseau AP du Master
    COM_LOG(LOG_LEVEL_INFO, "Connecting to master AP: %s", master_ap_ssid);
    WiFi.begin(master_ap_ssid, master_ap_pass);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
        delay(500);
        COM_LOG(LOG_LEVEL_DEBUG, "Waiting for AP connection... status=%d", WiFi.status());
    }

    if (WiFi.status() != WL_CONNECTED) {
        COM_LOG(LOG_LEVEL_ERROR, "Failed to connect to master AP");
        return;
    }

    COM_LOG(LOG_LEVEL_INFO, "Connected to master AP!");
    COM_LOG(LOG_LEVEL_INFO, "ESP32_com IP: %s", WiFi.localIP().toString().c_str());

    // WebSocket client vers le Master
    webSocket = new WebSocketsClient();
    if (!webSocket) {
        COM_LOG(LOG_LEVEL_ERROR, "Failed to create WebSocket client");
        return;
    }

    webSocket->begin(master_ip, master_port, "/");
    webSocket->onEvent(onWebSocketEvent_com);
    webSocket->setReconnectInterval(5000); // Reconnexion automatique

    COM_LOG(LOG_LEVEL_INFO, "WebSocket client started, connecting to %s:%d", master_ip, master_port);

    app_running = true;
}

// Arrêt de l'application ESP32_com
static void ESP32_com_app_stop(void) {
    if (!app_running) {
        return;
    }

    COM_LOG(LOG_LEVEL_INFO, "Stopping ESP32 Communication Client...");

    // Emergency stop: turn off all relays
    if (isIrrigating) {
        COM_LOG(LOG_LEVEL_WARN, "Emergency stop: turning off all relays");
        stopIrrigation();
    }

    // Ensure all relays are OFF
    digitalWrite(ZONE_1_RELAY_PIN, HIGH);
    digitalWrite(ZONE_2_RELAY_PIN, HIGH);
    digitalWrite(ZONE_3_RELAY_PIN, HIGH);
    digitalWrite(ZONE_4_RELAY_PIN, HIGH);
    digitalWrite(PUMP_RELAY_PIN, HIGH);

    if (webSocket) {
        webSocket->disconnect();
        delete webSocket;
        webSocket = nullptr;
    }

    WiFi.disconnect(true);

    app_running = false;
}

// Boucle principale de l'application
static void ESP32_com_app_loop(void) {
    if (!app_running || !webSocket) {
        return;
    }

    webSocket->loop();

    // Check irrigation timer (exactly like versio)
    checkIrrigationTimer();

    // Send status to master periodically (every 45 seconds like original)
    static unsigned long lastSend = 0;
    unsigned long currentTime = millis();
    if (currentTime - lastSend > 45000) {
        lastSend = currentTime;

        if (webSocket->isConnected()) {
            DynamicJsonDocument status(256);
            status["deviceType"] = "com";
            status["status"] = "ok";
            status["irrigating"] = isIrrigating;
            if (isIrrigating) {
                status["activeZone"] = activeZoneNumber;
                status["remainingSeconds"] = (irrigationEndTime > millis()) ?
                    ((irrigationEndTime - millis()) / 1000) : 0;
            }
            status["timestamp"] = millis();

            String payload;
            serializeJson(status, payload);
            webSocket->sendTXT(payload);

            COM_LOG(LOG_LEVEL_DEBUG, "Status sent to master (len=%u)", payload.length());
        } else {
            COM_LOG(LOG_LEVEL_WARN, "Cannot send status: WebSocket disconnected");
        }
    }
}

// ID réel assigné par le système (séquence, pas forcément 12)
static uint8_t esp32_com_real_app_id = 0;

// Enregistrement de l'application
SysError_t ESP32_com_register_app() {
    AppCallbacks_t callbacks = {
        .start = ESP32_com_app_start,
        .stop = ESP32_com_app_stop,
        .loop = ESP32_com_app_loop
    };

    uint8_t app_id;
    SysError_t result = app_register("ESP32_com", "Irrigation Relay Control Application",
                                    APP_TYPE_USER, &callbacks, &app_id);
    if (result == SYS_OK) {
        esp32_com_real_app_id = app_id;
    }
    return result;
}

// Obtenir l'ID réel assigné par le système
uint8_t ESP32_com_get_app_id(void) {
    return esp32_com_real_app_id;
}
