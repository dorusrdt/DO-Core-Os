#ifndef TIME_SYNC_MANAGER_H
#define TIME_SYNC_MANAGER_H

#include <Arduino.h>
#include <time.h>
#include "../core/kernel.h"

// Sources de temps disponibles
typedef enum {
    TIME_SOURCE_NTP = 0,
    TIME_SOURCE_RTC,
    TIME_SOURCE_SYSTEM,
    TIME_SOURCE_UNKNOWN
} TimeSource_t;

// Statuts de synchronisation
typedef enum {
    TIME_SYNC_STATUS_UNINITIALIZED = 0,
    TIME_SYNC_STATUS_OK,
    TIME_SYNC_STATUS_ERROR,
    TIME_SYNC_STATUS_NO_SOURCE,
    TIME_SYNC_STATUS_DEGRADED_MODE
} TimeSyncStatus_t;

#ifdef __cplusplus
extern "C" {
#endif

// Initialisation du gestionnaire de synchronisation
SysError_t time_sync_init(void);

// Synchronisation automatique (appelée par la tâche)
SysError_t time_sync_automatic(void);

// Obtenir la source de temps actuelle
TimeSource_t time_sync_get_current_source(void);

// Obtenir l'heure actuelle
time_t time_sync_get_current_time(void);

// Formater l'heure actuelle
String time_sync_format_current_time(void);

// Vérifier si le gestionnaire est initialisé
bool time_sync_is_initialized(void);

// Obtenir le statut de synchronisation
TimeSyncStatus_t time_sync_get_status(void);

// Obtenir des informations détaillées sur les sources de temps
String time_sync_get_source_info(void);

// Forcer une synchronisation immédiate (non-bloquante)
void time_sync_request_immediate(void);

// Vérifier si une synchronisation immédiate est demandée
bool time_sync_is_immediate_requested(void);

#ifdef __cplusplus
}
#endif

#endif // TIME_SYNC_MANAGER_H
