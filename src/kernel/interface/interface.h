#ifndef DO_CORE_INTERFACE_H
#define DO_CORE_INTERFACE_H

#include "../core/kernel.h"

// Constantes
#define MAX_COMMANDS 100  // Augmenté de 80 à 100 pour plus de marge
#define MAX_COMMAND_LENGTH 64
#define MAX_PARAMETERS 8

// Structure d'une commande
typedef struct {
    char name[32];  // Augmenté de 16 à 32 pour les commandes longues (ex: irrig_config_server)
    char description[64];  // Augmenté de 32 à 64 pour descriptions plus longues
    SysError_t (*handler)(int argc, char* argv[]);
} Command_t;

// Fonctions principales
SysError_t interface_init(void);
void interface_start(void);

// Commandes système
SysError_t cmd_help(int argc, char* argv[]);
SysError_t cmd_status(int argc, char* argv[]);
SysError_t cmd_tasks(int argc, char* argv[]);
SysError_t cmd_memory(int argc, char* argv[]);
SysError_t cmd_dmesg(int argc, char* argv[]);
SysError_t cmd_logs(int argc, char* argv[]);
SysError_t cmd_clear(int argc, char* argv[]);
SysError_t cmd_uptime(int argc, char* argv[]);
SysError_t cmd_version(int argc, char* argv[]);
SysError_t cmd_log_echo(int argc, char* argv[]);
SysError_t cmd_test_log(int argc, char* argv[]);
SysError_t cmd_debug_log(int argc, char* argv[]);
SysError_t cmd_validate_log(int argc, char* argv[]);
SysError_t cmd_neofetch(int argc, char* argv[]);

// Commandes réseau
SysError_t cmd_wifi(int argc, char* argv[]);
SysError_t cmd_wifi_enable(int argc, char* argv[]);
SysError_t cmd_network(int argc, char* argv[]);
SysError_t cmd_wifi_scan(int argc, char* argv[]);
SysError_t cmd_wifi_reconnect(int argc, char* argv[]);
SysError_t cmd_wifi_stats(int argc, char* argv[]);
SysError_t cmd_wifi_save(int argc, char* argv[]);
SysError_t cmd_wifi_clear(int argc, char* argv[]);
SysError_t cmd_wifi_auto(int argc, char* argv[]);

// Commandes NTP minimalistes
SysError_t cmd_ntp_status(int argc, char* argv[]);
SysError_t cmd_ntp_sync(int argc, char* argv[]);
SysError_t cmd_ntp_test(int argc, char* argv[]);
SysError_t cmd_ntp_timezone(int argc, char* argv[]);
SysError_t cmd_ntp_utilities(int argc, char* argv[]);
SysError_t cmd_ntp_time(int argc, char* argv[]);

// Commandes Application Manager
SysError_t cmd_app_status(int argc, char* argv[]);
SysError_t cmd_app_list(int argc, char* argv[]);
SysError_t cmd_app_start(int argc, char* argv[]);
SysError_t cmd_app_stop(int argc, char* argv[]);
SysError_t cmd_app_restart(int argc, char* argv[]);
SysError_t cmd_app_pause(int argc, char* argv[]);
SysError_t cmd_app_resume(int argc, char* argv[]);
SysError_t cmd_app_info(int argc, char* argv[]);
SysError_t cmd_app_start_all(int argc, char* argv[]);
SysError_t cmd_app_stop_all(int argc, char* argv[]);
SysError_t cmd_app_pause_all(int argc, char* argv[]);
SysError_t cmd_app_resume_all(int argc, char* argv[]);

// Commandes réseau
SysError_t cmd_network_test(int argc, char* argv[]);

// Network commands - To be implemented



// Commandes RTC et Time Sync
SysError_t cmd_rtc_status(int argc, char* argv[]);
SysError_t cmd_rtc_recovery(int argc, char* argv[]);
SysError_t cmd_rtc_temp(int argc, char* argv[]);
SysError_t cmd_rtc_battery(int argc, char* argv[]);
SysError_t cmd_time_status(int argc, char* argv[]);
SysError_t cmd_time_source(int argc, char* argv[]);
SysError_t cmd_time_sync(int argc, char* argv[]);
SysError_t cmd_time_sources(int argc, char* argv[]);

// Commandes ESP-NOW
// Commandes ESP-NOW supprimées - structure de base uniquement

// Reserved for future commands

#endif // DO_CORE_INTERFACE_H