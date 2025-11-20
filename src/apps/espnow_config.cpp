#include "espnow_config.h"
#include "kernel/core/log_system_optimized.h"
#include <Preferences.h>
#include <esp_mac.h>

static const char* NVS_NAMESPACE = "espnow_cfg";
static DeviceConfig_t g_device_config = {0};
static bool g_config_loaded = false;

// Générer device_id basé sur MAC address
static uint16_t generate_device_id() {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    return (mac[4] << 8) | mac[5];
}

SysError_t espnow_config_load(DeviceConfig_t* config) {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, true);  // read-only
    
    if (!prefs.isKey("device_id")) {
        kernel_log(LOG_LEVEL_WARN, "Config not found in NVS, initializing with defaults");
        prefs.end();
        
        // Générer device_id depuis MAC
        config->device_id = generate_device_id();
        config->master_id = 1;      // Défaut
        config->device_index = 0;   // Défaut (master)
        config->config_version = 1;
        
        // Sauvegarder
        return espnow_config_save(config);
    }
    
    config->device_id = prefs.getUShort("device_id");
    config->master_id = prefs.getUChar("master_id");
    config->device_index = prefs.getUChar("device_index");
    config->config_version = prefs.getUInt("config_version");
    
    prefs.end();
    
    kernel_log(LOG_LEVEL_INFO, "Config loaded: device_id=0x%04X, master_id=%d, device_index=%d",
              config->device_id, config->master_id, config->device_index);
    
    return SYS_OK;
}

SysError_t espnow_config_save(const DeviceConfig_t* config) {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);  // read-write
    
    prefs.putUShort("device_id", config->device_id);
    prefs.putUChar("master_id", config->master_id);
    prefs.putUChar("device_index", config->device_index);
    prefs.putUInt("config_version", config->config_version);
    
    prefs.end();
    
    kernel_log(LOG_LEVEL_INFO, "Config saved: device_id=0x%04X, master_id=%d, device_index=%d",
              config->device_id, config->master_id, config->device_index);
    
    return SYS_OK;
}

SysError_t espnow_config_set_master_id(uint8_t new_master_id) {
    if (new_master_id == 0 || new_master_id > 255) {
        kernel_log(LOG_LEVEL_ERROR, "Invalid master_id: %d (must be 1-255)", new_master_id);
        return SYS_ERROR;
    }
    
    DeviceConfig_t config;
    if (espnow_config_load(&config) != SYS_OK) {
        return SYS_ERROR;
    }
    
    uint8_t old_master_id = config.master_id;
    config.master_id = new_master_id;
    
    if (espnow_config_save(&config) != SYS_OK) {
        return SYS_ERROR;
    }
    
    kernel_log(LOG_LEVEL_INFO, "Master ID changed: %d → %d (restart required)",
              old_master_id, new_master_id);
    
    return SYS_OK;
}

SysError_t espnow_config_set_device_index(uint8_t new_index) {
    if (new_index > 254) {
        kernel_log(LOG_LEVEL_ERROR, "Invalid device_index: %d (must be 0-254)", new_index);
        return SYS_ERROR;
    }
    
    DeviceConfig_t config;
    if (espnow_config_load(&config) != SYS_OK) {
        return SYS_ERROR;
    }
    
    uint8_t old_index = config.device_index;
    config.device_index = new_index;
    
    if (espnow_config_save(&config) != SYS_OK) {
        return SYS_ERROR;
    }
    
    kernel_log(LOG_LEVEL_INFO, "Device index changed: %d → %d (restart required)",
              old_index, new_index);
    
    return SYS_OK;
}

SysError_t espnow_config_reset() {
    DeviceConfig_t config;
    
    // Générer device_id depuis MAC
    config.device_id = generate_device_id();
    config.master_id = 1;
    config.device_index = 0;
    config.config_version = 1;
    
    if (espnow_config_save(&config) != SYS_OK) {
        return SYS_ERROR;
    }
    
    kernel_log(LOG_LEVEL_INFO, "Config reset to defaults");
    
    return SYS_OK;
}

const DeviceConfig_t* espnow_config_get() {
    if (!g_config_loaded) {
        if (espnow_config_load(&g_device_config) != SYS_OK) {
            kernel_log(LOG_LEVEL_ERROR, "Failed to load config");
            return nullptr;
        }
        g_config_loaded = true;
    }
    return &g_device_config;
}
