#ifndef TASK_MANAGER_OPTIMIZED_H
#define TASK_MANAGER_OPTIMIZED_H

#include "../core/kernel.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Configuration optimisée
#define MAX_TASKS_OPTIMIZED 8  // Réduit de 16 à 8
#define MAX_CALLBACKS_OPTIMIZED 4  // Réduit de 8 à 4
#define QUEUE_SIZE_OPTIMIZED 8  // Réduit de 16 à 8

// Types simplifiés
typedef enum {
    TASK_STATE_OPTIMIZED_RUNNING = 0,
    TASK_STATE_OPTIMIZED_SUSPENDED,
    TASK_STATE_OPTIMIZED_DELETED
} TaskStateOptimized_t;

typedef enum {
    TASK_TYPE_OPTIMIZED_NORMAL = 0,
    TASK_TYPE_OPTIMIZED_PINNED
} TaskTypeOptimized_t;

// Structure simplifiée
typedef struct {
    uint8_t task_id;
    char name[16];  // Réduit de 32 à 16
    TaskHandle_t handle;
    TaskStateOptimized_t state;
    TaskTypeOptimized_t type;
    uint32_t priority;
    uint32_t stack_size;
    uint32_t stack_high_water_mark;
    uint32_t cpu_time;
} SystemTaskOptimized_t;

typedef struct {
    uint8_t task_id;
    char name[16];
    TaskStateOptimized_t state;
    uint32_t stack_high_water_mark;
    uint32_t cpu_time;
} TaskInfoOptimized_t;

// Fonctions essentielles seulement
SysError_t task_manager_optimized_init(void);
void task_manager_optimized_deinit(void);

SysError_t task_create_optimized(const char* name, void (*function)(void*), void* parameter, 
                                uint32_t priority, uint32_t stack_size, uint8_t* task_id);

SysError_t task_create_pinned_optimized(const char* name, void (*function)(void*), 
                                       void* parameter, uint32_t priority, uint32_t stack_size, 
                                       BaseType_t core_id, uint8_t* task_id);

SysError_t task_delete_optimized(uint8_t task_id);
uint8_t task_get_count_optimized(void);
bool task_exists_optimized(uint8_t task_id);

// Fonctions d'affichage simplifiées
void task_manager_print_stats_optimized(void);
void task_manager_print_all_tasks_optimized(void);

#endif // TASK_MANAGER_OPTIMIZED_H 