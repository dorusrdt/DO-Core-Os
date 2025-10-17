#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include "../core/kernel.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ElegantOTA.h>

// Configuration OTA
#define OTA_MANAGER_DEFAULT_PORT 3232  // Port OTA (différent du Master HTTP 8080)
#define OTA_MANAGER_DEFAULT_PATH "/update"
#define OTA_MANAGER_USERNAME_MAX_LENGTH 32
#define OTA_MANAGER_PASSWORD_MAX_LENGTH 64
#define OTA_MANAGER_HOSTNAME_MAX_LENGTH 32

// États OTA
typedef enum {
    OTA_STATE_IDLE = 0,
    OTA_STATE_STARTING,
    OTA_STATE_PROGRESS,
    OTA_STATE_SUCCESS,
    OTA_STATE_ERROR,
    OTA_STATE_DISABLED
} OtaState_t;

// Erreurs OTA
typedef enum {
    OTA_OK = 0,
    OTA_ERROR_INIT,
    OTA_ERROR_WIFI,
    OTA_ERROR_SERVER,
    OTA_ERROR_AUTH,
    OTA_ERROR_DISABLED,
    OTA_ERROR_ALREADY_RUNNING,
    OTA_ERROR_PARAM
} OtaError_t;

// Configuration OTA
typedef struct {
    bool enabled;
    uint16_t port;
    char hostname[OTA_MANAGER_HOSTNAME_MAX_LENGTH];
    char username[OTA_MANAGER_USERNAME_MAX_LENGTH];
    char password[OTA_MANAGER_PASSWORD_MAX_LENGTH];
    bool require_auth;
    bool auto_reboot;
    uint32_t version;
    uint32_t checksum;
} OtaConfig_t;

// Statistiques OTA
typedef struct {
    uint32_t total_updates;
    uint32_t successful_updates;
    uint32_t failed_updates;
    uint32_t last_update_timestamp;
    uint32_t total_bytes_uploaded;
    char last_version[32];
    OtaState_t current_state;
    uint8_t last_progress;
} OtaStats_t;

// Informations de version
typedef struct {
    char version[32];
    char build_date[32];
    char build_time[32];
    uint32_t firmware_size;
    char chip_model[32];
    uint32_t free_heap;
    uint32_t sketch_size;
    uint32_t free_sketch_space;
} OtaVersionInfo_t;

// Forward declarations pour les callbacks
void on_progress_callback(size_t current, size_t final);

// Classe OTA Manager
class OtaManager {
private:
    OtaConfig_t config;
    OtaStats_t stats;
    bool initialized;
    WebServer* server;
    SemaphoreHandle_t mutex;
    TaskHandle_t server_task_handle;
    
    // Fonctions privées
    uint32_t calculate_checksum(const void* data, size_t size);
    bool validate_checksum(const void* data, size_t size, uint32_t expected_checksum);
    static void server_task(void* parameter);
    void setup_callbacks();
    
    // Callbacks ElegantOTA
    static void on_start_callback();
    static void on_end_callback(bool success);
    
    // Déclarer les callbacks globaux comme friend
    friend void on_progress_callback(size_t current, size_t final);
    
public:
    // Constructeur/Destructeur
    OtaManager();
    ~OtaManager();
    
    // Initialisation et gestion
    OtaError_t init(void);
    OtaError_t deinit(void);
    bool is_initialized(void) const;
    
    // Contrôle OTA
    OtaError_t start(void);
    OtaError_t stop(void);
    bool is_running(void) const;
    
    // Configuration
    OtaError_t set_config(const OtaConfig_t* new_config);
    OtaError_t get_config(OtaConfig_t* config_out);
    OtaError_t reset_config(void);
    OtaError_t set_port(uint16_t port);
    OtaError_t set_hostname(const char* hostname);
    OtaError_t set_auth(const char* username, const char* password);
    OtaError_t enable_auth(bool enable);
    OtaError_t enable(bool enable = true);
    
    // Informations
    OtaVersionInfo_t get_version_info(void);
    OtaError_t get_stats(OtaStats_t* stats_out);
    OtaError_t reset_stats(void);
    void print_stats(void);
    void print_version_info(void);
    String get_ota_url(void);
    
    // Validation et diagnostic
    OtaError_t validate_config(void);
    void print_debug_info(void);
    
    // Persistance
    OtaError_t save_config_to_nvs(void);
    OtaError_t load_config_from_nvs(void);
};

// Instance globale
extern OtaManager ota_manager;

// Fonctions d'interface console
SysError_t cmd_ota_start(int argc, char* argv[]);
SysError_t cmd_ota_stop(int argc, char* argv[]);
SysError_t cmd_ota_status(int argc, char* argv[]);
SysError_t cmd_ota_config(int argc, char* argv[]);
SysError_t cmd_ota_info(int argc, char* argv[]);
SysError_t cmd_ota_stats(int argc, char* argv[]);
SysError_t cmd_ota_url(int argc, char* argv[]);

// Fonction d'initialisation du module OTA
SysError_t ota_manager_module_init(void);
void ota_manager_module_deinit(void);

// Fonctions utilitaires
const char* ota_state_to_string(OtaState_t state);
const char* ota_error_to_string(OtaError_t error);

#endif // OTA_MANAGER_H
