#ifndef DO_CORE_KERNEL_H
#define DO_CORE_KERNEL_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/timers.h>
#include <esp_system.h>
#include <esp_heap_caps.h>

// Version et informations du système
#define DO_CORE_VERSION "0.1.0"
#define DO_CORE_NAME "D'O-Core"
#define DO_CORE_DESCRIPTION "Mini OS basé sur Arduino Framework et FreeRTOS"

// Constantes système
#define MAX_TASKS 32
#define MAX_PROCESSES 16
#define MAX_EVENTS 64
#define MAX_DEVICES 20
#define MAX_CALLBACKS 32
#define STACK_SIZE_DEFAULT 4096
#define STACK_SIZE_SMALL 2048
#define STACK_SIZE_LARGE 8192
#define STACK_SIZE_SHELL 16384  // Taille de pile augmentée pour éviter les stack overflows (16KB)
#define QUEUE_SIZE_DEFAULT 10
#define QUEUE_SIZE_LARGE 50

// Système de logs
#define LOG_BUFFER_SIZE 100
#define LOG_MAX_MESSAGE_LENGTH 128
#define LOG_MAX_SOURCE_LENGTH 16

// Priorités des tâches
#define PRIORITY_IDLE 1
#define PRIORITY_LOW 2
#define PRIORITY_NORMAL 3
#define PRIORITY_HIGH 4
#define PRIORITY_CRITICAL 5

// États des tâches
typedef enum {
    TASK_STATE_CREATED,
    TASK_STATE_READY,
    TASK_STATE_RUNNING,
    TASK_STATE_BLOCKED,
    TASK_STATE_SUSPENDED,
    TASK_STATE_DELETED
} TaskState_t;

// États des processus
typedef enum {
    PROCESS_STATE_CREATED,
    PROCESS_STATE_READY,
    PROCESS_STATE_RUNNING,
    PROCESS_STATE_BLOCKED,
    PROCESS_STATE_SLEEPING,
    PROCESS_STATE_TERMINATED
} ProcessState_t;

// Types d'événements système
typedef enum {
    EVENT_SYSTEM_START,
    EVENT_SYSTEM_STOP,
    EVENT_TASK_CREATED,
    EVENT_TASK_DELETED,
    EVENT_MEMORY_LOW,
    EVENT_MEMORY_CRITICAL,
    EVENT_WIFI_CONNECTED,
    EVENT_WIFI_DISCONNECTED,
    EVENT_SENSOR_READ,
    EVENT_ACTUATOR_CONTROL,
    EVENT_USER_INPUT,
    EVENT_SYSTEM_ERROR,
    EVENT_CUSTOM = 100  // Événements personnalisés à partir de 100
} SystemEvent_t;

// Codes d'erreur système
typedef enum {
    SYS_OK = 0,
    SYS_ERROR = -1,
    SYS_INVALID_PARAM = -2,
    SYS_NO_MEMORY = -3,
    SYS_BUSY = -4,
    SYS_TIMEOUT = -5,
    SYS_NOT_FOUND = -6,
    SYS_ALREADY_EXISTS = -7,
    SYS_NOT_INITIALIZED = -8,
    SYS_ALREADY_INITIALIZED = -9,
    SYS_NO_DATA = -10
} SysError_t;

// Structure d'une tâche système
typedef struct {
    uint8_t task_id;
    char name[32];
    TaskState_t state;
    uint32_t priority;
    uint32_t stack_size;
    uint32_t stack_high_water_mark;
    uint32_t cpu_time;
    uint32_t creation_time;
    TaskHandle_t handle;
    void (*entry_point)(void*);
    void* parameter;
} SystemTask_t;

// Structure d'un processus
typedef struct {
    uint8_t process_id;
    char name[32];
    ProcessState_t state;
    uint32_t priority;
    uint32_t memory_usage;
    uint32_t cpu_time;
    uint32_t creation_time;
    SystemTask_t* tasks[MAX_TASKS];
    uint8_t task_count;
} Process_t;

// Structure d'un événement
typedef struct {
    SystemEvent_t type;
    uint32_t timestamp;
    uint8_t source_id;
    uint8_t data_size;
    void* data;
} Event_t;

// Structure d'un callback
typedef struct {
    uint8_t callback_id;
    SystemEvent_t event_type;
    void (*function)(Event_t*);
    bool active;
} Callback_t;

// Structure des statistiques système
typedef struct {
    uint32_t uptime;
    uint32_t free_heap;
    uint32_t min_free_heap;
    uint32_t total_allocated;
    uint8_t active_tasks;
    uint8_t active_processes;
    uint32_t cpu_usage;
    uint32_t event_count;
} SystemStats_t;

// Configuration système
void kernel_set_config(const char* key, const char* value);
const char* kernel_get_config(const char* key);

// Logs système
typedef enum {
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_CRITICAL
} LogLevel_t;

// Structure d'un message de log
typedef struct {
    uint32_t timestamp;
    LogLevel_t level;
    char source[LOG_MAX_SOURCE_LENGTH];
    char message[LOG_MAX_MESSAGE_LENGTH];
    uint8_t task_id;
} LogMessage_t;

// Structure du buffer de logs
typedef struct {
    LogMessage_t messages[LOG_BUFFER_SIZE];
    uint32_t head;
    uint32_t tail;
    uint32_t count;
    uint32_t total_messages;
    SemaphoreHandle_t mutex;
} LogBuffer_t;

// Déclarations des fonctions du noyau
void kernel_init(void);
void kernel_start(void);
void kernel_stop(void);
void kernel_reset(void);

// Fonctions utilitaires
uint32_t kernel_get_uptime(void);
uint32_t kernel_get_free_memory(void);
uint32_t kernel_get_min_free_memory(void);
SystemStats_t kernel_get_stats(void);
void kernel_print_stats(void);

// Gestion des erreurs
const char* kernel_get_error_string(SysError_t error);
void kernel_set_error_handler(void (*handler)(SysError_t, const char*));

// Utilitaires
const char* log_level_to_string(LogLevel_t level);

// Système de logs avancé
SysError_t log_system_init(void);
void log_system_deinit(void);
SysError_t log_system_add_message(LogLevel_t level, const char* source, const char* message);
SysError_t log_system_get_messages(LogMessage_t* messages, uint32_t max_count, uint32_t* actual_count);
SysError_t log_system_clear_buffer(void);
SysError_t log_system_print_buffer(void);
SysError_t log_system_print_dmesg(void);
SysError_t log_system_print_tail(uint32_t count);
SysError_t log_system_print_by_level(LogLevel_t level);
SysError_t log_system_print_by_source(const char* source);
uint32_t log_system_get_count(void);
uint32_t log_system_get_total_messages(void);

// Fonctions de logs
SysError_t kernel_log(LogLevel_t level, const char* format, ...);
void kernel_set_log_level(LogLevel_t level);

// Fonctions de contrôle
void log_system_enable_echo(bool enable);
bool log_system_is_echo_enabled(void);
SysError_t log_system_validate(void);

// Contrôle de l'echo des logs
void log_system_enable_echo(bool enable);
bool log_system_is_echo_enabled(void);

// Macros utiles
#define KERNEL_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            kernel_log(LOG_LEVEL_CRITICAL, "Assertion failed: %s", #condition); \
            kernel_reset(); \
        } \
    } while(0)

#define KERNEL_CHECK_NULL(ptr) \
    do { \
        if ((ptr) == NULL) { \
            kernel_log(LOG_LEVEL_ERROR, "Null pointer: %s", #ptr); \
            return SYS_INVALID_PARAM; \
        } \
    } while(0)

// Note: Les modules du noyau doivent être inclus séparément
// #include "task_manager.h"
// #include "memory_manager.h"
// #include "event_system.h"
// #include "system_monitor.h"

#endif // DO_CORE_KERNEL_H 