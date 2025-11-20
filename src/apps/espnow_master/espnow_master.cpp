#include "espnow_master.h"
#include "../../kernel/app/app_manager.h"
#include "../../kernel/core/log_system_optimized.h"

static uint8_t master_app_id = ESPNOW_MASTER_APP_ID;

// ============ Initialisation ============

void master_app_start(void) {
    kernel_log(LOG_LEVEL_INFO, "Master: App starting");

    // TODO: Initialiser ESP-NOW Master ici

    kernel_log(LOG_LEVEL_INFO, "Master: App started");
}

void master_app_stop(void) {
    kernel_log(LOG_LEVEL_INFO, "Master: App stopping");

    // TODO: Nettoyer les ressources ESP-NOW Master ici

    kernel_log(LOG_LEVEL_INFO, "Master: App stopped");
}

SysError_t espnow_master_register_app() {
    AppCallbacks_t callbacks = {0};
    callbacks.start = master_app_start;
    callbacks.stop = master_app_stop;

    uint8_t app_id;
    return app_register("espnow_master", "ESP-NOW Master Application",
                       APP_TYPE_USER, &callbacks, &app_id);
}
