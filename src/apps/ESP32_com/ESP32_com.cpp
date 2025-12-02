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

// Irrigation control state - NOW SUPPORTS MULTIPLE ZONES SIMULTANEOUSLY
// Structure pour tracker l'irrigation de chaque zone indépendamment
struct ZoneIrrigationState {
    bool isActive;                      // Zone is currently irrigating
    unsigned long endTime;              // When this zone's irrigation ends (ms)
    int durationSeconds;                // Original duration for display
    String zoneId;                      // Zone ID from master
};

static ZoneIrrigationState zoneStates[MAX_ZONES] = {
    {false, 0, 0, ""},
    {false, 0, 0, ""},
    {false, 0, 0, ""},
    {false, 0, 0, ""}
};

// Legacy variable for backward compatibility (can be removed later)
static bool isIrrigating = false;
static int activeZoneNumber = 0; // 1-4, 0 = none (for logging purposes)
static unsigned long irrigationEndTime = 0; // For logging purposes

// Forward declarations (default parameters go HERE)
static void startIrrigation(int zoneNumber, int durationSeconds, const String& zoneId = "");
static void stopIrrigation(int zoneNumber = 0);
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
                COM_LOG(LOG_LEVEL_ERROR, "❌ Failed to parse command JSON: %s | Raw: %.*s",
                       error.c_str(), length, payload);
                break;
            }

            // Ignore messages that are not commands (e.g., acknowledgments)
            if (!doc.containsKey("action")) {
                COM_LOG(LOG_LEVEL_DEBUG, "Ignoring non-command message from master");
                break;
            }

            String action = doc["action"].as<String>();

            if (action == "start_irrigation") {
                String zoneId = doc["zoneId"].as<String>();
                int physicalZoneNumber = doc["physicalZoneNumber"] | 1;
                int durationSeconds = doc["durationSeconds"] | 60;

                // Calculate time breakdown for better logging
                unsigned long minutes = durationSeconds / 60;
                unsigned long seconds = durationSeconds % 60;

                COM_LOG(LOG_LEVEL_INFO, "📥 Received START irrigation command:");
                COM_LOG(LOG_LEVEL_INFO, "   Zone ID: %s", zoneId.c_str());
                COM_LOG(LOG_LEVEL_INFO, "   Physical Zone: %d", physicalZoneNumber);
                if (minutes > 0) {
                    COM_LOG(LOG_LEVEL_INFO, "   Duration: %lu min %lu sec (%d total seconds)",
                           minutes, seconds, durationSeconds);
                } else {
                    COM_LOG(LOG_LEVEL_INFO, "   Duration: %lu sec", seconds);
                }

                // NOW: Pass zoneId to startIrrigation for tracking
                startIrrigation(physicalZoneNumber, durationSeconds, zoneId);

            } else if (action == "stop_irrigation") {
                String zoneId = doc["zoneId"].as<String>();
                int physicalZoneNumber = doc["physicalZoneNumber"] | 0;  // 0 means stop all

                COM_LOG(LOG_LEVEL_INFO, "📥 Received STOP irrigation command:");
                COM_LOG(LOG_LEVEL_INFO, "   Zone ID: %s", zoneId.c_str());

                // Log current irrigation status
                if (physicalZoneNumber > 0) {
                    if (physicalZoneNumber >= 1 && physicalZoneNumber <= MAX_ZONES) {
                        int zoneIdx = physicalZoneNumber - 1;
                        if (zoneStates[zoneIdx].isActive) {
                            COM_LOG(LOG_LEVEL_INFO, "   Zone %d is active - stopping now", physicalZoneNumber);
                        } else {
                            COM_LOG(LOG_LEVEL_INFO, "   Zone %d is not active", physicalZoneNumber);
                        }
                    }
                } else {
                    // Count active zones
                    int activeCount = 0;
                    for (int i = 0; i < MAX_ZONES; i++) {
                        if (zoneStates[i].isActive) activeCount++;
                    }
                    COM_LOG(LOG_LEVEL_INFO, "   Stopping all zones (%d currently active)", activeCount);
                }

                // NOW: Support stopping specific zone or all zones
                stopIrrigation(physicalZoneNumber);
            } else if (action == "pump_control") {
                // NEW: Handle pump control commands
                String pumpState = doc["pumpState"].as<String>();

                COM_LOG(LOG_LEVEL_INFO, "📥 Received PUMP CONTROL command:");
                COM_LOG(LOG_LEVEL_INFO, "   Pump State: %s", pumpState.c_str());

                // Control pump relay (active LOW)
                if (pumpState == "ON") {
                    digitalWrite(PUMP_RELAY_PIN, LOW);
                    COM_LOG(LOG_LEVEL_INFO, "   ✅ Pump: ON (GPIO %d = LOW)", PUMP_RELAY_PIN);
                } else if (pumpState == "OFF") {
                    digitalWrite(PUMP_RELAY_PIN, HIGH);
                    COM_LOG(LOG_LEVEL_INFO, "   ✅ Pump: OFF (GPIO %d = HIGH)", PUMP_RELAY_PIN);
                } else {
                    COM_LOG(LOG_LEVEL_WARN, "   ⚠️  Unknown pump state: %s", pumpState.c_str());
                }
            } else {
                COM_LOG(LOG_LEVEL_WARN, "⚠️  Unknown action received: %s", action.c_str());
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

// Start irrigation - NOW SUPPORTS MULTIPLE ZONES SIMULTANEOUSLY (active LOW)
static void startIrrigation(int zoneNumber, int durationSeconds, const String& zoneId) {
    // Validate zone number
    if (zoneNumber < 1 || zoneNumber > MAX_ZONES) {
        COM_LOG(LOG_LEVEL_ERROR, "Invalid zone number: %d", zoneNumber);
        return;
    }

    int zoneIndex = zoneNumber - 1;
    unsigned long minutes = durationSeconds / 60;
    unsigned long seconds = durationSeconds % 60;

    COM_LOG(LOG_LEVEL_INFO, "🚰 Starting irrigation for Zone %d:", zoneNumber);
    COM_LOG(LOG_LEVEL_INFO, "   GPIO: %d (Relay PIN)", zoneRelayPins[zoneIndex]);
    if (minutes > 0) {
        COM_LOG(LOG_LEVEL_INFO, "   Duration: %lu min %lu sec (%d total seconds)",
               minutes, seconds, durationSeconds);
    } else {
        COM_LOG(LOG_LEVEL_INFO, "   Duration: %lu sec", seconds);
    }

    // Store zone state
    unsigned long startTimeMs = millis();
    unsigned long endTimeMs = startTimeMs + (durationSeconds * 1000);

    zoneStates[zoneIndex].isActive = true;
    zoneStates[zoneIndex].endTime = endTimeMs;
    zoneStates[zoneIndex].durationSeconds = durationSeconds;
    zoneStates[zoneIndex].zoneId = zoneId;

    // NOTE: Pump is now controlled independently by ESP32_master based on tank level
    // No longer automatically tied to zone irrigation state
    COM_LOG(LOG_LEVEL_INFO, "   ℹ️  Pump control is now independent (tank level based)");

    // Turn on zone relay (active LOW - set to LOW to activate)
    digitalWrite(zoneRelayPins[zoneIndex], LOW);
    COM_LOG(LOG_LEVEL_INFO, "   ✅ Zone %d relay: ON (GPIO %d = LOW)", zoneNumber, zoneRelayPins[zoneIndex]);

    // Update legacy variables for backward compatibility
    isIrrigating = true;
    activeZoneNumber = zoneNumber;
    irrigationEndTime = endTimeMs;

    unsigned long endTimeSec = endTimeMs / 1000;
    COM_LOG(LOG_LEVEL_INFO, "   ⏱️  Zone %d will stop automatically at +%lu seconds", zoneNumber, (endTimeSec - (startTimeMs / 1000)));
}

// Stop irrigation - NOW SUPPORTS MULTIPLE ZONES (can stop individual zones - active LOW)
static void stopIrrigation(int zoneNumber) {
    if (zoneNumber == 0) {
        // Stop ALL zones
        COM_LOG(LOG_LEVEL_INFO, "🛑 Stopping ALL active irrigation zones");

        for (int i = 0; i < MAX_ZONES; i++) {
            if (zoneStates[i].isActive) {
                int zone = i + 1;
                digitalWrite(zoneRelayPins[i], HIGH);
                COM_LOG(LOG_LEVEL_INFO, "   ✅ Zone %d relay: OFF (GPIO %d = HIGH)", zone, zoneRelayPins[i]);
                zoneStates[i].isActive = false;
                zoneStates[i].endTime = 0;
                zoneStates[i].durationSeconds = 0;
                zoneStates[i].zoneId = "";
            }
        }

        // NOTE: Pump is now controlled independently by ESP32_master based on tank level
        // No longer automatically stopped when zones finish
        COM_LOG(LOG_LEVEL_INFO, "   ℹ️  Pump remains under independent tank level control");

        // Update legacy variables
        isIrrigating = false;
        activeZoneNumber = 0;
        irrigationEndTime = 0;

        COM_LOG(LOG_LEVEL_INFO, "   ✅ All irrigation zones stopped successfully");
    } else if (zoneNumber >= 1 && zoneNumber <= MAX_ZONES) {
        // Stop SPECIFIC zone
        int zoneIndex = zoneNumber - 1;

        if (!zoneStates[zoneIndex].isActive) {
            COM_LOG(LOG_LEVEL_DEBUG, "Zone %d not active - nothing to stop", zoneNumber);
            return;
        }

        COM_LOG(LOG_LEVEL_INFO, "🛑 Stopping irrigation for Zone %d:", zoneNumber);

        // Turn off zone relay (active LOW - set to HIGH to deactivate)
        digitalWrite(zoneRelayPins[zoneIndex], HIGH);
        COM_LOG(LOG_LEVEL_INFO, "   ✅ Zone %d relay: OFF (GPIO %d = HIGH)", zoneNumber, zoneRelayPins[zoneIndex]);

        zoneStates[zoneIndex].isActive = false;
        zoneStates[zoneIndex].endTime = 0;
        zoneStates[zoneIndex].durationSeconds = 0;
        zoneStates[zoneIndex].zoneId = "";

        // NOTE: Pump is now controlled independently by ESP32_master based on tank level
        // No longer affected by individual zone irrigation state
        COM_LOG(LOG_LEVEL_INFO, "   ℹ️  Pump control independent of zone state");

        // Update legacy variables for backward compatibility
        // Check if any zones are still active
        bool anyZoneActive = false;
        for (int i = 0; i < MAX_ZONES; i++) {
            if (zoneStates[i].isActive) {
                anyZoneActive = true;
                break;
            }
        }

        if (!anyZoneActive) {
            isIrrigating = false;
            activeZoneNumber = 0;
            irrigationEndTime = 0;
        }

        COM_LOG(LOG_LEVEL_INFO, "   ✅ Zone %d irrigation stopped successfully", zoneNumber);
    } else {
        COM_LOG(LOG_LEVEL_ERROR, "Invalid zone number: %d", zoneNumber);
    }
}

// Check irrigation timer - NOW HANDLES MULTIPLE ZONES INDEPENDENTLY
static void checkIrrigationTimer() {
    unsigned long currentTime = millis();

    // Check each zone independently
    for (int i = 0; i < MAX_ZONES; i++) {
        if (zoneStates[i].isActive && zoneStates[i].endTime > 0) {
            if (currentTime >= zoneStates[i].endTime) {
                // This zone's timer expired
                int zoneNum = i + 1;
                COM_LOG(LOG_LEVEL_INFO, "⏰ Irrigation timer expired for Zone %d - stopping automatically", zoneNum);
                stopIrrigation(zoneNum);
            }
        }
    }

    // Display remaining time every 5 seconds (for all active zones)
    static unsigned long lastTimerLog = 0;
    if (currentTime - lastTimerLog >= 5000) {
        lastTimerLog = currentTime;
        bool anyActive = false;

        for (int i = 0; i < MAX_ZONES; i++) {
            if (zoneStates[i].isActive && zoneStates[i].endTime > 0) {
                anyActive = true;
                int zoneNum = i + 1;
                unsigned long remainingMs = zoneStates[i].endTime - currentTime;
                unsigned long remainingSeconds = remainingMs / 1000;
                unsigned long remainingMinutes = remainingSeconds / 60;
                remainingSeconds = remainingSeconds % 60;

                if (remainingMinutes > 0) {
                    COM_LOG(LOG_LEVEL_INFO, "⏱️  Zone %d active | Time remaining: %lu min %lu sec",
                           zoneNum, remainingMinutes, remainingSeconds);
                } else {
                    COM_LOG(LOG_LEVEL_INFO, "⏱️  Zone %d active | Time remaining: %lu sec",
                           zoneNum, remainingSeconds);
                }
            }
        }

        if (!anyActive) {
            lastTimerLog = 0; // Reset for next active irrigation
        }
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
        stopIrrigation(0);  // Stop all zones
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
