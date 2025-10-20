#include "irrig_app_master_http.h"
#include "../../kernel/core/log_system_optimized.h"
#include <WebServer.h>
#include <ArduinoJson.h>

// ===== VARIABLES GLOBALES =====

static WebServer* g_http_server = nullptr;
static SensorDataCallback_t g_sensor_callback = nullptr;
static IrrigationStatusCallback_t g_status_callback = nullptr;

// Statistiques
static uint32_t g_sensor_data_received = 0;
static uint32_t g_status_received = 0;

// ===== INITIALISATION =====

SysError_t master_http_init(uint16_t port) {
    if (g_http_server) {
        kernel_log(LOG_LEVEL_WARN, "MasterHTTP: Server already running");
        return SYS_ALREADY_INITIALIZED;
    }
    
    g_http_server = new WebServer(port);
    
    // Route POST /api/sensors/data (Slave 1 → Master)
    g_http_server->on("/api/sensors/data", HTTP_POST, []() {
        kernel_log(LOG_LEVEL_INFO, "📥 MasterHTTP: Incoming POST /api/sensors/data from %s",
                   g_http_server->client().remoteIP().toString().c_str());
        
        if (!g_http_server->hasArg("plain")) {
            kernel_log(LOG_LEVEL_ERROR, "MasterHTTP: Missing body");
            g_http_server->send(400, "text/plain", "Missing body");
            return;
        }
        
        String body = g_http_server->arg("plain");
        
        DynamicJsonDocument doc(2048);
        DeserializationError error = deserializeJson(doc, body);
        
        if (error) {
            kernel_log(LOG_LEVEL_ERROR, "MasterHTTP: Invalid JSON: %s", error.c_str());
            g_http_server->send(400, "text/plain", "Invalid JSON");
            return;
        }
        
        // Parser données capteurs
        SensorDataPacket_t packet;
        packet.timestamp = doc["timestamp"];
        
        JsonArray moistureArray = doc["moisture"];
        for (int i = 0; i < MAX_SENSORS && i < moistureArray.size(); i++) {
            packet.moisture[i] = moistureArray[i];
        }
        
        packet.temperature = doc["temperature"];
        packet.humidity = doc["humidity"];
        packet.pressure = doc["pressure"];
        packet.battery_level = doc["battery_level"];
        packet.signal_strength = doc["signal_strength"];
        
        g_sensor_data_received++;
        
        // Appeler callback
        if (g_sensor_callback) {
            g_sensor_callback(&packet);
        }
        
        // Réponse
        DynamicJsonDocument response(128);
        response["status"] = "ok";
        response["received_at"] = millis();
        
        String responseStr;
        serializeJson(response, responseStr);
        
        g_http_server->send(200, "application/json", responseStr);
    });
    
    // Route POST /api/irrigation/status (Slave 2 → Master, optionnel)
    g_http_server->on("/api/irrigation/status", HTTP_POST, []() {
        if (!g_http_server->hasArg("plain")) {
            g_http_server->send(400, "text/plain", "Missing body");
            return;
        }
        
        String body = g_http_server->arg("plain");
        
        DynamicJsonDocument doc(512);
        DeserializationError error = deserializeJson(doc, body);
        
        if (error) {
            g_http_server->send(400, "text/plain", "Invalid JSON");
            return;
        }
        
        // Parser statut irrigation
        IrrigationStatusPacket_t status;
        status.zone_id = doc["zone_id"];
        status.is_irrigating = doc["is_irrigating"];
        status.remaining_seconds = doc["remaining_seconds"];
        status.pump_running = doc["pump_running"];
        
        JsonArray relayArray = doc["relay_states"];
        for (int i = 0; i < MAX_ZONES && i < relayArray.size(); i++) {
            status.relay_states[i] = relayArray[i];
        }
        
        status.timestamp = doc["timestamp"];
        
        g_status_received++;
        
        // Appeler callback
        if (g_status_callback) {
            g_status_callback(&status);
        }
        
        // Réponse
        DynamicJsonDocument response(128);
        response["status"] = "ok";
        response["received_at"] = millis();
        
        String responseStr;
        serializeJson(response, responseStr);
        
        g_http_server->send(200, "application/json", responseStr);
    });
    
    // Route GET /api/master/stats
    g_http_server->on("/api/master/stats", HTTP_GET, []() {
        DynamicJsonDocument doc(256);
        doc["device_id"] = "ESP32_MASTER";
        doc["uptime"] = millis() / 1000;
        doc["sensor_data_received"] = g_sensor_data_received;
        doc["status_received"] = g_status_received;
        
        String response;
        serializeJson(doc, response);
        
        g_http_server->send(200, "application/json", response);
    });
    
    g_http_server->begin();
    kernel_log(LOG_LEVEL_INFO, "MasterHTTP: Server started on port %d", port);
    
    return SYS_OK;
}

void master_http_stop(void) {
    if (g_http_server) {
        g_http_server->stop();
        delete g_http_server;
        g_http_server = nullptr;
        kernel_log(LOG_LEVEL_INFO, "MasterHTTP: Server stopped");
    }
}

void master_http_handle_requests(void) {
    if (g_http_server) {
        g_http_server->handleClient();
    }
}

void master_http_set_sensor_callback(SensorDataCallback_t callback) {
    g_sensor_callback = callback;
}

void master_http_set_status_callback(IrrigationStatusCallback_t callback) {
    g_status_callback = callback;
}
