// ===== CONFIGURATION DEVICE 1 : MASTER =====
// Ce fichier remplace src/main.cpp pour le Device 1 (Master)
// Compilation : Copier ce fichier vers src/main.cpp avant compilation

#include "kernel/core/system_types.h"
#include "kernel/core/log_system_optimized.h"
#include "kernel/app/app_manager.h"
#include "kernel/network/wifi_manager.h"
#include "kernel/network/ntp_manager.h"
#include "kernel/hal/time_sync_manager.h"
#include "apps/irrig_app_master/irrig_app_master.h"
#include "apps/irrig_app_master/irrig_app_master_http.h"
#include "apps/irrig_common/irrig_communication.h"
#include <Arduino.h>

// ===== CONFIGURATION RÉSEAU =====

#define DEVICE_ROLE "MASTER"
#define DEVICE_IP "192.168.1.100"      // IP fixe du Master
#define DEVICE_GATEWAY "192.168.1.1"
#define DEVICE_SUBNET "255.255.255.0"

#define SLAVE1_IP "192.168.1.101"      // IP du Slave Sensors
#define SLAVE1_PORT 8081

#define SLAVE2_IP "192.168.1.102"      // IP du Slave Relays
#define SLAVE2_PORT 8082

#define MASTER_HTTP_PORT 8080          // Port serveur HTTP du Master

// ===== VARIABLES GLOBALES =====

static bool system_initialized = false;
static bool system_running = false;

// Données capteurs reçues de Slave 1
static float g_received_moisture[12];
static float g_received_temperature = 0;
static float g_received_humidity = 0;
static float g_received_pressure = 0;

// ===== CALLBACK DONNÉES CAPTEURS =====

void on_sensor_data_received(SensorDataPacket_t* data) {
    if (!data) return;
    
    kernel_log(LOG_LEVEL_DEBUG, "Master: Received sensor data from Slave1");
    
    // Mettre à jour données globales
    for (int i = 0; i < 12; i++) {
        g_received_moisture[i] = data->moisture[i];
    }
    g_received_temperature = data->temperature;
    g_received_humidity = data->humidity;
    g_received_pressure = data->pressure;
    
    // Transmettre au Master pour traitement
    updateSensorDataFromSlave(data->moisture, data->temperature, data->humidity, data->pressure);
}

// ===== CALLBACK STATUT IRRIGATION =====

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
    
    Serial.println("=== D'O-Core Init - DEVICE 1 (MASTER) ===");
    
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
    
    // Configuration IP fixe
    IPAddress local_IP, gateway, subnet;
    local_IP.fromString(DEVICE_IP);
    gateway.fromString(DEVICE_GATEWAY);
    subnet.fromString(DEVICE_SUBNET);
    
    if (!WiFi.config(local_IP, gateway, subnet)) {
        Serial.println("Failed to configure static IP");
    }
    
    // Connexion WiFi
    if (load_wifi_credentials()) {
        Serial.printf("Connecting to WiFi: %s\n", get_stored_ssid());
        if (connect_to_wifi(get_stored_ssid(), get_stored_password()) == SYS_OK) {
            Serial.println("WiFi connected");
            Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
        }
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
    
    IrrigCommConfig_t comm_config = {
        .master_ip = DEVICE_IP,
        .master_port = MASTER_HTTP_PORT,
        .slave1_ip = SLAVE1_IP,
        .slave1_port = SLAVE1_PORT,
        .slave2_ip = SLAVE2_IP,
        .slave2_port = SLAVE2_PORT,
        .http_timeout_ms = 5000,
        .retry_count = 3,
        .retry_delay_ms = 1000
    };
    
    irrig_comm_init(&comm_config);
    
    // ===== CONFIGURATION MASTER =====
    
    IrrigAppConfig_t master_config;
    strcpy(master_config.server_url, "http://10.232.133.53:3000");
    strcpy(master_config.device_id, "ESP32_IRRIGATION_MASTER_001");
    strcpy(master_config.device_secret, "esp32-secure-key-2024");
    master_config.poll_interval_seconds = 10;
    master_config.sensor_read_interval_seconds = 5;
    master_config.data_send_interval_seconds = 15;
    master_config.max_zones = 4;
    master_config.max_sensors = 12;
    master_config.simulation_mode = false;  // Mode production
    
    // Enregistrer Master
    if (register_irrig_app_master(&master_config) == SYS_OK) {
        Serial.println("Master app registered");
    }
    
    // Initialiser serveur HTTP du Master
    master_http_init(MASTER_HTTP_PORT);
    master_http_set_sensor_callback(on_sensor_data_received);
    master_http_set_status_callback(on_irrigation_status_received);
    
    // Initialiser interface
    interface_init();
    
    system_initialized = true;
    system_running = true;
    
    // Créer tâches
    uint8_t task_id;
    task_create_pinned_to_core("SystemMain", system_main_task, NULL, PRIORITY_NORMAL, STACK_SIZE_SMALL, 0, &task_id);
    task_create_pinned_to_core("WiFiSupervision", wifi_supervision_task, NULL, PRIORITY_LOW, STACK_SIZE_SMALL, 0, &task_id);
    task_create_pinned_to_core("TimeSync", time_sync_task, NULL, PRIORITY_LOW, STACK_SIZE_SMALL, 0, &task_id);
    
    Serial.println("=== D'O-Core Ready - MASTER ===");
    Serial.printf("IP: %s:%d\n", DEVICE_IP, MASTER_HTTP_PORT);
    Serial.printf("Slave1: %s:%d\n", SLAVE1_IP, SLAVE1_PORT);
    Serial.printf("Slave2: %s:%d\n", SLAVE2_IP, SLAVE2_PORT);
    
    // Démarrer shell
    interface_start();
}

void loop() {
    // Gérer requêtes HTTP
    master_http_handle_requests();
    
    // Boucle App Manager
    app_manager_loop();
    
    delay(10);
}
