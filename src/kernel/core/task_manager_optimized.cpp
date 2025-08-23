#include "task_manager_optimized.h"
#include <string.h>

// Variables globales optimisées
static SystemTaskOptimized_t tasks[MAX_TASKS_OPTIMIZED];
static uint8_t next_task_id = 1;
static uint8_t active_task_count = 0;
static SemaphoreHandle_t task_manager_mutex;

// Initialisation optimisée
SysError_t task_manager_optimized_init(void) {
    Serial.println("Initializing Optimized Task Manager...");
    
    // Initialiser les structures
    memset(tasks, 0, sizeof(tasks));
    
    // Créer le mutex seulement
    task_manager_mutex = xSemaphoreCreateMutex();
    
    if (task_manager_mutex == NULL) {
        Serial.println("ERROR: Failed to create task manager mutex!");
        return SYS_ERROR;
    }
    
    Serial.println("Optimized Task Manager initialized!");
    return SYS_OK;
}

void task_manager_optimized_deinit(void) {
    if (task_manager_mutex != NULL) {
        vSemaphoreDelete(task_manager_mutex);
    }
}

// Création de tâche optimisée
SysError_t task_create_optimized(const char* name, void (*function)(void*), void* parameter, 
                                uint32_t priority, uint32_t stack_size, uint8_t* task_id) {
    if (xSemaphoreTake(task_manager_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return SYS_ERROR;
    }
    
    // Chercher un slot libre
    int free_slot = -1;
    for (int i = 0; i < MAX_TASKS_OPTIMIZED; i++) {
        if (tasks[i].task_id == 0) {
            free_slot = i;
            break;
        }
    }
    
    if (free_slot == -1) {
        xSemaphoreGive(task_manager_mutex);
        return SYS_ERROR;
    }
    
    // Créer la tâche
    TaskHandle_t handle;
    BaseType_t result = xTaskCreate(function, name, stack_size, parameter, priority, &handle);
    
    if (result != pdPASS) {
        xSemaphoreGive(task_manager_mutex);
        return SYS_ERROR;
    }
    
    // Enregistrer la tâche
    tasks[free_slot].task_id = next_task_id++;
    strncpy(tasks[free_slot].name, name, sizeof(tasks[free_slot].name) - 1);
    tasks[free_slot].handle = handle;
    tasks[free_slot].state = TASK_STATE_OPTIMIZED_RUNNING;
    tasks[free_slot].type = TASK_TYPE_OPTIMIZED_NORMAL;
    tasks[free_slot].priority = priority;
    tasks[free_slot].stack_size = stack_size;
    tasks[free_slot].stack_high_water_mark = uxTaskGetStackHighWaterMark(handle);
    tasks[free_slot].cpu_time = 0;
    
    active_task_count++;
    
    if (task_id != NULL) {
        *task_id = tasks[free_slot].task_id;
    }
    
    xSemaphoreGive(task_manager_mutex);
    return SYS_OK;
}

// Création de tâche épinglée optimisée
SysError_t task_create_pinned_optimized(const char* name, void (*function)(void*), 
                                       void* parameter, uint32_t priority, uint32_t stack_size, 
                                       BaseType_t core_id, uint8_t* task_id) {
    if (xSemaphoreTake(task_manager_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return SYS_ERROR;
    }
    
    // Chercher un slot libre
    int free_slot = -1;
    for (int i = 0; i < MAX_TASKS_OPTIMIZED; i++) {
        if (tasks[i].task_id == 0) {
            free_slot = i;
            break;
        }
    }
    
    if (free_slot == -1) {
        xSemaphoreGive(task_manager_mutex);
        return SYS_ERROR;
    }
    
    // Créer la tâche épinglée
    TaskHandle_t handle;
    BaseType_t result = xTaskCreatePinnedToCore(function, name, stack_size, parameter, priority, &handle, core_id);
    
    if (result != pdPASS) {
        xSemaphoreGive(task_manager_mutex);
        return SYS_ERROR;
    }
    
    // Enregistrer la tâche
    tasks[free_slot].task_id = next_task_id++;
    strncpy(tasks[free_slot].name, name, sizeof(tasks[free_slot].name) - 1);
    tasks[free_slot].handle = handle;
    tasks[free_slot].state = TASK_STATE_OPTIMIZED_RUNNING;
    tasks[free_slot].type = TASK_TYPE_OPTIMIZED_PINNED;
    tasks[free_slot].priority = priority;
    tasks[free_slot].stack_size = stack_size;
    tasks[free_slot].stack_high_water_mark = uxTaskGetStackHighWaterMark(handle);
    tasks[free_slot].cpu_time = 0;
    
    active_task_count++;
    
    if (task_id != NULL) {
        *task_id = tasks[free_slot].task_id;
    }
    
    xSemaphoreGive(task_manager_mutex);
    return SYS_OK;
}

// Suppression optimisée
SysError_t task_delete_optimized(uint8_t task_id) {
    if (xSemaphoreTake(task_manager_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return SYS_ERROR;
    }
    
    for (int i = 0; i < MAX_TASKS_OPTIMIZED; i++) {
        if (tasks[i].task_id == task_id) {
            if (tasks[i].handle != NULL) {
                vTaskDelete(tasks[i].handle);
            }
            tasks[i].task_id = 0;
            active_task_count--;
            xSemaphoreGive(task_manager_mutex);
            return SYS_OK;
        }
    }
    
    xSemaphoreGive(task_manager_mutex);
    return SYS_ERROR;
}

// Fonctions utilitaires optimisées
uint8_t task_get_count_optimized(void) {
    return active_task_count;
}

bool task_exists_optimized(uint8_t task_id) {
    for (int i = 0; i < MAX_TASKS_OPTIMIZED; i++) {
        if (tasks[i].task_id == task_id) {
            return true;
        }
    }
    return false;
}

// Affichage optimisé
void task_manager_print_stats_optimized(void) {
    Serial.println("=== Optimized Task Manager Stats ===");
    Serial.printf("Active tasks: %d/%d\n", active_task_count, MAX_TASKS_OPTIMIZED);
    Serial.printf("Next task ID: %d\n", next_task_id);
    Serial.println("===================================");
}

void task_manager_print_all_tasks_optimized(void) {
    Serial.println("=== Optimized Task List ===");
    
    for (int i = 0; i < MAX_TASKS_OPTIMIZED; i++) {
        if (tasks[i].task_id != 0) {
            const char* state_str = (tasks[i].state == TASK_STATE_OPTIMIZED_RUNNING) ? "RUNNING" :
                                   (tasks[i].state == TASK_STATE_OPTIMIZED_SUSPENDED) ? "SUSPENDED" : "DELETED";
            
            const char* type_str = (tasks[i].type == TASK_TYPE_OPTIMIZED_NORMAL) ? "NORMAL" : "PINNED";
            
            Serial.printf("ID: %d | %-12s | %s | %s | Stack: %lu/%lu\n", 
                         tasks[i].task_id, tasks[i].name, state_str, type_str,
                         tasks[i].stack_high_water_mark, tasks[i].stack_size);
        }
    }
    
    Serial.println("============================");
} 