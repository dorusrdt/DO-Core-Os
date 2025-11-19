#include "irrig_cli_commands.h"
#include "irrig_communication.h"
#include "../../kernel/core/log_system_optimized.h"
#include "../../kernel/app/app_manager.h"
#include <Preferences.h>
#include <string.h>
#include <stdio.h>

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

// Convertir string MAC "XX:XX:XX:XX:XX:XX" en uint8_t[6]
static bool parse_mac_address(const char* mac_str, uint8_t* mac) {
    if (!mac_str || !mac) return false;

    int values[6];
    int count = sscanf(mac_str, "%02X:%02X:%02X:%02X:%02X:%02X",
                       &values[0], &values[1], &values[2],
                       &values[3], &values[4], &values[5]);

    if (count != 6) {
        // Essayer format sans deux-points
        count = sscanf(mac_str, "%02X%02X%02X%02X%02X%02X",
                       &values[0], &values[1], &values[2],
                       &values[3], &values[4], &values[5]);
    }

    if (count == 6) {
        for (int i = 0; i < 6; i++) {
            mac[i] = (uint8_t)values[i];
        }
        return true;
    }

    return false;
}

// Convertir uint8_t[6] en string MAC "XX:XX:XX:XX:XX:XX"
static void format_mac_address(const uint8_t* mac, char* buffer, size_t buffer_size) {
    if (!mac || !buffer || buffer_size < 18) return;

    snprintf(buffer, buffer_size, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// Sauvegarder configuration en NVS
static bool save_config_to_nvs(void) {
    Preferences prefs;
    if (!prefs.begin(IRRIG_NVS_NAMESPACE, false)) {
        kernel_log(LOG_LEVEL_ERROR, "IrrigCLI: Failed to open NVS");
        return false;
    }

    IrrigCommConfig_t* config = irrig_comm_get_config();

    // Note: MAC addresses ne sont plus sauvegardées (hardcodées dans main.cpp)
    // Seulement les paramètres ESP-NOW et serveur

    prefs.putUChar("wifi_channel", config->wifi_channel);
    prefs.putUShort("send_timeout_ms", config->send_timeout_ms);
    prefs.putUChar("retry_count", config->retry_count);
    prefs.putUShort("retry_delay_ms", config->retry_delay_ms);

    prefs.putString("server_url", g_server_url);
    prefs.putUChar("device_role", (uint8_t)g_device_role);

    prefs.end();
    kernel_log(LOG_LEVEL_INFO, "IrrigCLI: Configuration saved to NVS");
    return true;
}

// Charger configuration depuis NVS
// Note: MAC addresses ne sont plus chargées (hardcodées dans main.cpp)
static bool load_config_from_nvs(void) {
    Preferences prefs;
    if (!prefs.begin(IRRIG_NVS_NAMESPACE, true)) {
        kernel_log(LOG_LEVEL_WARN, "IrrigCLI: No saved config in NVS");
        return false;
    }

    IrrigCommConfig_t* config = irrig_comm_get_config();

    // Note: MAC addresses ne sont plus chargées (hardcodées dans main.cpp)
    // Seulement les paramètres ESP-NOW et serveur

    config->wifi_channel = prefs.getUChar("wifi_channel", 0);
    config->send_timeout_ms = prefs.getUShort("send_timeout_ms", 1000);
    config->retry_count = prefs.getUChar("retry_count", 3);
    config->retry_delay_ms = prefs.getUShort("retry_delay_ms", 100);

    String server_url = prefs.getString("server_url", "");
    if (server_url.length() > 0) {
        strncpy(g_server_url, server_url.c_str(), sizeof(g_server_url) - 1);
        g_server_url_configured = true;
    }

    g_device_role = (DeviceRole_t)prefs.getUChar("device_role", DEVICE_ROLE_NONE);

    prefs.end();
    kernel_log(LOG_LEVEL_INFO, "IrrigCLI: Configuration loaded from NVS (MAC addresses are hardcoded)");
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

// Note: cmd_irrig_config_master/slave1/slave2 supprimées
// Les MAC addresses sont maintenant codées en dur dans main.cpp
// Pour modifier les MAC, éditer directement main.cpp

SysError_t cmd_irrig_config_show(int argc, char* argv[]) {
    IrrigCommConfig_t* config = irrig_comm_get_config();
    char mac_str[18];

    Serial.println("=== Irrigation System Configuration ===");
    Serial.println();
    Serial.println("Server (FastAPI):");
    Serial.printf("  URL: %s\n", g_server_url_configured ? g_server_url : "NOT CONFIGURED");
    Serial.println();
    Serial.println("ESP-NOW MAC Addresses (HARDCODED in main.cpp):");
    Serial.println("  Note: To change MAC addresses, edit main.cpp directly");
    Serial.println();
    Serial.println("Master Device (ESP-NOW):");
    format_mac_address(config->master_mac, mac_str, sizeof(mac_str));
    Serial.printf("  MAC: %s (hardcoded)\n", mac_str);
    Serial.println();
    Serial.println("Slave1 (Sensors) (ESP-NOW):");
    format_mac_address(config->slave1_mac, mac_str, sizeof(mac_str));
    Serial.printf("  MAC: %s (hardcoded)\n", mac_str);
    Serial.println();
    Serial.println("Slave2 (Relays) (ESP-NOW):");
    format_mac_address(config->slave2_mac, mac_str, sizeof(mac_str));
    Serial.printf("  MAC: %s (hardcoded)\n", mac_str);
    Serial.println();
    Serial.println("ESP-NOW Settings:");
    Serial.printf("  WiFi Channel: %d (0=auto, will use WiFi channel if connected)\n", config->wifi_channel);
    Serial.printf("  Send Timeout: %d ms\n", config->send_timeout_ms);
    Serial.printf("  Retry count: %d\n", config->retry_count);
    Serial.printf("  Retry delay: %d ms\n", config->retry_delay_ms);

    // Afficher MAC locale
    uint8_t local_mac[6];
    irrig_comm_get_local_mac(local_mac);
    format_mac_address(local_mac, mac_str, sizeof(mac_str));
    Serial.printf("  Local MAC: %s\n", mac_str);

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
    Serial.println("Note: MAC addresses are hardcoded in main.cpp and cannot be reset via CLI");

    IrrigCommConfig_t* config = irrig_comm_get_config();

    // Note: MAC addresses ne sont plus réinitialisées (hardcodées dans main.cpp)
    // Seulement les paramètres ESP-NOW et serveur

    strcpy(g_server_url, "http://192.168.1.100:8000");
    g_server_url_configured = false;

    config->wifi_channel = 0;  // Auto
    config->send_timeout_ms = 1000;
    config->retry_count = 3;
    config->retry_delay_ms = 100;

    Serial.println("Configuration reset to defaults!");
    Serial.println("Use 'irrig_config_save' to persist defaults");
    cmd_irrig_config_show(0, NULL);

    kernel_log(LOG_LEVEL_INFO, "IrrigCLI: Configuration reset to defaults (MAC addresses unchanged)");
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
