#include "irrig_communication.h"
#include "../../kernel/core/log_system_optimized.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Configuration globale
static IrrigCommConfig_t g_comm_config;
static bool g_comm_initialized = false;

// ===== INITIALISATION =====

SysError_t irrig_comm_init(IrrigCommConfig_t* config) {
    if (!config) {
        kernel_log(LOG_LEVEL_ERROR, "IrrigComm: Invalid config");
        return SYS_INVALID_PARAM;
    }
    
    memcpy(&g_comm_config, config, sizeof(IrrigCommConfig_t));
    
    kernel_log(LOG_LEVEL_INFO, "IrrigComm: HTTP mode initialized");
    kernel_log(LOG_LEVEL_INFO, "  Master: %s:%d", g_comm_config.master_ip, g_comm_config.master_port);
    kernel_log(LOG_LEVEL_INFO, "  Slave1: %s:%d", g_comm_config.slave1_ip, g_comm_config.slave1_port);
    kernel_log(LOG_LEVEL_INFO, "  Slave2: %s:%d", g_comm_config.slave2_ip, g_comm_config.slave2_port);
    kernel_log(LOG_LEVEL_INFO, "  Timeout: %dms, Retry: %d", 
               g_comm_config.http_timeout_ms, g_comm_config.retry_count);
    
    g_comm_initialized = true;
    return SYS_OK;
}

IrrigCommConfig_t* irrig_comm_get_config(void) {
    return &g_comm_config;
}

// ===== SLAVE 1 → MASTER : PUBLIER DONNÉES CAPTEURS =====

bool irrig_comm_publish_sensor_data(SensorDataPacket_t* data) {
    if (!g_comm_initialized || !data) {
        kernel_log(LOG_LEVEL_ERROR, "IrrigComm: Not initialized or invalid data");
        return false;
    }
    
    HTTPClient http;
    String url = String("http://") + g_comm_config.master_ip + ":" + 
                 String(g_comm_config.master_port) + "/api/sensors/data";
    
    bool success = false;
    
    for (uint8_t attempt = 0; attempt < g_comm_config.retry_count; attempt++) {
        if (!http.begin(url)) {
            kernel_log(LOG_LEVEL_ERROR, "IrrigComm: Failed to begin HTTP");
            delay(g_comm_config.retry_delay_ms);
            continue;
        }
        
        http.setTimeout(g_comm_config.http_timeout_ms);
        http.addHeader("Content-Type", "application/json");
        
        // Créer JSON
        DynamicJsonDocument doc(2048);
        doc["timestamp"] = data->timestamp;
        
        JsonArray moistureArray = doc.createNestedArray("moisture");
        for (int i = 0; i < MAX_SENSORS; i++) {
            moistureArray.add(data->moisture[i]);
        }
        
        doc["temperature"] = data->temperature;
        doc["humidity"] = data->humidity;
        doc["pressure"] = data->pressure;
        doc["battery_level"] = data->battery_level;
        doc["signal_strength"] = data->signal_strength;
        
        String payload;
        serializeJson(doc, payload);
        
        kernel_log(LOG_LEVEL_DEBUG, "IrrigComm: Sending sensor data to Master (attempt %d/%d)", 
                   attempt + 1, g_comm_config.retry_count);
        
        int httpCode = http.POST(payload);
        
        if (httpCode == 200) {
            kernel_log(LOG_LEVEL_DEBUG, "IrrigComm: Sensor data sent successfully");
            success = true;
            http.end();
            break;
        } else {
            kernel_log(LOG_LEVEL_WARN, "IrrigComm: HTTP error %d (attempt %d/%d)", 
                       httpCode, attempt + 1, g_comm_config.retry_count);
            http.end();
            delay(g_comm_config.retry_delay_ms);
        }
    }
    
    if (!success) {
        kernel_log(LOG_LEVEL_ERROR, "IrrigComm: Failed to send sensor data after %d attempts", 
                   g_comm_config.retry_count);
    }
    
    return success;
}

// ===== MASTER → SLAVE 2 : ENVOYER COMMANDE IRRIGATION =====

bool irrig_comm_send_irrigation_command(IrrigationCommandPacket_t* cmd) {
    if (!g_comm_initialized || !cmd) {
        kernel_log(LOG_LEVEL_ERROR, "IrrigComm: Not initialized or invalid command");
        return false;
    }
    
    HTTPClient http;
    String url = String("http://") + g_comm_config.slave2_ip + ":" + 
                 String(g_comm_config.slave2_port) + "/api/irrigation/command";
    
    bool success = false;
    
    for (uint8_t attempt = 0; attempt < g_comm_config.retry_count; attempt++) {
        if (!http.begin(url)) {
            kernel_log(LOG_LEVEL_ERROR, "IrrigComm: Failed to begin HTTP");
            delay(g_comm_config.retry_delay_ms);
            continue;
        }
        
        http.setTimeout(g_comm_config.http_timeout_ms);
        http.addHeader("Content-Type", "application/json");
        
        // Créer JSON
        DynamicJsonDocument doc(512);
        
        const char* cmd_str = "";
        switch (cmd->command) {
            case CMD_START_IRRIGATION: cmd_str = "start_irrigation"; break;
            case CMD_STOP_IRRIGATION: cmd_str = "stop_irrigation"; break;
            case CMD_EMERGENCY_STOP: cmd_str = "emergency_stop"; break;
            case CMD_TEST_RELAY: cmd_str = "test_relay"; break;
            case CMD_STATUS_REQUEST: cmd_str = "status_request"; break;
            default: cmd_str = "unknown"; break;
        }
        
        doc["command"] = cmd_str;
        doc["zone_id"] = cmd->zone_id;
        doc["duration_seconds"] = cmd->duration_seconds;
        doc["zone_server_id"] = cmd->zone_server_id;
        doc["timestamp"] = cmd->timestamp;
        
        String payload;
        serializeJson(doc, payload);
        
        kernel_log(LOG_LEVEL_INFO, "📤 Master → Slave2: Sending '%s' to http://%s:%d (attempt %d/%d)", 
                   cmd_str, g_comm_config.slave2_ip, g_comm_config.slave2_port,
                   attempt + 1, g_comm_config.retry_count);
        kernel_log(LOG_LEVEL_DEBUG, "   Payload: %s", payload.c_str());
        
        int httpCode = http.POST(payload);
        
        if (httpCode == 200) {
            kernel_log(LOG_LEVEL_INFO, "✅ Master → Slave2: Command '%s' sent successfully!", cmd_str);
            success = true;
            http.end();
            break;
        } else {
            kernel_log(LOG_LEVEL_WARN, "⚠️ Master → Slave2: HTTP error %d (attempt %d/%d)", 
                       httpCode, attempt + 1, g_comm_config.retry_count);
            http.end();
            delay(g_comm_config.retry_delay_ms);
        }
    }
    
    if (!success) {
        kernel_log(LOG_LEVEL_ERROR, "IrrigComm: Failed to send command after %d attempts", 
                   g_comm_config.retry_count);
    }
    
    return success;
}

// ===== SLAVE 2 → MASTER : PUBLIER STATUT IRRIGATION =====

bool irrig_comm_publish_irrigation_status(IrrigationStatusPacket_t* status) {
    if (!g_comm_initialized || !status) {
        kernel_log(LOG_LEVEL_ERROR, "IrrigComm: Not initialized or invalid status");
        return false;
    }
    
    HTTPClient http;
    String url = String("http://") + g_comm_config.master_ip + ":" + 
                 String(g_comm_config.master_port) + "/api/irrigation/status";
    
    if (!http.begin(url)) {
        kernel_log(LOG_LEVEL_ERROR, "IrrigComm: Failed to begin HTTP");
        return false;
    }
    
    http.setTimeout(g_comm_config.http_timeout_ms);
    http.addHeader("Content-Type", "application/json");
    
    // Créer JSON
    DynamicJsonDocument doc(512);
    doc["zone_id"] = status->zone_id;
    doc["is_irrigating"] = status->is_irrigating;
    doc["remaining_seconds"] = status->remaining_seconds;
    doc["pump_running"] = status->pump_running;
    
    JsonArray relayArray = doc.createNestedArray("relay_states");
    for (int i = 0; i < MAX_ZONES; i++) {
        relayArray.add(status->relay_states[i]);
    }
    
    doc["timestamp"] = status->timestamp;
    
    String payload;
    serializeJson(doc, payload);
    
    kernel_log(LOG_LEVEL_INFO, "📤 Slave2 → Master: Sending status");
    kernel_log(LOG_LEVEL_INFO, "   Zone: %d | Irrigating: %s | Remaining: %lus | Pump: %s",
               status->zone_id, 
               status->is_irrigating ? "YES" : "NO",
               status->remaining_seconds,
               status->pump_running ? "ON" : "OFF");
    kernel_log(LOG_LEVEL_DEBUG, "   Payload: %s", payload.c_str());
    
    int httpCode = http.POST(payload);
    http.end();
    
    if (httpCode == 200) {
        kernel_log(LOG_LEVEL_DEBUG, "IrrigComm: Status sent successfully");
        return true;
    } else {
        kernel_log(LOG_LEVEL_WARN, "IrrigComm: Failed to send status (HTTP %d)", httpCode);
        return false;
    }
}
