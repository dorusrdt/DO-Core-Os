#include "example_app.h"
#include "../../kernel/core/log_system_optimized.h"

// Variables statiques de l'application
static ExampleAppConfig_t app_config;
static bool app_initialized = false;

// Fonctions de l'application
SysError_t example_app_init(void) {
    if (app_initialized) {
        return SYS_ALREADY_INITIALIZED;
    }

    kernel_log(LOG_LEVEL_INFO, "Example App: Initializing");
    app_initialized = true;
    return SYS_OK;
}

void example_app_start(void) {
    kernel_log(LOG_LEVEL_INFO, "Example App: Starting");
}

void example_app_loop(void) {
    // Boucle principale de l'application
    vTaskDelay(pdMS_TO_TICKS(1000));
}

void example_app_stop(void) {
    kernel_log(LOG_LEVEL_INFO, "Example App: Stopping");
    app_initialized = false;
}

SysError_t register_example_app(const ExampleAppConfig_t* config) {
    if (!config) {
        return SYS_INVALID_PARAM;
    }

    // Copie de la configuration
    memcpy(&app_config, config, sizeof(ExampleAppConfig_t));

    // Création des callbacks de l'application
    AppCallbacks_t callbacks = {0};
    callbacks.init = example_app_init;
    callbacks.start = example_app_start;
    callbacks.stop = example_app_stop;
    callbacks.loop = example_app_loop;
    
    uint8_t app_id;
    
    // Enregistrement de l'application
    return app_register("ExampleApp", "Template for new applications", 
                     APP_TYPE_USER, &callbacks, &app_id);
}
