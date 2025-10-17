// ===== FIRMWARE UNIVERSEL : TOUS LES DEVICES =====
// Ce fichier remplace src/main.cpp pour créer un firmware universel
// Les 3 apps sont enregistrées, le rôle est choisi via CLI
// Compilation : Copier ce fichier vers src/main.cpp avant compilation

#include "kernel/core/system_types.h"
#include "kernel/core/log_system_optimized.h"
#include "kernel/app/app_manager.h"
#include "kernel/network/wifi_manager.h"
#include "kernel/network/ntp_manager.h"
#include "kernel/hal/time_sync_manager.h"
#include "apps/irrig_app_master/irrig_app_master.h"
#include "apps/irrig_app_master/irrig_app_master_http.h"
#include "apps/irrig_app_slave_sensors/irrig_app_slave_sensors.h"
#include "apps/irrig_app_slave_relays/irrig_app_slave_relays.h"
#include "apps/irrig_common/irrig_communication.h"
#include "apps/irrig_common/irrig_cli_commands.h"
#include <Arduino.h>

// ===== CONFIGURATION RÉSEAU PAR DÉFAUT =====

#define DEFAULT_MASTER_IP "192.168.1.100"
#define DEFAULT_MASTER_PORT 8080
#define DEFAULT_SLAVE1_IP "192.168.1.101"
#define DEFAULT_SLAVE1_PORT 8081
#define DEFAULT_SLAVE2_IP "192.168.1.102"
#define DEFAULT_SLAVE2_PORT 8082

// ===== VARIABLES GLOBALES =====

static bool system_initialized = false;
static bool system_running = false;

// IDs des apps enregistrées
static uint8_t g_master_app_id = 0;
static uint8_t g_slave1_app_id = 0;
static uint8_t g_slave2_app_id = 0;

// Données capteurs reçues (pour Master)
static float g_received_moisture[12];
static float g_received_temperature = 0;
static float g_received_humidity = 0;
static float g_received_pressure = 0;

// ===== CALLBACKS POUR MASTER =====

void on_sensor_data_received(SensorDataPacket_t* data) {
    if (!data) return;
    
    kernel_log(LOG_LEVEL_DEBUG, "Master: Received sensor data from Slave1");
    
    for (int i = 0; i < 12; i++) {
        g_received_moisture[i] = data->moisture[i];
    }
    g_received_temperature = data->temperature;
    g_received_humidity = data->humidity;
    g_received_pressure = data->pressure;
    
    updateSensorDataFromSlave(data->moisture, data->temperature, data->humidity, data->pressure);
}

void on_irrigation_status_received(IrrigationStatusPacket_t* status) {
    if (!status) return;
    
    kernel_log(LOG_LEVEL_DEBUG, "Master: Received irrigation status from Slave2");
    kernel_log(LOG_LEVEL_DEBUG, "  Zone: %d, Irrigating: %s, Remaining: %lus",
               status->zone_id, status->is_irrigating ? "YES" : "NO", status->remaining_seconds);
}

// ===== SETUP =====

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("=== D'O-Core Init - UNIVERSAL FIRMWARE ===");
    
    // Initialiser NVS
    esp_err_t nvs_ret = nvs_flash_init();
    if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES || nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_ret);
    
    // Initialiser composants DO-Core
    task_manager_init();
    memory_manager_init();
    log_system_init();
    system_monitor_init();
    system_monitor_start();
    
    // Initialiser WiFi
    wifi_manager_init(nullptr);
    WiFi.mode(WIFI_STA);
    
    // Connexion WiFi (avec credentials sauvegardés)
    if (load_wifi_credentials()) {
        Serial.printf("Connecting to WiFi: %s\n", get_stored_ssid());
        if (connect_to_wifi(get_stored_ssid(), get_stored_password()) == SYS_OK) {
            Serial.println("WiFi connected");
            Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
        }
    } else {
        Serial.println("No WiFi credentials saved. Use 'wifi_save' command.");
    }
    
    // Initialiser NTP
    ntp_init();
    
    // Initialiser RTC
    rtc_manager_init();
    
    // Initialiser Time Sync
    time_sync_init();
    time_sync_automatic();
    
    // Initialiser App Manager
    app_manager_init();
    
    // ===== CONFIGURATION COMMUNICATION HTTP =====
    
    IrrigCommConfig_t comm_config;
    strcpy(comm_config.master_ip, DEFAULT_MASTER_IP);
    comm_config.master_port = DEFAULT_MASTER_PORT;
    strcpy(comm_config.slave1_ip, DEFAULT_SLAVE1_IP);
    comm_config.slave1_port = DEFAULT_SLAVE1_PORT;
    strcpy(comm_config.slave2_ip, DEFAULT_SLAVE2_IP);
    comm_config.slave2_port = DEFAULT_SLAVE2_PORT;
    comm_config.http_timeout_ms = 5000;
    comm_config.retry_count = 3;
    comm_config.retry_delay_ms = 1000;
    
    irrig_comm_init(&comm_config);
    
    // ===== ENREGISTRER LES 3 APPS =====
    
    Serial.println("Registering all irrigation apps...");
    
    // 1. Master App
    IrrigAppConfig_t master_config;
    strcpy(master_config.server_url, "http://10.232.133.53:3000");
    strcpy(master_config.device_id, "ESP32_IRRIGATION_MASTER_001");
    strcpy(master_config.device_secret, "esp32-secure-key-2024");
    master_config.poll_interval_seconds = 10;
    master_config.sensor_read_interval_seconds = 5;
    master_config.data_send_interval_seconds = 15;
    master_config.max_zones = 4;
    master_config.max_sensors = 12;
    master_config.simulation_mode = false;
    
    if (register_irrig_app_master(&master_config) == SYS_OK) {
        g_master_app_id = 1;  // Assume ID 1
        Serial.println("✓ Master app registered (ID: 1)");
    } else {
        Serial.println("✗ Failed to register Master app");
    }
    
    // Initialiser serveur HTTP du Master (toujours disponible)
    master_http_init(DEFAULT_MASTER_PORT);
    master_http_set_sensor_callback(on_sensor_data_received);
    master_http_set_status_callback(on_irrigation_status_received);
    
    // 2. Slave Sensors App
    IrrigSensorConfig_t sensor_config = {
        .simulation_mode = false,
        .read_interval_ms = 5000,
        .samples_per_read = 5,
        .enable_http_server = true,
        .http_server_port = DEFAULT_SLAVE1_PORT
    };
    
    if (register_irrig_app_slave_sensors(&sensor_config) == SYS_OK) {
        g_slave1_app_id = 2;  // Assume ID 2
        Serial.println("✓ Slave Sensors app registered (ID: 2)");
    } else {
        Serial.println("✗ Failed to register Slave Sensors app");
    }
    
    // 3. Slave Relays App
    IrrigRelayConfig_t relay_config = {
        .enable_http_server = true,
        .http_server_port = DEFAULT_SLAVE2_PORT,
        .safety_timeout_seconds = 3600,
        .enable_pump_control = true
    };
    
    if (register_irrig_app_slave_relays(&relay_config) == SYS_OK) {
        g_slave2_app_id = 3;  // Assume ID 3
        Serial.println("✓ Slave Relays app registered (ID: 3)");
    } else {
        Serial.println("✗ Failed to register Slave Relays app");
    }
    
    // Enregistrer les IDs dans le système CLI
    irrig_cli_set_app_ids(g_master_app_id, g_slave1_app_id, g_slave2_app_id);
    
    // Initialiser interface
    interface_init();
    
    // Charger configuration depuis NVS (y compris le rôle)
    Serial.println("Loading configuration from NVS...");
    cmd_irrig_config_load(0, NULL);
    
    system_initialized = true;
    system_running = true;
    
    // Créer tâches système
    uint8_t task_id;
    task_create_pinned_to_core("SystemMain", system_main_task, NULL, PRIORITY_NORMAL, STACK_SIZE_SMALL, 0, &task_id);
    task_create_pinned_to_core("WiFiSupervision", wifi_supervision_task, NULL, PRIORITY_LOW, STACK_SIZE_SMALL, 0, &task_id);
    task_create_pinned_to_core("TimeSync", time_sync_task, NULL, PRIORITY_LOW, STACK_SIZE_SMALL, 0, &task_id);
    
    Serial.println("=== D'O-Core Ready - UNIVERSAL FIRMWARE ===");
    Serial.println();
    Serial.println("📋 All apps registered:");
    Serial.println("  ID 1: IrrigAppMaster");
    Serial.println("  ID 2: IrrigAppSlaveSensors");
    Serial.println("  ID 3: IrrigAppSlaveRelays");
    Serial.println();
    Serial.println("🎯 To configure this device:");
    Serial.println("  1. Set role:    irrig_set_role <master|slave1|slave2>");
    Serial.println("  2. Activate:    irrig_activate_role");
    Serial.println("  3. Save config: irrig_config_save");
    Serial.println();
    Serial.println("📊 Check status:");
    Serial.println("  - app_list         : Show all apps");
    Serial.println("  - irrig_get_role   : Show current role");
    Serial.println("  - irrig_config_show: Show network config");
    Serial.println();
    
    // Activer automatiquement le rôle si configuré
    DeviceRole_t role = irrig_cli_get_device_role();
    if (role != DEVICE_ROLE_NONE) {
        const char* role_name = (role == DEVICE_ROLE_MASTER) ? "MASTER" :
                               (role == DEVICE_ROLE_SLAVE1) ? "SLAVE1" :
                               (role == DEVICE_ROLE_SLAVE2) ? "SLAVE2" : "UNKNOWN";
        Serial.printf("🚀 Auto-activating role: %s\n", role_name);
        irrig_cli_auto_activate_role();
    } else {
        Serial.println("⚠️  No role configured. Use 'irrig_set_role' to configure.");
    }
    
    Serial.println();
    
    // Démarrer shell
    interface_start();
}

void loop() {
    // Gérer requêtes HTTP du Master (toujours actif pour recevoir données)
    master_http_handle_requests();
    
    // Boucle App Manager
    app_manager_loop();
    
    delay(10);
}
