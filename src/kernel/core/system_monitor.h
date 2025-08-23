#ifndef DO_CORE_SYSTEM_MONITOR_H
#define DO_CORE_SYSTEM_MONITOR_H

#include "kernel.h"

// Constantes du moniteur système
#define MONITOR_UPDATE_INTERVAL_MS 1000
#define MONITOR_HISTORY_SIZE 60  // 1 minute d'historique
#define MONITOR_ALERT_THRESHOLD_CPU 80
#define MONITOR_ALERT_THRESHOLD_MEMORY 90
#define MONITOR_ALERT_THRESHOLD_TEMP 70

// Types d'alertes système
typedef enum {
    ALERT_TYPE_INFO,
    ALERT_TYPE_WARNING,
    ALERT_TYPE_ERROR,
    ALERT_TYPE_CRITICAL
} AlertType_t;

// Types de métriques
typedef enum {
    METRIC_CPU_USAGE,
    METRIC_MEMORY_USAGE,
    METRIC_FREE_HEAP,
    METRIC_TASK_COUNT,
    METRIC_EVENT_COUNT,
    METRIC_TEMPERATURE,
    METRIC_UPTIME,
    METRIC_WIFI_RSSI,
    METRIC_CUSTOM
} MetricType_t;

// Structure d'une métrique
typedef struct {
    MetricType_t type;
    char name[32];
    float value;
    float min_value;
    float max_value;
    float average_value;
    uint32_t timestamp;
    uint32_t update_count;
} SystemMetric_t;

// Structure d'une alerte
typedef struct {
    AlertType_t type;
    char message[128];
    uint32_t timestamp;
    uint32_t count;
    bool acknowledged;
    bool active;
} SystemAlert_t;

// Structure des performances système
typedef struct {
    uint32_t cpu_usage_percent;
    uint32_t memory_usage_percent;
    uint32_t free_heap_bytes;
    uint32_t min_free_heap_bytes;
    uint32_t active_tasks;
    uint32_t total_tasks;
    uint32_t events_per_second;
    float temperature_celsius;
    uint32_t uptime_seconds;
    int32_t wifi_rssi;
    uint32_t context_switches_per_second;
    uint32_t idle_time_percent;
} SystemPerformance_t;

// Structure de l'historique des performances
typedef struct {
    SystemPerformance_t data[MONITOR_HISTORY_SIZE];
    uint8_t index;
    uint8_t count;
    uint32_t last_update;
} PerformanceHistory_t;

// Structure des seuils d'alerte
typedef struct {
    uint32_t cpu_warning;
    uint32_t cpu_critical;
    uint32_t memory_warning;
    uint32_t memory_critical;
    float temperature_warning;
    float temperature_critical;
    uint32_t heap_warning;
    uint32_t heap_critical;
} AlertThresholds_t;

// Statistiques du moniteur
typedef struct {
    uint32_t total_alerts;
    uint32_t active_alerts;
    uint32_t acknowledged_alerts;
    uint32_t monitoring_uptime;
    uint32_t metric_updates;
    uint32_t alert_triggers;
    uint32_t system_resets;
} MonitorStats_t;

// Callback pour les alertes
typedef void (*AlertCallback_t)(SystemAlert_t* alert);
typedef void (*MetricCallback_t)(SystemMetric_t* metric);

// Initialisation et configuration
SysError_t system_monitor_init(void);
void system_monitor_deinit(void);
SysError_t system_monitor_set_config(const char* key, const char* value);

// Surveillance des performances
SysError_t system_monitor_start(void);
void system_monitor_stop(void);
bool system_monitor_is_running(void);
SysError_t system_monitor_set_update_interval(uint32_t interval_ms);

// Collecte de métriques
SysError_t system_monitor_get_performance(SystemPerformance_t* performance);
SysError_t system_monitor_get_metric(MetricType_t type, SystemMetric_t* metric);
SysError_t system_monitor_add_custom_metric(const char* name, float value);
SysError_t system_monitor_update_metric(MetricType_t type, float value);

// Historique des performances
SysError_t system_monitor_get_history(PerformanceHistory_t* history);
SysError_t system_monitor_clear_history(void);
SysError_t system_monitor_get_average_performance(SystemPerformance_t* average);
SysError_t system_monitor_get_peak_performance(SystemPerformance_t* peak);

// Gestion des alertes
SysError_t system_monitor_set_thresholds(AlertThresholds_t* thresholds);
SysError_t system_monitor_get_thresholds(AlertThresholds_t* thresholds);
SysError_t system_monitor_add_alert(AlertType_t type, const char* message);
SysError_t system_monitor_acknowledge_alert(uint32_t alert_id);
SysError_t system_monitor_clear_alert(uint32_t alert_id);
SysError_t system_monitor_clear_all_alerts(void);

// Récupération des alertes
uint32_t system_monitor_get_alert_count(void);
SysError_t system_monitor_get_alerts(SystemAlert_t* alerts, uint32_t max_count, uint32_t* actual_count);
SysError_t system_monitor_get_active_alerts(SystemAlert_t* alerts, uint32_t max_count, uint32_t* actual_count);

// Callbacks et événements
SysError_t system_monitor_register_alert_callback(AlertCallback_t callback);
SysError_t system_monitor_unregister_alert_callback(AlertCallback_t callback);
SysError_t system_monitor_register_metric_callback(MetricCallback_t callback);
SysError_t system_monitor_unregister_metric_callback(MetricCallback_t callback);

// Statistiques et reporting
SysError_t system_monitor_get_stats(MonitorStats_t* stats);
void system_monitor_print_stats(void);
void system_monitor_print_performance(void);
void system_monitor_print_alerts(void);
void system_monitor_print_metrics(void);

// Utilitaires
const char* alert_type_to_string(AlertType_t type);
const char* metric_type_to_string(MetricType_t type);
uint32_t system_monitor_get_uptime(void);
float system_monitor_get_cpu_usage(void);
float system_monitor_get_memory_usage(void);
float system_monitor_get_temperature(void);

// Gestion des ressources
SysError_t system_monitor_check_system_health(void);
bool system_monitor_is_system_healthy(void);
SysError_t system_monitor_get_health_score(uint8_t* score);

// Diagnostic et debug
void system_monitor_dump_state(void);
SysError_t system_monitor_debug_info(char* buffer, size_t buffer_size);
void system_monitor_print_detailed_stats(void);

// Tâches système du moniteur
void system_monitor_task(void* parameter);
void system_monitor_alert_task(void* parameter);
void system_monitor_metric_task(void* parameter);

// Callbacks système
void system_monitor_on_performance_update(SystemPerformance_t* performance);
void system_monitor_on_alert_triggered(SystemAlert_t* alert);
void system_monitor_on_metric_updated(SystemMetric_t* metric);
void system_monitor_on_system_error(SysError_t error);

// Macros utilitaires
#define MONITOR_ALERT(type, message) system_monitor_add_alert(type, message)
#define MONITOR_METRIC(type, value) system_monitor_update_metric(type, value)
#define MONITOR_CUSTOM_METRIC(name, value) system_monitor_add_custom_metric(name, value)

// Vérifications de sécurité
#define MONITOR_CHECK_HEALTH() \
    do { \
        if (!system_monitor_is_system_healthy()) { \
            kernel_log(LOG_LEVEL_WARN, "System health check failed"); \
        } \
    } while(0)

#endif // DO_CORE_SYSTEM_MONITOR_H 