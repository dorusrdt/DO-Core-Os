// ===== CONFIGURATION DEVICE 3 : SLAVE RELAYS =====
// Ce fichier remplace src/main.cpp pour le Device 3 (Slave Relays)
// Compilation : Copier ce fichier vers src/main.cpp avant compilation

#include "kernel/core/system_types.h"
#include "kernel/core/log_system_optimized.h"
#include "kernel/app/app_manager.h"
#include "kernel/network/wifi_manager.h"
#include "kernel/network/ntp_manager.h"
#include "kernel/hal/time_sync_manager.h"
#include "apps/irrig_app_slave_relays/irrig_app_slave_relays.h"
#include "apps/irrig_common/irrig_communication.h"
#include <Arduino.h>

// ===== CONFIGURATION RÉSEAU =====

#define DEVICE_ROLE "SLAVE_RELAYS"
#define DEVICE_IP "192.168.1.102"      // IP fixe du Slave Relays
#define DEVICE_GATEWAY "192.168.1.1"
#define DEVICE_SUBNET "255.255.255.0"

#define MASTER_IP "192.168.1.100"      // IP du Master
#define MASTER_PORT 8080

#define SLAVE2_HTTP_PORT 8082          // Port serveur HTTP de ce Slave

// ===== VARIABLES GLOBALES =====

static bool system_initialized = false;
static bool system_running = false;

// ===== SETUP =====

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("=== D'O-Core Init - DEVICE 3 (SLAVE RELAYS) ===");
    
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
    
    // Initialiser Time Sync
    time_sync_init();
    time_sync_automatic();
    
    // Initialiser App Manager
    app_manager_init();
    
    // ===== CONFIGURATION COMMUNICATION HTTP =====
    
    IrrigCommConfig_t comm_config = {
        .master_ip = MASTER_IP,
        .master_port = MASTER_PORT,
        .slave1_ip = "",  // Non utilisé par Slave2
        .slave1_port = 0,
        .slave2_ip = DEVICE_IP,
        .slave2_port = SLAVE2_HTTP_PORT,
        .http_timeout_ms = 5000,
        .retry_count = 3,
        .retry_delay_ms = 1000
    };
    
    irrig_comm_init(&comm_config);
    
    // ===== CONFIGURATION SLAVE RELAYS =====
    
    IrrigRelayConfig_t relay_config = {
        .safety_timeout_ms = 300000,       // 5 minutes max par irrigation
        .enable_http_server = true,        // Exposer serveur HTTP
        .http_server_port = SLAVE2_HTTP_PORT,
        .status_publish_interval_ms = 10000  // Publier statut toutes les 10s
    };
    
    // Enregistrer Slave Relays
    if (register_irrig_app_slave_relays(&relay_config) == SYS_OK) {
        Serial.println("Slave Relays app registered");
    }
    
    // Initialiser interface
    interface_init();
    
    system_initialized = true;
    system_running = true;
    
    // Créer tâches
    uint8_t task_id;
    task_create_pinned_to_core("SystemMain", system_main_task, NULL, PRIORITY_NORMAL, STACK_SIZE_SMALL, 0, &task_id);
    task_create_pinned_to_core("WiFiSupervision", wifi_supervision_task, NULL, PRIORITY_LOW, STACK_SIZE_SMALL, 0, &task_id);
    task_create_pinned_to_core("TimeSync", time_sync_task, NULL, PRIORITY_LOW, STACK_SIZE_SMALL, 0, &task_id);
    
    Serial.println("=== D'O-Core Ready - SLAVE RELAYS ===");
    Serial.printf("IP: %s:%d\n", DEVICE_IP, SLAVE2_HTTP_PORT);
    Serial.printf("Master: %s:%d\n", MASTER_IP, MASTER_PORT);
    Serial.println("Hardware: 4 zone relays + pump relay");
    
    // Démarrer shell
    interface_start();
}

void loop() {
    // Boucle App Manager
    app_manager_loop();
    
    delay(10);
}
