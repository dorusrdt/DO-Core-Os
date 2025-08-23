#include "event_system.h"
#include <string.h>

// Variables globales du système d'événements
static ExtendedEvent_t event_queue[EVENT_QUEUE_SIZE];
static EventSubscriber_t subscribers[EVENT_MAX_SUBSCRIBERS];
static EventFilter_t event_filters[16];
static EventSystemStats_t event_stats;
static SemaphoreHandle_t event_system_mutex;
static QueueHandle_t event_processing_queue;

// Variables de contrôle
static uint8_t next_subscriber_id = 1;
static uint32_t next_sequence_number = 1;
static uint8_t filter_count = 0;
static bool event_system_initialized = false;

// Initialisation du système d'événements
SysError_t event_system_init(void) {
    Serial.println("Initializing Event System...");
    
    // Initialiser les structures
    memset(event_queue, 0, sizeof(event_queue));
    memset(subscribers, 0, sizeof(subscribers));
    memset(event_filters, 0, sizeof(event_filters));
    memset(&event_stats, 0, sizeof(event_stats));
    
    // Créer les objets de synchronisation
    event_system_mutex = xSemaphoreCreateMutex();
    event_processing_queue = xQueueCreate(EVENT_QUEUE_SIZE, sizeof(ExtendedEvent_t));
    
    if (event_system_mutex == NULL || event_processing_queue == NULL) {
        Serial.println("ERROR: Failed to create event system objects!");
        return SYS_ERROR;
    }
    
    event_system_initialized = true;
    
    Serial.println("Event System initialized successfully!");
    return SYS_OK;
}

void event_system_deinit(void) {
    event_system_initialized = false;
    
    if (event_processing_queue != NULL) {
        vQueueDelete(event_processing_queue);
    }
    
    if (event_system_mutex != NULL) {
        vSemaphoreDelete(event_system_mutex);
    }
    
    Serial.println("Event System deinitialized");
}

// Publication d'événements
SysError_t event_publish(ExtendedSystemEvent_t type, const void* data, uint8_t data_size) {
    return event_publish_priority(type, EVENT_PRIORITY_NORMAL, data, data_size);
}

SysError_t event_publish_priority(ExtendedSystemEvent_t type, EventPriority_t priority, 
                                 const void* data, uint8_t data_size) {
    if (!event_system_initialized) {
        return SYS_ERROR;
    }
    
    if (data_size > EVENT_MAX_DATA_SIZE) {
        return SYS_INVALID_PARAM;
    }
    
    ExtendedEvent_t event;
    event.type = type;
    event.priority = priority;
    event.timestamp = xTaskGetTickCount();
    event.source_id = 0; // Source système par défaut
    event.data_size = data_size;
    event.sequence_number = next_sequence_number++;
    
    if (data != NULL && data_size > 0) {
        memcpy(event.data, data, data_size);
    }
    
    // Simuler le traitement de l'événement
    event_stats.total_events_published++;
    event_stats.total_events_processed++;
    
    Serial.printf("Event published: Type=%s, Priority=%s, Seq=%lu\n",
                 event_type_to_string(type),
                 event_priority_to_string(priority),
                 event.sequence_number);
    
    return SYS_OK;
}

// Abonnement aux événements
SysError_t event_subscribe(ExtendedSystemEvent_t type, EventCallback_t callback, uint8_t* subscriber_id) {
    if (!event_system_initialized || callback == NULL || subscriber_id == NULL) {
        return SYS_INVALID_PARAM;
    }
    
    if (xSemaphoreTake(event_system_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return SYS_BUSY;
    }
    
    // Chercher un slot libre
    int slot = -1;
    for (int i = 0; i < EVENT_MAX_SUBSCRIBERS; i++) {
        if (!subscribers[i].active) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        xSemaphoreGive(event_system_mutex);
        return SYS_NO_MEMORY;
    }
    
    // Créer l'abonné
    EventSubscriber_t* subscriber = &subscribers[slot];
    subscriber->subscriber_id = next_subscriber_id++;
    strncpy(subscriber->name, "Subscriber", sizeof(subscriber->name) - 1);
    subscriber->event_types[0] = type;
    subscriber->event_count = 1;
    subscriber->callback = callback;
    subscriber->active = true;
    subscriber->last_event_time = 0;
    subscriber->total_events_received = 0;
    
    *subscriber_id = subscriber->subscriber_id;
    event_stats.subscriber_count++;
    
    Serial.printf("Event subscriber created: ID=%d, Type=%d\n", *subscriber_id, type);
    
    xSemaphoreGive(event_system_mutex);
    return SYS_OK;
}

SysError_t event_subscribe_multiple(const ExtendedSystemEvent_t* types, uint8_t count, 
                                   EventCallback_t callback, uint8_t* subscriber_id) {
    if (!event_system_initialized || types == NULL || callback == NULL || subscriber_id == NULL) {
        return SYS_INVALID_PARAM;
    }
    
    if (count > EVENT_MAX_TYPES_PER_SUBSCRIBER) {
        return SYS_INVALID_PARAM;
    }
    
    if (xSemaphoreTake(event_system_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return SYS_BUSY;
    }
    
    // Chercher un slot libre
    int slot = -1;
    for (int i = 0; i < EVENT_MAX_SUBSCRIBERS; i++) {
        if (!subscribers[i].active) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        xSemaphoreGive(event_system_mutex);
        return SYS_NO_MEMORY;
    }
    
    // Créer l'abonné
    EventSubscriber_t* subscriber = &subscribers[slot];
    subscriber->subscriber_id = next_subscriber_id++;
    strncpy(subscriber->name, "MultiSubscriber", sizeof(subscriber->name) - 1);
    subscriber->event_count = count;
    subscriber->callback = callback;
    subscriber->active = true;
    subscriber->last_event_time = 0;
    subscriber->total_events_received = 0;
    
    // Copier les types d'événements
    for (int i = 0; i < count; i++) {
        subscriber->event_types[i] = types[i];
    }
    
    *subscriber_id = subscriber->subscriber_id;
    event_stats.subscriber_count++;
    
    Serial.printf("Multi-event subscriber created: ID=%d, Types=%d\n", *subscriber_id, count);
    
    xSemaphoreGive(event_system_mutex);
    return SYS_OK;
}

// Statistiques et monitoring
SysError_t event_get_stats(EventSystemStats_t* stats) {
    if (!event_system_initialized || stats == NULL) {
        return SYS_INVALID_PARAM;
    }
    
    memcpy(stats, &event_stats, sizeof(EventSystemStats_t));
    return SYS_OK;
}

void event_print_stats(void) {
    EventSystemStats_t stats;
    if (event_get_stats(&stats) == SYS_OK) {
        Serial.println("=== Event System Statistics ===");
        Serial.printf("Total events published: %lu\n", stats.total_events_published);
        Serial.printf("Total events processed: %lu\n", stats.total_events_processed);
        Serial.printf("Total events dropped: %lu\n", stats.total_events_dropped);
        Serial.printf("Queue overflow count: %lu\n", stats.queue_overflow_count);
        Serial.printf("Subscriber count: %lu\n", stats.subscriber_count);
        Serial.printf("Average processing time: %lu ms\n", stats.average_processing_time);
        Serial.printf("Max processing time: %lu ms\n", stats.max_processing_time);
        Serial.printf("Events per second: %lu\n", stats.events_per_second);
        Serial.println("================================");
    }
}

void event_print_subscriber_list(void) {
    Serial.println("=== Event Subscribers ===");
    
    for (int i = 0; i < EVENT_MAX_SUBSCRIBERS; i++) {
        if (subscribers[i].active) {
            Serial.printf("ID: %d, Name: %s, Events: %d, Received: %lu\n",
                         subscribers[i].subscriber_id, subscribers[i].name,
                         subscribers[i].event_count, subscribers[i].total_events_received);
        }
    }
    
    Serial.println("=========================");
}

// Utilitaires
const char* event_type_to_string(ExtendedSystemEvent_t type) {
    switch (type) {
        case EVENT_SYSTEM_RESET: return "SYSTEM_RESET";
        case EVENT_SYSTEM_WARNING: return "SYSTEM_WARNING";
        case EVENT_SYSTEM_INFO: return "SYSTEM_INFO";
        case EVENT_TASK_SUSPENDED: return "TASK_SUSPENDED";
        case EVENT_TASK_RESUMED: return "TASK_RESUMED";
        case EVENT_MEMORY_ALLOCATED: return "MEMORY_ALLOCATED";
        case EVENT_MEMORY_FREED: return "MEMORY_FREED";
        case EVENT_WIFI_IP_ACQUIRED: return "WIFI_IP_ACQUIRED";
        case EVENT_SENSOR_READ_COMPLETED: return "SENSOR_READ_COMPLETED";
        case EVENT_ACTUATOR_STATE_CHANGED: return "ACTUATOR_STATE_CHANGED";
        case EVENT_USER_COMMAND: return "USER_COMMAND";
        case EVENT_SECURITY_ALARM: return "SECURITY_ALARM";
        case EVENT_IRRIGATION_START: return "IRRIGATION_START";
        case EVENT_GREENHOUSE_TEMP_CHANGED: return "GREENHOUSE_TEMP_CHANGED";
        default: return "UNKNOWN";
    }
}

const char* event_priority_to_string(EventPriority_t priority) {
    switch (priority) {
        case EVENT_PRIORITY_IDLE: return "IDLE";
        case EVENT_PRIORITY_LOW: return "LOW";
        case EVENT_PRIORITY_NORMAL: return "NORMAL";
        case EVENT_PRIORITY_HIGH: return "HIGH";
        case EVENT_PRIORITY_CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

uint32_t event_get_timestamp(void) {
    return xTaskGetTickCount();
}

uint32_t event_get_sequence_number(void) {
    return next_sequence_number;
}

// Callbacks système
void event_system_on_system_start(void) {
    event_publish(EVENT_SYSTEM_RESET, NULL, 0);
}

void event_system_on_system_stop(void) {
    event_publish(EVENT_SYSTEM_WARNING, NULL, 0);
}

void event_system_on_memory_low(void) {
    event_publish(EVENT_MEMORY_FRAGMENTATION_HIGH, NULL, 0);
}

void event_system_on_error(SysError_t error) {
    event_publish(EVENT_SYSTEM_WARNING, &error, sizeof(error));
}

void event_system_on_event_published(ExtendedEvent_t* event) {
    // Callback appelé quand un événement est publié
}

void event_system_on_event_processed(ExtendedEvent_t* event) {
    // Callback appelé quand un événement est traité
}

void event_system_on_event_dropped(ExtendedEvent_t* event) {
    // Callback appelé quand un événement est supprimé
    Serial.printf("WARNING: Event dropped - Type: %s\n", event_type_to_string(event->type));
}

void event_system_on_queue_overflow(void) {
    Serial.println("WARNING: Event queue overflow!");
} 