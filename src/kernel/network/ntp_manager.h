#ifndef NTP_MANAGER_H
#define NTP_MANAGER_H

#include <Arduino.h>
#include <time.h>
#include "../core/kernel.h"

// Configuration NTP simple
#define NTP_SERVER "pool.ntp.org"
#define NTP_GMT_OFFSET_SEC 0  // UTC+1 (Maroc - WET/WEST) CORRIGÉ
#define NTP_DAYLIGHT_OFFSET_SEC 3600  // +1 heure en été (WEST)

// États de synchronisation NTP
typedef enum {
    NTP_STATUS_UNINITIALIZED = 0,
    NTP_STATUS_DISCONNECTED,
    NTP_STATUS_SYNCING,
    NTP_STATUS_SYNCED,
    NTP_STATUS_FAILED,
    NTP_STATUS_TIMEOUT
} NtpStatus_t;

// Informations de synchronisation
typedef struct {
    time_t last_sync_time;
    char last_sync_server[32];
    uint8_t successful_syncs;
    uint8_t failed_syncs;
} NtpSyncInfo_t;

#ifdef __cplusplus
extern "C" {
#endif

// Initialisation du NTP Manager
SysError_t ntp_init(void);

// Synchronisation manuelle
NtpStatus_t ntp_sync(void);

// Obtenir le statut de synchronisation
NtpStatus_t ntp_get_status(void);

// Obtenir les informations de synchronisation
void ntp_get_sync_info(NtpSyncInfo_t* info);

// Obtenir l'heure actuelle
time_t ntp_get_time(void);

// Formater l'heure en string
String ntp_format_time(time_t timestamp, const char* format = "%Y-%m-%d %H:%M:%S");

// Vérifier si l'heure est synchronisée
bool ntp_is_synced(void);

// Fonctions avancées pour l'intégration
bool ntp_is_time_valid(time_t timestamp);
time_t ntp_get_uptime_since_sync(void);
String ntp_get_timezone_string(void);
bool ntp_set_timezone(int gmt_offset_sec, int daylight_offset_sec);

// Fonctions utilitaires
bool ntp_is_business_hours(void);
bool ntp_is_night_time(void);
uint8_t ntp_get_hour(void);
uint8_t ntp_get_minute(void);
uint8_t ntp_get_second(void);

#ifdef __cplusplus
}
#endif

#endif // NTP_MANAGER_H 