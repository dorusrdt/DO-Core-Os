#include "ESP32_master.h"
#include "../../kernel/app/app_manager.h"
#include "../../kernel/core/log_system_optimized.h"
#include <WiFi.h>
#include <WebSocketsServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Configuration AP pour les clients (ESP32_sensor et ESP32_com)
const char* ap_ssid = "ESP32_MASTER";
const char* ap_pass = "12345678";

// Configuration serveur FastAPI
static const char* serverIP = "192.168.1.72";
static const int serverPort = 8000;

// Configuration WiFi externe (à configurer via CLI)
static String wifi_ssid = "";
static String wifi_pass = "";

static WebSocketsServer* webSocket = nullptr;
static bool app_running = false;

// Collecte des données slaves
static DynamicJsonDocument slaveData(1024);  // JSON pour stocker les données des slaves

// Fonction pour mettre à jour les données d'un slave
void updateSlaveData(uint8_t slaveId, String data) {
    String slaveKey = "slave_" + String(slaveId);
    slaveData[slaveKey] = data;
    slaveData["last_update"] = millis();
}

// Fonction pour envoyer les données au serveur FastAPI
void sendToServer() {
    if (WiFi.status() != WL_CONNECTED) {
        kernel_log(LOG_LEVEL_WARN, "Cannot send to server: no WiFi connection");
        return;
    }

    HTTPClient http;
    String url = String("http://") + serverIP + ":" + String(serverPort) + "/data";
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    // Créer le payload JSON
    DynamicJsonDocument payload(1024);
    payload["timestamp"] = millis();
    payload["master_ip"] = WiFi.localIP().toString();
    payload["ap_ip"] = WiFi.softAPIP().toString();

    // Copier les données slaves
    JsonObject slaves = payload.createNestedObject("slaves");
    for (JsonPair kv : slaveData.as<JsonObject>()) {
        slaves[kv.key().c_str()] = kv.value();
    }

    String jsonString;
    serializeJson(payload, jsonString);

    kernel_log(LOG_LEVEL_INFO, "Sending data to server: %s", url.c_str());

    int httpResponseCode = http.POST(jsonString);

    if (httpResponseCode > 0) {
        String response = http.getString();
        kernel_log(LOG_LEVEL_INFO, "Server response: %d - %s", httpResponseCode, response.c_str());
    } else {
        kernel_log(LOG_LEVEL_ERROR, "Failed to send data to server: %d", httpResponseCode);
    }

    http.end();
}

// Gestionnaire d'événements WebSocket (serveur pour clients)
void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            kernel_log(LOG_LEVEL_INFO, "WebSocket client #%u disconnected", num);
            break;
        case WStype_CONNECTED: {
            IPAddress ip = webSocket->remoteIP(num);
            kernel_log(LOG_LEVEL_INFO, "WebSocket client #%u connected from %d.%d.%d.%d",
                      num, ip[0], ip[1], ip[2], ip[3]);
            break;
        }
        case WStype_TEXT: {
            kernel_log(LOG_LEVEL_INFO, "[Client #%u → Master] %.*s", num, length, payload);
            // Collecter les données du client
            String clientMessage = String((char*)payload, length);
            updateSlaveData(num, clientMessage);
            break;
        }
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

// Démarrage de l'application ESP32_master
static void ESP32_master_app_start(void) {
    if (app_running) {
        return;
    }

    kernel_log(LOG_LEVEL_INFO, "Starting ESP32 Master (WebSocket Server)...");

    // === AP Mode pour les clients (ESP32_sensor et ESP32_com) ===
    if (WiFi.softAP(ap_ssid, ap_pass)) {
        kernel_log(LOG_LEVEL_INFO, "AP started: %s", ap_ssid);
        kernel_log(LOG_LEVEL_INFO, "AP IP: %s", WiFi.softAPIP().toString().c_str());
    } else {
        kernel_log(LOG_LEVEL_ERROR, "Failed to start AP");
        return;
    }

    // === WebSocket Server ===
    webSocket = new WebSocketsServer(81);
    if (!webSocket) {
        kernel_log(LOG_LEVEL_ERROR, "Failed to create WebSocket server");
        return;
    }

    webSocket->begin();
    webSocket->onEvent(onWebSocketEvent);

    kernel_log(LOG_LEVEL_INFO, "WebSocket server started on port 81");

    app_running = true;
}

// Arrêt de l'application ESP32_master
static void ESP32_master_app_stop(void) {
    if (!app_running) {
        return;
    }

    kernel_log(LOG_LEVEL_INFO, "Stopping ESP32 Master...");

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

    // Envoyer un ping aux clients connectés toutes les 30 secondes
    static unsigned long lastPing = 0;
    if (millis() - lastPing > 30000) {
        lastPing = millis();
        webSocket->broadcastTXT("ping from ESP32_master");
    }

    // Envoyer les données collectées au serveur FastAPI toutes les 30 secondes
    static unsigned long lastServerSend = 0;
    if (millis() - lastServerSend > 30000) {
        lastServerSend = millis();
        sendToServer();
    }

    // Envoyer un message à ESP32_com toutes les 10 secondes
    static unsigned long lastComMessage = 0;
    if (millis() - lastComMessage > 10000) {
        lastComMessage = millis();
        // Envoyer à tous les clients (ESP32_com recevra le message)
        webSocket->broadcastTXT("Hello ESP32_com from Master!");
        kernel_log(LOG_LEVEL_INFO, "Sent specific message to ESP32_com");
    }
}

// Enregistrement de l'application
SysError_t ESP32_master_register_app() {
    AppCallbacks_t callbacks = {
        .start = ESP32_master_app_start,
        .stop = ESP32_master_app_stop,
        .loop = ESP32_master_app_loop
    };

    uint8_t app_id;
    return app_register("ESP32_master", "WebSocket Master Application",
                       APP_TYPE_USER, &callbacks, &app_id);
}