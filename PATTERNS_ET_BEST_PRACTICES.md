# 🎨 PATTERNS ET BEST PRACTICES D'O-CORE OS

## 📐 PATTERNS ARCHITECTURAUX

### 1. Microkernel Pattern

**Description**: Architecture en couches avec kernel minimal et services modulaires

**Implémentation**:
```
Applications (pluggables)
    ↓
App Manager (lifecycle)
    ↓
Kernel Services (task, memory, log, monitor)
    ↓
HAL (RTC, Time Sync, LED)
    ↓
Network Stack (WiFi, HTTP, NTP, OTA)
    ↓
Hardware (ESP32)
```

**Avantages**:
- Modularité: Chaque service indépendant
- Extensibilité: Ajouter services sans modifier kernel
- Testabilité: Services isolés
- Maintenabilité: Code organisé par responsabilité

**Exemple**:
```cpp
// Ajouter nouveau service sans modifier kernel
SysError_t my_service_init(void) { ... }
void my_service_start(void) { ... }
void my_service_loop(void) { ... }
void my_service_stop(void) { ... }
```

---

### 2. Manager Pattern

**Description**: Classe/module qui gère un ensemble de ressources

**Implémentation**:
```cpp
// Task Manager
SysError_t task_create(...);
SysError_t task_delete(...);
SysError_t task_get_info(...);
uint8_t task_get_count();

// Memory Manager
void* memory_alloc(size_t size);
void memory_free(void* ptr);
SysError_t memory_get_stats(...);

// App Manager
SysError_t app_register(...);
SysError_t app_start(...);
SysError_t app_stop(...);
```

**Avantages**:
- Centralisation: Gestion centralisée des ressources
- Cohérence: Interface uniforme
- Monitoring: Statistiques globales
- Contrôle: Limites et quotas

---

### 3. Callback Pattern

**Description**: Enregistrement de fonctions pour événements asynchrones

**Implémentation**:
```cpp
// Définir callback
typedef void (*SensorDataCallback_t)(SensorDataPacket_t* data);

// Enregistrer
void irrig_comm_set_sensor_callback(SensorDataCallback_t callback);

// Appeler
void on_sensor_data_received(SensorDataPacket_t* data) {
    kernel_log(LOG_LEVEL_INFO, "Sensor data received");
    // Traiter données
}

// Dans main.cpp
irrig_comm_set_sensor_callback(on_sensor_data_received);
```

**Avantages**:
- Découplage: Producteur/consommateur indépendants
- Asynchrone: Non-bloquant
- Flexible: Plusieurs callbacks possibles

---

### 4. State Machine Pattern

**Description**: Gestion d'états avec transitions définies

**Implémentation**:
```cpp
// États d'application
typedef enum {
    APP_STATE_UNLOADED,
    APP_STATE_LOADING,
    APP_STATE_RUNNING,
    APP_STATE_PAUSED,
    APP_STATE_STOPPED,
    APP_STATE_ERROR
} AppState_t;

// Transitions
app_start(app_id)   // UNLOADED → LOADING → RUNNING
app_pause(app_id)   // RUNNING → PAUSED
app_resume(app_id)  // PAUSED → RUNNING
app_stop(app_id)    // RUNNING → STOPPED
```

**Avantages**:
- Clarté: États explicites
- Sécurité: Transitions validées
- Debugging: Traçabilité des changements d'état

---

### 5. Circular Buffer Pattern

**Description**: Buffer de taille fixe avec réutilisation automatique

**Implémentation**:
```cpp
// Log System
typedef struct {
    LogMessage_t messages[LOG_BUFFER_SIZE];  // 1000 messages
    uint32_t head;
    uint32_t tail;
    uint32_t count;
    uint32_t total_messages;
    SemaphoreHandle_t mutex;
} LogBuffer_t;

// Ajouter message
void log_add_message(LogMessage_t* msg) {
    messages[head] = *msg;
    head = (head + 1) % LOG_BUFFER_SIZE;
    if (count < LOG_BUFFER_SIZE) count++;
    else tail = (tail + 1) % LOG_BUFFER_SIZE;
}
```

**Avantages**:
- Mémoire fixe: Pas de croissance infinie
- Performance: O(1) insertion/suppression
- Simplicité: Pas de réallocation

---

### 6. Singleton Pattern

**Description**: Une seule instance d'une classe

**Implémentation**:
```cpp
// HTTP Client Manager
class HttpClientManager {
private:
    static HttpClientManager* instance;
    HttpClientManager() { }
    
public:
    static HttpClientManager* getInstance() {
        if (instance == nullptr) {
            instance = new HttpClientManager();
        }
        return instance;
    }
};

// Utilisation
HttpClientManager* client = HttpClientManager::getInstance();
client->get("/api/data");
```

**Avantages**:
- Unicité: Une seule instance garantie
- Accès global: Disponible partout
- Lazy initialization: Créé à la première utilisation

---

### 7. Observer Pattern

**Description**: Notification automatique des observateurs

**Implémentation**:
```cpp
// System Monitor
typedef void (*AlertCallback_t)(SystemAlert_t* alert);

void system_monitor_register_alert_callback(AlertCallback_t callback) {
    // Enregistrer callback
}

void system_monitor_add_alert(AlertType_t type, const char* message) {
    // Créer alerte
    // Notifier tous les observateurs
    for (int i = 0; i < callback_count; i++) {
        callbacks[i](&alert);
    }
}
```

**Avantages**:
- Découplage: Observateurs indépendants
- Scalabilité: Ajouter observateurs dynamiquement
- Réactivité: Notification immédiate

---

## 🛡️ BEST PRACTICES

### 1. Gestion des erreurs

**Pattern**: Toujours vérifier codes d'erreur

```cpp
// ✅ BON
SysError_t result = task_create("MyTask", my_function, NULL, 
                                PRIORITY_NORMAL, STACK_SIZE_MEDIUM, &task_id);
if (result != SYS_OK) {
    kernel_log(LOG_LEVEL_ERROR, "Task creation failed: %s", 
               kernel_get_error_string(result));
    return result;
}

// ❌ MAUVAIS
task_create("MyTask", my_function, NULL, PRIORITY_NORMAL, STACK_SIZE_MEDIUM, &task_id);
// Pas de vérification d'erreur!
```

### 2. Logging structuré

**Pattern**: Logs avec contexte et niveau approprié

```cpp
// ✅ BON
kernel_log(LOG_LEVEL_INFO, "WiFi connected - IP: %s, RSSI: %d dBm",
          WiFi.localIP().toString().c_str(), WiFi.RSSI());

// ❌ MAUVAIS
Serial.println("WiFi OK");  // Pas de contexte, pas de niveau
```

### 3. Gestion de la mémoire

**Pattern**: Allocation/désallocation pairées

```cpp
// ✅ BON
void* buffer = memory_alloc(1024);
if (buffer == NULL) {
    kernel_log(LOG_LEVEL_ERROR, "Memory allocation failed");
    return SYS_NO_MEMORY;
}
// Utiliser buffer
memory_free(buffer);

// ❌ MAUVAIS
void* buffer = malloc(1024);  // Pas de vérification
// Oublier free() → fuite mémoire
```

### 4. Synchronisation des tâches

**Pattern**: Utiliser mutex pour sections critiques

```cpp
// ✅ BON
SemaphoreHandle_t mutex = xSemaphoreCreateMutex();

void critical_section(void) {
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(1000))) {
        // Section critique
        xSemaphoreGive(mutex);
    } else {
        kernel_log(LOG_LEVEL_WARN, "Mutex timeout");
    }
}

// ❌ MAUVAIS
void critical_section(void) {
    // Accès concurrent sans synchronisation!
    shared_variable++;
}
```

### 5. Timeouts

**Pattern**: Toujours définir timeouts

```cpp
// ✅ BON
const uint32_t TIMEOUT_MS = 5000;
if (xSemaphoreTake(mutex, pdMS_TO_TICKS(TIMEOUT_MS))) {
    // Opération
    xSemaphoreGive(mutex);
} else {
    kernel_log(LOG_LEVEL_ERROR, "Operation timeout");
}

// ❌ MAUVAIS
xSemaphoreTake(mutex, portMAX_DELAY);  // Attendre indéfiniment!
```

### 6. Validation des paramètres

**Pattern**: Valider entrées au début de fonction

```cpp
// ✅ BON
SysError_t save_wifi_credentials(const char* ssid, const char* password) {
    if (!ssid || !password) {
        kernel_log(LOG_LEVEL_ERROR, "Invalid credentials: null pointer");
        return SYS_INVALID_PARAM;
    }
    if (strlen(ssid) == 0 || strlen(ssid) > 31) {
        kernel_log(LOG_LEVEL_ERROR, "Invalid SSID length");
        return SYS_INVALID_PARAM;
    }
    // Traiter
    return SYS_OK;
}

// ❌ MAUVAIS
void save_wifi_credentials(const char* ssid, const char* password) {
    strcpy(stored_ssid, ssid);  // Buffer overflow possible!
}
```

### 7. Nommage cohérent

**Pattern**: Conventions de nommage uniformes

```cpp
// ✅ BON
// Fonctions: verbe_objet
task_create()
task_delete()
memory_alloc()
memory_free()
wifi_connect()
wifi_disconnect()

// Variables: objet_descripteur
task_count
free_heap
wifi_status
sensor_data

// Constantes: MAJUSCULES
MAX_TASKS
STACK_SIZE_MEDIUM
LOG_BUFFER_SIZE

// ❌ MAUVAIS
createTask()        // Camel case
DeleteTask()        // Majuscule initiale
mem_alloc()         // Inconsistant
tc                  // Abréviation obscure
```

### 8. Documentation

**Pattern**: Documenter API publique

```cpp
// ✅ BON
/**
 * @brief Crée une nouvelle tâche FreeRTOS
 * @param name Nom de la tâche (max 32 caractères)
 * @param function Pointeur vers fonction d'entrée
 * @param parameter Paramètre passé à la fonction
 * @param priority Priorité (PRIORITY_LOW à PRIORITY_CRITICAL)
 * @param stack_size Taille de pile (STACK_SIZE_SMALL, MEDIUM, LARGE)
 * @param task_id Pointeur pour recevoir ID de la tâche
 * @return SYS_OK si succès, code d'erreur sinon
 */
SysError_t task_create(const char* name, void (*function)(void*), 
                      void* parameter, uint32_t priority, 
                      uint32_t stack_size, uint8_t* task_id);

// ❌ MAUVAIS
SysError_t task_create(...);  // Pas de documentation
```

### 9. Tests et validation

**Pattern**: Valider avant déploiement

```cpp
// ✅ BON
// Tester en mode simulation
irrig_config.simulation_mode = true;

// Vérifier santé système
if (!system_monitor_is_system_healthy()) {
    kernel_log(LOG_LEVEL_ERROR, "System health check failed");
    return SYS_ERROR;
}

// Valider configuration
SysError_t result = ota_manager.validate_config();
if (result != OTA_OK) {
    kernel_log(LOG_LEVEL_ERROR, "OTA config invalid");
}

// ❌ MAUVAIS
// Déployer sans tests
// Pas de vérification de santé
// Ignorer erreurs de validation
```

### 10. Performance

**Pattern**: Optimiser boucles critiques

```cpp
// ✅ BON
// Éviter allocations dans boucles
void sensor_read_loop(void) {
    static SensorDataPacket_t packet;  // Alloué une fois
    
    while (system_running) {
        sensors_read_all(&packet);
        irrig_comm_publish_sensor_data(&packet);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

// ❌ MAUVAIS
void sensor_read_loop(void) {
    while (system_running) {
        SensorDataPacket_t* packet = malloc(sizeof(...));  // Allocation à chaque itération!
        sensors_read_all(packet);
        irrig_comm_publish_sensor_data(packet);
        free(packet);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
```

---

## 🔄 PATTERNS DE COMMUNICATION

### 1. Request-Response (HTTP)

```cpp
// Master → Serveur
HttpResponse_t response = http_client.get("/api/config");
if (response.status_code == 200) {
    // Parser JSON
    // Mettre à jour configuration
} else {
    kernel_log(LOG_LEVEL_ERROR, "Config fetch failed: %d", response.status_code);
}
```

### 2. Publish-Subscribe (ESP-NOW)

```cpp
// Slave1 publie données
irrig_comm_publish_sensor_data(&sensor_data);

// Master s'abonne
irrig_comm_set_sensor_callback(on_sensor_data_received);
```

### 3. Command-Response (ESP-NOW)

```cpp
// Master envoie commande
irrig_comm_send_irrigation_command(&cmd);

// Slave2 répond avec statut
irrig_comm_publish_irrigation_status(&status);
```

---

## 🎯 PATTERNS D'APPLICATION

### 1. Cycle de vie d'application

```cpp
// Enregistrement
SysError_t register_my_app(const IrrigAppConfig_t* config) {
    AppCallbacks_t callbacks = {
        .init = my_app_init,
        .start = my_app_start,
        .stop = my_app_stop,
        .pause = my_app_pause,
        .resume = my_app_resume,
        .loop = my_app_loop
    };
    
    return app_register("MyApp", "Description", APP_TYPE_USER, 
                       &callbacks, &app_id);
}

// Démarrage
app_start(app_id);

// Boucle principale
void app_manager_loop(void) {
    for (int i = 0; i < app_count; i++) {
        if (apps[i].state == APP_STATE_RUNNING) {
            apps[i].callbacks.loop();
        }
    }
}

// Arrêt
app_stop(app_id);
```

### 2. Gestion d'état d'application

```cpp
// État global
static AppState_t app_state = APP_STATE_UNLOADED;

// Transitions
void my_app_start(void) {
    app_state = APP_STATE_RUNNING;
    kernel_log(LOG_LEVEL_INFO, "App started");
}

void my_app_pause(void) {
    app_state = APP_STATE_PAUSED;
    kernel_log(LOG_LEVEL_INFO, "App paused");
}

void my_app_resume(void) {
    app_state = APP_STATE_RUNNING;
    kernel_log(LOG_LEVEL_INFO, "App resumed");
}

void my_app_stop(void) {
    app_state = APP_STATE_STOPPED;
    kernel_log(LOG_LEVEL_INFO, "App stopped");
}
```

---

## 📊 PATTERNS DE MONITORING

### 1. Health Check

```cpp
// Vérifier santé système
bool system_monitor_is_system_healthy(void) {
    // Vérifier CPU usage
    if (system_monitor_get_cpu_usage() > 90) return false;
    
    // Vérifier mémoire
    if (system_monitor_get_memory_usage() > 95) return false;
    
    // Vérifier WiFi
    if (WiFi.status() != WL_CONNECTED) return false;
    
    // Vérifier RTC
    if (!rtc_is_initialized()) return false;
    
    return true;
}
```

### 2. Alertes

```cpp
// Ajouter alerte
system_monitor_add_alert(ALERT_TYPE_WARNING, "Low memory: 10% remaining");

// Récupérer alertes
SystemAlert_t alerts[10];
uint32_t count;
system_monitor_get_active_alerts(alerts, 10, &count);

// Traiter alertes
for (int i = 0; i < count; i++) {
    kernel_log(LOG_LEVEL_WARN, "Alert: %s", alerts[i].message);
}
```

### 3. Métriques

```cpp
// Mettre à jour métrique
system_monitor_update_metric(METRIC_CPU_USAGE, 45.5);
system_monitor_update_metric(METRIC_MEMORY_USAGE, 32.1);

// Ajouter métrique personnalisée
system_monitor_add_custom_metric("irrigation_cycles", 42);

// Récupérer performance
SystemPerformance_t perf;
system_monitor_get_performance(&perf);
kernel_log(LOG_LEVEL_INFO, "CPU: %d%, Memory: %d%", 
          perf.cpu_usage_percent, perf.memory_usage_percent);
```

---

## 🔐 PATTERNS DE SÉCURITÉ

### 1. Validation d'entrée

```cpp
// Valider avant traitement
bool validate_sensor_data(SensorDataPacket_t* data) {
    if (!data) return false;
    
    // Vérifier plages
    for (int i = 0; i < MAX_SENSORS; i++) {
        if (data->moisture[i] < 0 || data->moisture[i] > 100) {
            return false;
        }
    }
    
    // Vérifier timestamp
    if (data->timestamp == 0) return false;
    
    return true;
}
```

### 2. Authentification

```cpp
// Vérifier device ID et secret
bool authenticate_device(const char* device_id, const char* device_secret) {
    if (!device_id || !device_secret) return false;
    
    if (strcmp(device_id, EXPECTED_DEVICE_ID) != 0) return false;
    if (strcmp(device_secret, EXPECTED_DEVICE_SECRET) != 0) return false;
    
    return true;
}
```

### 3. Chiffrement (futur)

```cpp
// À implémenter: AES-128 pour ESP-NOW
void encrypt_sensor_data(SensorDataPacket_t* data, uint8_t* encrypted) {
    // Utiliser mbedTLS
    // mbedtls_aes_crypt_cbc(...)
}

void decrypt_sensor_data(uint8_t* encrypted, SensorDataPacket_t* data) {
    // Utiliser mbedTLS
    // mbedtls_aes_crypt_cbc(...)
}
```

---

## 📋 RÉSUMÉ PATTERNS

| Pattern | Utilisation | Exemple |
|---------|-------------|---------|
| **Microkernel** | Architecture générale | Kernel + Services |
| **Manager** | Gestion ressources | Task, Memory, App Manager |
| **Callback** | Événements asynchrones | Sensor data, Commands |
| **State Machine** | Gestion d'états | App lifecycle |
| **Circular Buffer** | Logs, queues | Log system |
| **Singleton** | Instance unique | HTTP Client Manager |
| **Observer** | Notifications | System Monitor alerts |
| **Request-Response** | HTTP | Master ↔ Serveur |
| **Publish-Subscribe** | ESP-NOW | Slave1 → Master |
| **Command-Response** | ESP-NOW | Master → Slave2 |

