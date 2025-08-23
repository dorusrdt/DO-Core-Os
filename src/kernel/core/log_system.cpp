#include "kernel.h"
#include <string.h>
#include <stdarg.h>

// Variables globales du système de logs
static LogBuffer_t log_buffer;
static LogLevel_t current_log_level = LOG_LEVEL_INFO;
static bool log_system_initialized = false;
static bool log_echo_enabled = false; // Désactiver l'echo par défaut

// Initialisation du système de logs
SysError_t log_system_init(void) {
    Serial.println("Initializing Log System...");
    
    // Initialiser le buffer de logs
    memset(&log_buffer, 0, sizeof(log_buffer));
    
    // Créer le mutex
    log_buffer.mutex = xSemaphoreCreateMutex();
    if (log_buffer.mutex == NULL) {
        Serial.println("ERROR: Failed to create log system mutex!");
        return SYS_ERROR;
    }
    
    log_system_initialized = true;
    
    // Ajouter un message de démarrage
    log_system_add_message(LOG_LEVEL_INFO, "SYSTEM", "Log system initialized");
    
    Serial.println("Log System initialized successfully!");
    return SYS_OK;
}

void log_system_deinit(void) {
    log_system_initialized = false;
    
    if (log_buffer.mutex != NULL) {
        vSemaphoreDelete(log_buffer.mutex);
    }
    
    Serial.println("Log System deinitialized");
}

// Ajouter un message au buffer
SysError_t log_system_add_message(LogLevel_t level, const char* source, const char* message) {
    // Validation complète des paramètres
    if (!log_system_initialized) {
        Serial.println("ERROR: Log system not initialized");
        return SYS_ERROR;
    }
    
    if (source == NULL) {
        Serial.println("ERROR: Source parameter is NULL");
        return SYS_INVALID_PARAM;
    }
    
    if (message == NULL) {
        Serial.println("ERROR: Message parameter is NULL");
        return SYS_INVALID_PARAM;
    }
    
    if (level < LOG_LEVEL_DEBUG || level > LOG_LEVEL_CRITICAL) {
        Serial.printf("ERROR: Invalid log level: %d\n", level);
        return SYS_INVALID_PARAM;
    }
    
    // Vérifier la longueur des chaînes
    if (strlen(source) >= LOG_MAX_SOURCE_LENGTH) {
        Serial.printf("ERROR: Source too long (%d chars, max %d)\n", 
                     strlen(source), LOG_MAX_SOURCE_LENGTH - 1);
        return SYS_INVALID_PARAM;
    }
    
    if (strlen(message) >= LOG_MAX_MESSAGE_LENGTH) {
        Serial.printf("ERROR: Message too long (%d chars, max %d)\n", 
                     strlen(message), LOG_MAX_MESSAGE_LENGTH - 1);
        return SYS_INVALID_PARAM;
    }
    
    if (level < current_log_level) {
        return SYS_OK; // Ignorer les messages de niveau inférieur
    }
    
    // Tentative d'obtenir le mutex avec timeout
    if (xSemaphoreTake(log_buffer.mutex, pdMS_TO_TICKS(200)) != pdTRUE) {
        Serial.println("WARNING: Failed to acquire log buffer mutex, message dropped");
        return SYS_BUSY;
    }
    
    // Validation de l'état du buffer
    if (log_buffer.head >= LOG_BUFFER_SIZE) {
        Serial.printf("ERROR: Invalid head index: %lu\n", log_buffer.head);
        xSemaphoreGive(log_buffer.mutex);
        return SYS_ERROR;
    }
    
    // Créer le message
    LogMessage_t* log_msg = &log_buffer.messages[log_buffer.head];
    log_msg->timestamp = xTaskGetTickCount();
    log_msg->level = level;
    log_msg->task_id = xPortGetCoreID(); // ID du CPU pour l'instant
    
    // Copie sécurisée de la source
    strncpy(log_msg->source, source, LOG_MAX_SOURCE_LENGTH - 1);
    log_msg->source[LOG_MAX_SOURCE_LENGTH - 1] = '\0';
    
    // Copie sécurisée du message
    strncpy(log_msg->message, message, LOG_MAX_MESSAGE_LENGTH - 1);
    log_msg->message[LOG_MAX_MESSAGE_LENGTH - 1] = '\0';
    
    // Mettre à jour les indices de manière sécurisée
    log_buffer.head = (log_buffer.head + 1) % LOG_BUFFER_SIZE;
    log_buffer.total_messages++;
    
    if (log_buffer.count < LOG_BUFFER_SIZE) {
        log_buffer.count++;
    } else {
        // Buffer plein, déplacer la queue de manière sécurisée
        log_buffer.tail = (log_buffer.tail + 1) % LOG_BUFFER_SIZE;
    }
    
    // Validation post-ajout
    if (log_buffer.head >= LOG_BUFFER_SIZE || log_buffer.tail >= LOG_BUFFER_SIZE) {
        Serial.printf("ERROR: Buffer indices corrupted after add - head: %lu, tail: %lu\n", 
                     log_buffer.head, log_buffer.tail);
        xSemaphoreGive(log_buffer.mutex);
        return SYS_ERROR;
    }
    
    xSemaphoreGive(log_buffer.mutex);
    
    // Afficher le message en temps réel seulement si l'echo est activé
    if (log_echo_enabled) {
        const char* level_str = log_level_to_string(level);
        Serial.printf("[%lu] %s: %s: %s\n", log_msg->timestamp, level_str, source, message);
    }
    
    return SYS_OK;
}

// Obtenir les messages du buffer
SysError_t log_system_get_messages(LogMessage_t* messages, uint32_t max_count, uint32_t* actual_count) {
    // Validation des paramètres
    if (!log_system_initialized) {
        Serial.println("ERROR: Log system not initialized");
        return SYS_ERROR;
    }
    
    if (messages == NULL || actual_count == NULL) {
        Serial.println("ERROR: Invalid parameters for log_system_get_messages");
        return SYS_INVALID_PARAM;
    }
    
    if (max_count == 0) {
        *actual_count = 0;
        return SYS_OK;
    }
    
    // Tentative d'obtenir le mutex avec timeout plus long
    if (xSemaphoreTake(log_buffer.mutex, pdMS_TO_TICKS(500)) != pdTRUE) {
        Serial.println("ERROR: Failed to acquire log buffer mutex (timeout)");
        return SYS_BUSY;
    }
    
    // Protection contre les indices invalides
    if (log_buffer.count == 0) {
        *actual_count = 0;
        xSemaphoreGive(log_buffer.mutex);
        return SYS_OK;
    }
    
    // Validation des indices
    if (log_buffer.head >= LOG_BUFFER_SIZE || log_buffer.tail >= LOG_BUFFER_SIZE) {
        Serial.printf("ERROR: Invalid buffer indices - head: %lu, tail: %lu\n", 
                     log_buffer.head, log_buffer.tail);
        xSemaphoreGive(log_buffer.mutex);
        return SYS_ERROR;
    }
    
    // Limiter le nombre de messages à récupérer
    uint32_t max_to_read = (log_buffer.count < max_count) ? log_buffer.count : max_count;
    
    uint32_t count = 0;
    uint32_t index = log_buffer.tail;
    
    // Boucle sécurisée avec protection contre les boucles infinies
    while (count < max_to_read && count < LOG_BUFFER_SIZE) {
        // Validation de l'index
        if (index >= LOG_BUFFER_SIZE) {
            Serial.printf("ERROR: Invalid index %lu in log buffer\n", index);
            break;
        }
        
        // Copie sécurisée du message
        messages[count] = log_buffer.messages[index];
        index = (index + 1) % LOG_BUFFER_SIZE;
        count++;
        
        // Protection contre les boucles infinies
        if (count > LOG_BUFFER_SIZE) {
            Serial.println("ERROR: Infinite loop detected in log buffer read");
            break;
        }
    }
    
    *actual_count = count;
    
    // Libération du mutex
    xSemaphoreGive(log_buffer.mutex);
    
    return SYS_OK;
}

// Afficher le buffer complet (comme dmesg)
SysError_t log_system_print_buffer(void) {
    LogMessage_t messages[LOG_BUFFER_SIZE];
    uint32_t actual_count;
    
    if (log_system_get_messages(messages, LOG_BUFFER_SIZE, &actual_count) != SYS_OK) {
        return SYS_ERROR;
    }
    
    Serial.println("=== System Log Buffer (dmesg) ===");
    Serial.printf("Total messages: %lu, Buffer count: %lu\n", 
                 log_buffer.total_messages, log_buffer.count);
    Serial.println("=================================");
    
    for (uint32_t i = 0; i < actual_count; i++) {
        const LogMessage_t* msg = &messages[i];
        const char* level_str = log_level_to_string(msg->level);
        Serial.printf("[%lu] %s: %s: %s\n", 
                     msg->timestamp, level_str, msg->source, msg->message);
    }
    
    Serial.println("=================================");
    return SYS_OK;
}

// Afficher les derniers messages (comme dmesg | tail)
SysError_t log_system_print_tail(uint32_t count) {
    LogMessage_t messages[LOG_BUFFER_SIZE];
    uint32_t actual_count;
    
    if (log_system_get_messages(messages, LOG_BUFFER_SIZE, &actual_count) != SYS_OK) {
        return SYS_ERROR;
    }
    
    uint32_t start = (actual_count > count) ? (actual_count - count) : 0;
    
    Serial.printf("=== Last %lu Messages (tail) ===\n", count);
    
    for (uint32_t i = start; i < actual_count; i++) {
        const LogMessage_t* msg = &messages[i];
        const char* level_str = log_level_to_string(msg->level);
        Serial.printf("[%lu] %s: %s: %s\n", 
                     msg->timestamp, level_str, msg->source, msg->message);
    }
    
    Serial.println("================================");
    return SYS_OK;
}

// Afficher les messages par niveau
SysError_t log_system_print_by_level(LogLevel_t level) {
    LogMessage_t messages[LOG_BUFFER_SIZE];
    uint32_t actual_count;
    
    if (log_system_get_messages(messages, LOG_BUFFER_SIZE, &actual_count) != SYS_OK) {
        return SYS_ERROR;
    }
    
    const char* level_str = log_level_to_string(level);
    Serial.printf("=== Messages with level %s ===\n", level_str);
    
    for (uint32_t i = 0; i < actual_count; i++) {
        const LogMessage_t* msg = &messages[i];
        if (msg->level == level) {
            Serial.printf("[%lu] %s: %s: %s\n", 
                         msg->timestamp, level_str, msg->source, msg->message);
        }
    }
    
    Serial.println("================================");
    return SYS_OK;
}

// Afficher les messages par source
SysError_t log_system_print_by_source(const char* source) {
    if (source == NULL) {
        return SYS_INVALID_PARAM;
    }
    
    LogMessage_t messages[LOG_BUFFER_SIZE];
    uint32_t actual_count;
    
    if (log_system_get_messages(messages, LOG_BUFFER_SIZE, &actual_count) != SYS_OK) {
        return SYS_ERROR;
    }
    
    Serial.printf("=== Messages from source '%s' ===\n", source);
    
    for (uint32_t i = 0; i < actual_count; i++) {
        const LogMessage_t* msg = &messages[i];
        if (strcmp(msg->source, source) == 0) {
            const char* level_str = log_level_to_string(msg->level);
            Serial.printf("[%lu] %s: %s: %s\n", 
                         msg->timestamp, level_str, msg->source, msg->message);
        }
    }
    
    Serial.println("================================");
    return SYS_OK;
}

// Vider le buffer
SysError_t log_system_clear_buffer(void) {
    if (!log_system_initialized) {
        return SYS_ERROR;
    }
    
    if (xSemaphoreTake(log_buffer.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return SYS_BUSY;
    }
    
    log_buffer.head = 0;
    log_buffer.tail = 0;
    log_buffer.count = 0;
    
    xSemaphoreGive(log_buffer.mutex);
    
    log_system_add_message(LOG_LEVEL_INFO, "SYSTEM", "Log buffer cleared");
    return SYS_OK;
}

// Obtenir le nombre de messages
uint32_t log_system_get_count(void) {
    return log_buffer.count;
}

uint32_t log_system_get_total_messages(void) {
    return log_buffer.total_messages;
}

// Fonction dmesg (alias pour print_buffer)
SysError_t log_system_print_dmesg(void) {
    return log_system_print_buffer();
}

// Fonction kernel_log améliorée
SysError_t kernel_log(LogLevel_t level, const char* format, ...) {
    if (!log_system_initialized) {
        return SYS_ERROR;
    }
    
    char message[LOG_MAX_MESSAGE_LENGTH];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    return log_system_add_message(level, "KERNEL", message);
}

// Utilitaires
const char* log_level_to_string(LogLevel_t level) {
    switch (level) {
        case LOG_LEVEL_DEBUG: return "DEBUG";
        case LOG_LEVEL_INFO: return "INFO";
        case LOG_LEVEL_WARN: return "WARN";
        case LOG_LEVEL_ERROR: return "ERROR";
        case LOG_LEVEL_CRITICAL: return "CRIT";
        default: return "UNKNOWN";
    }
}

void kernel_set_log_level(LogLevel_t level) {
    current_log_level = level;
    log_system_add_message(LOG_LEVEL_INFO, "SYSTEM", "Log level changed");
}

// Contrôle de l'echo des logs
void log_system_enable_echo(bool enable) {
    log_echo_enabled = enable;
    log_system_add_message(LOG_LEVEL_INFO, "SYSTEM", 
                          enable ? "Log echo enabled" : "Log echo disabled");
}

bool log_system_is_echo_enabled(void) {
    return log_echo_enabled;
}

// Fonction de validation du système de logs
SysError_t log_system_validate(void) {
    if (!log_system_initialized) {
        Serial.println("ERROR: Log system not initialized");
        return SYS_ERROR;
    }
    
    if (log_buffer.mutex == NULL) {
        Serial.println("ERROR: Log buffer mutex is NULL");
        return SYS_ERROR;
    }
    
    // Validation des indices
    if (log_buffer.head >= LOG_BUFFER_SIZE) {
        Serial.printf("ERROR: Invalid head index: %lu (max: %d)\n", 
                     log_buffer.head, LOG_BUFFER_SIZE - 1);
        return SYS_ERROR;
    }
    
    if (log_buffer.tail >= LOG_BUFFER_SIZE) {
        Serial.printf("ERROR: Invalid tail index: %lu (max: %d)\n", 
                     log_buffer.tail, LOG_BUFFER_SIZE - 1);
        return SYS_ERROR;
    }
    
    // Validation du compteur
    if (log_buffer.count > LOG_BUFFER_SIZE) {
        Serial.printf("ERROR: Invalid count: %lu (max: %d)\n", 
                     log_buffer.count, LOG_BUFFER_SIZE);
        return SYS_ERROR;
    }
    
    // Validation de la cohérence
    if (log_buffer.count == 0 && log_buffer.head != log_buffer.tail) {
        Serial.printf("ERROR: Inconsistent state - count=0 but head(%lu) != tail(%lu)\n", 
                     log_buffer.head, log_buffer.tail);
        return SYS_ERROR;
    }
    
    if (log_buffer.count == LOG_BUFFER_SIZE && log_buffer.head != log_buffer.tail) {
        Serial.printf("ERROR: Inconsistent state - buffer full but head(%lu) != tail(%lu)\n", 
                     log_buffer.head, log_buffer.tail);
        return SYS_ERROR;
    }
    
    Serial.println("Log system validation: OK");
    return SYS_OK;
} 