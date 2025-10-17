#include "irrig_cli_commands.h"
#include "irrig_communication.h"
#include "../../kernel/core/log_system_optimized.h"
#include "../../kernel/app/app_manager.h"
#include <Preferences.h>
#include <string.h>

// Namespace pour les préférences NVS
#define IRRIG_NVS_NAMESPACE "irrig_cfg"

// Variables globales pour la configuration serveur FastAPI
static char g_server_url[128] = "http://192.168.1.100:8000";
static bool g_server_url_configured = false;

// Variables globales pour le rôle du device (enum défini dans .h)
static DeviceRole_t g_device_role = DEVICE_ROLE_NONE;
static uint8_t g_master_app_id = 0;
static uint8_t g_slave1_app_id = 0;
static uint8_t g_slave2_app_id = 0;

// ===== UTILITAIRES =====

// Sauvegarder configuration en NVS
static bool save_config_to_nvs(void) {
    Preferences prefs;
    if (!prefs.begin(IRRIG_NVS_NAMESPACE, false)) {
        kernel_log(LOG_LEVEL_ERROR, "IrrigCLI: Failed to open NVS");
        return false;
    }
    
    IrrigCommConfig_t* config = irrig_comm_get_config();
    
    prefs.putString("master_ip", config->master_ip);
    prefs.putUShort("master_port", config->master_port);
    prefs.putString("slave1_ip", config->slave1_ip);
    prefs.putUShort("slave1_port", config->slave1_port);
    prefs.putString("slave2_ip", config->slave2_ip);
    prefs.putUShort("slave2_port", config->slave2_port);
    prefs.putString("server_url", g_server_url);
    prefs.putUChar("device_role", (uint8_t)g_device_role);
    
    prefs.end();
    kernel_log(LOG_LEVEL_INFO, "IrrigCLI: Configuration saved to NVS");
    return true;
}

// Charger configuration depuis NVS
static bool load_config_from_nvs(void) {
    Preferences prefs;
    if (!prefs.begin(IRRIG_NVS_NAMESPACE, true)) {
        kernel_log(LOG_LEVEL_WARN, "IrrigCLI: No saved config in NVS");
        return false;
    }
    
    IrrigCommConfig_t* config = irrig_comm_get_config();
    
    String master_ip = prefs.getString("master_ip", "");
    if (master_ip.length() > 0) {
        strncpy(config->master_ip, master_ip.c_str(), sizeof(config->master_ip) - 1);
    }
    
    config->master_port = prefs.getUShort("master_port", 8080);
    
    String slave1_ip = prefs.getString("slave1_ip", "");
    if (slave1_ip.length() > 0) {
        strncpy(config->slave1_ip, slave1_ip.c_str(), sizeof(config->slave1_ip) - 1);
    }
    
    config->slave1_port = prefs.getUShort("slave1_port", 8081);
    
    String slave2_ip = prefs.getString("slave2_ip", "");
    if (slave2_ip.length() > 0) {
        strncpy(config->slave2_ip, slave2_ip.c_str(), sizeof(config->slave2_ip) - 1);
    }
    
    config->slave2_port = prefs.getUShort("slave2_port", 8082);
    
    String server_url = prefs.getString("server_url", "");
    if (server_url.length() > 0) {
        strncpy(g_server_url, server_url.c_str(), sizeof(g_server_url) - 1);
        g_server_url_configured = true;
    }
    
    g_device_role = (DeviceRole_t)prefs.getUChar("device_role", DEVICE_ROLE_NONE);
    
    prefs.end();
    kernel_log(LOG_LEVEL_INFO, "IrrigCLI: Configuration loaded from NVS");
    return true;
}

// ===== COMMANDES CLI =====

SysError_t cmd_irrig_config_server(int argc, char* argv[]) {
    if (argc < 2) {
        Serial.println("Usage: irrig_config_server <url>");
        Serial.println("Example: irrig_config_server http://192.168.1.100:8000");
        return SYS_INVALID_PARAM;
    }
    
    strncpy(g_server_url, argv[1], sizeof(g_server_url) - 1);
    g_server_url[sizeof(g_server_url) - 1] = '\0';
    g_server_url_configured = true;
    
    Serial.printf("Server URL configured: %s\n", g_server_url);
    Serial.println("Use 'irrig_config_save' to persist configuration");
    
    kernel_log(LOG_LEVEL_INFO, "IrrigCLI: Server URL set to %s", g_server_url);
    return SYS_OK;
}

SysError_t cmd_irrig_config_master(int argc, char* argv[]) {
    if (argc < 3) {
        Serial.println("Usage: irrig_config_master <ip> <port>");
        Serial.println("Example: irrig_config_master 192.168.1.101 8080");
        return SYS_INVALID_PARAM;
    }
    
    IrrigCommConfig_t* config = irrig_comm_get_config();
    
    strncpy(config->master_ip, argv[1], sizeof(config->master_ip) - 1);
    config->master_ip[sizeof(config->master_ip) - 1] = '\0';
    config->master_port = atoi(argv[2]);
    
    Serial.printf("Master configured: %s:%d\n", config->master_ip, config->master_port);
    Serial.println("Use 'irrig_config_save' to persist configuration");
    
    kernel_log(LOG_LEVEL_INFO, "IrrigCLI: Master set to %s:%d", 
               config->master_ip, config->master_port);
    return SYS_OK;
}

SysError_t cmd_irrig_config_slave1(int argc, char* argv[]) {
    if (argc < 3) {
        Serial.println("Usage: irrig_config_slave1 <ip> <port>");
        Serial.println("Example: irrig_config_slave1 192.168.1.102 8081");
        return SYS_INVALID_PARAM;
    }
    
    IrrigCommConfig_t* config = irrig_comm_get_config();
    
    strncpy(config->slave1_ip, argv[1], sizeof(config->slave1_ip) - 1);
    config->slave1_ip[sizeof(config->slave1_ip) - 1] = '\0';
    config->slave1_port = atoi(argv[2]);
    
    Serial.printf("Slave1 (Sensors) configured: %s:%d\n", config->slave1_ip, config->slave1_port);
    Serial.println("Use 'irrig_config_save' to persist configuration");
    
    kernel_log(LOG_LEVEL_INFO, "IrrigCLI: Slave1 set to %s:%d", 
               config->slave1_ip, config->slave1_port);
    return SYS_OK;
}

SysError_t cmd_irrig_config_slave2(int argc, char* argv[]) {
    if (argc < 3) {
        Serial.println("Usage: irrig_config_slave2 <ip> <port>");
        Serial.println("Example: irrig_config_slave2 192.168.1.103 8082");
        return SYS_INVALID_PARAM;
    }
    
    IrrigCommConfig_t* config = irrig_comm_get_config();
    
    strncpy(config->slave2_ip, argv[1], sizeof(config->slave2_ip) - 1);
    config->slave2_ip[sizeof(config->slave2_ip) - 1] = '\0';
    config->slave2_port = atoi(argv[2]);
    
    Serial.printf("Slave2 (Relays) configured: %s:%d\n", config->slave2_ip, config->slave2_port);
    Serial.println("Use 'irrig_config_save' to persist configuration");
    
    kernel_log(LOG_LEVEL_INFO, "IrrigCLI: Slave2 set to %s:%d", 
               config->slave2_ip, config->slave2_port);
    return SYS_OK;
}

SysError_t cmd_irrig_config_show(int argc, char* argv[]) {
    IrrigCommConfig_t* config = irrig_comm_get_config();
    
    Serial.println("=== Irrigation System Configuration ===");
    Serial.println();
    Serial.println("Server (FastAPI):");
    Serial.printf("  URL: %s\n", g_server_url_configured ? g_server_url : "NOT CONFIGURED");
    Serial.println();
    Serial.println("Master Device:");
    Serial.printf("  IP: %s\n", config->master_ip);
    Serial.printf("  Port: %d\n", config->master_port);
    Serial.println();
    Serial.println("Slave1 (Sensors):");
    Serial.printf("  IP: %s\n", config->slave1_ip);
    Serial.printf("  Port: %d\n", config->slave1_port);
    Serial.println();
    Serial.println("Slave2 (Relays):");
    Serial.printf("  IP: %s\n", config->slave2_ip);
    Serial.printf("  Port: %d\n", config->slave2_port);
    Serial.println();
    Serial.println("HTTP Settings:");
    Serial.printf("  Timeout: %d ms\n", config->http_timeout_ms);
    Serial.printf("  Retry count: %d\n", config->retry_count);
    Serial.printf("  Retry delay: %d ms\n", config->retry_delay_ms);
    Serial.println("=======================================");
    
    return SYS_OK;
}

SysError_t cmd_irrig_config_save(int argc, char* argv[]) {
    Serial.println("Saving configuration to NVS...");
    
    if (save_config_to_nvs()) {
        Serial.println("Configuration saved successfully!");
        Serial.println("Configuration will persist across reboots.");
        return SYS_OK;
    } else {
        Serial.println("ERROR: Failed to save configuration");
        return SYS_ERROR;
    }
}

SysError_t cmd_irrig_config_load(int argc, char* argv[]) {
    Serial.println("Loading configuration from NVS...");
    
    if (load_config_from_nvs()) {
        Serial.println("Configuration loaded successfully!");
        cmd_irrig_config_show(0, NULL);
        return SYS_OK;
    } else {
        Serial.println("No saved configuration found or load failed");
        Serial.println("Use 'irrig_config_show' to see current configuration");
        return SYS_ERROR;
    }
}

SysError_t cmd_irrig_config_reset(int argc, char* argv[]) {
    Serial.println("Resetting configuration to defaults...");
    
    IrrigCommConfig_t* config = irrig_comm_get_config();
    
    // Valeurs par défaut
    strcpy(config->master_ip, "192.168.1.101");
    config->master_port = 8080;
    strcpy(config->slave1_ip, "192.168.1.102");
    config->slave1_port = 8081;
    strcpy(config->slave2_ip, "192.168.1.103");
    config->slave2_port = 8082;
    strcpy(g_server_url, "http://192.168.1.100:8000");
    g_server_url_configured = false;
    
    config->http_timeout_ms = 5000;
    config->retry_count = 3;
    config->retry_delay_ms = 1000;
    
    Serial.println("Configuration reset to defaults!");
    Serial.println("Use 'irrig_config_save' to persist defaults");
    cmd_irrig_config_show(0, NULL);
    
    kernel_log(LOG_LEVEL_INFO, "IrrigCLI: Configuration reset to defaults");
    return SYS_OK;
}

// ===== COMMANDES DE GESTION DU RÔLE =====

SysError_t cmd_irrig_set_role(int argc, char* argv[]) {
    if (argc < 2) {
        Serial.println("Usage: irrig_set_role <master|slave1|slave2>");
        Serial.println("Example: irrig_set_role master");
        return SYS_INVALID_PARAM;
    }
    
    DeviceRole_t new_role = DEVICE_ROLE_NONE;
    
    if (strcmp(argv[1], "master") == 0) {
        new_role = DEVICE_ROLE_MASTER;
    } else if (strcmp(argv[1], "slave1") == 0) {
        new_role = DEVICE_ROLE_SLAVE1;
    } else if (strcmp(argv[1], "slave2") == 0) {
        new_role = DEVICE_ROLE_SLAVE2;
    } else {
        Serial.printf("ERROR: Invalid role '%s'\n", argv[1]);
        Serial.println("Valid roles: master, slave1, slave2");
        return SYS_INVALID_PARAM;
    }
    
    g_device_role = new_role;
    
    const char* role_name = (new_role == DEVICE_ROLE_MASTER) ? "MASTER" :
                           (new_role == DEVICE_ROLE_SLAVE1) ? "SLAVE1 (Sensors)" :
                           (new_role == DEVICE_ROLE_SLAVE2) ? "SLAVE2 (Relays)" : "NONE";
    
    Serial.printf("Device role set to: %s\n", role_name);
    Serial.println("Use 'irrig_config_save' to persist this configuration");
    Serial.println("Use 'irrig_activate_role' to start the corresponding app");
    
    kernel_log(LOG_LEVEL_INFO, "IrrigCLI: Device role set to %s", role_name);
    return SYS_OK;
}

SysError_t cmd_irrig_get_role(int argc, char* argv[]) {
    const char* role_name = (g_device_role == DEVICE_ROLE_MASTER) ? "MASTER" :
                           (g_device_role == DEVICE_ROLE_SLAVE1) ? "SLAVE1 (Sensors)" :
                           (g_device_role == DEVICE_ROLE_SLAVE2) ? "SLAVE2 (Relays)" : "NOT CONFIGURED";
    
    Serial.println("=== Device Role Configuration ===");
    Serial.printf("Current role: %s\n", role_name);
    
    if (g_device_role != DEVICE_ROLE_NONE) {
        uint8_t target_app_id = (g_device_role == DEVICE_ROLE_MASTER) ? g_master_app_id :
                                (g_device_role == DEVICE_ROLE_SLAVE1) ? g_slave1_app_id :
                                g_slave2_app_id;
        Serial.printf("Target app ID: %d\n", target_app_id);
    }
    
    Serial.println("=================================");
    return SYS_OK;
}

SysError_t cmd_irrig_activate_role(int argc, char* argv[]) {
    if (g_device_role == DEVICE_ROLE_NONE) {
        Serial.println("ERROR: Device role not configured!");
        Serial.println("Use 'irrig_set_role <master|slave1|slave2>' first");
        return SYS_ERROR;
    }
    
    uint8_t target_app_id = 0;
    const char* role_name = "";
    
    switch (g_device_role) {
        case DEVICE_ROLE_MASTER:
            target_app_id = g_master_app_id;
            role_name = "MASTER";
            break;
        case DEVICE_ROLE_SLAVE1:
            target_app_id = g_slave1_app_id;
            role_name = "SLAVE1 (Sensors)";
            break;
        case DEVICE_ROLE_SLAVE2:
            target_app_id = g_slave2_app_id;
            role_name = "SLAVE2 (Relays)";
            break;
        default:
            Serial.println("ERROR: Invalid role");
            return SYS_ERROR;
    }
    
    if (target_app_id == 0) {
        Serial.printf("ERROR: App for role %s not registered!\n", role_name);
        Serial.println("Make sure all apps are registered in main.cpp");
        return SYS_ERROR;
    }
    
    Serial.printf("Activating role: %s (App ID: %d)\n", role_name, target_app_id);
    
    // Arrêter toutes les autres apps
    if (g_master_app_id != 0 && g_master_app_id != target_app_id) {
        app_stop(g_master_app_id);
    }
    if (g_slave1_app_id != 0 && g_slave1_app_id != target_app_id) {
        app_stop(g_slave1_app_id);
    }
    if (g_slave2_app_id != 0 && g_slave2_app_id != target_app_id) {
        app_stop(g_slave2_app_id);
    }
    
    // Démarrer l'app cible
    SysError_t result = app_start(target_app_id);
    
    if (result == SYS_OK) {
        Serial.printf("SUCCESS: %s app started\n", role_name);
        kernel_log(LOG_LEVEL_INFO, "IrrigCLI: Activated role %s", role_name);
    } else {
        Serial.printf("ERROR: Failed to start %s app (error: %d)\n", role_name, result);
        kernel_log(LOG_LEVEL_ERROR, "IrrigCLI: Failed to activate role %s", role_name);
    }
    
    return result;
}

SysError_t cmd_irrig_test_all(int argc, char* argv[]) {
    Serial.println("=== TEST MODE: Starting ALL apps ===");
    kernel_log(LOG_LEVEL_WARN, "IrrigCLI: TEST MODE - Starting all 3 apps");
    
    bool all_started = true;
    
    // Démarrer Master
    if (g_master_app_id != 0) {
        Serial.printf("Starting Master (ID: %d)...\n", g_master_app_id);
        if (app_start(g_master_app_id) == SYS_OK) {
            Serial.println("✓ Master started");
        } else {
            Serial.println("✗ Master failed");
            all_started = false;
        }
    }
    
    // Démarrer Slave1
    if (g_slave1_app_id != 0) {
        Serial.printf("Starting Slave1 (ID: %d)...\n", g_slave1_app_id);
        if (app_start(g_slave1_app_id) == SYS_OK) {
            Serial.println("✓ Slave1 started");
        } else {
            Serial.println("✗ Slave1 failed");
            all_started = false;
        }
    }
    
    // Démarrer Slave2
    if (g_slave2_app_id != 0) {
        Serial.printf("Starting Slave2 (ID: %d)...\n", g_slave2_app_id);
        if (app_start(g_slave2_app_id) == SYS_OK) {
            Serial.println("✓ Slave2 started");
        } else {
            Serial.println("✗ Slave2 failed");
            all_started = false;
        }
    }
    
    Serial.println("===================================");
    if (all_started) {
        Serial.println("✓ All apps started successfully!");
        Serial.println("⚠️  WARNING: This is for testing only!");
        Serial.println("   GPIO conflicts may occur.");
        kernel_log(LOG_LEVEL_WARN, "IrrigCLI: All apps started (TEST MODE)");
        return SYS_OK;
    } else {
        Serial.println("✗ Some apps failed to start");
        kernel_log(LOG_LEVEL_ERROR, "IrrigCLI: Failed to start all apps");
        return SYS_ERROR;
    }
}

// ===== FONCTION D'ACCÈS POUR AUTRES MODULES =====

const char* irrig_cli_get_server_url(void) {
    return g_server_url_configured ? g_server_url : NULL;
}

bool irrig_cli_is_server_configured(void) {
    return g_server_url_configured;
}

void irrig_cli_set_app_ids(uint8_t master_id, uint8_t slave1_id, uint8_t slave2_id) {
    g_master_app_id = master_id;
    g_slave1_app_id = slave1_id;
    g_slave2_app_id = slave2_id;
    kernel_log(LOG_LEVEL_INFO, "IrrigCLI: App IDs registered (Master:%d, Slave1:%d, Slave2:%d)",
               master_id, slave1_id, slave2_id);
}

DeviceRole_t irrig_cli_get_device_role(void) {
    return g_device_role;
}

bool irrig_cli_auto_activate_role(void) {
    if (g_device_role == DEVICE_ROLE_NONE) {
        return false;
    }
    
    SysError_t result = cmd_irrig_activate_role(0, NULL);
    return (result == SYS_OK);
}
