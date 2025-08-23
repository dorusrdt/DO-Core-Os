#include "system_monitor.h"
#include "task_manager.h"
#include <string.h>
#include <esp_system.h>
#include <esp_heap_caps.h>

// Variables globales du moniteur système
static SystemPerformance_t current_performance;
static PerformanceHistory_t performance_history;
static SystemAlert_t system_alerts[16];
static AlertThresholds_t alert_thresholds;
static MonitorStats_t monitor_stats;
static SemaphoreHandle_t monitor_mutex;

// Variables de contrôle
static bool monitor_initialized = false;
static bool monitor_running = false;
static uint32_t monitor_update_interval = MONITOR_UPDATE_INTERVAL_MS;
static uint32_t next_alert_id = 1;

// Callbacks
static AlertCallback_t alert_callbacks[MAX_CALLBACKS];
static MetricCallback_t metric_callbacks[MAX_CALLBACKS];
static uint8_t alert_callback_count = 0;
static uint8_t metric_callback_count = 0;

// Initialisation du moniteur système
SysError_t system_monitor_init(void) {
    Serial.println("Initializing System Monitor...");
    
    // Initialiser les structures
    memset(&current_performance, 0, sizeof(current_performance));
    memset(&performance_history, 0, sizeof(performance_history));
    memset(system_alerts, 0, sizeof(system_alerts));
    memset(&monitor_stats, 0, sizeof(monitor_stats));
    memset(alert_callbacks, 0, sizeof(alert_callbacks));
    memset(metric_callbacks, 0, sizeof(metric_callbacks));
    
    // Initialiser les seuils d'alerte par défaut
    alert_thresholds.cpu_warning = 70;
    alert_thresholds.cpu_critical = 90;
    alert_thresholds.memory_warning = 80;
    alert_thresholds.memory_critical = 95;
    alert_thresholds.temperature_warning = 60.0;
    alert_thresholds.temperature_critical = 80.0;
    alert_thresholds.heap_warning = 10000;
    alert_thresholds.heap_critical = 5000;
    
    // Créer les objets de synchronisation
    monitor_mutex = xSemaphoreCreateMutex();
    
    if (monitor_mutex == NULL) {
        Serial.println("ERROR: Failed to create monitor mutex!");
        return SYS_ERROR;
    }
    
    monitor_initialized = true;
    
    Serial.println("System Monitor initialized successfully!");
    return SYS_OK;
}

void system_monitor_deinit(void) {
    monitor_running = false;
    
    if (monitor_mutex != NULL) {
        vSemaphoreDelete(monitor_mutex);
    }
    
    Serial.println("System Monitor deinitialized");
}

// Démarrage/arrêt du monitoring
SysError_t system_monitor_start(void) {
    if (!monitor_initialized) {
        return SYS_ERROR;
    }
    
    if (monitor_running) {
        return SYS_OK; // Déjà en cours
    }
    
    monitor_running = true;
    
    Serial.println("System Monitor started");
    return SYS_OK;
}

void system_monitor_stop(void) {
    monitor_running = false;
    Serial.println("System Monitor stopped");
}

bool system_monitor_is_running(void) {
    return monitor_running;
}

// Gestion des performances
SysError_t system_monitor_get_performance(SystemPerformance_t* performance) {
    if (!monitor_initialized || performance == NULL) {
        return SYS_INVALID_PARAM;
    }
    
    if (xSemaphoreTake(monitor_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return SYS_BUSY;
    }
    
    // Mettre à jour les métriques
    current_performance.free_heap_bytes = esp_get_free_heap_size();
    current_performance.min_free_heap_bytes = esp_get_minimum_free_heap_size();
    current_performance.active_tasks = task_get_count();
    current_performance.uptime_seconds = xTaskGetTickCount() / 1000;
    current_performance.temperature_celsius = 25.0;
    current_performance.wifi_rssi = -50;
    current_performance.cpu_usage_percent = 50; // Valeur par défaut
    current_performance.memory_usage_percent = 30; // Valeur par défaut
    current_performance.events_per_second = 0;
    current_performance.context_switches_per_second = 1000;
    current_performance.idle_time_percent = 50;
    
    memcpy(performance, &current_performance, sizeof(SystemPerformance_t));
    
    xSemaphoreGive(monitor_mutex);
    return SYS_OK;
}

// Gestion des alertes
SysError_t system_monitor_add_alert(AlertType_t type, const char* message) {
    if (!monitor_initialized || message == NULL) {
        return SYS_INVALID_PARAM;
    }
    
    if (xSemaphoreTake(monitor_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return SYS_BUSY;
    }
    
    // Chercher un slot libre
    int slot = -1;
    for (int i = 0; i < 16; i++) {
        if (!system_alerts[i].active) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        xSemaphoreGive(monitor_mutex);
        return SYS_NO_MEMORY;
    }
    
    // Créer l'alerte
    SystemAlert_t* alert = &system_alerts[slot];
    alert->type = type;
    strncpy(alert->message, message, sizeof(alert->message) - 1);
    alert->timestamp = xTaskGetTickCount();
    alert->count = 1;
    alert->acknowledged = false;
    alert->active = true;
    
    monitor_stats.total_alerts++;
    monitor_stats.active_alerts++;
    monitor_stats.alert_triggers++;
    
    Serial.printf("Alert: %s - %s\n", alert_type_to_string(type), message);
    
    xSemaphoreGive(monitor_mutex);
    return SYS_OK;
}

// Statistiques et monitoring
SysError_t system_monitor_get_stats(MonitorStats_t* stats) {
    if (!monitor_initialized || stats == NULL) {
        return SYS_INVALID_PARAM;
    }
    
    if (xSemaphoreTake(monitor_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return SYS_BUSY;
    }
    
    memcpy(stats, &monitor_stats, sizeof(MonitorStats_t));
    
    xSemaphoreGive(monitor_mutex);
    return SYS_OK;
}

void system_monitor_print_stats(void) {
    MonitorStats_t stats;
    if (system_monitor_get_stats(&stats) == SYS_OK) {
        Serial.println("=== System Monitor Statistics ===");
        Serial.printf("Total alerts: %lu\n", stats.total_alerts);
        Serial.printf("Active alerts: %lu\n", stats.active_alerts);
        Serial.printf("Acknowledged alerts: %lu\n", stats.acknowledged_alerts);
        Serial.printf("Monitoring uptime: %lu seconds\n", stats.monitoring_uptime);
        Serial.printf("Metric updates: %lu\n", stats.metric_updates);
        Serial.printf("Alert triggers: %lu\n", stats.alert_triggers);
        Serial.printf("System resets: %lu\n", stats.system_resets);
        Serial.println("================================");
    }
}

void system_monitor_print_performance(void) {
    SystemPerformance_t perf;
    if (system_monitor_get_performance(&perf) == SYS_OK) {
        Serial.println("=== System Performance ===");
        Serial.printf("CPU Usage: %lu%%\n", perf.cpu_usage_percent);
        Serial.printf("Memory Usage: %lu%%\n", perf.memory_usage_percent);
        Serial.printf("Free Heap: %lu bytes\n", perf.free_heap_bytes);
        Serial.printf("Min Free Heap: %lu bytes\n", perf.min_free_heap_bytes);
        Serial.printf("Active Tasks: %lu\n", perf.active_tasks);
        Serial.printf("Uptime: %lu seconds\n", perf.uptime_seconds);
        Serial.printf("Temperature: %.1f°C\n", perf.temperature_celsius);
        Serial.printf("WiFi RSSI: %ld dBm\n", perf.wifi_rssi);
        Serial.printf("Events/sec: %lu\n", perf.events_per_second);
        Serial.printf("Context Switches/sec: %lu\n", perf.context_switches_per_second);
        Serial.printf("Idle Time: %lu%%\n", perf.idle_time_percent);
        Serial.println("=========================");
    }
}

void system_monitor_print_alerts(void) {
    Serial.println("=== System Alerts ===");
    
    for (int i = 0; i < 16; i++) {
        if (system_alerts[i].active) {
            Serial.printf("ID: %d, Type: %s, Message: %s, Count: %lu, Time: %lu\n",
                         i, alert_type_to_string(system_alerts[i].type),
                         system_alerts[i].message, system_alerts[i].count,
                         system_alerts[i].timestamp);
        }
    }
    
    Serial.println("====================");
}

// Utilitaires
const char* alert_type_to_string(AlertType_t type) {
    switch (type) {
        case ALERT_TYPE_INFO: return "INFO";
        case ALERT_TYPE_WARNING: return "WARNING";
        case ALERT_TYPE_ERROR: return "ERROR";
        case ALERT_TYPE_CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

const char* metric_type_to_string(MetricType_t type) {
    switch (type) {
        case METRIC_CPU_USAGE: return "CPU_USAGE";
        case METRIC_MEMORY_USAGE: return "MEMORY_USAGE";
        case METRIC_FREE_HEAP: return "FREE_HEAP";
        case METRIC_TASK_COUNT: return "TASK_COUNT";
        case METRIC_EVENT_COUNT: return "EVENT_COUNT";
        case METRIC_TEMPERATURE: return "TEMPERATURE";
        case METRIC_UPTIME: return "UPTIME";
        case METRIC_WIFI_RSSI: return "WIFI_RSSI";
        case METRIC_CUSTOM: return "CUSTOM";
        default: return "UNKNOWN";
    }
}

uint32_t system_monitor_get_uptime(void) {
    return xTaskGetTickCount() / 1000;
}

float system_monitor_get_cpu_usage(void) {
    SystemPerformance_t perf;
    if (system_monitor_get_performance(&perf) == SYS_OK) {
        return (float)perf.cpu_usage_percent;
    }
    return 0.0;
}

float system_monitor_get_memory_usage(void) {
    SystemPerformance_t perf;
    if (system_monitor_get_performance(&perf) == SYS_OK) {
        return (float)perf.memory_usage_percent;
    }
    return 0.0;
}

float system_monitor_get_temperature(void) {
    SystemPerformance_t perf;
    if (system_monitor_get_performance(&perf) == SYS_OK) {
        return perf.temperature_celsius;
    }
    return 0.0;
}

// Vérification de la santé du système
SysError_t system_monitor_check_system_health(void) {
    SystemPerformance_t perf;
    if (system_monitor_get_performance(&perf) != SYS_OK) {
        return SYS_ERROR;
    }
    
    // Vérifier les conditions critiques
    if (perf.cpu_usage_percent > alert_thresholds.cpu_critical ||
        perf.memory_usage_percent > alert_thresholds.memory_critical ||
        perf.free_heap_bytes < alert_thresholds.heap_critical ||
        perf.temperature_celsius > alert_thresholds.temperature_critical) {
        return SYS_ERROR;
    }
    
    return SYS_OK;
}

bool system_monitor_is_system_healthy(void) {
    return system_monitor_check_system_health() == SYS_OK;
}

// Callbacks système
void system_monitor_on_performance_update(SystemPerformance_t* performance) {
    // Callback appelé quand les performances sont mises à jour
}

void system_monitor_on_alert_triggered(SystemAlert_t* alert) {
    // Callback appelé quand une alerte est déclenchée
}

void system_monitor_on_metric_updated(SystemMetric_t* metric) {
    // Callback appelé quand une métrique est mise à jour
}

void system_monitor_on_system_error(SysError_t error) {
    // Callback appelé quand une erreur système se produit
    system_monitor_add_alert(ALERT_TYPE_ERROR, "System error occurred");
} 