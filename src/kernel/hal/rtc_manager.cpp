#include "rtc_manager.h"
#include "../core/kernel.h"
#include <ThreeWire.h>
#include <RtcDS1302.h>

// Variables globales
static ThreeWire myWire(RTC_DAT_PIN, RTC_CLK_PIN, RTC_RST_PIN);
static RtcDS1302<ThreeWire> rtc(myWire);
static RtcStatus_t g_rtc_status = RTC_STATUS_UNINITIALIZED;
static bool g_rtc_initialized = false;
static uint32_t g_rtc_recovery_attempts = 0;
static uint32_t g_rtc_last_error_time = 0;

// Initialisation du RTC Manager
SysError_t rtc_manager_init(void) {
    if (g_rtc_initialized) {
        return SYS_ALREADY_INITIALIZED;
    }

    kernel_log(LOG_LEVEL_INFO, "Initializing RTC Manager (DS1302) - RST:%d, DAT:%d, CLK:%d",
                RTC_RST_PIN, RTC_DAT_PIN, RTC_CLK_PIN);

    // Initialiser le RTC DS1302
    rtc.Begin();

    // Attendre un peu après l'initialisation
    delay(50);

    // Toujours commencer par désactiver la protection écriture et vérifier l'oscillateur
    kernel_log(LOG_LEVEL_INFO, "Checking RTC write protection and oscillator...");
    kernel_log(LOG_LEVEL_DEBUG, "RTC IsRunning: %s, IsWriteProtected: %s, IsDateTimeValid: %s",
               rtc.GetIsRunning() ? "YES" : "NO",
               rtc.GetIsWriteProtected() ? "YES" : "NO",
               rtc.IsDateTimeValid() ? "YES" : "NO");

    // Désactiver la protection écriture si nécessaire
    if (rtc.GetIsWriteProtected()) {
        kernel_log(LOG_LEVEL_INFO, "RTC write protection detected - disabling");
        rtc.SetIsWriteProtected(false);
        delay(10);
        kernel_log(LOG_LEVEL_DEBUG, "Write protection disabled");
    } else {
        kernel_log(LOG_LEVEL_DEBUG, "Write protection already disabled");
    }

    // Démarrer l'oscillateur si nécessaire
    if (!rtc.GetIsRunning()) {
        kernel_log(LOG_LEVEL_INFO, "RTC oscillator not running - starting");
        rtc.SetIsRunning(true);
        delay(100); // Attendre que l'oscillateur démarre

        if (!rtc.GetIsRunning()) {
            kernel_log(LOG_LEVEL_ERROR, "Failed to start RTC oscillator");
            g_rtc_status = RTC_STATUS_ERROR;
            g_rtc_last_error_time = millis();
            return SYS_ERROR;
        }
        kernel_log(LOG_LEVEL_DEBUG, "RTC oscillator started successfully");
    } else {
        kernel_log(LOG_LEVEL_DEBUG, "RTC oscillator already running");
    }

    // Maintenant vérifier si l'heure est valide
    if (!rtc.IsDateTimeValid()) {
        kernel_log(LOG_LEVEL_WARN, "RTC time is not valid - initializing with compile time");

        // Régler la date et l'heure de compilation
        RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
        rtc.SetDateTime(compiled);

        // Vérifier si la programmation a fonctionné
        delay(100);
        if (!rtc.IsDateTimeValid()) {
            kernel_log(LOG_LEVEL_ERROR, "RTC initialization failed!");
            g_rtc_status = RTC_STATUS_ERROR;
            g_rtc_last_error_time = millis();
            return SYS_ERROR;
        } else {
            kernel_log(LOG_LEVEL_INFO, "RTC initialized with compile time");
            g_rtc_status = RTC_STATUS_OK;
        }
    } else {
        kernel_log(LOG_LEVEL_INFO, "RTC time is already valid");
        g_rtc_status = RTC_STATUS_OK;
    }

    // Vérification finale avant de valider l'initialisation
    kernel_log(LOG_LEVEL_DEBUG, "Final RTC status check:");
    kernel_log(LOG_LEVEL_DEBUG, "  IsRunning: %s", rtc.GetIsRunning() ? "YES" : "NO");
    kernel_log(LOG_LEVEL_DEBUG, "  IsWriteProtected: %s", rtc.GetIsWriteProtected() ? "YES" : "NO");
    kernel_log(LOG_LEVEL_DEBUG, "  IsDateTimeValid: %s", rtc.IsDateTimeValid() ? "YES" : "NO");

    // Vérifier que tout est OK
    if (!rtc.GetIsRunning() || rtc.GetIsWriteProtected() || !rtc.IsDateTimeValid()) {
        kernel_log(LOG_LEVEL_ERROR, "RTC final check failed - initialization incomplete");
        g_rtc_status = RTC_STATUS_ERROR;
        return SYS_ERROR;
    }

    g_rtc_initialized = true;
    g_rtc_recovery_attempts = 0;

    kernel_log(LOG_LEVEL_INFO, "RTC Manager initialized successfully");
    kernel_log(LOG_LEVEL_INFO, "RTC Status: %s", rtc_get_status_string().c_str());

    // Afficher l'heure actuelle
    RtcDateTime now = rtc.GetDateTime();
    char timeStr[20];
    snprintf(timeStr, sizeof(timeStr), "%04u-%02u-%02u %02u:%02u:%02u",
             now.Year(), now.Month(), now.Day(),
             now.Hour(), now.Minute(), now.Second());
    kernel_log(LOG_LEVEL_INFO, "RTC Current Time: %s", timeStr);

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

    if (g_rtc_status != RTC_STATUS_OK) {
        return 0;
    }

    // Vérifier si le RTC fonctionne et a une heure valide
    if (!rtc.GetIsRunning() || !rtc.IsDateTimeValid()) {
        kernel_log(LOG_LEVEL_ERROR, "RTC not running or invalid time");
        g_rtc_status = RTC_STATUS_ERROR;
        g_rtc_last_error_time = millis();
        return 0;
    }

    RtcDateTime now = rtc.GetDateTime();

    // Convertir RtcDateTime en timestamp Unix
    // RtcDateTime stocke l'année comme année complète (2024), mais tm_year attend années depuis 1900
    struct tm timeStruct;
    timeStruct.tm_year = now.Year() - 1900;  // Années depuis 1900
    timeStruct.tm_mon = now.Month() - 1;     // Mois 0-11
    timeStruct.tm_mday = now.Day();          // Jour du mois 1-31
    timeStruct.tm_hour = now.Hour();         // Heures 0-23
    timeStruct.tm_min = now.Minute();        // Minutes 0-59
    timeStruct.tm_sec = now.Second();        // Secondes 0-59
    timeStruct.tm_isdst = -1;                // DST inconnu

    time_t timestamp = mktime(&timeStruct);

    // Vérifier si l'heure est raisonnable
    if (timestamp < 1577836800) { // Avant 2020
        kernel_log(LOG_LEVEL_WARN, "RTC time seems invalid: %ld", timestamp);
        return 0;
    }

    return timestamp;
}

// Définir l'heure du RTC (timestamp Unix)
SysError_t rtc_set_time(time_t timestamp) {
    if (!g_rtc_initialized) {
        return SYS_NOT_INITIALIZED;
    }

    if (g_rtc_status != RTC_STATUS_OK) {
        return SYS_ERROR;
    }

    // Convertir timestamp Unix en RtcDateTime
    struct tm* timeStruct = localtime(&timestamp);
    RtcDateTime dt(timeStruct->tm_year + 1900,  // Année complète
                   timeStruct->tm_mon + 1,      // Mois 1-12
                   timeStruct->tm_mday,         // Jour
                   timeStruct->tm_hour,         // Heure
                   timeStruct->tm_min,          // Minute
                   timeStruct->tm_sec);         // Seconde

    rtc.SetDateTime(dt);

    // Vérifier que la programmation a fonctionné
    delay(100);
    if (!rtc.IsDateTimeValid()) {
        kernel_log(LOG_LEVEL_ERROR, "RTC time setting failed");
        g_rtc_status = RTC_STATUS_ERROR;
        return SYS_ERROR;
    }

    kernel_log(LOG_LEVEL_INFO, "RTC time set to: %ld", timestamp);
    return SYS_OK;
}

// Obtenir la température du RTC
float rtc_get_temperature(void) {
    // Le DS1302 n'a pas de capteur de température intégré
    // Retourner une valeur par défaut
    return -999.0f;
}

// Vérifier l'état de la pile
bool rtc_is_battery_ok(void) {
    if (!g_rtc_initialized) {
        return false;
    }

    // Le DS1302 n'a pas de détection de pile intégrée comme le DS3231
    // On considère que la pile est OK si le RTC fonctionne
    return rtc.GetIsRunning() && rtc.IsDateTimeValid();
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

    // Pour le DS1302, la récupération consiste à :
    // 1. Vérifier si l'oscillateur fonctionne
    // 2. Réinitialiser l'heure si nécessaire
    // 3. Vérifier la validité des données

    delay(100); // Petit délai

    // Vérifier et redémarrer l'oscillateur si nécessaire
    if (!rtc.GetIsRunning()) {
        kernel_log(LOG_LEVEL_INFO, "Restarting RTC oscillator");
        rtc.SetIsRunning(true);
        delay(100);
    }

    // Désactiver la protection écriture si nécessaire
    if (rtc.GetIsWriteProtected()) {
        kernel_log(LOG_LEVEL_INFO, "Disabling RTC write protection");
        rtc.SetIsWriteProtected(false);
    }

    // Vérifier si l'heure est valide maintenant
    if (rtc.GetIsRunning() && rtc.IsDateTimeValid()) {
        kernel_log(LOG_LEVEL_INFO, "RTC recovery successful!");
        g_rtc_status = RTC_STATUS_OK;
        g_rtc_recovery_attempts = 0;
        return SYS_OK;
    } else {
        // Si toujours invalide, essayer de remettre l'heure de compilation
        kernel_log(LOG_LEVEL_WARN, "RTC still invalid, resetting to compile time");
        RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
        rtc.SetDateTime(compiled);

        delay(100);
        if (rtc.IsDateTimeValid()) {
            kernel_log(LOG_LEVEL_INFO, "RTC recovery with compile time successful!");
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

