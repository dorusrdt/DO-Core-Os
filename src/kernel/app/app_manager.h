#ifndef APP_MANAGER_H
#define APP_MANAGER_H

#include "../core/kernel.h"
#include "../core/minimal_config.h"

// Configuration optimisée pour la mémoire Flash
#define MAX_APPS 4
#define APP_NAME_MAX_LENGTH 16
#define APP_DESCRIPTION_MAX_LENGTH 32
#define APP_STACK_SIZE_DEFAULT 8192  // Taille par défaut de la pile pour les applications (augmenté pour éviter stack overflow)
#define APP_PRIORITY_DEFAULT 5

// États d'une application
typedef enum {
    APP_STATE_UNLOADED = 0,
    APP_STATE_LOADING,
    APP_STATE_RUNNING,
    APP_STATE_PAUSED,
    APP_STATE_STOPPED,
    APP_STATE_ERROR
} AppState_t;

// Types d'applications
typedef enum {
    APP_TYPE_SYSTEM = 0,
    APP_TYPE_USER,
    APP_TYPE_BACKGROUND
} AppType_t;

// Structure d'une application
typedef struct {
    uint8_t app_id;
    char name[APP_NAME_MAX_LENGTH];
    char description[APP_DESCRIPTION_MAX_LENGTH];
    AppState_t state;
    AppType_t type;
    uint32_t stack_size;
    uint32_t priority;
    uint32_t memory_used;
    uint32_t cpu_time;
    uint32_t start_time;
    uint32_t last_activity;
    bool auto_start;
    bool persistent;
} AppInfo_t;

// Callbacks d'application
typedef SysError_t (*AppInitCallback_t)(void);
typedef void (*AppStartCallback_t)(void);
typedef void (*AppStopCallback_t)(void);
typedef void (*AppPauseCallback_t)(void);
typedef void (*AppResumeCallback_t)(void);
typedef void (*AppLoopCallback_t)(void);

// Structure pour les callbacks d'application
typedef struct {
    AppInitCallback_t init;
    AppStartCallback_t start;
    AppStopCallback_t stop;
    AppPauseCallback_t pause;
    AppResumeCallback_t resume;
    AppLoopCallback_t loop;
} AppCallbacks_t;

// Structure complète d'une application
typedef struct {
    AppInfo_t info;
    AppCallbacks_t callbacks;
    TaskHandle_t task_handle;
    bool callbacks_loaded;
} Application_t;

// Fonctions principales
SysError_t app_manager_init(void);
void app_manager_deinit(void);

// Gestion des applications
SysError_t app_register(const char* name, const char* description, AppType_t type,
                       const AppCallbacks_t* callbacks, uint8_t* app_id);
SysError_t app_unregister(uint8_t app_id);
SysError_t app_start(uint8_t app_id);
SysError_t app_stop(uint8_t app_id);
SysError_t app_pause(uint8_t app_id);
SysError_t app_resume(uint8_t app_id);
SysError_t app_restart(uint8_t app_id);

// Informations et statistiques
uint8_t app_get_count(void);
bool app_exists(uint8_t app_id);
AppState_t app_get_state(uint8_t app_id);
SysError_t app_get_info(uint8_t app_id, AppInfo_t* info);
void app_manager_print_status(void);
void app_manager_print_all_apps(void);

// Gestion globale
SysError_t app_manager_start_all(void);
SysError_t app_manager_stop_all(void);
SysError_t app_manager_pause_all(void);
SysError_t app_manager_resume_all(void);

// Fonction de boucle à appeler dans le loop principal
void app_manager_loop(void);

// Fonctions utilitaires
uint32_t app_get_memory_usage(uint8_t app_id);
uint32_t app_get_cpu_time(uint8_t app_id);
bool app_is_running(uint8_t app_id);
bool app_is_paused(uint8_t app_id);

#ifdef __cplusplus
extern "C" {
#endif

// Fonctions C pour compatibilité
SysError_t app_manager_c_init(void);
uint8_t app_manager_c_get_count(void);
AppState_t app_manager_c_get_state(uint8_t app_id);

#ifdef __cplusplus
}
#endif

#endif // APP_MANAGER_H