#include "rtc_manager.h"
#include "../core/kernel.h"
#include <Wire.h>
#include <RTClib.h>

// Variables globales
static RTC_DS3231 rtc;
static RtcStatus_t g_rtc_status = RTC_STATUS_UNINITIALIZED;
static bool g_rtc_initialized = false;
static uint32_t g_rtc_recovery_attempts = 0;
static uint32_t g_rtc_last_error_time = 0;

// Initialisation du RTC Manager
SysError_t rtc_manager_init(void) {
    if (g_rtc_initialized) {
        return SYS_ALREADY_INITIALIZED;
    }

    kernel_log(LOG_LEVEL_INFO, "Initializing RTC Manager (DS3231) - SDA:%d, SCL:%d", 
               RTC_SDA_PIN, RTC_SCL_PIN);

    // Initialiser Wire avec les pins spécifiées
    Wire.begin(RTC_SDA_PIN, RTC_SCL_PIN);
    Wire.setClock(100000); // 100kHz

    // Initialiser le RTC DS3231 avec retry
    int retry_count = 0;
    const int max_retries = 3;
    
    while (retry_count < max_retries) {
        if (rtc.begin()) {
            break;
        }
        
        retry_count++;
        kernel_log(LOG_LEVEL_WARN, "RTC init attempt %d/%d failed", retry_count, max_retries);
        
        if (retry_count < max_retries) {
            delay(100); // Attendre avant de réessayer
        }
    }
    
    if (retry_count >= max_retries) {
        kernel_log(LOG_LEVEL_ERROR, "DS3231 RTC not found after %d attempts!", max_retries);
        g_rtc_status = RTC_STATUS_NOT_FOUND;
        g_rtc_last_error_time = millis();
        return SYS_ERROR;
    }

    // Vérifier si le RTC a perdu l'alimentation
    if (rtc.lostPower()) {
        kernel_log(LOG_LEVEL_WARN, "RTC lost power - battery may be low");
        g_rtc_status = RTC_STATUS_BATTERY_LOW;
        
        // Essayer de récupérer avec l'heure de compilation
        kernel_log(LOG_LEVEL_INFO, "Attempting RTC recovery with compile time");
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        
        // Vérifier si la récupération a fonctionné
        delay(100);
        if (rtc.lostPower()) {
            kernel_log(LOG_LEVEL_ERROR, "RTC recovery failed - battery critically low");
            g_rtc_status = RTC_STATUS_BATTERY_LOW;
        } else {
            kernel_log(LOG_LEVEL_INFO, "RTC recovery successful");
            g_rtc_status = RTC_STATUS_OK;
        }
    } else {
        g_rtc_status = RTC_STATUS_OK;
    }
    

    g_rtc_initialized = true;
    g_rtc_recovery_attempts = 0;

    kernel_log(LOG_LEVEL_INFO, "RTC Manager initialized successfully");
    kernel_log(LOG_LEVEL_INFO, "RTC Status: %s", rtc_get_status_string().c_str());
    kernel_log(LOG_LEVEL_INFO, "RTC Temperature: %.1f°C", rtc.getTemperature());
    
    return SYS_OK;
}

// Obtenir le statut du RTC
RtcStatus_t rtc_get_status(void) {
    return g_rtc_status;
}

// Vérifier si le RTC est initialisé
bool rtc_is_initialized(void) {
    return g_rtc_initialized;
}

// Obtenir l'heure du RTC (timestamp Unix)
time_t rtc_get_time(void) {
    if (!g_rtc_initialized) {
        return 0;
    }

    // Vérifier le statut et tenter une récupération si nécessaire
    if (g_rtc_status == RTC_STATUS_ERROR || g_rtc_status == RTC_STATUS_COMM_ERROR) {
        kernel_log(LOG_LEVEL_WARN, "RTC communication error, attempting recovery");
        rtc_recovery_attempt();
    }

    if (g_rtc_status != RTC_STATUS_OK && g_rtc_status != RTC_STATUS_BATTERY_LOW) {
        return 0;
    }

    try {
        DateTime now = rtc.now();
        time_t timestamp = now.unixtime();
        
        // Vérifier si l'heure est raisonnable
        if (timestamp < 1577836800) { // Avant 2020
            kernel_log(LOG_LEVEL_WARN, "RTC time seems invalid: %ld", timestamp);
            return 0;
        }
        
        return timestamp;
    } catch (...) {
        kernel_log(LOG_LEVEL_ERROR, "RTC read error - communication failure");
        g_rtc_status = RTC_STATUS_COMM_ERROR;
        g_rtc_last_error_time = millis();
        return 0;
    }
}

// Définir l'heure du RTC (timestamp Unix)
SysError_t rtc_set_time(time_t timestamp) {
    if (!g_rtc_initialized) {
        return SYS_NOT_INITIALIZED;
    }

    if (g_rtc_status != RTC_STATUS_OK) {
        return SYS_ERROR;
    }

    DateTime dt(timestamp);
    rtc.adjust(dt);

    kernel_log(LOG_LEVEL_INFO, "RTC time set to: %ld", timestamp);
    return SYS_OK;
}

// Obtenir la température du RTC
float rtc_get_temperature(void) {
    if (!g_rtc_initialized || g_rtc_status != RTC_STATUS_OK) {
        return -999.0f;
    }

    return rtc.getTemperature();
}

// Vérifier l'état de la pile
bool rtc_is_battery_ok(void) {
    if (!g_rtc_initialized) {
        return false;
    }

    // Le DS3231 a une pile de sauvegarde intégrée
    // On considère qu'elle est toujours OK sauf si le RTC a perdu l'alimentation
    return !rtc.lostPower();
}

// Tentative de récupération du RTC
SysError_t rtc_recovery_attempt(void) {
    if (!g_rtc_initialized) {
        return SYS_NOT_INITIALIZED;
    }

    // Limiter les tentatives de récupération
    if (g_rtc_recovery_attempts >= 5) {
        kernel_log(LOG_LEVEL_ERROR, "RTC recovery attempts exceeded limit");
        return SYS_ERROR;
    }

    // Attendre avant de réessayer
    uint32_t time_since_last_error = millis() - g_rtc_last_error_time;
    if (time_since_last_error < 30000) { // 30 secondes
        kernel_log(LOG_LEVEL_DEBUG, "RTC recovery too soon, waiting...");
        return SYS_ERROR;
    }

    g_rtc_recovery_attempts++;
    kernel_log(LOG_LEVEL_INFO, "RTC recovery attempt %d/5", g_rtc_recovery_attempts);

    // Réinitialiser Wire
    Wire.end();
    delay(100);
    Wire.begin(RTC_SDA_PIN, RTC_SCL_PIN);
    Wire.setClock(100000);

    // Réessayer l'initialisation
    if (rtc.begin()) {
        kernel_log(LOG_LEVEL_INFO, "RTC recovery successful!");
        g_rtc_status = RTC_STATUS_OK;
        g_rtc_recovery_attempts = 0;
        return SYS_OK;
    } else {
        kernel_log(LOG_LEVEL_WARN, "RTC recovery failed");
        g_rtc_status = RTC_STATUS_COMM_ERROR;
        g_rtc_last_error_time = millis();
        return SYS_ERROR;
    }
}

// Obtenir des informations détaillées sur l'état du RTC
String rtc_get_status_string(void) {
    switch (g_rtc_status) {
        case RTC_STATUS_UNINITIALIZED:
            return "UNINITIALIZED";
        case RTC_STATUS_OK:
            return "OK";
        case RTC_STATUS_ERROR:
            return "ERROR";
        case RTC_STATUS_NOT_FOUND:
            return "NOT_FOUND";
        case RTC_STATUS_BATTERY_LOW:
            return "BATTERY_LOW";
        case RTC_STATUS_COMM_ERROR:
            return "COMM_ERROR";
        default:
            return "UNKNOWN";
    }
}

