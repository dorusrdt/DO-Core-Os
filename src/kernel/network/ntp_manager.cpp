#include "ntp_manager.h"
#include <WiFi.h>
#include "../core/kernel.h"

// Variables globales
static NtpStatus_t g_ntp_status = NTP_STATUS_UNINITIALIZED;
static NtpSyncInfo_t g_ntp_info;
static bool g_ntp_configured = false;

// Initialisation du NTP Manager
SysError_t ntp_init(void) {
    if (g_ntp_status != NTP_STATUS_UNINITIALIZED) {
        return SYS_ALREADY_INITIALIZED;
    }
    
    // Initialiser les informations
    memset(&g_ntp_info, 0, sizeof(g_ntp_info));
    
    // Configurer le serveur NTP avec l'API Arduino standard
    configTime(NTP_GMT_OFFSET_SEC, NTP_DAYLIGHT_OFFSET_SEC, NTP_SERVER);
    g_ntp_configured = true;
    
    g_ntp_status = NTP_STATUS_DISCONNECTED;
    
    kernel_log(LOG_LEVEL_INFO, "NTP Manager initialized with Arduino configTime");
    return SYS_OK;
}

// Synchronisation manuelle
NtpStatus_t ntp_sync(void) {
    if (g_ntp_status == NTP_STATUS_UNINITIALIZED) {
        return NTP_STATUS_UNINITIALIZED;
    }
    
    if (!g_ntp_configured) {
        kernel_log(LOG_LEVEL_ERROR, "NTP not configured");
        return NTP_STATUS_FAILED;
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        g_ntp_status = NTP_STATUS_DISCONNECTED;
        kernel_log(LOG_LEVEL_WARN, "NTP sync failed: WiFi disconnected");
        return g_ntp_status;
    }
    
    g_ntp_status = NTP_STATUS_SYNCING;
    kernel_log(LOG_LEVEL_INFO, "NTP syncing with %s", NTP_SERVER);
    
    // Utiliser l'API Arduino standard pour obtenir l'heure
    struct tm timeinfo;
    bool success = false;
    
    // Tentative de synchronisation avec timeout
    unsigned long start_time = millis();
    const unsigned long timeout = 10000; // 10 secondes
    
    while (millis() - start_time < timeout) {
        if (getLocalTime(&timeinfo)) {
            success = true;
            break;
        }
        delay(100);
    }
    
    if (!success) {
        g_ntp_status = NTP_STATUS_TIMEOUT;
        g_ntp_info.failed_syncs++;
        kernel_log(LOG_LEVEL_WARN, "NTP sync failed: timeout after %lu ms", millis() - start_time);
    return g_ntp_status;
    }
    
    // Conversion en timestamp Unix
    time_t epoch = mktime(&timeinfo);
    
    // Mettre à jour les informations
    g_ntp_info.last_sync_time = epoch;
    strncpy(g_ntp_info.last_sync_server, NTP_SERVER, sizeof(g_ntp_info.last_sync_server) - 1);
    g_ntp_info.last_sync_server[sizeof(g_ntp_info.last_sync_server) - 1] = '\0';
    g_ntp_info.successful_syncs++;
    
    g_ntp_status = NTP_STATUS_SYNCED;
    kernel_log(LOG_LEVEL_INFO, "NTP sync successful: %s", ntp_format_time(epoch).c_str());
    
    return g_ntp_status;
}

// Obtenir le statut de synchronisation
NtpStatus_t ntp_get_status(void) {
    return g_ntp_status;
}

// Obtenir les informations de synchronisation
void ntp_get_sync_info(NtpSyncInfo_t* info) {
    if (info != nullptr) {
        *info = g_ntp_info;
    }
}

// Obtenir l'heure actuelle
time_t ntp_get_time(void) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        return mktime(&timeinfo);
    }
    return 0;
}

// Formater l'heure en string
String ntp_format_time(time_t timestamp, const char* format) {
    struct tm timeinfo;
    localtime_r(&timestamp, &timeinfo);
    
    static char buffer[32];
    strftime(buffer, sizeof(buffer), format, &timeinfo);
    
    return String(buffer);
}

// Vérifier si l'heure est synchronisée
bool ntp_is_synced(void) {
    if (g_ntp_status != NTP_STATUS_SYNCED) {
        return false;
    }
    
    // Vérifier que l'heure actuelle est raisonnable (après 2020)
    time_t current_time = ntp_get_time();
    return (current_time > 1577836800); // 1er janvier 2020
}

// Fonctions avancées pour l'intégration
bool ntp_is_time_valid(time_t timestamp) {
    if (!ntp_is_synced()) {
        return false;
    }
    
    time_t current_time = ntp_get_time();
    time_t diff = abs(current_time - timestamp);
    
    // Considérer valide si la différence est inférieure à 1 heure
    return (diff < 3600);
}

time_t ntp_get_uptime_since_sync(void) {
    if (!ntp_is_synced()) {
        return 0;
    }
    
    time_t current_time = ntp_get_time();
    time_t sync_time = g_ntp_info.last_sync_time;
    
    return (current_time - sync_time);
}

String ntp_get_timezone_string(void) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "UTC%+d", NTP_GMT_OFFSET_SEC / 3600);
    return String(buffer);
}

bool ntp_set_timezone(int gmt_offset_sec, int daylight_offset_sec) {
    if (g_ntp_status == NTP_STATUS_UNINITIALIZED) {
        return false;
    }
    
    // Reconfigurer avec les nouveaux paramètres
    configTime(gmt_offset_sec, daylight_offset_sec, NTP_SERVER);
    
    kernel_log(LOG_LEVEL_INFO, "Timezone updated: UTC%+d", gmt_offset_sec / 3600);
    return true;
}

// Fonctions utilitaires
bool ntp_is_business_hours(void) {
    if (!ntp_is_synced()) {
        return false;
    }
    
    time_t current_time = ntp_get_time();
    struct tm* timeinfo = localtime(&current_time);
    
    // Heures de bureau: 8h-18h, lundi-vendredi
    return (timeinfo->tm_hour >= 8 && timeinfo->tm_hour < 18 && 
            timeinfo->tm_wday >= 1 && timeinfo->tm_wday <= 5);
}

bool ntp_is_night_time(void) {
    if (!ntp_is_synced()) {
        return false;
    }
    
    time_t current_time = ntp_get_time();
    struct tm* timeinfo = localtime(&current_time);
    
    // Nuit: 22h-6h
    return (timeinfo->tm_hour >= 22 || timeinfo->tm_hour < 6);
}

uint8_t ntp_get_hour(void) {
    if (!ntp_is_synced()) {
        return 0;
    }
    
    time_t current_time = ntp_get_time();
    struct tm* timeinfo = localtime(&current_time);
    return timeinfo->tm_hour;
}

uint8_t ntp_get_minute(void) {
    if (!ntp_is_synced()) {
        return 0;
    }
    
    time_t current_time = ntp_get_time();
    struct tm* timeinfo = localtime(&current_time);
    return timeinfo->tm_min;
}

uint8_t ntp_get_second(void) {
    if (!ntp_is_synced()) {
        return 0;
    }
    
    time_t current_time = ntp_get_time();
    struct tm* timeinfo = localtime(&current_time);
    return timeinfo->tm_sec;
}