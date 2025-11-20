#include "espnow_slave.h"
#include "../../kernel/app/app_manager.h"
#include "../../kernel/core/log_system_optimized.h"

static uint8_t slave_app_id = ESPNOW_SLAVE_APP_ID;

// ============ Initialisation ============

void slave_app_start(void) {
    kernel_log(LOG_LEVEL_INFO, "Slave: App starting");

    // TODO: Initialiser ESP-NOW Slave ici

    kernel_log(LOG_LEVEL_INFO, "Slave: App started");
}

void slave_app_stop(void) {
    kernel_log(LOG_LEVEL_INFO, "Slave: App stopping");

    // TODO: Nettoyer les ressources ESP-NOW Slave ici

    kernel_log(LOG_LEVEL_INFO, "Slave: App stopped");
}

SysError_t espnow_slave_register_app() {
    AppCallbacks_t callbacks = {0};
    callbacks.start = slave_app_start;
    callbacks.stop = slave_app_stop;

    uint8_t app_id;
    return app_register("espnow_slave", "ESP-NOW Slave Application",
                       APP_TYPE_USER, &callbacks, &app_id);
}
