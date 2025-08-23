#ifndef LOG_SYSTEM_OPTIMIZED_H
#define LOG_SYSTEM_OPTIMIZED_H

#include "../core/kernel.h"

// Configuration optimisée
#define LOG_BUFFER_SIZE_OPTIMIZED 16  // Réduit de 32 à 16
#define LOG_MAX_SOURCE_LENGTH_OPTIMIZED 8   // Réduit de 12 à 8
#define LOG_MAX_MESSAGE_LENGTH_OPTIMIZED 32 // Réduit de 64 à 32

// Structure optimisée
typedef struct {
    uint32_t timestamp;
    LogLevel_t level;
    char source[LOG_MAX_SOURCE_LENGTH_OPTIMIZED];
    char message[LOG_MAX_MESSAGE_LENGTH_OPTIMIZED];
} LogMessageOptimized_t;

typedef struct {
    LogMessageOptimized_t messages[LOG_BUFFER_SIZE_OPTIMIZED];
    uint32_t head;
    uint32_t count;
    uint32_t total_messages;
    SemaphoreHandle_t mutex;
} LogBufferOptimized_t;

// Fonctions optimisées
SysError_t log_system_optimized_init(void);
void log_system_optimized_deinit(void);

SysError_t log_system_add_message_optimized(LogLevel_t level, const char* source, const char* message);
SysError_t kernel_log_optimized(LogLevel_t level, const char* format, ...);

uint32_t log_system_get_count_optimized(void);
uint32_t log_system_get_total_messages_optimized(void);

void log_system_enable_echo_optimized(bool enable);
bool log_system_is_echo_enabled_optimized(void);

// Fonctions d'affichage simplifiées
void log_system_print_recent_optimized(uint32_t count);

#endif // LOG_SYSTEM_OPTIMIZED_H 