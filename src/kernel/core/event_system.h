#ifndef DO_CORE_EVENT_SYSTEM_H
#define DO_CORE_EVENT_SYSTEM_H

#include "kernel.h"

// Constantes du système d'événements
#define EVENT_QUEUE_SIZE 64
#define EVENT_MAX_SUBSCRIBERS 16
#define EVENT_MAX_DATA_SIZE 256
#define EVENT_MAX_TYPES_PER_SUBSCRIBER 16

// Priorités des événements
typedef enum {
    EVENT_PRIORITY_IDLE = 0,
    EVENT_PRIORITY_LOW = 1,
    EVENT_PRIORITY_NORMAL = 2,
    EVENT_PRIORITY_HIGH = 3,
    EVENT_PRIORITY_CRITICAL = 4
} EventPriority_t;

// Types d'événements étendus (éviter les conflits avec kernel.h)
typedef enum {
    // Événements système étendus (commencent après les événements de base)
    EVENT_SYSTEM_RESET = 100,
    EVENT_SYSTEM_WARNING,
    EVENT_SYSTEM_INFO,
    
    // Événements de tâches étendus
    EVENT_TASK_SUSPENDED = 110,
    EVENT_TASK_RESUMED,
    EVENT_TASK_BLOCKED,
    EVENT_TASK_UNBLOCKED,
    EVENT_TASK_PRIORITY_CHANGED,
    
    // Événements mémoire étendus
    EVENT_MEMORY_ALLOCATED = 120,
    EVENT_MEMORY_FREED,
    EVENT_MEMORY_LEAK_DETECTED,
    EVENT_MEMORY_FRAGMENTATION_HIGH,
    EVENT_MEMORY_POOL_FULL,
    
    // Événements réseau étendus
    EVENT_WIFI_IP_ACQUIRED = 130,
    EVENT_NETWORK_DATA_RECEIVED,
    EVENT_NETWORK_DATA_SENT,
    EVENT_NETWORK_CONNECTION_LOST,
    EVENT_NETWORK_TIMEOUT,
    
    // Événements capteurs
    EVENT_SENSOR_READ_COMPLETED = 140,
    EVENT_SENSOR_ERROR,
    EVENT_SENSOR_CALIBRATION_NEEDED,
    EVENT_SENSOR_THRESHOLD_EXCEEDED,
    EVENT_SENSOR_OFFLINE,
    
    // Événements actuateurs étendus
    EVENT_ACTUATOR_STATE_CHANGED = 150,
    EVENT_ACTUATOR_ERROR,
    EVENT_ACTUATOR_CALIBRATION_NEEDED,
    EVENT_ACTUATOR_TIMEOUT,
    EVENT_ACTUATOR_OFFLINE,
    
    // Événements utilisateur étendus
    EVENT_USER_COMMAND = 160,
    EVENT_USER_LOGIN,
    EVENT_USER_LOGOUT,
    EVENT_USER_SESSION_TIMEOUT,
    EVENT_USER_PERMISSION_DENIED,
    
    // Événements de sécurité
    EVENT_SECURITY_ALARM = 170,
    EVENT_SECURITY_BREACH,
    EVENT_SECURITY_AUTHORIZED,
    EVENT_SECURITY_UNAUTHORIZED,
    EVENT_SECURITY_SYSTEM_ARMED,
    EVENT_SECURITY_SYSTEM_DISARMED,
    
    // Événements d'irrigation
    EVENT_IRRIGATION_START = 180,
    EVENT_IRRIGATION_STOP,
    EVENT_IRRIGATION_ZONE_CHANGED,
    EVENT_IRRIGATION_SCHEDULE_UPDATED,
    EVENT_IRRIGATION_ERROR,
    EVENT_IRRIGATION_COMPLETED,
    
    // Événements de serre
    EVENT_GREENHOUSE_TEMP_CHANGED = 190,
    EVENT_GREENHOUSE_HUMIDITY_CHANGED,
    EVENT_GREENHOUSE_LIGHT_CHANGED,
    EVENT_GREENHOUSE_VENTILATION_CHANGED,
    EVENT_GREENHOUSE_HEATING_ON,
    EVENT_GREENHOUSE_HEATING_OFF,
    
    // Événements personnalisés (à partir de 200)
    EVENT_CUSTOM_START = 200,
    EVENT_CUSTOM_END = 255
} ExtendedSystemEvent_t;

// Structure d'un événement étendu
typedef struct {
    ExtendedSystemEvent_t type;
    EventPriority_t priority;
    uint32_t timestamp;
    uint8_t source_id;
    uint8_t data_size;
    uint8_t data[EVENT_MAX_DATA_SIZE];
    uint32_t sequence_number;
} ExtendedEvent_t;

// Structure d'un abonné
typedef struct {
    uint8_t subscriber_id;
    char name[32];
    ExtendedSystemEvent_t event_types[EVENT_MAX_TYPES_PER_SUBSCRIBER];
    uint8_t event_count;
    void (*callback)(ExtendedEvent_t*);
    bool active;
    uint32_t last_event_time;
    uint32_t total_events_received;
} EventSubscriber_t;

// Structure d'un filtre d'événement
typedef struct {
    ExtendedSystemEvent_t event_type;
    uint8_t source_id;
    EventPriority_t min_priority;
    bool (*custom_filter)(ExtendedEvent_t*);
} EventFilter_t;

// Statistiques du système d'événements
typedef struct {
    uint32_t total_events_published;
    uint32_t total_events_processed;
    uint32_t total_events_dropped;
    uint32_t queue_overflow_count;
    uint32_t subscriber_count;
    uint32_t average_processing_time;
    uint32_t max_processing_time;
    uint32_t events_per_second;
} EventSystemStats_t;

// Callback pour les événements système
typedef void (*EventCallback_t)(ExtendedEvent_t*);
typedef bool (*EventFilterCallback_t)(ExtendedEvent_t*);

// Initialisation et configuration
SysError_t event_system_init(void);
void event_system_deinit(void);
SysError_t event_system_set_config(const char* key, const char* value);

// Publication d'événements
SysError_t event_publish(ExtendedSystemEvent_t type, const void* data, uint8_t data_size);
SysError_t event_publish_priority(ExtendedSystemEvent_t type, EventPriority_t priority, 
                                 const void* data, uint8_t data_size);
SysError_t event_publish_from_source(ExtendedSystemEvent_t type, uint8_t source_id,
                                    const void* data, uint8_t data_size);
SysError_t event_publish_delayed(ExtendedSystemEvent_t type, uint32_t delay_ms,
                                const void* data, uint8_t data_size);

// Abonnement aux événements
SysError_t event_subscribe(ExtendedSystemEvent_t type, EventCallback_t callback, uint8_t* subscriber_id);
SysError_t event_subscribe_multiple(const ExtendedSystemEvent_t* types, uint8_t count, 
                                   EventCallback_t callback, uint8_t* subscriber_id);
SysError_t event_subscribe_with_filter(ExtendedSystemEvent_t type, EventCallback_t callback,
                                      EventFilterCallback_t filter, uint8_t* subscriber_id);
SysError_t event_unsubscribe(uint8_t subscriber_id);
SysError_t event_unsubscribe_all(void);

// Gestion des abonnés
SysError_t event_get_subscriber_info(uint8_t subscriber_id, EventSubscriber_t* info);
uint8_t event_get_subscriber_count(void);
SysError_t event_get_subscriber_list(EventSubscriber_t* subscribers, uint8_t max_count, uint8_t* actual_count);

// Filtrage et routage
SysError_t event_add_filter(EventFilter_t* filter);
SysError_t event_remove_filter(EventFilter_t* filter);
SysError_t event_clear_filters(void);

// Statistiques et monitoring
SysError_t event_get_stats(EventSystemStats_t* stats);
void event_print_stats(void);
void event_print_subscriber_list(void);
uint32_t event_get_queue_size(void);
uint32_t event_get_processed_count(void);

// Gestion de la file d'attente
SysError_t event_queue_clear(void);
SysError_t event_queue_set_size(uint32_t size);
uint32_t event_queue_get_size(void);
bool event_queue_is_full(void);
bool event_queue_is_empty(void);

// Utilitaires
const char* event_type_to_string(ExtendedSystemEvent_t type);
const char* event_priority_to_string(EventPriority_t priority);
uint32_t event_get_timestamp(void);
uint32_t event_get_sequence_number(void);

// Gestion des événements système
void event_system_on_system_start(void);
void event_system_on_system_stop(void);
void event_system_on_memory_low(void);
void event_system_on_error(SysError_t error);

// Callbacks système
void event_system_on_event_published(ExtendedEvent_t* event);
void event_system_on_event_processed(ExtendedEvent_t* event);
void event_system_on_event_dropped(ExtendedEvent_t* event);
void event_system_on_queue_overflow(void);

// Debug et diagnostic
void event_system_dump_state(void);
SysError_t event_debug_info(uint8_t subscriber_id, char* buffer, size_t buffer_size);
void event_print_queue_contents(void);

// Macros utilitaires
#define EVENT_PUBLISH(type, data, size) event_publish(type, data, size)
#define EVENT_PUBLISH_PRIORITY(type, priority, data, size) event_publish_priority(type, priority, data, size)
#define EVENT_SUBSCRIBE(type, callback, id) event_subscribe(type, callback, id)

// Vérifications de sécurité
#define EVENT_CHECK_TYPE(type) \
    do { \
        if ((type) >= EVENT_CUSTOM_START && (type) <= EVENT_CUSTOM_END) { \
            kernel_log(LOG_LEVEL_WARN, "Custom event type: %d", (type)); \
        } \
    } while(0)

#endif // DO_CORE_EVENT_SYSTEM_H 