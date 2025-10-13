#include "ota_manager.h"
#include "../core/log_system_optimized.h"
#include <Preferences.h>
#include <esp_ota_ops.h>

// Instance globale
OtaManager ota_manager;

// Instance statique pour callbacks
static OtaManager* g_ota_instance = nullptr;

// Constructeur
OtaManager::OtaManager() : initialized(false), server(nullptr), mutex(nullptr), server_task_handle(nullptr) {
    memset(&config, 0, sizeof(config));
    memset(&stats, 0, sizeof(stats));
    g_ota_instance = this;
}

// Destructeur
OtaManager::~OtaManager() {
    deinit();
}

// Calcul checksum
uint32_t OtaManager::calculate_checksum(const void* data, size_t size) {
    uint32_t checksum = 0;
    const uint8_t* ptr = (const uint8_t*)data;
    for (size_t i = 0; i < size; i++) {
        checksum += ptr[i];
    }
    return checksum;
}

// Validation checksum
bool OtaManager::validate_checksum(const void* data, size_t size, uint32_t expected_checksum) {
    return calculate_checksum(data, size) == expected_checksum;
}

// Callbacks ElegantOTA
void OtaManager::on_start_callback() {
    if (g_ota_instance) {
        kernel_log(LOG_LEVEL_INFO, "🔄 OTA Update Started");
        g_ota_instance->stats.current_state = OTA_STATE_STARTING;
    }
}

// Callback progression
void on_progress_callback(size_t current, size_t final) {
    if (!g_ota_instance) return;
    
    // Protection contre division par zéro
    if (final == 0) {
        kernel_log(LOG_LEVEL_WARN, "OTA Progress: final size is 0");
        return;
    }

    if (xSemaphoreTake(g_ota_instance->mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        uint8_t progress = (current * 100) / final;
        g_ota_instance->stats.last_progress = progress;
        g_ota_instance->stats.current_state = OTA_STATE_PROGRESS;

        if (progress % 10 == 0) {
            kernel_log(LOG_LEVEL_INFO, "📥 OTA Progress: %d%%", progress);
        }

        xSemaphoreGive(g_ota_instance->mutex);
    }
}

void OtaManager::on_end_callback(bool success) {
    if (g_ota_instance) {
        if (success) {
            kernel_log(LOG_LEVEL_INFO, "✅ OTA Update Successful!");
            g_ota_instance->stats.successful_updates++;
            g_ota_instance->stats.current_state = OTA_STATE_SUCCESS;
            g_ota_instance->stats.last_update_timestamp = millis() / 1000;
        } else {
            kernel_log(LOG_LEVEL_ERROR, "❌ OTA Update Failed!");
            g_ota_instance->stats.failed_updates++;
            g_ota_instance->stats.current_state = OTA_STATE_ERROR;
        }
        g_ota_instance->stats.total_updates++;
    }
}

// Setup callbacks
void OtaManager::setup_callbacks() {
    ElegantOTA.onStart(on_start_callback);
    ElegantOTA.onProgress(on_progress_callback);
    ElegantOTA.onEnd(on_end_callback);
}

// Tâche serveur web
void OtaManager::server_task(void* parameter) {
    OtaManager* manager = (OtaManager*)parameter;

    kernel_log(LOG_LEVEL_INFO, "OTA server task started");

    while (manager->is_running()) {
        if (manager->server) {
            manager->server->handleClient();
            ElegantOTA.loop();
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    kernel_log(LOG_LEVEL_INFO, "OTA server task stopped");
    vTaskDelete(NULL);
}

// Initialisation
OtaError_t OtaManager::init(void) {
    if (initialized) {
        return OTA_OK;
    }

    // Créer mutex
    mutex = xSemaphoreCreateMutex();
    if (!mutex) {
        kernel_log(LOG_LEVEL_ERROR, "Failed to create OTA mutex");
        return OTA_ERROR_INIT;
    }

    // Configuration par défaut
    reset_config();

    // Charger config depuis NVS
    load_config_from_nvs();

    initialized = true;
    stats.current_state = OTA_STATE_IDLE;

    kernel_log(LOG_LEVEL_INFO, "OTA Manager initialized");

    return OTA_OK;
}

// Désinitialisation
OtaError_t OtaManager::deinit(void) {
    if (!initialized) {
        return OTA_OK;
    }

    stop();

    if (mutex) {
        vSemaphoreDelete(mutex);
        mutex = nullptr;
    }

    initialized = false;
    kernel_log(LOG_LEVEL_INFO, "OTA Manager deinitialized");

    return OTA_OK;
}

// Vérifier si initialisé
bool OtaManager::is_initialized(void) const {
    return initialized;
}

// Démarrer OTA
OtaError_t OtaManager::start(void) {
    if (!initialized) {
        return OTA_ERROR_INIT;
    }

    if (!config.enabled) {
        kernel_log(LOG_LEVEL_WARN, "OTA is disabled");
        return OTA_ERROR_DISABLED;
    }

    if (WiFi.status() != WL_CONNECTED) {
        kernel_log(LOG_LEVEL_ERROR, "WiFi not connected");
        return OTA_ERROR_WIFI;
    }

    if (server != nullptr) {
        kernel_log(LOG_LEVEL_WARN, "OTA server already running");
        return OTA_ERROR_ALREADY_RUNNING;
    }

    xSemaphoreTake(mutex, portMAX_DELAY);

    // Créer serveur web
    server = new WebServer(config.port);
    if (!server) {
        xSemaphoreGive(mutex);
        kernel_log(LOG_LEVEL_ERROR, "Failed to create web server");
        return OTA_ERROR_SERVER;
    }

    // Configurer ElegantOTA
    ElegantOTA.begin(server);
    setup_callbacks();

    // Démarrer serveur
    server->begin();

    // Créer tâche serveur
    BaseType_t result = xTaskCreatePinnedToCore(
        server_task,
        "OTA_Server",
        4096,
        this,
        5,
        &server_task_handle,
        0
    );

    xSemaphoreGive(mutex);

    if (result != pdPASS) {
        delete server;
        server = nullptr;
        kernel_log(LOG_LEVEL_ERROR, "Failed to create OTA server task");
        return OTA_ERROR_SERVER;
    }

    stats.current_state = OTA_STATE_IDLE;

    kernel_log(LOG_LEVEL_INFO, "✅ OTA Server started on port %d", config.port);
    kernel_log(LOG_LEVEL_INFO, "🌐 OTA URL: http://%s:%d/update",
               WiFi.localIP().toString().c_str(), config.port);

    return OTA_OK;
}

// Arrêter OTA
OtaError_t OtaManager::stop(void) {
    if (!initialized || !server) {
        return OTA_OK;
    }

    xSemaphoreTake(mutex, portMAX_DELAY);

    // Arrêter tâche
    if (server_task_handle) {
        vTaskDelete(server_task_handle);
        server_task_handle = nullptr;
    }

    // Arrêter serveur
    if (server) {
        server->stop();
        delete server;
        server = nullptr;
    }

    stats.current_state = OTA_STATE_DISABLED;

    xSemaphoreGive(mutex);

    kernel_log(LOG_LEVEL_INFO, "OTA Server stopped");

    return OTA_OK;
}

// Vérifier si en cours
bool OtaManager::is_running(void) const {
    return server != nullptr && server_task_handle != nullptr;
}

// Configuration par défaut
OtaError_t OtaManager::reset_config(void) {
    memset(&config, 0, sizeof(config));

    config.enabled = true;
    config.port = OTA_MANAGER_DEFAULT_PORT;
    strcpy(config.hostname, "do-core-ota");
    strcpy(config.username, "admin");
    strcpy(config.password, "admin");
    config.require_auth = false;
    config.auto_reboot = true;
    config.version = 1;
    config.checksum = calculate_checksum(&config, sizeof(config) - sizeof(config.checksum));

    return OTA_OK;
}

// Obtenir URL OTA
String OtaManager::get_ota_url(void) {
    if (WiFi.status() != WL_CONNECTED) {
        return "WiFi not connected";
    }

    char url[128];
    snprintf(url, sizeof(url), "http://%s:%d/update",
             WiFi.localIP().toString().c_str(), config.port);
    return String(url);
}

// Obtenir infos version
OtaVersionInfo_t OtaManager::get_version_info(void) {
    OtaVersionInfo_t info;
    memset(&info, 0, sizeof(info));

    strcpy(info.version, DO_CORE_VERSION);
    strcpy(info.build_date, __DATE__);
    strcpy(info.build_time, __TIME__);
    strcpy(info.chip_model, "ESP32");  // Compatible avec toutes versions
    info.free_heap = ESP.getFreeHeap();
    info.sketch_size = ESP.getSketchSize();
    info.free_sketch_space = ESP.getFreeSketchSpace();
    info.firmware_size = ESP.getSketchSize();

    return info;
}

// Afficher infos version
void OtaManager::print_version_info(void) {
    OtaVersionInfo_t info = get_version_info();

    Serial.println("\n=== OTA Version Info ===");
    Serial.printf("Version: %s\n", info.version);
    Serial.printf("Build Date: %s %s\n", info.build_date, info.build_time);
    Serial.printf("Chip: %s\n", info.chip_model);
    Serial.printf("Free Heap: %lu bytes\n", info.free_heap);
    Serial.printf("Sketch Size: %lu bytes\n", info.sketch_size);
    Serial.printf("Free Space: %lu bytes\n", info.free_sketch_space);
    Serial.println("========================\n");
}

// Afficher stats
void OtaManager::print_stats(void) {
    Serial.println("\n=== OTA Statistics ===");
    Serial.printf("Total Updates: %lu\n", stats.total_updates);
    Serial.printf("Successful: %lu\n", stats.successful_updates);
    Serial.printf("Failed: %lu\n", stats.failed_updates);
    Serial.printf("State: %s\n", ota_state_to_string(stats.current_state));
    Serial.printf("Last Progress: %d%%\n", stats.last_progress);
    Serial.println("======================\n");
}

// Sauvegarder config NVS
OtaError_t OtaManager::save_config_to_nvs(void) {
    Preferences prefs;
    prefs.begin("ota", false);

    config.checksum = calculate_checksum(&config, sizeof(config) - sizeof(config.checksum));

    bool success = prefs.putBytes("config", &config, sizeof(config)) == sizeof(config);
    prefs.end();

    if (success) {
        kernel_log(LOG_LEVEL_INFO, "OTA config saved to NVS");
        return OTA_OK;
    } else {
        kernel_log(LOG_LEVEL_ERROR, "Failed to save OTA config");
        return OTA_ERROR_INIT;
    }
}

// Charger config NVS
OtaError_t OtaManager::load_config_from_nvs(void) {
    Preferences prefs;
    prefs.begin("ota", true);

    size_t len = prefs.getBytesLength("config");
    if (len == sizeof(config)) {
        prefs.getBytes("config", &config, sizeof(config));

        if (validate_checksum(&config, sizeof(config) - sizeof(config.checksum), config.checksum)) {
            kernel_log(LOG_LEVEL_INFO, "OTA config loaded from NVS");
            prefs.end();
            return OTA_OK;
        }
    }

    prefs.end();
    kernel_log(LOG_LEVEL_WARN, "No valid OTA config in NVS, using defaults");
    return OTA_ERROR_INIT;
}

// Fonctions utilitaires
const char* ota_state_to_string(OtaState_t state) {
    switch (state) {
        case OTA_STATE_IDLE: return "IDLE";
        case OTA_STATE_STARTING: return "STARTING";
        case OTA_STATE_PROGRESS: return "PROGRESS";
        case OTA_STATE_SUCCESS: return "SUCCESS";
        case OTA_STATE_ERROR: return "ERROR";
        case OTA_STATE_DISABLED: return "DISABLED";
        default: return "UNKNOWN";
    }
}

const char* ota_error_to_string(OtaError_t error) {
    switch (error) {
        case OTA_OK: return "OK";
        case OTA_ERROR_INIT: return "INIT_ERROR";
        case OTA_ERROR_WIFI: return "WIFI_ERROR";
        case OTA_ERROR_SERVER: return "SERVER_ERROR";
        case OTA_ERROR_AUTH: return "AUTH_ERROR";
        case OTA_ERROR_DISABLED: return "DISABLED";
        case OTA_ERROR_ALREADY_RUNNING: return "ALREADY_RUNNING";
        case OTA_ERROR_PARAM: return "PARAM_ERROR";
        default: return "UNKNOWN";
    }
}

// Commandes CLI
SysError_t cmd_ota_start(int argc, char* argv[]) {
    OtaError_t result = ota_manager.start();
    if (result == OTA_OK) {
        Serial.println("✅ OTA Server started");
        Serial.println(ota_manager.get_ota_url());
    } else {
        Serial.printf("❌ Failed to start OTA: %s\n", ota_error_to_string(result));
    }
    return SYS_OK;
}

SysError_t cmd_ota_stop(int argc, char* argv[]) {
    ota_manager.stop();
    Serial.println("OTA Server stopped");
    return SYS_OK;
}

SysError_t cmd_ota_status(int argc, char* argv[]) {
    Serial.printf("OTA Status: %s\n", ota_manager.is_running() ? "RUNNING" : "STOPPED");
    if (ota_manager.is_running()) {
        Serial.println(ota_manager.get_ota_url());
    }
    return SYS_OK;
}

SysError_t cmd_ota_info(int argc, char* argv[]) {
    ota_manager.print_version_info();
    return SYS_OK;
}

SysError_t cmd_ota_stats(int argc, char* argv[]) {
    ota_manager.print_stats();
    return SYS_OK;
}

SysError_t cmd_ota_url(int argc, char* argv[]) {
    if (ota_manager.is_running()) {
        Serial.println(ota_manager.get_ota_url());
    } else {
        Serial.println("OTA Server not running");
    }
    return SYS_OK;
}

// Initialisation module
SysError_t ota_manager_module_init(void) {
    OtaError_t result = ota_manager.init();
    return (result == OTA_OK) ? SYS_OK : SYS_ERROR;
}

void ota_manager_module_deinit(void) {
    ota_manager.deinit();
}
