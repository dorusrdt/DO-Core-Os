#include "task_manager.h"
#include <string.h>
#include <esp_task_wdt.h>

// Variables globales du gestionnaire de tâches
static SystemTask_t tasks[MAX_TASKS];
static uint8_t next_task_id = 1;
static uint8_t active_task_count = 0;
static SemaphoreHandle_t task_manager_mutex;
static QueueHandle_t task_event_queue;
static TimerHandle_t task_monitor_timer;

// Callbacks pour les événements de tâche
static TaskEventCallback_t task_event_callbacks[MAX_CALLBACKS];
static uint8_t callback_count = 0;

// Statistiques globales
static uint32_t total_tasks_created = 0;
static uint32_t total_tasks_deleted = 0;
static uint32_t total_context_switches = 0;

// Fonction de monitoring des tâches
static void task_manager_monitor_tasks(void) {
    // Vérification de sécurité
    if (task_manager_mutex == NULL) {
        return;
    }
    
    if (xSemaphoreTake(task_manager_mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return; // Ne pas bloquer si le mutex n'est pas disponible
    }
    
    // Mettre à jour les statistiques des tâches
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].task_id != 0 && tasks[i].state != TASK_STATE_DELETED && tasks[i].handle != NULL) {
            // Vérification de sécurité avant d'accéder au handle
            if (tasks[i].handle != NULL) {
                // Mettre à jour le stack high water mark
                tasks[i].stack_high_water_mark = uxTaskGetStackHighWaterMark(tasks[i].handle);
                
                // Mettre à jour le temps CPU (approximation)
                tasks[i].cpu_time += 1; // Incrémenter le temps CPU
            }
        }
    }
    
    xSemaphoreGive(task_manager_mutex);
}

// Initialisation du gestionnaire de tâches
SysError_t task_manager_init(void) {
    Serial.println("Initializing Task Manager...");
    
    // Initialiser les structures
    memset(tasks, 0, sizeof(tasks));
    memset(task_event_callbacks, 0, sizeof(task_event_callbacks));
    
    // Créer les objets de synchronisation
    task_manager_mutex = xSemaphoreCreateMutex();
    task_event_queue = xQueueCreate(QUEUE_SIZE_DEFAULT, sizeof(uint8_t));
    
    if (task_manager_mutex == NULL || task_event_queue == NULL) {
        Serial.println("ERROR: Failed to create task manager objects!");
        return SYS_ERROR;
    }
    
    // Désactiver temporairement le timer de monitoring pour éviter les crashes
    /*
    task_monitor_timer = xTimerCreate("TaskMonitor", pdMS_TO_TICKS(5000), pdTRUE, NULL,
                                     [](TimerHandle_t timer) {
                                         task_manager_monitor_tasks();
                                     });
    
    if (task_monitor_timer == NULL) {
        Serial.println("ERROR: Failed to create task monitor timer!");
        return SYS_ERROR;
    }
    
    // Démarrer le timer
    xTimerStart(task_monitor_timer, 0);
    */
    
    Serial.println("Task Manager initialized successfully!");
    return SYS_OK;
}

void task_manager_deinit(void) {
    if (task_monitor_timer != NULL) {
        xTimerStop(task_monitor_timer, 0);
        xTimerDelete(task_monitor_timer, 0);
    }
    
    if (task_event_queue != NULL) {
        vQueueDelete(task_event_queue);
    }
    
    if (task_manager_mutex != NULL) {
        vSemaphoreDelete(task_manager_mutex);
    }
    
    Serial.println("Task Manager deinitialized");
}

// Création de tâches
SysError_t task_create(const char* name, void (*function)(void*), void* parameter, 
                      uint32_t priority, uint32_t stack_size, uint8_t* task_id) {
    if (xSemaphoreTake(task_manager_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return SYS_BUSY;
    }
    
    // Validation des paramètres
    if (name == NULL || function == NULL || task_id == NULL) {
        xSemaphoreGive(task_manager_mutex);
        return SYS_INVALID_PARAM;
    }
    
    if (stack_size < TASK_STACK_MIN_SIZE || stack_size > TASK_STACK_MAX_SIZE) {
        stack_size = STACK_SIZE_DEFAULT;
    }
    
    if (priority < TASK_PRIORITY_MIN || priority > TASK_PRIORITY_MAX) {
        priority = PRIORITY_NORMAL;
    }
    
    // Chercher un slot libre
    int slot = -1;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_STATE_DELETED || tasks[i].task_id == 0) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        xSemaphoreGive(task_manager_mutex);
        return SYS_NO_MEMORY;
    }
    
    // Créer la tâche
    SystemTask_t* task = &tasks[slot];
    task->task_id = next_task_id++;
    strncpy(task->name, name, sizeof(task->name) - 1);
    task->state = TASK_STATE_CREATED;
    task->priority = priority;
    task->stack_size = stack_size;
    task->entry_point = function;
    task->parameter = parameter;
    task->creation_time = xTaskGetTickCount();
    task->cpu_time = 0;
    task->stack_high_water_mark = 0;
    
    // Créer la tâche FreeRTOS
    BaseType_t result = xTaskCreate(function, task->name, stack_size, parameter, 
                                   priority, &task->handle);
    
    if (result != pdPASS) {
        task->state = TASK_STATE_DELETED;
        xSemaphoreGive(task_manager_mutex);
        return SYS_ERROR;
    }
    
    active_task_count++;
    total_tasks_created++;
    
    // Notifier les callbacks
    task_manager_on_task_created(task->task_id);
    
    *task_id = task->task_id;
    
    Serial.printf("Task created: ID=%d, Name=%s, Priority=%lu, Stack=%lu\n", 
                 task->task_id, task->name, task->priority, task->stack_size);
    
    xSemaphoreGive(task_manager_mutex);
    return SYS_OK;
}

SysError_t task_create_with_options(const char* name, void (*function)(void*), 
                                   void* parameter, const TaskOptions_t* options, 
                                   uint8_t* task_id) {
    if (options == NULL) {
        return task_create(name, function, parameter, PRIORITY_NORMAL, STACK_SIZE_DEFAULT, task_id);
    }
    
    // Si l'assignation de cœur est demandée
    if (options->pinned_to_core && options->cpu_core >= 0 && options->cpu_core <= 1) {
        return task_create_pinned_to_core(name, function, parameter, options->priority, 
                                        options->stack_size, options->cpu_core, task_id);
    }
    
    return task_create(name, function, parameter, options->priority, options->stack_size, task_id);
}

SysError_t task_create_pinned_to_core(const char* name, void (*function)(void*), 
                                     void* parameter, uint32_t priority, uint32_t stack_size, 
                                     BaseType_t core_id, uint8_t* task_id) {
    if (xSemaphoreTake(task_manager_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return SYS_BUSY;
    }
    
    // Validation des paramètres
    if (name == NULL || function == NULL || task_id == NULL) {
        xSemaphoreGive(task_manager_mutex);
        return SYS_INVALID_PARAM;
    }
    
    // Validation du cœur CPU
    if (core_id < 0 || core_id > 1) {
        xSemaphoreGive(task_manager_mutex);
        return SYS_INVALID_PARAM;
    }
    
    if (stack_size < TASK_STACK_MIN_SIZE || stack_size > TASK_STACK_MAX_SIZE) {
        stack_size = STACK_SIZE_DEFAULT;
    }
    
    if (priority < TASK_PRIORITY_MIN || priority > TASK_PRIORITY_MAX) {
        priority = PRIORITY_NORMAL;
    }
    
    // Chercher un slot libre
    int slot = -1;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_STATE_DELETED || tasks[i].task_id == 0) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        xSemaphoreGive(task_manager_mutex);
        return SYS_NO_MEMORY;
    }
    
    // Créer la tâche
    SystemTask_t* task = &tasks[slot];
    task->task_id = next_task_id++;
    strncpy(task->name, name, sizeof(task->name) - 1);
    task->state = TASK_STATE_CREATED;
    task->priority = priority;
    task->stack_size = stack_size;
    task->entry_point = function;
    task->parameter = parameter;
    task->creation_time = xTaskGetTickCount();
    task->cpu_time = 0;
    task->stack_high_water_mark = 0;
    
    // Créer la tâche FreeRTOS avec assignation de cœur
    BaseType_t result = xTaskCreatePinnedToCore(function, task->name, stack_size, parameter, 
                                               priority, &task->handle, core_id);
    
    if (result != pdPASS) {
        task->state = TASK_STATE_DELETED;
        xSemaphoreGive(task_manager_mutex);
        return SYS_ERROR;
    }
    
    active_task_count++;
    total_tasks_created++;
    
    // Notifier les callbacks
    task_manager_on_task_created(task->task_id);
    
    *task_id = task->task_id;
    
    Serial.printf("Task created on CPU%d: ID=%d, Name=%s, Priority=%lu, Stack=%lu\n", 
                 core_id, task->task_id, task->name, task->priority, task->stack_size);
    
    xSemaphoreGive(task_manager_mutex);
    return SYS_OK;
}

// Suppression de tâches
SysError_t task_delete(uint8_t task_id) {
    if (xSemaphoreTake(task_manager_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return SYS_BUSY;
    }
    
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].task_id == task_id) {
            if (tasks[i].state != TASK_STATE_DELETED) {
                vTaskDelete(tasks[i].handle);
                tasks[i].state = TASK_STATE_DELETED;
                active_task_count--;
                total_tasks_deleted++;
                
                // Notifier les callbacks
                task_manager_on_task_deleted(task_id);
                
                Serial.printf("Task deleted: ID=%d, Name=%s\n", task_id, tasks[i].name);
            }
            xSemaphoreGive(task_manager_mutex);
            return SYS_OK;
        }
    }
    
    xSemaphoreGive(task_manager_mutex);
    return SYS_NOT_FOUND;
}

SysError_t task_delete_by_name(const char* name) {
    if (name == NULL) {
        return SYS_INVALID_PARAM;
    }
    
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].task_id != 0 && strcmp(tasks[i].name, name) == 0) {
            return task_delete(tasks[i].task_id);
        }
    }
    
    return SYS_NOT_FOUND;
}

// Contrôle des tâches
SysError_t task_suspend(uint8_t task_id) {
    if (xSemaphoreTake(task_manager_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return SYS_BUSY;
    }
    
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].task_id == task_id) {
            if (tasks[i].state == TASK_STATE_RUNNING || tasks[i].state == TASK_STATE_READY) {
                vTaskSuspend(tasks[i].handle);
                tasks[i].state = TASK_STATE_SUSPENDED;
                
                // Notifier les callbacks
                task_manager_on_task_suspended(task_id);
                
                Serial.printf("Task suspended: ID=%d, Name=%s\n", task_id, tasks[i].name);
            }
            xSemaphoreGive(task_manager_mutex);
            return SYS_OK;
        }
    }
    
    xSemaphoreGive(task_manager_mutex);
    return SYS_NOT_FOUND;
}

SysError_t task_resume(uint8_t task_id) {
    if (xSemaphoreTake(task_manager_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return SYS_BUSY;
    }
    
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].task_id == task_id) {
            if (tasks[i].state == TASK_STATE_SUSPENDED) {
                vTaskResume(tasks[i].handle);
                tasks[i].state = TASK_STATE_READY;
                
                // Notifier les callbacks
                task_manager_on_task_resumed(task_id);
                
                Serial.printf("Task resumed: ID=%d, Name=%s\n", task_id, tasks[i].name);
            }
            xSemaphoreGive(task_manager_mutex);
            return SYS_OK;
        }
    }
    
    xSemaphoreGive(task_manager_mutex);
    return SYS_NOT_FOUND;
}

SysError_t task_suspend_by_name(const char* name) {
    if (name == NULL) {
        return SYS_INVALID_PARAM;
    }
    
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].task_id != 0 && strcmp(tasks[i].name, name) == 0) {
            return task_suspend(tasks[i].task_id);
        }
    }
    
    return SYS_NOT_FOUND;
}

SysError_t task_resume_by_name(const char* name) {
    if (name == NULL) {
        return SYS_INVALID_PARAM;
    }
    
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].task_id != 0 && strcmp(tasks[i].name, name) == 0) {
            return task_resume(tasks[i].task_id);
        }
    }
    
    return SYS_NOT_FOUND;
}

SysError_t task_delay(uint32_t milliseconds) {
    vTaskDelay(pdMS_TO_TICKS(milliseconds));
    return SYS_OK;
}

SysError_t task_yield(void) {
    taskYIELD();
    return SYS_OK;
}

// Informations sur les tâches
SysError_t task_get_info(uint8_t task_id, TaskInfo_t* info) {
    if (info == NULL) {
        return SYS_INVALID_PARAM;
    }
    
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].task_id == task_id) {
            memcpy(&info->basic_info, &tasks[i], sizeof(SystemTask_t));
            info->type = TASK_TYPE_APPLICATION; // Par défaut
            info->is_suspended = (tasks[i].state == TASK_STATE_SUSPENDED);
            info->is_blocked = (tasks[i].state == TASK_STATE_BLOCKED);
            info->last_run_time = xTaskGetTickCount();
            info->wake_count = 0; // À implémenter si nécessaire
            
            return SYS_OK;
        }
    }
    
    return SYS_NOT_FOUND;
}

SysError_t task_get_info_by_name(const char* name, TaskInfo_t* info) {
    if (name == NULL || info == NULL) {
        return SYS_INVALID_PARAM;
    }
    
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].task_id != 0 && strcmp(tasks[i].name, name) == 0) {
            return task_get_info(tasks[i].task_id, info);
        }
    }
    
    return SYS_NOT_FOUND;
}

uint8_t task_get_current_id(void) {
    TaskHandle_t current = xTaskGetCurrentTaskHandle();
    
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].handle == current) {
            return tasks[i].task_id;
        }
    }
    
    return 0;
}

const char* task_get_current_name(void) {
    TaskHandle_t current = xTaskGetCurrentTaskHandle();
    
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].handle == current) {
            return tasks[i].name;
        }
    }
    
    return "Unknown";
}

TaskHandle_t task_get_current_handle(void) {
    return xTaskGetCurrentTaskHandle();
}

// Énumération des tâches
uint8_t task_get_count(void) {
    return active_task_count;
}

SysError_t task_get_list(TaskInfo_t* task_list, uint8_t max_count, uint8_t* actual_count) {
    if (task_list == NULL || actual_count == NULL) {
        return SYS_INVALID_PARAM;
    }
    
    uint8_t count = 0;
    
    for (int i = 0; i < MAX_TASKS && count < max_count; i++) {
        if (tasks[i].task_id != 0 && tasks[i].state != TASK_STATE_DELETED) {
            task_get_info(tasks[i].task_id, &task_list[count]);
            count++;
        }
    }
    
    *actual_count = count;
    return SYS_OK;
}

SysError_t task_get_active_list(TaskInfo_t* task_list, uint8_t max_count, uint8_t* actual_count) {
    if (task_list == NULL || actual_count == NULL) {
        return SYS_INVALID_PARAM;
    }
    
    uint8_t count = 0;
    
    for (int i = 0; i < MAX_TASKS && count < max_count; i++) {
        if (tasks[i].task_id != 0 && tasks[i].state == TASK_STATE_RUNNING) {
            task_get_info(tasks[i].task_id, &task_list[count]);
            count++;
        }
    }
    
    *actual_count = count;
    return SYS_OK;
}

// Gestion des priorités
SysError_t task_set_priority(uint8_t task_id, uint32_t priority) {
    if (priority < TASK_PRIORITY_MIN || priority > TASK_PRIORITY_MAX) {
        return SYS_INVALID_PARAM;
    }
    
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].task_id == task_id) {
            vTaskPrioritySet(tasks[i].handle, priority);
            tasks[i].priority = priority;
            return SYS_OK;
        }
    }
    
    return SYS_NOT_FOUND;
}

SysError_t task_get_priority(uint8_t task_id, uint32_t* priority) {
    if (priority == NULL) {
        return SYS_INVALID_PARAM;
    }
    
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].task_id == task_id) {
            *priority = tasks[i].priority;
            return SYS_OK;
        }
    }
    
    return SYS_NOT_FOUND;
}

// Monitoring et statistiques
void task_manager_print_stats(void) {
    Serial.println("=== Task Manager Statistics ===");
    Serial.printf("Total tasks created: %lu\n", total_tasks_created);
    Serial.printf("Total tasks deleted: %lu\n", total_tasks_deleted);
    Serial.printf("Active tasks: %d\n", active_task_count);
    Serial.printf("Total context switches: %lu\n", total_context_switches);
    Serial.println("================================");
}

void task_manager_print_all_tasks(void) {
    Serial.println("=== All Tasks ===");
    
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].task_id != 0 && tasks[i].state != TASK_STATE_DELETED) {
            Serial.printf("ID: %d, Name: %s, State: %s, Priority: %lu\n",
                         tasks[i].task_id, tasks[i].name,
                         task_state_to_string(tasks[i].state), tasks[i].priority);
        }
    }
    
    Serial.println("==================");
}

// Utilitaires
const char* task_state_to_string(TaskState_t state) {
    switch (state) {
        case TASK_STATE_CREATED: return "CREATED";
        case TASK_STATE_READY: return "READY";
        case TASK_STATE_RUNNING: return "RUNNING";
        case TASK_STATE_BLOCKED: return "BLOCKED";
        case TASK_STATE_SUSPENDED: return "SUSPENDED";
        case TASK_STATE_DELETED: return "DELETED";
        default: return "UNKNOWN";
    }
}

const char* task_type_to_string(TaskType_t type) {
    switch (type) {
        case TASK_TYPE_SYSTEM: return "SYSTEM";
        case TASK_TYPE_APPLICATION: return "APPLICATION";
        case TASK_TYPE_USER: return "USER";
        case TASK_TYPE_BACKGROUND: return "BACKGROUND";
        default: return "UNKNOWN";
    }
}

bool task_exists(uint8_t task_id) {
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].task_id == task_id && tasks[i].state != TASK_STATE_DELETED) {
            return true;
        }
    }
    return false;
}

bool task_exists_by_name(const char* name) {
    if (name == NULL) {
        return false;
    }
    
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].task_id != 0 && strcmp(tasks[i].name, name) == 0) {
            return true;
        }
    }
    return false;
}

bool task_is_running(uint8_t task_id) {
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].task_id == task_id) {
            return (tasks[i].state == TASK_STATE_RUNNING);
        }
    }
    return false;
}

bool task_is_suspended(uint8_t task_id) {
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].task_id == task_id) {
            return (tasks[i].state == TASK_STATE_SUSPENDED);
        }
    }
    return false;
}

// Callbacks système
void task_manager_on_task_created(uint8_t task_id) {
    for (int i = 0; i < callback_count; i++) {
        if (task_event_callbacks[i] != NULL) {
            task_event_callbacks[i](task_id, EVENT_TASK_CREATED);
        }
    }
}

void task_manager_on_task_deleted(uint8_t task_id) {
    for (int i = 0; i < callback_count; i++) {
        if (task_event_callbacks[i] != NULL) {
            task_event_callbacks[i](task_id, EVENT_TASK_DELETED);
        }
    }
}

void task_manager_on_task_suspended(uint8_t task_id) {
    for (int i = 0; i < callback_count; i++) {
        if (task_event_callbacks[i] != NULL) {
            task_event_callbacks[i](task_id, EVENT_TASK_DELETED); // Utiliser un événement disponible
        }
    }
}

void task_manager_on_task_resumed(uint8_t task_id) {
    for (int i = 0; i < callback_count; i++) {
        if (task_event_callbacks[i] != NULL) {
            task_event_callbacks[i](task_id, EVENT_TASK_CREATED); // Utiliser un événement disponible
        }
    }
} 