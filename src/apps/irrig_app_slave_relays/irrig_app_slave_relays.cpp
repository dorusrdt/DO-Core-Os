#include "irrig_app_slave_relays.h"
#include "../../kernel/core/log_system_optimized.h"
#include "../irrig_common/irrig_communication.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// ===== VARIABLES GLOBALES =====

static IrrigRelayConfig_t app_config;
static bool app_initialized = false;

// État irrigation
static bool is_irrigating = false;
static uint8_t active_zone_id = 0;
static unsigned long irrigation_end_time = 0;
static bool pump_running = false;
static bool relay_states[MAX_ZONES] = {false, false, false, false};

// Serveur HTTP
static WebServer* http_server = nullptr;

// Statistiques
static uint32_t total_commands = 0;
static uint32_t total_irrigations = 0;
static uint32_t total_irrigation_seconds = 0;
static unsigned long last_status_publish = 0;

// ===== CALLBACKS DO-CORE =====

SysError_t irrig_app_slave_relays_init(void) {
    if (app_initialized) {
        return SYS_ALREADY_INITIALIZED;
    }
    
    kernel_log(LOG_LEVEL_INFO, "IrrigSlaveRelays: Initializing...");
    kernel_log(LOG_LEVEL_INFO, "  Safety timeout: %lums", app_config.safety_timeout_ms);
    
    // Initialiser hardware
    relays_init_hardware();
    
    // Initialiser serveur HTTP si activé
    if (app_config.enable_http_server) {
        relays_start_http_server();
    }
    
    app_initialized = true;
    kernel_log(LOG_LEVEL_INFO, "IrrigSlaveRelays: Initialization complete");
    
    return SYS_OK;
}

void irrig_app_slave_relays_start(void) {
    kernel_log(LOG_LEVEL_INFO, "IrrigSlaveRelays: Starting...");
    
    if (WiFi.status() == WL_CONNECTED) {
        kernel_log(LOG_LEVEL_INFO, "WiFi connected: %s", WiFi.localIP().toString().c_str());
    } else {
        kernel_log(LOG_LEVEL_WARN, "WiFi not connected");
    }
    
    kernel_log(LOG_LEVEL_INFO, "IrrigSlaveRelays: Ready");
}

void irrig_app_slave_relays_loop(void) {
    unsigned long current_time = millis();
    
    // Gérer requêtes HTTP
    if (app_config.enable_http_server && http_server) {
        relays_handle_http_requests();
    }
    
    // Vérifier timer irrigation
    relays_check_irrigation_timer();
    
    // Publier statut périodiquement
    if (app_config.status_publish_interval_ms > 0 && 
        current_time - last_status_publish >= app_config.status_publish_interval_ms) {
        
        IrrigationStatusPacket_t status;
        relays_get_status(&status);
        
        if (irrig_comm_publish_irrigation_status(&status)) {
            kernel_log(LOG_LEVEL_DEBUG, "IrrigSlaveRelays: Status published");
        }
        
        last_status_publish = current_time;
    }
}

void irrig_app_slave_relays_stop(void) {
    kernel_log(LOG_LEVEL_INFO, "IrrigSlaveRelays: Stopping...");
    
    // Arrêt d'urgence
    relays_emergency_stop();
    
    // Arrêter serveur HTTP
    if (http_server) {
        relays_stop_http_server();
    }
    
    kernel_log(LOG_LEVEL_INFO, "IrrigSlaveRelays: Statistics:");
    kernel_log(LOG_LEVEL_INFO, "  Total commands: %lu", total_commands);
    kernel_log(LOG_LEVEL_INFO, "  Total irrigations: %lu", total_irrigations);
    kernel_log(LOG_LEVEL_INFO, "  Total irrigation time: %lus", total_irrigation_seconds);
    
    app_initialized = false;
}

SysError_t register_irrig_app_slave_relays(const IrrigRelayConfig_t* config) {
    if (!config) {
        kernel_log(LOG_LEVEL_ERROR, "Invalid configuration");
        return SYS_INVALID_PARAM;
    }
    
    // Copier configuration
    memcpy(&app_config, config, sizeof(IrrigRelayConfig_t));
    
    // Créer callbacks
    AppCallbacks_t callbacks = {0};
    callbacks.init = irrig_app_slave_relays_init;
    callbacks.start = irrig_app_slave_relays_start;
    callbacks.stop = irrig_app_slave_relays_stop;
    callbacks.loop = irrig_app_slave_relays_loop;
    
    uint8_t app_id;
    
    // Enregistrer application
    SysError_t result = app_register("IrrigSlaveRelays",
                                   "Irrigation Control Module",
                                   APP_TYPE_USER,
                                   &callbacks,
                                   &app_id);
    
    if (result == SYS_OK) {
        kernel_log(LOG_LEVEL_INFO, "IrrigSlaveRelays registered with ID %d", app_id);
    } else {
        kernel_log(LOG_LEVEL_ERROR, "Failed to register (error: %d)", result);
    }
    
    return result;
}

// ===== INITIALISATION HARDWARE =====

void relays_init_hardware(void) {
    kernel_log(LOG_LEVEL_INFO, "Initializing relay hardware...");
    
    // Configurer pins relais en sortie
    pinMode(ZONE_1_RELAY_PIN, OUTPUT);
    pinMode(ZONE_2_RELAY_PIN, OUTPUT);
    pinMode(ZONE_3_RELAY_PIN, OUTPUT);
    pinMode(ZONE_4_RELAY_PIN, OUTPUT);
    pinMode(PUMP_RELAY_PIN, OUTPUT);
    
    // Configurer pins indicateurs
    pinMode(STATUS_LED_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    
    // Tous OFF au démarrage
    relays_set_all_off();
    
    kernel_log(LOG_LEVEL_INFO, "Relay pins initialized:");
    kernel_log(LOG_LEVEL_INFO, "  Zones: %d, %d, %d, %d", 
               ZONE_1_RELAY_PIN, ZONE_2_RELAY_PIN, ZONE_3_RELAY_PIN, ZONE_4_RELAY_PIN);
    kernel_log(LOG_LEVEL_INFO, "  Pump: %d", PUMP_RELAY_PIN);
}

// ===== EXÉCUTION COMMANDES =====

void relays_execute_command(IrrigationCommandPacket_t* cmd) {
    if (!cmd) return;
    
    total_commands++;
    
    kernel_log(LOG_LEVEL_INFO, "IrrigSlaveRelays: Executing command %d for zone %d", 
               cmd->command, cmd->zone_id);
    
    switch (cmd->command) {
        case CMD_START_IRRIGATION:
            relays_start_irrigation(cmd->zone_id, cmd->duration_seconds);
            break;
            
        case CMD_STOP_IRRIGATION:
            relays_stop_irrigation();
            break;
            
        case CMD_EMERGENCY_STOP:
            relays_emergency_stop();
            break;
            
        case CMD_TEST_RELAY:
            kernel_log(LOG_LEVEL_INFO, "Testing relay zone %d", cmd->zone_id);
            relays_set_zone(cmd->zone_id, true);
            delay(1000);
            relays_set_zone(cmd->zone_id, false);
            break;
            
        default:
            kernel_log(LOG_LEVEL_WARN, "Unknown command: %d", cmd->command);
            break;
    }
}

void relays_start_irrigation(uint8_t zone_id, uint16_t duration_seconds) {
    if (zone_id < 1 || zone_id > MAX_ZONES) {
        kernel_log(LOG_LEVEL_ERROR, "Invalid zone ID: %d", zone_id);
        return;
    }
    
    if (is_irrigating) {
        kernel_log(LOG_LEVEL_WARN, "Irrigation already in progress for zone %d", active_zone_id);
        return;
    }
    
    // Vérifier timeout sécurité
    if (duration_seconds * 1000 > app_config.safety_timeout_ms) {
        kernel_log(LOG_LEVEL_WARN, "Duration %ds exceeds safety timeout, capping to %lus", 
                   duration_seconds, app_config.safety_timeout_ms / 1000);
        duration_seconds = app_config.safety_timeout_ms / 1000;
    }
    
    kernel_log(LOG_LEVEL_INFO, "Starting irrigation: Zone %d, Duration %ds", zone_id, duration_seconds);
    
    // Activer zone
    relays_set_zone(zone_id, true);
    
    // Activer pompe
    relays_set_pump(true);
    
    // LED status ON
    digitalWrite(STATUS_LED_PIN, HIGH);
    
    // Définir état
    is_irrigating = true;
    active_zone_id = zone_id;
    irrigation_end_time = millis() + (duration_seconds * 1000);
    
    total_irrigations++;
    total_irrigation_seconds += duration_seconds;
}

void relays_stop_irrigation(void) {
    if (!is_irrigating) {
        kernel_log(LOG_LEVEL_DEBUG, "No irrigation in progress");
        return;
    }
    
    kernel_log(LOG_LEVEL_INFO, "Stopping irrigation for zone %d", active_zone_id);
    
    // Désactiver zone active
    relays_set_zone(active_zone_id, false);
    
    // Désactiver pompe
    relays_set_pump(false);
    
    // LED status OFF
    digitalWrite(STATUS_LED_PIN, LOW);
    
    // Réinitialiser état
    is_irrigating = false;
    active_zone_id = 0;
    irrigation_end_time = 0;
}

void relays_emergency_stop(void) {
    kernel_log(LOG_LEVEL_WARN, "EMERGENCY STOP activated");
    
    // Tout OFF
    relays_set_all_off();
    
    // Buzzer alerte
    digitalWrite(BUZZER_PIN, HIGH);
    delay(500);
    digitalWrite(BUZZER_PIN, LOW);
    
    // Réinitialiser état
    is_irrigating = false;
    active_zone_id = 0;
    irrigation_end_time = 0;
}

// ===== GESTION TIMERS =====

void relays_check_irrigation_timer(void) {
    if (!is_irrigating) return;
    
    if (millis() >= irrigation_end_time) {
        kernel_log(LOG_LEVEL_INFO, "Irrigation timer expired");
        relays_stop_irrigation();
    }
}

// ===== CONTRÔLE GPIO =====

void relays_set_zone(uint8_t zone_id, bool state) {
    if (zone_id < 1 || zone_id > MAX_ZONES) return;
    
    int pin = -1;
    switch (zone_id) {
        case 1: pin = ZONE_1_RELAY_PIN; break;
        case 2: pin = ZONE_2_RELAY_PIN; break;
        case 3: pin = ZONE_3_RELAY_PIN; break;
        case 4: pin = ZONE_4_RELAY_PIN; break;
    }
    
    if (pin != -1) {
        digitalWrite(pin, state ? HIGH : LOW);
        relay_states[zone_id - 1] = state;
        kernel_log(LOG_LEVEL_DEBUG, "Zone %d relay: %s", zone_id, state ? "ON" : "OFF");
    }
}

void relays_set_pump(bool state) {
    digitalWrite(PUMP_RELAY_PIN, state ? HIGH : LOW);
    pump_running = state;
    kernel_log(LOG_LEVEL_INFO, "Pump: %s", state ? "ON" : "OFF");
}

void relays_set_all_off(void) {
    digitalWrite(ZONE_1_RELAY_PIN, LOW);
    digitalWrite(ZONE_2_RELAY_PIN, LOW);
    digitalWrite(ZONE_3_RELAY_PIN, LOW);
    digitalWrite(ZONE_4_RELAY_PIN, LOW);
    digitalWrite(PUMP_RELAY_PIN, LOW);
    digitalWrite(STATUS_LED_PIN, LOW);
    
    for (int i = 0; i < MAX_ZONES; i++) {
        relay_states[i] = false;
    }
    pump_running = false;
    
    kernel_log(LOG_LEVEL_INFO, "All relays OFF");
}

// ===== STATUT =====

void relays_get_status(IrrigationStatusPacket_t* status) {
    if (!status) return;
    
    status->zone_id = active_zone_id;
    status->is_irrigating = is_irrigating;
    
    if (is_irrigating && irrigation_end_time > millis()) {
        status->remaining_seconds = (irrigation_end_time - millis()) / 1000;
    } else {
        status->remaining_seconds = 0;
    }
    
    status->pump_running = pump_running;
    
    for (int i = 0; i < MAX_ZONES; i++) {
        status->relay_states[i] = relay_states[i];
    }
    
    status->timestamp = millis();
}

// ===== SERVEUR HTTP =====

void relays_start_http_server(void) {
    if (http_server) {
        kernel_log(LOG_LEVEL_WARN, "HTTP server already running");
        return;
    }
    
    http_server = new WebServer(app_config.http_server_port);
    
    // Route POST /api/irrigation/command
    http_server->on("/api/irrigation/command", HTTP_POST, []() {
        if (!http_server->hasArg("plain")) {
            http_server->send(400, "text/plain", "Missing body");
            return;
        }
        
        String body = http_server->arg("plain");
        
        DynamicJsonDocument doc(512);
        DeserializationError error = deserializeJson(doc, body);
        
        if (error) {
            http_server->send(400, "text/plain", "Invalid JSON");
            return;
        }
        
        // Parser commande
        IrrigationCommandPacket_t cmd;
        String command_str = doc["command"].as<String>();
        
        if (command_str == "start_irrigation") {
            cmd.command = CMD_START_IRRIGATION;
        } else if (command_str == "stop_irrigation") {
            cmd.command = CMD_STOP_IRRIGATION;
        } else if (command_str == "emergency_stop") {
            cmd.command = CMD_EMERGENCY_STOP;
        } else if (command_str == "test_relay") {
            cmd.command = CMD_TEST_RELAY;
        } else {
            http_server->send(400, "text/plain", "Unknown command");
            return;
        }
        
        cmd.zone_id = doc["zone_id"];
        cmd.duration_seconds = doc["duration_seconds"];
        strncpy(cmd.zone_server_id, doc["zone_server_id"].as<String>().c_str(), 63);
        cmd.timestamp = doc["timestamp"];
        
        // Exécuter commande
        relays_execute_command(&cmd);
        
        // Réponse
        DynamicJsonDocument response(256);
        response["status"] = "ok";
        response["command_id"] = String(millis());
        response["executed_at"] = millis();
        
        String responseStr;
        serializeJson(response, responseStr);
        
        http_server->send(200, "application/json", responseStr);
    });
    
    // Route GET /api/irrigation/status
    http_server->on("/api/irrigation/status", HTTP_GET, []() {
        IrrigationStatusPacket_t status;
        relays_get_status(&status);
        
        DynamicJsonDocument doc(512);
        doc["device_id"] = "ESP32_SLAVE_RELAYS";
        doc["zone_id"] = status.zone_id;
        doc["is_irrigating"] = status.is_irrigating;
        doc["remaining_seconds"] = status.remaining_seconds;
        doc["pump_running"] = status.pump_running;
        
        JsonArray relayArray = doc.createNestedArray("relay_states");
        for (int i = 0; i < MAX_ZONES; i++) {
            relayArray.add(status.relay_states[i]);
        }
        
        doc["timestamp"] = status.timestamp;
        doc["total_commands"] = total_commands;
        doc["total_irrigations"] = total_irrigations;
        
        String response;
        serializeJson(doc, response);
        
        http_server->send(200, "application/json", response);
    });
    
    // Route POST /api/irrigation/emergency_stop
    http_server->on("/api/irrigation/emergency_stop", HTTP_POST, []() {
        relays_emergency_stop();
        
        DynamicJsonDocument doc(128);
        doc["status"] = "ok";
        doc["stopped_at"] = millis();
        
        String response;
        serializeJson(doc, response);
        
        http_server->send(200, "application/json", response);
    });
    
    http_server->begin();
    kernel_log(LOG_LEVEL_INFO, "HTTP server started on port %d", app_config.http_server_port);
}

void relays_stop_http_server(void) {
    if (http_server) {
        http_server->stop();
        delete http_server;
        http_server = nullptr;
        kernel_log(LOG_LEVEL_INFO, "HTTP server stopped");
    }
}

void relays_handle_http_requests(void) {
    if (http_server) {
        http_server->handleClient();
    }
}
