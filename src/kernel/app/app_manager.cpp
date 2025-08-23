#include "app_manager.h"
#include "../core/task_manager.h"
#include "../core/log_system_optimized.h"
#include <string.h>

// Variables globales
static Application_t apps[MAX_APPS];
static uint8_t next_app_id = 1;
static uint8_t active_app_count = 0;
static SemaphoreHandle_t app_manager_mutex;
static bool app_manager_initialized = false;

// Tâche principale pour les applications
static void app_task_wrapper(void* parameter) {
    Application_t* app = (Application_t*)parameter;
    
    if (!app || !app->callbacks_loaded) {
        vTaskDelete(NULL);
        return;
    }
    
    // Initialiser l'application
    if (app->callbacks.init) {
        SysError_t result = app->callbacks.init();
        if (result != SYS_OK) {
            app->info.state = APP_STATE_ERROR;
            SERIAL_PRINTF_MINIMAL("App %s init fail\n", app->info.name);
            vTaskDelete(NULL);
            return;
        }
    }
    
    app->info.state = APP_STATE_RUNNING;
    app->info.start_time = xTaskGetTickCount();
    SERIAL_PRINTF_MINIMAL("App %s started\n", app->info.name);
    
    // Démarrer l'application
    if (app->callbacks.start) {
        app->callbacks.start();
    }
    
    // Boucle principale de l'application
    while (app->info.state == APP_STATE_RUNNING) {
        // Mettre à jour le temps d'activité
        app->info.last_activity = xTaskGetTickCount();
        
        // Appeler la fonction de boucle de l'application
        if (app->callbacks.loop) {
            app->callbacks.loop();
        }
        
        // Petite pause pour éviter de surcharger le CPU
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    // Arrêter l'application
    if (app->callbacks.stop) {
        app->callbacks.stop();
    }
    
    app->info.state = APP_STATE_STOPPED;
    SERIAL_PRINTF_MINIMAL("App %s stopped\n", app->info.name);
    
    vTaskDelete(NULL);
}

// Initialisation de l'Application Manager
SysError_t app_manager_init(void) {
    if (app_manager_initialized) {
        return SYS_ALREADY_INITIALIZED;
    }
    
    SERIAL_PRINTLN_MINIMAL("Init App Manager");
    
    // Initialiser les structures
    memset(apps, 0, sizeof(apps));
    next_app_id = 1;
    active_app_count = 0;
    
    // Créer le mutex
    app_manager_mutex = xSemaphoreCreateMutex();
    if (!app_manager_mutex) {
        SERIAL_PRINTLN_MINIMAL("App mutex fail");
        return SYS_ERROR;
    }
    
    app_manager_initialized = true;
    SERIAL_PRINTLN_MINIMAL("App Manager OK");
    return SYS_OK;
}

// Désinitialisation
void app_manager_deinit(void) {
    if (!app_manager_initialized) {
        return;
    }
    
    // Arrêter toutes les applications
    app_manager_stop_all();
    
    // Supprimer le mutex
    if (app_manager_mutex) {
        vSemaphoreDelete(app_manager_mutex);
        app_manager_mutex = nullptr;
    }
    
    app_manager_initialized = false;
    SERIAL_PRINTLN_MINIMAL("App Manager deinit");
}

// Enregistrer une application
SysError_t app_register(const char* name, const char* description, AppType_t type, 
                       const AppCallbacks_t* callbacks, uint8_t* app_id) {
    if (!app_manager_initialized || !name || !app_id) {
        return SYS_INVALID_PARAM;
    }
    
    if (xSemaphoreTake(app_manager_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return SYS_BUSY;
    }
    
    // Vérifier si on a de la place
    if (active_app_count >= MAX_APPS) {
        xSemaphoreGive(app_manager_mutex);
        return SYS_NO_MEMORY;
    }
    
    // Chercher un slot libre
    int slot = -1;
    for (int i = 0; i < MAX_APPS; i++) {
        if (apps[i].info.app_id == 0) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        xSemaphoreGive(app_manager_mutex);
        return SYS_NO_MEMORY;
    }
    
    // Initialiser l'application
    Application_t* app = &apps[slot];
    app->info.app_id = next_app_id++;
    strncpy(app->info.name, name, APP_NAME_MAX_LENGTH - 1);
    app->info.name[APP_NAME_MAX_LENGTH - 1] = '\0';
    
    if (description) {
        strncpy(app->info.description, description, APP_DESCRIPTION_MAX_LENGTH - 1);
        app->info.description[APP_DESCRIPTION_MAX_LENGTH - 1] = '\0';
    } else {
        strcpy(app->info.description, "No description");
    }
    
    app->info.state = APP_STATE_UNLOADED;
    app->info.type = type;
    app->info.stack_size = APP_STACK_SIZE_DEFAULT;
    app->info.priority = APP_PRIORITY_DEFAULT;
    app->info.memory_used = 0;
    app->info.cpu_time = 0;
    app->info.start_time = 0;
    app->info.last_activity = 0;
    app->info.auto_start = false;
    app->info.persistent = false;
    
    // Copier les callbacks
    if (callbacks) {
        app->callbacks = *callbacks;
        app->callbacks_loaded = true;
    } else {
        memset(&app->callbacks, 0, sizeof(AppCallbacks_t));
        app->callbacks_loaded = false;
    }
    
    app->task_handle = nullptr;
    active_app_count++;
    
    *app_id = app->info.app_id;
    
    SERIAL_PRINTF_MINIMAL("App %s registered (ID: %d)\n", name, *app_id);
    
    xSemaphoreGive(app_manager_mutex);
    return SYS_OK;
}

// Désenregistrer une application
SysError_t app_unregister(uint8_t app_id) {
    if (!app_manager_initialized) {
        return SYS_NOT_INITIALIZED;
    }
    
    if (xSemaphoreTake(app_manager_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return SYS_BUSY;
    }
    
    // Trouver l'application
    Application_t* app = nullptr;
    for (int i = 0; i < MAX_APPS; i++) {
        if (apps[i].info.app_id == app_id) {
            app = &apps[i];
            break;
        }
    }
    
    if (!app) {
        xSemaphoreGive(app_manager_mutex);
        return SYS_INVALID_PARAM;
    }
    
    // Arrêter l'application si elle tourne
    if (app->info.state == APP_STATE_RUNNING) {
        app_stop(app_id);
    }
    
    // Supprimer la tâche si elle existe
    if (app->task_handle) {
        vTaskDelete(app->task_handle);
        app->task_handle = nullptr;
    }
    
    // Réinitialiser l'application
    memset(app, 0, sizeof(Application_t));
    active_app_count--;
    
    SERIAL_PRINTF_MINIMAL("App %d unregistered\n", app_id);
    
    xSemaphoreGive(app_manager_mutex);
    return SYS_OK;
}

// Démarrer une application
SysError_t app_start(uint8_t app_id) {
    if (!app_manager_initialized) {
        return SYS_NOT_INITIALIZED;
    }
    
    if (xSemaphoreTake(app_manager_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return SYS_BUSY;
    }
    
    // Trouver l'application
    Application_t* app = nullptr;
    for (int i = 0; i < MAX_APPS; i++) {
        if (apps[i].info.app_id == app_id) {
            app = &apps[i];
            break;
        }
    }
    
    if (!app) {
        xSemaphoreGive(app_manager_mutex);
        return SYS_INVALID_PARAM;
    }
    
    // Vérifier l'état
    if (app->info.state == APP_STATE_RUNNING) {
        xSemaphoreGive(app_manager_mutex);
        return SYS_ALREADY_EXISTS;
    }
    
    if (app->info.state == APP_STATE_ERROR) {
        xSemaphoreGive(app_manager_mutex);
        return SYS_ERROR;
    }
    
    // Créer la tâche pour l'application
    app->info.state = APP_STATE_LOADING;
    BaseType_t result = xTaskCreate(app_task_wrapper, app->info.name, 
                                   app->info.stack_size, app, 
                                   app->info.priority, &app->task_handle);
    
    if (result != pdPASS) {
        app->info.state = APP_STATE_ERROR;
        xSemaphoreGive(app_manager_mutex);
        return SYS_ERROR;
    }
    
    xSemaphoreGive(app_manager_mutex);
    return SYS_OK;
}

// Arrêter une application
SysError_t app_stop(uint8_t app_id) {
    if (!app_manager_initialized) {
        return SYS_NOT_INITIALIZED;
    }
    
    if (xSemaphoreTake(app_manager_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return SYS_BUSY;
    }
    
    // Trouver l'application
    Application_t* app = nullptr;
    for (int i = 0; i < MAX_APPS; i++) {
        if (apps[i].info.app_id == app_id) {
            app = &apps[i];
            break;
        }
    }
    
    if (!app) {
        xSemaphoreGive(app_manager_mutex);
        return SYS_INVALID_PARAM;
    }
    
    // Arrêter l'application
    if (app->info.state == APP_STATE_RUNNING) {
        app->info.state = APP_STATE_STOPPED;
        
        // Appeler le callback d'arrêt
        if (app->callbacks.stop) {
            app->callbacks.stop();
        }
        
        // Supprimer la tâche
        if (app->task_handle) {
            vTaskDelete(app->task_handle);
            app->task_handle = nullptr;
        }
        
        SERIAL_PRINTF_MINIMAL("App %s stopped\n", app->info.name);
    }
    
    xSemaphoreGive(app_manager_mutex);
    return SYS_OK;
}

// Mettre en pause une application
SysError_t app_pause(uint8_t app_id) {
    if (!app_manager_initialized) {
        return SYS_NOT_INITIALIZED;
    }
    
    if (xSemaphoreTake(app_manager_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return SYS_BUSY;
    }
    
    // Trouver l'application
    Application_t* app = nullptr;
    for (int i = 0; i < MAX_APPS; i++) {
        if (apps[i].info.app_id == app_id) {
            app = &apps[i];
            break;
        }
    }
    
    if (!app) {
        xSemaphoreGive(app_manager_mutex);
        return SYS_INVALID_PARAM;
    }
    
    // Mettre en pause
    if (app->info.state == APP_STATE_RUNNING) {
        app->info.state = APP_STATE_PAUSED;
        
        if (app->callbacks.pause) {
            app->callbacks.pause();
        }
        
        SERIAL_PRINTF_MINIMAL("App %s paused\n", app->info.name);
    }
    
    xSemaphoreGive(app_manager_mutex);
    return SYS_OK;
}

// Reprendre une application
SysError_t app_resume(uint8_t app_id) {
    if (!app_manager_initialized) {
        return SYS_NOT_INITIALIZED;
    }
    
    if (xSemaphoreTake(app_manager_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return SYS_BUSY;
    }
    
    // Trouver l'application
    Application_t* app = nullptr;
    for (int i = 0; i < MAX_APPS; i++) {
        if (apps[i].info.app_id == app_id) {
            app = &apps[i];
            break;
        }
    }
    
    if (!app) {
        xSemaphoreGive(app_manager_mutex);
        return SYS_INVALID_PARAM;
    }
    
    // Reprendre
    if (app->info.state == APP_STATE_PAUSED) {
        app->info.state = APP_STATE_RUNNING;
        
        if (app->callbacks.resume) {
            app->callbacks.resume();
        }
        
        SERIAL_PRINTF_MINIMAL("App %s resumed\n", app->info.name);
    }
    
    xSemaphoreGive(app_manager_mutex);
    return SYS_OK;
}

// Redémarrer une application
SysError_t app_restart(uint8_t app_id) {
    SysError_t result = app_stop(app_id);
    if (result != SYS_OK) {
        return result;
    }
    
    vTaskDelay(pdMS_TO_TICKS(100)); // Petite pause
    
    return app_start(app_id);
}

// Obtenir le nombre d'applications
uint8_t app_get_count(void) {
    return active_app_count;
}

// Vérifier si une application existe
bool app_exists(uint8_t app_id) {
    if (!app_manager_initialized) {
        return false;
    }
    
    for (int i = 0; i < MAX_APPS; i++) {
        if (apps[i].info.app_id == app_id) {
            return true;
        }
    }
    return false;
}

// Obtenir l'état d'une application
AppState_t app_get_state(uint8_t app_id) {
    if (!app_manager_initialized) {
        return APP_STATE_ERROR;
    }
    
    for (int i = 0; i < MAX_APPS; i++) {
        if (apps[i].info.app_id == app_id) {
            return apps[i].info.state;
        }
    }
    return APP_STATE_ERROR;
}

// Obtenir les informations d'une application
SysError_t app_get_info(uint8_t app_id, AppInfo_t* info) {
    if (!app_manager_initialized || !info) {
        return SYS_INVALID_PARAM;
    }
    
    for (int i = 0; i < MAX_APPS; i++) {
        if (apps[i].info.app_id == app_id) {
            *info = apps[i].info;
            return SYS_OK;
        }
    }
    
    return SYS_INVALID_PARAM;
}

// Afficher le statut du gestionnaire d'applications
void app_manager_print_status(void) {
    if (!app_manager_initialized) {
        Serial.println("App Manager: Not initialized");
        return;
    }
    
    Serial.println("=== App Manager Status ===");
    Serial.printf("Active apps: %d/%d\n", active_app_count, MAX_APPS);
    Serial.printf("Next app ID: %d\n", next_app_id);
    Serial.println("==========================");
}

// Afficher toutes les applications
void app_manager_print_all_apps(void) {
    if (!app_manager_initialized) {
        Serial.println("App Manager: Not initialized");
        return;
    }
    
    Serial.println("=== All Applications ===");
    
    for (int i = 0; i < MAX_APPS; i++) {
        if (apps[i].info.app_id != 0) {
            Serial.printf("ID: %d | %s | %s | State: ", 
                         apps[i].info.app_id, 
                         apps[i].info.name, 
                         apps[i].info.description);
            
            switch (apps[i].info.state) {
                case APP_STATE_UNLOADED: Serial.print("UNLOADED"); break;
                case APP_STATE_LOADING: Serial.print("LOADING"); break;
                case APP_STATE_RUNNING: Serial.print("RUNNING"); break;
                case APP_STATE_PAUSED: Serial.print("PAUSED"); break;
                case APP_STATE_STOPPED: Serial.print("STOPPED"); break;
                case APP_STATE_ERROR: Serial.print("ERROR"); break;
                default: Serial.print("UNKNOWN"); break;
            }
            
            Serial.printf(" | Memory: %lu | CPU: %lu\n", 
                         apps[i].info.memory_used, 
                         apps[i].info.cpu_time);
        }
    }
    
    Serial.println("========================");
}

// Démarrer toutes les applications
SysError_t app_manager_start_all(void) {
    if (!app_manager_initialized) {
        return SYS_NOT_INITIALIZED;
    }
    
    SERIAL_PRINTLN_MINIMAL("Starting all apps");
    
    for (int i = 0; i < MAX_APPS; i++) {
        if (apps[i].info.app_id != 0 && apps[i].info.auto_start) {
            app_start(apps[i].info.app_id);
        }
    }
    
    return SYS_OK;
}

// Arrêter toutes les applications
SysError_t app_manager_stop_all(void) {
    if (!app_manager_initialized) {
        return SYS_NOT_INITIALIZED;
    }
    
    SERIAL_PRINTLN_MINIMAL("Stopping all apps");
    
    for (int i = 0; i < MAX_APPS; i++) {
        if (apps[i].info.app_id != 0) {
            app_stop(apps[i].info.app_id);
        }
    }
    
    return SYS_OK;
}

// Mettre en pause toutes les applications
SysError_t app_manager_pause_all(void) {
    if (!app_manager_initialized) {
        return SYS_NOT_INITIALIZED;
    }
    
    SERIAL_PRINTLN_MINIMAL("Pausing all apps");
    
    for (int i = 0; i < MAX_APPS; i++) {
        if (apps[i].info.app_id != 0) {
            app_pause(apps[i].info.app_id);
        }
    }
    
    return SYS_OK;
}

// Reprendre toutes les applications
SysError_t app_manager_resume_all(void) {
    if (!app_manager_initialized) {
        return SYS_NOT_INITIALIZED;
    }
    
    SERIAL_PRINTLN_MINIMAL("Resuming all apps");
    
    for (int i = 0; i < MAX_APPS; i++) {
        if (apps[i].info.app_id != 0) {
            app_resume(apps[i].info.app_id);
        }
    }
    
    return SYS_OK;
}

// Boucle principale du gestionnaire d'applications
void app_manager_loop(void) {
    if (!app_manager_initialized) {
        return;
    }
    
    // Mettre à jour les statistiques des applications
    for (int i = 0; i < MAX_APPS; i++) {
        if (apps[i].info.app_id != 0 && apps[i].info.state == APP_STATE_RUNNING) {
            // Mettre à jour le temps CPU
            if (apps[i].task_handle) {
                apps[i].info.cpu_time = xTaskGetTickCount() - apps[i].info.start_time;
            }
            
            // Mettre à jour l'utilisation mémoire (estimation)
            apps[i].info.memory_used = apps[i].info.stack_size;
        }
    }
}

// Fonctions utilitaires
uint32_t app_get_memory_usage(uint8_t app_id) {
    AppInfo_t info;
    if (app_get_info(app_id, &info) == SYS_OK) {
        return info.memory_used;
    }
    return 0;
}

uint32_t app_get_cpu_time(uint8_t app_id) {
    AppInfo_t info;
    if (app_get_info(app_id, &info) == SYS_OK) {
        return info.cpu_time;
    }
    return 0;
}

bool app_is_running(uint8_t app_id) {
    return app_get_state(app_id) == APP_STATE_RUNNING;
}

bool app_is_paused(uint8_t app_id) {
    return app_get_state(app_id) == APP_STATE_PAUSED;
}

// Fonctions C pour compatibilité
SysError_t app_manager_c_init(void) {
    return app_manager_init();
}

uint8_t app_manager_c_get_count(void) {
    return app_get_count();
}

AppState_t app_manager_c_get_state(uint8_t app_id) {
    return app_get_state(app_id);
} 