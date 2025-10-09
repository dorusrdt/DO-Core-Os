#include "irrig_app_master.h"
#include "../../kernel/core/log_system_optimized.h"

// Variables statiques de l'application
static IrrigAppConfig_t app_config;
static bool app_initialized = false;
static uint32_t loop_counter = 0;

// Fonctions de l'application
SysError_t irrig_app_master_init(void) {
    if (app_initialized) {
        return SYS_ALREADY_INITIALIZED;
    }

    kernel_log(LOG_LEVEL_INFO, "IrrigAppMaster: Initializing irrigation system...");
    kernel_log(LOG_LEVEL_INFO, "IrrigAppMaster: Device ID: %s", app_config.device_id);
    kernel_log(LOG_LEVEL_INFO, "IrrigAppMaster: Server: %s", app_config.server_url);
    kernel_log(LOG_LEVEL_INFO, "IrrigAppMaster: Max zones: %d, Max sensors: %d",
               app_config.max_zones, app_config.max_sensors);
    kernel_log(LOG_LEVEL_INFO, "IrrigAppMaster: Simulation mode: %s",
               app_config.simulation_mode ? "ENABLED" : "DISABLED");

    app_initialized = true;
    loop_counter = 0;

    return SYS_OK;
}

void irrig_app_master_start(void) {
    kernel_log(LOG_LEVEL_INFO, "IrrigAppMaster: Starting irrigation system...");

    if (app_config.simulation_mode) {
        kernel_log(LOG_LEVEL_INFO, "IrrigAppMaster: Running in SIMULATION mode");
    } else {
        kernel_log(LOG_LEVEL_INFO, "IrrigAppMaster: Running in REAL HARDWARE mode");
    }

    kernel_log(LOG_LEVEL_INFO, "IrrigAppMaster: System ready for irrigation control");
}

void irrig_app_master_loop(void) {
    // Boucle principale de l'application
    loop_counter++;

    // Log périodique pour montrer que l'application fonctionne
    if (loop_counter % 10 == 0) {  // Tous les 10 cycles (10 secondes)
        kernel_log(LOG_LEVEL_INFO, "IrrigAppMaster: Running... (loop #%lu)", loop_counter);
    }

    // Simulation d'activité
    if (app_config.simulation_mode && loop_counter % 5 == 0) {
        kernel_log(LOG_LEVEL_DEBUG, "IrrigAppMaster: Simulating sensor readings...");
    }

    // Délai obligatoire pour éviter la surcharge CPU
    vTaskDelay(pdMS_TO_TICKS(1000)); // 1 seconde
}

void irrig_app_master_stop(void) {
    kernel_log(LOG_LEVEL_INFO, "IrrigAppMaster: Stopping irrigation system...");
    kernel_log(LOG_LEVEL_INFO, "IrrigAppMaster: Total loops executed: %lu", loop_counter);
    kernel_log(LOG_LEVEL_INFO, "IrrigAppMaster: Cleanup completed");

    app_initialized = false;
    loop_counter = 0;
}

SysError_t register_irrig_app_master(const IrrigAppConfig_t* config) {
    if (!config) {
        kernel_log(LOG_LEVEL_ERROR, "IrrigAppMaster: Invalid configuration provided");
        return SYS_INVALID_PARAM;
    }

    // Copie de la configuration
    memcpy(&app_config, config, sizeof(IrrigAppConfig_t));

    // Validation de base de la configuration
    if (strlen(app_config.device_id) == 0) {
        kernel_log(LOG_LEVEL_ERROR, "IrrigAppMaster: Device ID cannot be empty");
        return SYS_INVALID_PARAM;
    }

    if (strlen(app_config.server_url) == 0) {
        kernel_log(LOG_LEVEL_ERROR, "IrrigAppMaster: Server URL cannot be empty");
        return SYS_INVALID_PARAM;
    }

    // Création des callbacks de l'application
    AppCallbacks_t callbacks = {0};
    callbacks.init = irrig_app_master_init;
    callbacks.start = irrig_app_master_start;
    callbacks.stop = irrig_app_master_stop;
    callbacks.loop = irrig_app_master_loop;
    // pause et resume sont optionnels, laissés à NULL

    uint8_t app_id;

    // Enregistrement de l'application
    SysError_t result = app_register("IrrigAppMaster",
                                   "Smart Irrigation Control System",
                                   APP_TYPE_USER,
                                   &callbacks,
                                   &app_id);

    if (result == SYS_OK) {
        kernel_log(LOG_LEVEL_INFO, "IrrigAppMaster: Successfully registered with ID %d", app_id);
    } else {
        kernel_log(LOG_LEVEL_ERROR, "IrrigAppMaster: Failed to register application (error: %d)", result);
    }

    return result;
}