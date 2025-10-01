#ifndef SST_APP_H
#define SST_APP_H

#include "../../kernel/core/kernel.h"
#include "../../kernel/app/app_manager.h"
#include "sst_data.h"
#include "sst_buttons.h"
#include <WiFi.h>
#include <WebServer.h>

// Configuration de l'application SST
#define SST_APP_NAME "SST"
#define SST_APP_DESCRIPTION "SST Application - Hello World"

// Configuration de l'enregistrement automatique
#define BACKEND_HOST "192.168.1.218"
#define BACKEND_PORT 5000
#define REGISTRATION_TIMEOUT_MS 30000  // 30 secondes timeout pour la requête

// États d'enregistrement du device
typedef enum {
    DEVICE_STATE_UNREGISTERED = 0,
    DEVICE_STATE_DISCOVERABLE,
    DEVICE_STATE_REGISTERING,
    DEVICE_STATE_REGISTERED,
    DEVICE_STATE_ERROR
} DeviceRegistrationState_t;

// Structure d'informations du device pour la découverte
typedef struct {
    String device_id;
    String device_name;
    String mac_address;
    String ip_address;
    uint32_t chip_id;
    String firmware_version;
    time_t registration_time;
    DeviceRegistrationState_t state;
    String api_token;  // JWT token après enregistrement
    uint32_t last_sync_time;
} DeviceInfo_t;

// Fonctions publiques de l'application SST
SysError_t sst_app_init(void);
void sst_app_start(void);
void sst_app_stop(void);
void sst_app_pause(void);
void sst_app_resume(void);
void sst_app_loop(void);

// Fonction d'enregistrement de l'application
SysError_t sst_app_register(uint8_t* app_id);

// Fonctions d'enregistrement automatique du device
SysError_t sst_device_auto_registration_init(void);
SysError_t sst_device_auto_register(void);
DeviceRegistrationState_t sst_device_get_registration_state(void);
SysError_t sst_device_set_registered(const char* api_token);
bool sst_device_is_registered(void);
const DeviceInfo_t* sst_device_get_info(void);

// Fonctions DMD (utilisent les fonctions globales de main.cpp)
extern void sst_dmd_display_days_without_accident(void);
extern void sst_dmd_display_text(const char* text);
extern void sst_dmd_clear_screen(void);

#endif // SST_APP_H
