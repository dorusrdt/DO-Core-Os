# 📊 ANALYSE COMPLÈTE DE L'ARCHITECTURE D'O-CORE OS

## 🎯 Vue d'ensemble

**D'O-Core OS** est un système d'exploitation temps réel léger et modulaire conçu pour ESP32. C'est une **mini OS** basée sur FreeRTOS avec une architecture microkernel, optimisée pour les applications IoT distribuées, particulièrement les systèmes d'irrigation intelligente.

### Caractéristiques principales
- **Plateforme**: ESP32 (tous les variants)
- **Framework**: Arduino + ESP-IDF + FreeRTOS
- **Architecture**: Microkernel modulaire
- **Langage**: C/C++
- **Taille**: ~15,000 lignes de code
- **Modules**: 20+
- **Commandes CLI**: 100+

---

## 🏗️ ARCHITECTURE GÉNÉRALE

### Modèle en couches

```
┌─────────────────────────────────────────────────────────────┐
│                   APPLICATIONS UTILISATEUR                   │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────��──��┐      │
│  │ Master App   │  │ Sensors App  │  │ Relays App   │      │
│  │ (Irrigation) │  │ (Capteurs)   │  │ (Relais)     │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
└─────────────────────────────────────────────────────────────┘
                            ▲
                            │ App Manager API
┌─────────────────────────────────────────────────────────────┐
│                  APPLICATION MANAGER                         │
│  • Lifecycle Management  • State Management  • IPC           │
└─────────────────────────────────────────────────────────────┘
                            ▲
                            │ Kernel API
┌─────────────────────────────────────────────────────────────┐
│                    KERNEL SERVICES                           │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐   │
│  │  Task    │  │  Memory  │  │   Log    │  │  Monitor │   │
│  │ Manager  │  │ Manager  │  │  System  │  │  System  │   │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘   │
└─────────────────────────────────────────────────────────────┘
                            ▲
                            │ HAL API
┌─────────────────────────────────────────────────────────────┐
│              HARDWARE ABSTRACTION LAYER (HAL)                │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐   │
│  │   RTC    │  │   Time   │  │ Heartbeat│  │   GPIO   │   │
│  │ Manager  ���  │   Sync   │  │   LED    │  │  Control │   │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘   │
└─────────────────────────────────────────────────────────────┘
                            ▲
                            │ Network API
┌─────────────────────────────────────────────────────────────┐
│                      NETWORK STACK                           │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐   │
│  │   WiFi   │  │   HTTP   │  │   NTP    │  │   OTA    │   │
│  │ Manager  │  │  Client  │  │ Manager  │  │ Manager  │   │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘   │
└─────────────────────────────────────────────────────────────┘
                            ▲
                            │
┌─────────────────────────────────────────────────────────────┐
│                  ESP32 HARDWARE (ESP-IDF)                    │
│  • FreeRTOS  • WiFi  • NVS  • SPI  • I2C  • GPIO           │
└─────────────────────────────────────────────────────────────┘
```

---

## 🔧 COMPOSANTS PRINCIPAUX

### 1. KERNEL CORE (`src/kernel/core/`)

#### 1.1 Task Manager (`task_manager.h/cpp`)
**Responsabilité**: Gestion des tâches FreeRTOS

**Caractéristiques**:
- Création/suppression de tâches
- Priorités: LOW, NORMAL, HIGH, CRITICAL
- Pinning à des cœurs CPU spécifiques
- Statistiques par tâche (CPU time, stack usage)
- Suspension/reprise de tâches
- Max 32 tâches simultanées

**API clés**:
```cpp
task_create_pinned_to_core(name, function, param, priority, stack_size, core_id, &task_id)
task_get_info(task_id, &info)
task_get_count()
task_manager_print_stats()
```

#### 1.2 Memory Manager (`memory_manager.h/cpp`)
**Responsabilité**: Gestion de la mémoire heap et pools

**Caractéristiques**:
- Allocation/désallocation dynamique
- 4 pools mémoire pré-alloués (32, 64, 128, 256 bytes)
- Détection de fuites mémoire
- Fragmentation tracking
- Validation d'intégrité des blocs

**API clés**:
```cpp
memory_alloc(size)
memory_free(ptr)
memory_get_stats(&stats)
memory_validate_all_blocks()
```

#### 1.3 Log System (`log_system_optimized.h/cpp`)
**Responsabilité**: Système de logging centralisé

**Caractéristiques**:
- Buffer circulaire de 1000 messages
- 5 niveaux: DEBUG, INFO, WARN, ERROR, CRITICAL
- Persistance en NVS flash
- Filtrage par niveau/source/temps
- Echo optionnel sur Serial

**API clés**:
```cpp
kernel_log(LOG_LEVEL_INFO, "Message: %s", data)
log_system_get_count()
log_system_print_tail(count)
log_system_print_by_level(level)
```

#### 1.4 System Monitor (`system_monitor.h/cpp`)
**Responsabilité**: Surveillance de la santé système

**Caractéristiques**:
- Monitoring CPU, mémoire, WiFi
- Historique des performances (60 points)
- Système d'alertes avec seuils configurables
- Détection d'anomalies
- Health score (0-100)

**API clés**:
```cpp
system_monitor_get_performance(&perf)
system_monitor_add_alert(type, message)
system_monitor_is_system_healthy()
system_monitor_get_uptime()
```

#### 1.5 Event System (`event_system.h/cpp`)
**Responsabilité**: Système d'événements asynchrone

**Caractéristiques**:
- Queue d'événements (64 max)
- Callbacks enregistrables
- Types d'événements: SYSTEM_START, TASK_CREATED, MEMORY_LOW, WIFI_CONNECTED, etc.

---

### 2. HARDWARE ABSTRACTION LAYER (HAL) (`src/kernel/hal/`)

#### 2.1 RTC Manager (`rtc_manager.h/cpp`)
**Responsabilité**: Gestion du RTC DS3231 (I2C)

**Caractéristiques**:
- Lecture/écriture de l'heure
- Détection de batterie faible
- Lecture de température
- Pins: SDA=25, SCL=26, Address=0x68

**API clés**:
```cpp
rtc_manager_init()
rtc_get_time()  // Retourne timestamp Unix
rtc_set_time(timestamp)
rtc_get_temperature()
rtc_is_battery_ok()
```

#### 2.2 Time Sync Manager (`time_sync_manager.h/cpp`)
**Responsabilité**: Synchronisation automatique du temps

**Hiérarchie des sources**:
1. **NTP** (priorité haute) - Via WiFi
2. **RTC** (priorité moyenne) - Horloge matérielle
3. **SYSTEM** (priorité basse) - Horloge système

**Caractéristiques**:
- Sync automatique toutes les 15 minutes
- Sync immédiate au démarrage WiFi
- Fallback automatique si NTP indisponible
- Gestion des fuseaux horaires

**API clés**:
```cpp
time_sync_init()
time_sync_automatic()  // Appelée par tâche
time_sync_get_current_source()
time_sync_get_current_time()
time_sync_request_immediate()
```

#### 2.3 Heartbeat LED (`heartbeat_led.h/cpp`)
**Responsabilité**: Indicateur visuel de l'état système

**États et patterns**:
| État | Pattern | Signification |
|------|---------|---------------|
| BOOTING | Rapide (100ms) | Démarrage en cours |
| READY | Lent (1s) | Système prêt, WiFi OK |
| RUNNING | Double pulse (200ms) | Application active |
| WIFI_ERROR | Très rapide (50ms) | Pas de WiFi |
| ERROR | Fixe ON | Erreur critique |

**Pin**: GPIO 2 (LED intégrée ESP32)

---

### 3. NETWORK STACK (`src/kernel/network/`)

#### 3.1 WiFi Manager (`wifi_manager.h/cpp`)
**Responsabilité**: Gestion de la connexion WiFi

**Caractéristiques**:
- Mode STA (Station)
- Sauvegarde persistante des credentials (NVS)
- Auto-reconnexion
- Monitoring RSSI
- Supervision en tâche dédiée

**API clés**:
```cpp
wifi_manager_init(config)
connect_to_wifi(ssid, password)
save_wifi_credentials(ssid, password)
load_wifi_credentials()
```

#### 3.2 NTP Manager (`ntp_manager.h/cpp`)
**Responsabilité**: Synchronisation du temps via NTP

**Caractéristiques**:
- Serveur: pool.ntp.org
- Timezone: UTC+1 (Maroc)
- Détection heures de bureau vs nuit
- Retry automatique

**API clés**:
```cpp
ntp_init()
ntp_sync()
ntp_is_synced()
ntp_is_business_hours()
ntp_is_night_time()
```

#### 3.3 HTTP Client (`http_client.h/cpp`)
**Responsabilité**: Client HTTP/REST complet

**Caractéristiques**:
- Méthodes: GET, POST, PUT, DELETE, PATCH, HEAD, OPTIONS
- Retry automatique (3 tentatives)
- Timeout configurable (10s par défaut)
- Statistiques de requêtes
- Support JSON (ArduinoJson)

**API clés**:
```cpp
http_client.get(endpoint, headers)
http_client.post(endpoint, data, headers)
http_client.put(endpoint, data, headers)
http_client.delete_request(endpoint, headers)
http_client.get_stats(&stats)
```

#### 3.4 OTA Manager (`ota_manager.h/cpp`)
**Responsabilité**: Mises à jour firmware Over-The-Air

**Caractéristiques**:
- Web interface ElegantOTA
- Port: 3232
- Authentification optionnelle
- Auto-reboot après update
- Tracking des versions

**API clés**:
```cpp
ota_manager.init()
ota_manager.start()
ota_manager.get_version_info()
ota_manager.get_ota_url()
```

---

### 4. APPLICATION MANAGER (`src/kernel/app/`)

#### 4.1 App Manager (`app_manager.h/cpp`)
**Responsabilité**: Gestion du cycle de vie des applications

**Caractéristiques**:
- Enregistrement dynamique d'apps
- États: UNLOADED → LOADING ��� RUNNING → PAUSED → STOPPED
- Callbacks: init, start, stop, pause, resume, loop
- Isolation des ressources par app
- Max 4 applications simultanées

**Cycle de vie**:
```
UNLOADED
   ↓
LOADING → init() callback
   ↓
RUNNING → start() callback → loop() callback (répété)
   ↓
PAUSED → pause() callback
   ↓
STOPPED → stop() callback
   ↓
ERROR
```

**API clés**:
```cpp
app_register(name, description, type, callbacks, &app_id)
app_start(app_id)
app_stop(app_id)
app_pause(app_id)
app_resume(app_id)
app_manager_loop()  // À appeler dans loop()
```

---

### 5. INTERFACE CLI (`src/kernel/interface/`)

#### 5.1 Interface (`interface.h/cpp`)
**Responsabilité**: Shell interactif avec 100+ commandes

**Catégories de commandes**:
- **Système**: help, status, tasks, memory, logs, uptime, version
- **WiFi**: wifi_save, wifi_scan, wifi_status, wifi_reconnect
- **NTP**: ntp_status, ntp_sync, ntp_time
- **Applications**: app_list, app_start, app_stop, app_info
- **HTTP**: http_get, http_post, http_put, http_delete
- **RTC/Time**: rtc_status, time_status, time_sync
- **Irrigation**: irrig_set_role, irrig_config_show, irrig_status

**Exemple d'utilisation**:
```bash
D'O-Core> help
D'O-Core> system_info
D'O-Core> wifi_save MySSID MyPassword
D'O-Core> app_list
D'O-Core> app_start 1
D'O-Core> irrig_set_role master
```

---

## 🌾 SYSTÈME D'IRRIGATION (IRRIG DISTRO)

### Architecture Master-Slave

```
┌──────────────────────────────────────────────────���──────────┐
│                    MASTER CONTROLLER                         │
│  • Orchestration des schedules                              │
│  • Polling configuration serveur                            │
│  • Agrégation données capteurs                              │
│  • Décisions d'irrigation                                   │
│  • Communication HTTP avec serveur                          │
└─────────────────────────────────────────────────────────────┘
         ↑                                    ↓
    ESP-NOW                              HTTP/REST
         ↑                                    ↓
    ┌────────────────────────────────────────────┐
    │                                            │
┌───────────────────┐              ┌──────────────────────┐
│  SLAVE 1: SENSORS │              │  SLAVE 2: RELAYS     │
│  • Lecture ADC    │              │  • Contrôle GPIO     │
│  • Capteurs humidité│             │  • Relais irrigation │
│  • Temp/Humidité  │              │  • Pompe             │
│  • Envoi données  │              │  • Exécution commandes│
└───────────────────┘              └──────────────────────┘
```

### 1. Master App (`src/apps/irrig_app_master/`)

**Responsabilité**: Orchestration centrale du système d'irrigation

**Fonctionnalités**:
- Polling configuration serveur (10s)
- Lecture données capteurs (5s)
- Envoi données serveur (15s)
- Gestion des schedules d'irrigation
- Détection seuils d'urgence (humidité < 15%)
- Génération de données simulées (mode simulation)

**Structure de données - Zone**:
```cpp
struct ZoneSlot {
    int id;                    // Index 0-3
    int physicalZoneNumber;    // Numéro relais 1-4
    bool configured;
    String zoneId;             // ID serveur
    int waterPerDay;           // ml/jour
    String irrigationTimes[5]; // Heures (ex: "08:00")
    int scheduleCount;         // 1-5 créneaux
    int humidityThreshold;     // Seuil urgence %
};
```

**Structure de données - Capteur**:
```cpp
struct SensorSlot {
    String id;                 // "s_01" à "s_12"
    bool assigned;
    String zoneId;             // Référence zone
};
```

**Callbacks HTTP**:
```cpp
void on_sensor_data_received(SensorDataPacket_t* data)
void on_irrigation_status_received(IrrigationStatusPacket_t* status)
```

**Configuration**:
```cpp
typedef struct {
    char server_url[128];
    char device_id[64];
    char device_secret[32];
    uint16_t poll_interval_seconds;
    uint16_t sensor_read_interval_seconds;
    uint16_t data_send_interval_seconds;
    uint8_t max_zones;
    uint8_t max_sensors;
    bool simulation_mode;
} IrrigAppConfig_t;
```

### 2. Slave Sensors App (`src/apps/irrig_app_slave_sensors/`)

**Responsabilité**: Lecture des capteurs et envoi au Master

**Fonctionnalités**:
- Lecture ADC des 12 capteurs d'humidité
- Capteurs de température/humidité (DHT/BME280)
- Moyennage sur N échantillons
- Envoi via ESP-NOW au Master
- Mode simulation pour test

**Pins ADC**:
```
Capteur 1-6:  GPIO 32-36, 39
Capteur 7-12: GPIO 25-27, 14, 12-13
```

**Paquet de données**:
```cpp
typedef struct {
    uint32_t timestamp;
    float moisture[12];        // % humidité
    float temperature;         // °C
    float humidity;            // %
    float pressure;            // hPa
    float battery_level;       // %
    int8_t signal_strength;    // dBm
} SensorDataPacket_t;
```

**Configuration**:
```cpp
typedef struct {
    bool simulation_mode;
    uint16_t read_interval_ms;
    uint8_t samples_per_read;
    bool enable_http_server;
    uint16_t http_server_port;
} IrrigSensorConfig_t;
```

### 3. Slave Relays App (`src/apps/irrig_app_slave_relays/`)

**Responsabilité**: Contrôle des relais d'irrigation

**Fonctionnalités**:
- Réception commandes du Master (ESP-NOW)
- Contrôle GPIO des relais
- Gestion de la pompe
- Timeout de sécurité (1h par défaut)
- Publication statut irrigation

**Pins Relais**:
```
Zone 1: GPIO 15
Zone 2: GPIO 4
Zone 3: GPIO 18
Zone 4: GPIO 19
Pompe:  GPIO 5
```

**Paquet de commande**:
```cpp
typedef struct {
    IrrigationCommand_t command;  // START, STOP, EMERGENCY_STOP
    uint8_t zone_id;              // 1-4
    uint16_t duration_seconds;
    char zone_server_id[64];
    uint32_t timestamp;
} IrrigationCommandPacket_t;
```

**Paquet de statut**:
```cpp
typedef struct {
    uint8_t zone_id;
    bool is_irrigating;
    uint32_t remaining_seconds;
    bool pump_running;
    bool relay_states[4];
    uint32_t timestamp;
} IrrigationStatusPacket_t;
```

**Configuration**:
```cpp
typedef struct {
    uint32_t safety_timeout_ms;
    bool enable_http_server;
    uint16_t http_server_port;
    uint16_t status_publish_interval_ms;
} IrrigRelayConfig_t;
```

### 4. Communication ESP-NOW (`src/apps/irrig_common/irrig_communication.h/cpp`)

**Responsabilité**: Couche de communication ESP-NOW

**Caractéristiques**:
- Protocole sans WiFi (fonctionne même sans connexion)
- Portée: ~250m en ligne de vue
- Débit: ~250 kbps
- Fiabilité: Retry automatique
- MAC addresses codées en dur (pour test)

**Configuration**:
```cpp
typedef struct {
    uint8_t master_mac[6];
    uint8_t slave1_mac[6];
    uint8_t slave2_mac[6];
    uint8_t local_mac[6];
    uint16_t send_timeout_ms;
    uint8_t retry_count;
    uint16_t retry_delay_ms;
    uint8_t wifi_channel;
} IrrigCommConfig_t;
```

**API**:
```cpp
irrig_comm_init(&config)
irrig_comm_publish_sensor_data(&data)
irrig_comm_send_irrigation_command(&cmd)
irrig_comm_publish_irrigation_status(&status)
irrig_comm_set_sensor_callback(callback)
irrig_comm_set_command_callback(callback)
```

**Flux de communication**:
```
Slave1 (Sensors)
    ↓ ESP-NOW (ESPNOW_MSG_SENSOR_DATA)
Master
    ↓ HTTP (POST /api/sensor-data)
Serveur
    ↓ HTTP (GET /api/config)
Master
    ↓ ESP-NOW (ESPNOW_MSG_IRRIGATION_CMD)
Slave2 (Relays)
    ↓ GPIO (Contrôle relais)
Irrigation
```

---

## 🔄 FLUX D'INITIALISATION (BOOT SEQUENCE)

### Phase 1: Démarrage matériel (setup())

```
1. Serial.begin(115200)
   ↓
2. NVS Flash init
   ↓
3. Task Manager init
   ↓
4. Memory Manager init
   ↓
5. Log System init
   ���
6. System Monitor init + start
   ↓
7. WiFi Manager init
   ↓
8. NTP Manager init
   ↓
9. HTTP Client init
   ↓
10. Heartbeat LED init (HEARTBEAT_BOOTING)
    ↓
11. OTA Manager init
    ↓
12. RTC Manager init
    ↓
13. Time Sync Manager init
    ↓
14. App Manager init
    ↓
15. WiFi mode STA
    ↓
16. Load WiFi credentials from NVS
    ↓
17. Auto-connect WiFi (si credentials trouvées)
    ↓
18. ESP-NOW communication init
    ↓
19. Register 3 apps (Master, Slave1, Slave2)
    ↓
20. Initial time sync (NTP → RTC → System)
    ↓
21. Create system tasks:
    - system_main_task
    - wifi_supervision_task
    - time_sync_task
    - heartbeat_task
    ↓
22. Interface (CLI) init + start
    ↓
23. Display system logo (neofetch style)
    ↓
24. system_running = true
```

### Phase 2: Boucle principale (loop())

```
Infini:
    1. app_manager_loop()  // Appelle loop() de chaque app
    2. delay(10ms)
```

### Phase 3: Tâches système (FreeRTOS)

**Tâche 1: system_main_task**
- Toutes les 5 minutes: Log heap et compteur
- Vérifie system_running

**Tâche 2: wifi_supervision_task**
- Toutes les 1s: Vérifie statut WiFi
- Détecte changements de connexion
- Toutes les 30 min: Log statut
- Reconnexion automatique si déconnecté

**Tâche 3: time_sync_task**
- Toutes les 15 min: Sync NTP → RTC → System
- Sync immédiate si demandée
- Fallback RTC si NTP indisponible

**Tâche 4: heartbeat_task**
- Gère LED GPIO 2 selon état système
- Patterns visuels pour diagnostic

---

## 📊 FLUX DE DONNÉES (IRRIGATION)

### Scénario: Irrigation programmée

```
Serveur
  ↓ HTTP GET /api/config
Master (polling 10s)
  ├─ Parse configuration
  ├─ Détecte schedule: "08:00 - Zone 1 - 30min"
  ├─ Heure actuelle = 08:00 → Déclenche irrigation
  │
  ├─ ESP-NOW → Slave2
  │  └─ IrrigationCommandPacket_t
  │     ├─ command: CMD_START_IRRIGATION
  │     ├─ zone_id: 1
  │     ├─ duration_seconds: 1800
  │
  └─ Slave2 (Relays)
     ├─ Reçoit commande
     ├─ GPIO 15 = HIGH (Zone 1 ON)
     ├─ GPIO 5 = HIGH (Pompe ON)
     ├─ Timer: 1800s
     │
     └─ Toutes les 10s: Publie statut
        └─ IrrigationStatusPacket_t
           ├─ zone_id: 1
           ├─ is_irrigating: true
           ├─ remaining_seconds: 1790
           │
           └─ Master reçoit
              └─ HTTP POST /api/irrigation-status
                 └─ Serveur met à jour UI
```

### Scénario: Seuil d'urgence

```
Slave1 (Sensors) - Toutes les 5s
  ├─ Lit ADC capteurs
  ├─ moisture[0] = 12% (< 15% seuil)
  │
  └─ ESP-NOW → Master
     └─ SensorDataPacket_t
        ├─ moisture[12]
        ├─ temperature
        ├─ humidity

Master reçoit
  ├─ Détecte moisture < 15%
  ├─ Déclenche irrigation d'urgence
  │
  └─ ESP-NOW → Slave2
     └─ IrrigationCommandPacket_t
        ├─ command: CMD_START_IRRIGATION
        ├─ zone_id: (zone du capteur)
        ├─ duration_seconds: 600 (10 min)
```

---

## 🎯 RÔLES DES DEVICES (HARDCODED)

### Configuration dans main.cpp (lignes 759+)

```cpp
// Décommenter UNE SEULE ligne selon le device

//#define DEVICE_ROLE_MASTER     // Master controller
 #define DEVICE_ROLE_SLAVE1    // Slave sensors
// #define DEVICE_ROLE_SLAVE2   // Slave relays
```

### Comportement selon rôle

**DEVICE_ROLE_MASTER**:
- Démarre app Master (ID: 1)
- Enregistre callbacks: on_sensor_data_received, on_irrigation_status_received
- Polling serveur HTTP
- Orchestration irrigation

**DEVICE_ROLE_SLAVE1**:
- Démarre app Slave Sensors (ID: 2)
- Enregistre callback: on_command_received_slave
- Lecture capteurs ADC
- Envoi données ESP-NOW

**DEVICE_ROLE_SLAVE2**:
- Démarre app Slave Relays (ID: 3)
- Enregistre callback: on_command_received_slave
- Contrôle GPIO relais
- Exécution commandes Master

---

## 🔐 PERSISTANCE DES DONNÉES (NVS)

### Namespaces NVS utilisés

| Namespace | Clés | Contenu |
|-----------|------|---------|
| `wifi` | ssid, password, valid | Credentials WiFi |
| `irrig_config` | server_url, device_id, device_secret, role | Config irrigation |
| `logs` | log_buffer | Buffer de logs |
| `ota` | version, checksum | Info OTA |

### Exemple: Sauvegarde credentials WiFi

```cpp
// Sauvegarde
Preferences prefs;
prefs.begin("wifi", false);
prefs.putString("ssid", "MySSID");
prefs.putString("password", "MyPassword");
prefs.putBool("valid", true);
prefs.end();

// Chargement
prefs.begin("wifi", true);
String ssid = prefs.getString("ssid", "");
String password = prefs.getString("password", "");
bool valid = prefs.getBool("valid", false);
prefs.end();
```

---

## 🌐 PROTOCOLES DE COMMUNICATION

### 1. ESP-NOW (Slave ↔ Master)

**Avantages**:
- Fonctionne sans WiFi
- Portée: ~250m
- Latence faible
- Fiable avec retry

**Désavantages**:
- Pas de routage
- Portée limitée
- Bande passante limitée

**Utilisation**:
- Slave1 → Master: Données capteurs
- Master → Slave2: Commandes irrigation
- Slave2 → Master: Statut irrigation

### 2. HTTP/REST (Master ↔ Serveur)

**Endpoints**:
- `GET /api/config` - Récupérer configuration
- `POST /api/sensor-data` - Envoyer données capteurs
- `POST /api/irrigation-status` - Envoyer statut irrigation
- `POST /api/device-register` - Enregistrer device

**Format JSON**:
```json
{
  "device_id": "ESP32_IRRIGATION_11100454456464674",
  "timestamp": 1704067200,
  "moisture": [45.2, 52.1, ...],
  "temperature": 28.5,
  "humidity": 65.0,
  "pressure": 1013.25
}
```

### 3. NTP (Synchronisation temps)

**Serveur**: pool.ntp.org
**Intervalle**: 15 minutes
**Timezone**: UTC+1 (Maroc)

---

## 📈 PERFORMANCE ET RESSOURCES

### Utilisation mémoire

| Composant | RAM (approx) |
|-----------|--------------|
| Kernel core | 20 KB |
| Task Manager | 5 KB |
| Memory Manager | 3 KB |
| Log System | 15 KB (buffer 1000 msgs) |
| WiFi Manager | 10 KB |
| HTTP Client | 8 KB |
| App Manager | 5 KB |
| **Total Kernel** | **~70 KB** |
| Master App | 15 KB |
| Slave1 App | 10 KB |
| Slave2 App | 8 KB |
| **Total Apps** | **~33 KB** |
| **TOTAL** | **~103 KB / 320 KB** |

### Utilisation CPU

| Tâche | CPU % (idle) | CPU % (active) |
|-------|--------------|----------------|
| system_main_task | 0.1% | 0.1% |
| wifi_supervision_task | 0.2% | 0.5% |
| time_sync_task | 0.1% | 2% (lors sync) |
| heartbeat_task | 0.5% | 0.5% |
| App tasks | 1-5% | 10-30% |
| **Total** | **~2%** | **~15-40%** |

---

## 🐛 POINTS IMPORTANTS À NOTER

### 1. MAC Addresses hardcodées

Les adresses MAC sont codées en dur dans main.cpp pour test:
```cpp
uint8_t master_mac_hardcoded[6] = {0x5C, 0x01, 0x3B, 0x4D, 0x65, 0x68};
uint8_t slave1_mac_hardcoded[6] = {0x00, 0x4B, 0x12, 0x2C, 0x6D, 0xEC};
```

**À faire**: Implémenter découverte automatique ou configuration CLI.

### 2. Rôles hardcodés

Les rôles des devices sont définis par `#define` dans main.cpp:
```cpp
#define DEVICE_ROLE_SLAVE1  // À changer pour chaque device
```

**À faire**: Implémenter sélection de rôle via CLI persistante.

### 3. Simulation mode

Master App peut fonctionner en mode simulation:
```cpp
irrig_config.simulation_mode = true;  // Génère données fictives
```

Utile pour test sans hardware réel.

### 4. Timezone fixe

Timezone codée en dur: UTC+1 (Maroc)
```cpp
#define NTP_GMT_OFFSET_SEC 3600
```

**À faire**: Rendre configurable via CLI.

### 5. Documentation obsolète

Certains fichiers .md sont obsolètes (ex: ARCHITECTURE_MASTER_SLAVE_HTTP.md).
**À faire**: Mettre à jour la documentation selon le code réel.

---

## 🚀 FLUX D'EXÉCUTION COMPLET

### Démarrage du système

```
1. ESP32 boot
   ↓
2. Arduino setup()
   ├─ Initialise tous les managers
   ├─ Charge credentials WiFi
   ├─ Connecte WiFi (auto)
   ├─ Initialise ESP-NOW
   ├─ Enregistre 3 apps
   ├─ Crée tâches système
   └─ Démarre CLI
   ↓
3. Arduino loop()
   ├─ app_manager_loop()
   └─ delay(10ms)
   ↓
4. FreeRTOS scheduler
   ├─ system_main_task (toutes les 5 min)
   ├─ wifi_supervision_task (toutes les 1s)
   ├─ time_sync_task (toutes les 15 min)
   ├─ heartbeat_task (continu)
   └─ App tasks (selon app)
```

### Cycle d'irrigation (Master)

```
1. Polling serveur (10s)
   ├─ GET /api/config
   ├─ Parse zones et schedules
   └─ Détecte irrigation à faire
   ↓
2. Lecture capteurs (5s)
   ├─ Reçoit SensorDataPacket via ESP-NOW
   ├─ Agrège données
   └─ Détecte seuils d'urgence
   ↓
3. Décision irrigation
   ├─ Schedule match heure actuelle?
   ├─ Humidité < seuil?
   └─ Envoie commande Slave2
   ↓
4. Envoi données serveur (15s)
   ├─ POST /api/sensor-data
   ├─ POST /api/irrigation-status
   └─ Reçoit ACK serveur
```

---

## 📋 RÉSUMÉ ARCHITECTURE

| Aspect | Détail |
|--------|--------|
| **Type** | Microkernel modulaire |
| **Plateforme** | ESP32 |
| **OS** | FreeRTOS |
| **Langage** | C/C++ |
| **Modules** | 20+ |
| **Commandes CLI** | 100+ |
| **Apps** | 3 (Master, Slave1, Slave2) |
| **Tâches max** | 32 |
| **Apps max** | 4 |
| **RAM utilisée** | ~103 KB / 320 KB |
| **CPU idle** | ~2% |
| **CPU active** | ~15-40% |
| **Persistance** | NVS Flash |
| **Communication** | WiFi, ESP-NOW, HTTP, NTP |
| **Monitoring** | System Monitor + Heartbeat LED |
| **Logging** | 1000 messages circulaire |
| **OTA** | ElegantOTA (port 3232) |

---

## 🎓 CONCLUSION

D'O-Core OS est une **mini OS bien structurée** avec:

✅ **Architecture claire** en couches (Applications → Kernel → HAL → Hardware)
✅ **Modularité** avec App Manager pour applications pluggables
✅ **Robustesse** avec monitoring, logging, gestion d'erreurs
✅ **Flexibilité** avec support WiFi, ESP-NOW, HTTP, NTP, OTA
✅ **Productivité** avec CLI interactive et 100+ commandes
✅ **Scalabilité** pour systèmes IoT distribués (Master-Slave)

**Cas d'usage idéal**: Systèmes d'irrigation intelligents, capteurs distribués, automatisation industrielle légère.

