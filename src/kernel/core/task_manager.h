#ifndef DO_CORE_TASK_MANAGER_H
#define DO_CORE_TASK_MANAGER_H

#include "kernel.h"

// Constantes spécifiques au gestionnaire de tâches
#define TASK_NAME_MAX_LENGTH 32
#define TASK_STACK_MIN_SIZE 1024
#define TASK_STACK_MAX_SIZE 32768
#define TASK_PRIORITY_MIN 1
#define TASK_PRIORITY_MAX 25

// Types de tâches
typedef enum {
    TASK_TYPE_SYSTEM,      // Tâche système critique
    TASK_TYPE_APPLICATION, // Tâche d'application
    TASK_TYPE_USER,        // Tâche utilisateur
    TASK_TYPE_BACKGROUND   // Tâche en arrière-plan
} TaskType_t;

// Options de création de tâche
typedef struct {
    uint32_t stack_size;
    uint32_t priority;
    TaskType_t type;
    bool auto_delete;
    int32_t cpu_core;  // -1 pour auto-assignation, 0 pour CPU0, 1 pour CPU1
    bool pinned_to_core;  // Si true, force l'assignation au cœur spécifié
} TaskOptions_t;

// Statistiques de tâche
typedef struct {
    uint32_t run_time;
    uint32_t sleep_time;
    uint32_t context_switches;
    uint32_t stack_high_water_mark;
    uint32_t stack_used;
    uint32_t cpu_usage_percent;
} TaskStats_t;

// Informations détaillées sur une tâche
typedef struct {
    SystemTask_t basic_info;
    TaskType_t type;
    TaskStats_t stats;
    uint32_t last_run_time;
    uint32_t wake_count;
    bool is_suspended;
    bool is_blocked;
} TaskInfo_t;

// Callback pour les événements de tâche
typedef void (*TaskEventCallback_t)(uint8_t task_id, SystemEvent_t event);

// Initialisation et configuration
SysError_t task_manager_init(void);
void task_manager_deinit(void);
SysError_t task_manager_set_config(const char* key, const char* value);

// Création et suppression de tâches
SysError_t task_create(const char* name, void (*function)(void*), void* parameter, 
                      uint32_t priority, uint32_t stack_size, uint8_t* task_id);
SysError_t task_create_with_options(const char* name, void (*function)(void*), 
                                   void* parameter, const TaskOptions_t* options, 
                                   uint8_t* task_id);
SysError_t task_create_pinned_to_core(const char* name, void (*function)(void*), 
                                     void* parameter, uint32_t priority, uint32_t stack_size, 
                                     BaseType_t core_id, uint8_t* task_id);
SysError_t task_delete(uint8_t task_id);
SysError_t task_delete_by_name(const char* name);

// Contrôle des tâches
SysError_t task_suspend(uint8_t task_id);
SysError_t task_resume(uint8_t task_id);
SysError_t task_suspend_by_name(const char* name);
SysError_t task_resume_by_name(const char* name);
SysError_t task_delay(uint32_t milliseconds);
SysError_t task_yield(void);

// Informations sur les tâches
SysError_t task_get_info(uint8_t task_id, TaskInfo_t* info);
SysError_t task_get_info_by_name(const char* name, TaskInfo_t* info);
SysError_t task_get_stats(uint8_t task_id, TaskStats_t* stats);
uint8_t task_get_current_id(void);
const char* task_get_current_name(void);
TaskHandle_t task_get_current_handle(void);

// Énumération des tâches
uint8_t task_get_count(void);
SysError_t task_get_list(TaskInfo_t* task_list, uint8_t max_count, uint8_t* actual_count);
SysError_t task_get_active_list(TaskInfo_t* task_list, uint8_t max_count, uint8_t* actual_count);

// Gestion des priorités
SysError_t task_set_priority(uint8_t task_id, uint32_t priority);
SysError_t task_get_priority(uint8_t task_id, uint32_t* priority);
SysError_t task_set_priority_by_name(const char* name, uint32_t priority);

// Monitoring et statistiques
void task_manager_print_stats(void);
void task_manager_print_all_tasks(void);
SysError_t task_manager_get_system_stats(SystemStats_t* stats);
uint32_t task_manager_get_total_cpu_usage(void);

// Gestion des événements
SysError_t task_register_event_callback(TaskEventCallback_t callback);
SysError_t task_unregister_event_callback(TaskEventCallback_t callback);

// Utilitaires
const char* task_state_to_string(TaskState_t state);
const char* task_type_to_string(TaskType_t type);
bool task_exists(uint8_t task_id);
bool task_exists_by_name(const char* name);
bool task_is_running(uint8_t task_id);
bool task_is_suspended(uint8_t task_id);

// Tâches système prédéfinies
SysError_t task_create_idle_task(void);
SysError_t task_create_watchdog_task(void);
SysError_t task_create_monitor_task(void);

// Gestion des ressources
SysError_t task_allocate_stack(uint32_t size, void** stack_ptr);
SysError_t task_free_stack(void* stack_ptr);
uint32_t task_get_stack_size(uint8_t task_id);

// Debug et diagnostic
void task_manager_dump_state(void);
SysError_t task_debug_info(uint8_t task_id, char* buffer, size_t buffer_size);
void task_manager_print_stack_usage(void);

// Callbacks système
void task_manager_on_task_created(uint8_t task_id);
void task_manager_on_task_deleted(uint8_t task_id);
void task_manager_on_task_suspended(uint8_t task_id);
void task_manager_on_task_resumed(uint8_t task_id);

#endif // DO_CORE_TASK_MANAGER_H 