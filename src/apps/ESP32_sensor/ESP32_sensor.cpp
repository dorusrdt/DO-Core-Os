#include "ESP32_sensor.h"
#include "../../kernel/app/app_manager.h"
#include "../../kernel/core/log_system_optimized.h"
#include <WiFi.h>
#include <WebSocketsClient.h>

// Configuration AP du Master
static const char* master_ap_ssid = "ESP32_MASTER";
static const char* master_ap_pass = "12345678";
static const char* master_ip = "192.168.4.1";
static const uint16_t master_port = 81;

static WebSocketsClient* webSocket = nullptr;
static bool app_running = false;

// Gestionnaire d'événements WebSocket
static void onWebSocketEvent_sensor(WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            kernel_log(LOG_LEVEL_WARN, "WebSocket disconnected from master");
            break;
        case WStype_CONNECTED:
            kernel_log(LOG_LEVEL_INFO, "WebSocket connected to master");
            break;
        case WStype_TEXT:
            kernel_log(LOG_LEVEL_INFO, "[Master → Slave] %.*s", length, payload);
            // Ici on peut traiter les messages du master
            break;
        case WStype_BIN:
            kernel_log(LOG_LEVEL_INFO, "WebSocket binary message received");
            break;
        case WStype_ERROR:
            kernel_log(LOG_LEVEL_ERROR, "WebSocket error");
            break;
        case WStype_FRAGMENT_TEXT_START:
        case WStype_FRAGMENT_BIN_START:
        case WStype_FRAGMENT:
        case WStype_FRAGMENT_FIN:
            break;
    }
}

// Démarrage de l'application ESP32_sensor
static void ESP32_sensor_app_start(void) {
    if (app_running) {
        return;
    }

    kernel_log(LOG_LEVEL_INFO, "Starting ESP32 Sensor (WebSocket)...");

    // Connexion au réseau AP du Master
    kernel_log(LOG_LEVEL_INFO, "Connecting to master AP: %s", master_ap_ssid);
    WiFi.begin(master_ap_ssid, master_ap_pass);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
        delay(500);
    }

    if (WiFi.status() != WL_CONNECTED) {
        kernel_log(LOG_LEVEL_ERROR, "Failed to connect to master AP");
        return;
    }

    kernel_log(LOG_LEVEL_INFO, "Connected to master AP!");
    kernel_log(LOG_LEVEL_INFO, "Slave IP: %s", WiFi.localIP().toString().c_str());

    // WebSocket client vers le Master
    webSocket = new WebSocketsClient();
    if (!webSocket) {
        kernel_log(LOG_LEVEL_ERROR, "Failed to create WebSocket client");
        return;
    }

    webSocket->begin(master_ip, master_port, "/");
    webSocket->onEvent(onWebSocketEvent_sensor);
    webSocket->setReconnectInterval(5000); // Reconexion automatique

    kernel_log(LOG_LEVEL_INFO, "WebSocket client started, connecting to %s:%d", master_ip, master_port);

    app_running = true;
}

// Arrêt de l'application ESP32_sensor
static void ESP32_sensor_app_stop(void) {
    if (!app_running) {
        return;
    }

    kernel_log(LOG_LEVEL_INFO, "Stopping ESP-NOW Slave...");

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

    // Envoyer des données périodiques au master
    static unsigned long lastSend = 0;
    if (millis() - lastSend > 5000) { // Toutes les 5 secondes
        lastSend = millis();

        if (webSocket->isConnected()) {
            char message[64];
            snprintf(message, sizeof(message), "Hello Master! Slave IP: %s",
                    WiFi.localIP().toString().c_str());
            webSocket->sendTXT(message);
        }
    }
}

// Enregistrement de l'application
SysError_t ESP32_sensor_register_app() {
    AppCallbacks_t callbacks = {
        .start = ESP32_sensor_app_start,
        .stop = ESP32_sensor_app_stop,
        .loop = ESP32_sensor_app_loop
    };

    uint8_t app_id;
    return app_register("ESP32_sensor", "WebSocket Sensor Application",
                       APP_TYPE_USER, &callbacks, &app_id);
}