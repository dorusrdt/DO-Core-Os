#include "time_sync_manager.h"
#include "rtc_manager.h"
#include "../network/ntp_manager.h"
#include "../core/kernel.h"
#include <sys/time.h>

// Variables globales
static TimeSyncStatus_t g_sync_status = TIME_SYNC_STATUS_UNINITIALIZED;
static bool g_sync_initialized = false;
static TimeSource_t g_current_source = TIME_SOURCE_UNKNOWN;
static uint32_t g_sync_failures = 0;
static uint32_t g_last_sync_attempt = 0;

// Initialisation du gestionnaire de synchronisation
SysError_t time_sync_init(void) {
    if (g_sync_initialized) {
        return SYS_ALREADY_INITIALIZED;
    }

    kernel_log(LOG_LEVEL_INFO, "Initializing Time Sync Manager");

    g_sync_initialized = true;
    g_sync_status = TIME_SYNC_STATUS_OK;

    kernel_log(LOG_LEVEL_INFO, "Time Sync Manager initialized successfully");
    return SYS_OK;
}

// Vérifier si le gestionnaire est initialisé
bool time_sync_is_initialized(void) {
    return g_sync_initialized;
}

// Synchronisation automatique (appelée par la tâche)
SysError_t time_sync_automatic(void) {
    if (!g_sync_initialized) {
        return SYS_NOT_INITIALIZED;
    }

    g_last_sync_attempt = millis();
    
    // Priorité : NTP > RTC > System Clock
    
    // 1. Vérifier NTP
    if (ntp_is_synced()) {
        time_t ntp_time = ntp_get_time();
        if (ntp_time > 1577836800) { // Après 2020
            g_current_source = TIME_SOURCE_NTP;
            g_sync_status = TIME_SYNC_STATUS_OK;
            g_sync_failures = 0;
            
            // Synchroniser le RTC avec NTP (si disponible)
            if (rtc_is_initialized() && rtc_get_status() != RTC_STATUS_NOT_FOUND) {
                SysError_t rtc_result = rtc_set_time(ntp_time);
                if (rtc_result == SYS_OK) {
                    kernel_log(LOG_LEVEL_INFO, "RTC synchronized with NTP");
                } else {
                    kernel_log(LOG_LEVEL_WARN, "Failed to sync RTC with NTP");
                }
            }
            
            return SYS_OK;
        }
    }

    // 2. Vérifier RTC (avec gestion des erreurs)
    if (rtc_is_initialized()) {
        RtcStatus_t rtc_status = rtc_get_status();
        
        if (rtc_status == RTC_STATUS_OK || rtc_status == RTC_STATUS_BATTERY_LOW) {
            time_t rtc_time = rtc_get_time();
            if (rtc_time > 1577836800) { // Après 2020
                g_current_source = TIME_SOURCE_RTC;
                g_sync_status = (rtc_status == RTC_STATUS_BATTERY_LOW) ? 
                               TIME_SYNC_STATUS_DEGRADED_MODE : TIME_SYNC_STATUS_OK;
                g_sync_failures = 0;
                
                // Synchroniser l'heure système avec le RTC
                struct timeval tv;
                tv.tv_sec = rtc_time;
                tv.tv_usec = 0;
                settimeofday(&tv, nullptr);
                
                if (rtc_status == RTC_STATUS_BATTERY_LOW) {
                    kernel_log(LOG_LEVEL_WARN, "System time synchronized with RTC (battery low)");
                } else {
                    kernel_log(LOG_LEVEL_INFO, "System time synchronized with RTC");
                }
                return SYS_OK;
            }
        } else if (rtc_status == RTC_STATUS_COMM_ERROR) {
            // Tenter une récupération du RTC
            kernel_log(LOG_LEVEL_INFO, "Attempting RTC recovery...");
            rtc_recovery_attempt();
        }
    }

    // 3. Vérifier l'heure système (mode dégradé)
    time_t sys_time = time(nullptr);
    if (sys_time > 1577836800) { // Après 2020
        g_current_source = TIME_SOURCE_SYSTEM;
        g_sync_status = TIME_SYNC_STATUS_DEGRADED_MODE;
        g_sync_failures++;
        
        kernel_log(LOG_LEVEL_WARN, "Using system clock (degraded mode) - failures: %d", g_sync_failures);
        return SYS_OK;
    }

    // Aucune source valide
    g_current_source = TIME_SOURCE_UNKNOWN;
    g_sync_status = TIME_SYNC_STATUS_NO_SOURCE;
    g_sync_failures++;
    
    kernel_log(LOG_LEVEL_ERROR, "No valid time source available - failures: %d", g_sync_failures);
    return SYS_ERROR;
}

// Obtenir la source de temps actuelle
TimeSource_t time_sync_get_current_source(void) {
    return g_current_source;
}

// Obtenir l'heure actuelle
time_t time_sync_get_current_time(void) {
    return time(nullptr);
}

// Formater l'heure actuelle
String time_sync_format_current_time(void) {
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    
    char buffer[64];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return String(buffer);
}

// Obtenir le statut de synchronisation
TimeSyncStatus_t time_sync_get_status(void) {
    return g_sync_status;
}

// Obtenir des informations détaillées sur les sources de temps
String time_sync_get_source_info(void) {
    String info = "Time Sources Status:\n";
    
    // NTP Status
    info += "  NTP: ";
    if (ntp_is_synced()) {
        info += "SYNCED";
    } else {
        info += "NOT_SYNCED";
    }
    info += "\n";
    
    // RTC Status
    info += "  RTC: ";
    if (rtc_is_initialized()) {
        info += rtc_get_status_string();
        if (rtc_get_status() == RTC_STATUS_OK || rtc_get_status() == RTC_STATUS_BATTERY_LOW) {
            time_t rtc_time = rtc_get_time();
            if (rtc_time > 0) {
                info += " (time available)";
            } else {
                info += " (no time)";
            }
        }
    } else {
        info += "NOT_INITIALIZED";
    }
    info += "\n";
    
    // System Clock
    info += "  System: ";
    time_t sys_time = time(nullptr);
    if (sys_time > 1577836800) {
        info += "VALID";
    } else {
        info += "INVALID";
    }
    info += "\n";
    
    // Current Source
    info += "  Current: ";
    switch (g_current_source) {
        case TIME_SOURCE_NTP:
            info += "NTP";
            break;
        case TIME_SOURCE_RTC:
            info += "RTC";
            break;
        case TIME_SOURCE_SYSTEM:
            info += "SYSTEM";
            break;
        default:
            info += "UNKNOWN";
            break;
    }
    info += "\n";
    
    // Sync Status
    info += "  Sync Status: ";
    switch (g_sync_status) {
        case TIME_SYNC_STATUS_OK:
            info += "OK";
            break;
        case TIME_SYNC_STATUS_DEGRADED_MODE:
            info += "DEGRADED";
            break;
        case TIME_SYNC_STATUS_NO_SOURCE:
            info += "NO_SOURCE";
            break;
        case TIME_SYNC_STATUS_ERROR:
            info += "ERROR";
            break;
        default:
            info += "UNKNOWN";
            break;
    }
    info += "\n";
    
    // Failure count
    info += "  Failures: " + String(g_sync_failures) + "\n";
    
    return info;
}
