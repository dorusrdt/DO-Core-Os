#ifndef RTC_MANAGER_H
#define RTC_MANAGER_H

#include <Arduino.h>
#include <time.h>
#include "../core/kernel.h"

// Configuration RTC DS3231
#define RTC_SDA_PIN 21
#define RTC_SCL_PIN 22
#define RTC_I2C_ADDRESS 0x68


// États du RTC
typedef enum {
    RTC_STATUS_UNINITIALIZED = 0,
    RTC_STATUS_OK,
    RTC_STATUS_ERROR,
    RTC_STATUS_NOT_FOUND,
    RTC_STATUS_BATTERY_LOW,
    RTC_STATUS_COMM_ERROR
} RtcStatus_t;

#ifdef __cplusplus
extern "C" {
#endif

// Initialisation du RTC Manager
SysError_t rtc_manager_init(void);

// Obtenir le statut du RTC
RtcStatus_t rtc_get_status(void);

// Obtenir l'heure du RTC (timestamp Unix)
time_t rtc_get_time(void);

// Définir l'heure du RTC (timestamp Unix)
SysError_t rtc_set_time(time_t timestamp);

// Vérifier si le RTC est initialisé
bool rtc_is_initialized(void);

// Obtenir la température du RTC
float rtc_get_temperature(void);

// Vérifier l'état de la pile
bool rtc_is_battery_ok(void);

// Tentative de récupération du RTC
SysError_t rtc_recovery_attempt(void);

// Obtenir des informations détaillées sur l'état du RTC
String rtc_get_status_string(void);

#ifdef __cplusplus
}
#endif

#endif // RTC_MANAGER_H
