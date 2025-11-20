#include "espnow_simple.h"
#include "../../kernel/app/app_manager.h"
#include "../../kernel/core/log_system_optimized.h"
#include <esp_mac.h>

static uint8_t simple_app_id = ESPNOW_SIMPLE_APP_ID;

// ============ App Lifecycle ============

void simple_app_start(void) {
    kernel_log(LOG_LEVEL_INFO, "ESP-NOW Simple: App starting");

    // Afficher l'adresse MAC
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    kernel_log(LOG_LEVEL_INFO, "ESP-NOW Simple: Device MAC Address: %02X:%02X:%02X:%02X:%02X:%02X",
              mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    Serial.printf("\n");
    Serial.printf("========================================\n");
    Serial.printf("ESP-NOW Simple: Device MAC Address\n");
    Serial.printf("========================================\n");
    Serial.printf("MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    Serial.printf("========================================\n");
    Serial.printf("\n");

    kernel_log(LOG_LEVEL_INFO, "ESP-NOW Simple: MAC address displayed");
}

void simple_app_stop(void) {
    kernel_log(LOG_LEVEL_INFO, "ESP-NOW Simple: App stopped");
}

SysError_t espnow_simple_register_app() {
    AppCallbacks_t callbacks = {0};
    callbacks.start = simple_app_start;
    callbacks.stop = simple_app_stop;

    uint8_t app_id;
    return app_register("espnow_simple", "ESP-NOW Simple - Display MAC Address",
                       APP_TYPE_USER, &callbacks, &app_id);
}

