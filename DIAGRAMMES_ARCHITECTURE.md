# 📐 DIAGRAMMES D'ARCHITECTURE D'O-CORE OS

## 1. ARCHITECTURE GÉNÉRALE EN COUCHES

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        APPLICATIONS UTILISATEUR                          │
│  ┌──────────────────┐  ┌──────────────────┐  ┌──────────────────┐      │
│  │  Master App      │  │  Slave1 App      │  │  Slave2 App      │      │
│  │  (Irrigation)    │  │  (Sensors)       │  │  (Relays)        │      │
│  │  ID: 1           │  │  ID: 2           │  │  ID: 3           │      │
│  └──────────────────┘  └────────��─────────┘  └──────────────────┘      │
└─────────────────────────────────────────────────────────────────────────┘
                                    ▲
                                    │ App Manager API
┌─────────────────────────────────────────────────────────────────────────┐
│                      APPLICATION MANAGER                                 │
│  • Lifecycle: init → start → running → pause → stop                    │
│  • State Management: UNLOADED → LOADING → RUNNING → PAUSED → STOPPED  │
│  • Resource Isolation: Memory, CPU per app                             │
│  • Max 4 apps simultanées                                              │
└─────────────────────────────────────────────────────────────────────────┘
                                    ▲
                                    │ Kernel API
┌─────────────────────────────────────────────────────────────────────────┐
│                        KERNEL SERVICES                                   │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌────────────┐ │
│  │ Task Manager │  │Memory Manager│  │ Log System   │  │  Monitor   │ │
│  │              │  │              │  │              │  │  System    │ │
│  │ • 32 tâches  │  │ • 4 pools    │  │ • 1000 msgs  │  │ • CPU      │ │
│  │ • Priorités  │  │ • Leak det.  │  │ • Circulaire │  │ • Memory   │ │
│  │ • Pinning    │  │ • Validation │  │ • Filtrage   │  │ • Alertes  │ │
│  └──────────────┘  └──────────────┘  └──────────────┘  └────────────┘ │
│  ┌──────────────┐                                                       │
│  │Event System  │                                                       │
│  │ • 64 events  │                                                       │
│  │ • Callbacks  │                                                       │
│  └──────────────┘                                                       │
└─────────────────────────────────────────────────────────────────────────┘
                                    ▲
                                    │ HAL API
┌─────────────────────────────────────────────────────────────────────────┐
│              HARDWARE ABSTRACTION LAYER (HAL)                            │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌────────────┐ │
│  │ RTC Manager  │  │ Time Sync    │  │ Heartbeat    │  │   GPIO     │ │
│  │              │  │ Manager      │  │   LED        │  │  Control   │ │
│  ��� • DS3231 I2C │  │              │  │              │  │            │ │
│  │ • Read/Write │  │ • NTP→RTC→Sys│  │ • GPIO 2     │  │ • Relais   │ │
│  │ • Temp       │  │ • 15min sync │  │ • 6 patterns │  │ • Capteurs │ │
│  │ • Battery    │  │ • Fallback   │  │ • Status LED │  │            │ │
│  └──────────────┘  └──────────────┘  └──────────────┘  └────────────┘ │
└─────────────────────────────────────────────────────────────────────────┘
                                    ▲
                                    │ Network API
┌─────────────────────────────────────────────────────────────────────────┐
│                        NETWORK STACK                                     │
│  ┌──────────────┐  ┌──────────────┐  ┌───────────��──┐  ┌────────────┐ │
│  │ WiFi Manager │  │ HTTP Client  │  │ NTP Manager  │  │   OTA      │ │
│  │              │  │              │  │              │  │  Manager   │ │
│  │ • STA mode   │  │ • GET/POST   │  │ • pool.ntp   │  │ • Port 3232│ │
│  │ • Auto-conn  │  │ • PUT/DELETE │  │ • UTC+1      │  │ • Elegant  │ │
│  │ • NVS creds  │  │ • PATCH/HEAD │  │ • Timezone   │  │ • Auto-RBT │ │
│  │ • RSSI track │  │ • Retry 3x   │  │ • Business   │  │ • Checksum │ │
│  └──────────────┘  └──────────────┘  └──────────────┘  └────────────┘ │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │              ESP-NOW Communication (250m, 250kbps)               │  │
│  │  • Slave1 → Master: SensorDataPacket_t                          │  │
│  │  • Master → Slave2: IrrigationCommandPacket_t                   │  │
│  │  • Slave2 → Master: IrrigationStatusPacket_t                    │  │
│  └──────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────┘
                                    ▲
                                    │
┌─────────────────────────────────────────────────────────────────────────┐
│                    ESP32 HARDWARE (ESP-IDF)                              │
│  • FreeRTOS Kernel  • WiFi 802.11 b/g/n  • NVS Flash                  │
│  • I2C/SPI/GPIO     • ADC (12-bit)        • Dual Core (240 MHz)        │
│  • 320 KB RAM       • 4 MB Flash          • Watchdog Timer             │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. SYSTÈME D'IRRIGATION (MASTER-SLAVE)

```
                        ┌─────────────────────┐
                        │   SERVEUR CLOUD     │
                        │  • Configuration    │
                        │  • Historique       │
                        │  • Dashboard        │
                        └──────────┬──────────┘
                                   │
                            HTTP/REST (80)
                                   │
                    ┌──────────────▼──────────────┐
                    │   MASTER CONTROLLER         │
                    │   (ESP32 Device 1)          │
                    │                             │
                    │  • Polling config (10s)     │
                    │  • Agrégation capteurs      │
                    │  • Décisions irrigation     │
                    │  • Envoi données serveur    │
                    │                             │
                    │  App ID: 1                  │
                    └──────────┬──────────────────┘
                               │
                ┌──────────────┼──────────────┐
                │              │              │
           ESP-NOW         ESP-NOW        HTTP
           (250m)          (250m)         (80)
                │              │              │
    ┌───────────▼────┐  ┌──────▼──────┐  ┌──▼──────────┐
    │  SLAVE 1       │  │  SLAVE 2    │  │  SERVEUR    │
    │  SENSORS       │  │  RELAYS     │  │  (Backup)   │
    │                │  │             │  │             │
    │ • ADC 12 pins  │  │ • GPIO 15   │  │ • Config    │
    │ • DHT/BME280   │  │ • GPIO 4    │  │ • Logs      │
    │ • Lecture 5s   │  │ • GPIO 18   │  │             │
    │ • Envoi data   │  │ • GPIO 19   │  │             │
    │                │  │ • GPIO 5    │  │             │
    │ App ID: 2      │  │ • Timeout   │  │             │
    │                │  │   1h        │  │             │
    │ (ESP32 Dev 2)  │  │             │  │             │
    │                │  │ App ID: 3   │  │             │
    │                │  │             │  │             │
    │                │  │ (ESP32 Dev 3)  │             │
    └────────────��───┘  └─────────────┘  └─────────────┘
```

---

## 3. FLUX DE COMMUNICATION

### Scénario: Irrigation programmée

```
T=0s: Serveur
  └─ Crée schedule: "08:00 - Zone 1 - 30min"

T=10s: Master (polling)
  ├─ GET /api/config
  ├─ Parse schedule
  └─ Stocke en mémoire

T=28800s (08:00): Master
  ├─ Détecte heure = 08:00
  ├─ Crée IrrigationCommandPacket_t
  │  ├─ command: CMD_START_IRRIGATION
  │  ├─ zone_id: 1
  │  ├─ duration_seconds: 1800
  │
  └─ Envoie ESP-NOW → Slave2
     │
     └─ Slave2 reçoit
        ├─ GPIO 15 = HIGH (Zone 1)
        ├─ GPIO 5 = HIGH (Pompe)
        ├─ Timer = 1800s
        └─ Publie statut

T=28810s: Master
  ├─ Reçoit statut Slave2
  ├─ POST /api/irrigation-status
  │  └─ Serveur met à jour UI

T=30600s (08:30): Slave2
  ├─ Timer écoulé
  ├─ GPIO 15 = LOW (Zone 1)
  ├─ GPIO 5 = LOW (Pompe)
  └─ Publie statut (is_irrigating=false)

T=30610s: Master
  ├─ Reçoit statut Slave2
  ├─ POST /api/irrigation-status
  └─ Serveur met à jour UI
```

---

## 4. CYCLE DE VIE D'UNE APPLICATION

```
┌─────────────────────────────────────────────────────────────┐
│                    APP LIFECYCLE                             │
└─────────────────────────────────────────────────────────────┘

    UNLOADED
       │
       │ app_register()
       ▼
    LOADING
       │
       │ init() callback
       ▼
    RUNNING ◄─────────────────┐
       │                      │
       │ app_pause()          │ app_resume()
       ▼                      │
    PAUSED ──────────────────┘
       │
       │ app_stop()
       ▼
    STOPPED
       │
       │ app_start()
       ▼
    RUNNING (restart)

    ERROR (any state)
       │
       │ app_restart()
       ▼
    LOADING (retry)
```

---

## 5. GESTION DE LA MÉMOIRE

```
┌─────────────────────────────────────────────────────────────┐
│                    HEAP MEMORY (320 KB)                      │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  Kernel Core (~70 KB)                                │  │
│  │  • Task Manager: 5 KB                                │  │
│  │  • Memory Manager: 3 KB                              │  │
│  │  • Log System: 15 KB (1000 messages)                 │  │
│  │  • System Monitor: 8 KB                              │  │
│  │  • Event System: 5 KB                                │  │
│  │  • Other: 34 KB                                      │  │
│  └──────────────────────────────────────────────────────┘  │
│                                                              │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  Applications (~33 KB)                               │  │
│  │  • Master App: 15 KB                                 │  │
│  │  • Slave1 App: 10 KB                                 │  │
│  │  • Slave2 App: 8 KB                                  │  │
│  └──────────────────────────────────────────────────────┘  │
│                                                              │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  Free Memory (~217 KB)                               │  │
│  │  • Available for dynamic allocation                  │  │
│  │  • Buffers, queues, etc.                             │  │
│  └──────────────────────────────────────────────────────┘  │
│                                                              │
└─────────────────────────────────────────────────────────────┘

Memory Pools:
┌─────────────────────────────────────────────────────────────┐
│  Pool 0: 32 bytes × 16 blocks = 512 bytes                  │
│  Pool 1: 64 bytes × 16 blocks = 1024 bytes                 │
│  Pool 2: 128 bytes × 16 blocks = 2048 bytes                │
│  Pool 3: 256 bytes × 16 blocks = 4096 bytes                │
└─────────────────────────────────────────────────────────────┘
```

---

## 6. SYSTÈME DE LOGGING

```
┌─────────────────────────────────────────────────────────────┐
│                    LOG SYSTEM (1000 messages)                │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  Circular Buffer:                                           │
│  ┌──────────────────────────────────────────────────────┐  │
│  │ [0]  │ [1]  │ [2]  │ ... │ [999] │ [0] (wrap)      │  │
│  │ HEAD │      │      │     │ TAIL  │                 │  │
│  └──────────────────────────────────────────────────────┘  │
│                                                              │
│  Levels:                                                    │
│  • DEBUG:    Detailed diagnostic information               │
│  • INFO:     General informational messages                │
│  • WARN:     Warning messages (potential issues)           │
│  • ERROR:    Error messages (failures)                     │
│  • CRITICAL: Critical errors (system failure)              │
│                                                              │
│  Features:                                                  │
│  • Timestamp: Milliseconds depuis boot                     │
│  • Source: Module/fonction d'origine                       │
│  • Task ID: Tâche qui a généré le log                      │
│  • Message: Texte du log (max 128 chars)                   │
│  • Persistence: Sauvegarde en NVS Flash                    │
│  • Filtering: Par niveau, source, temps                    │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

---

## 7. SYSTÈME DE MONITORING

```
┌─────────────────────────────────────────────────────────────┐
│              SYSTEM MONITOR (Health Check)                   │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  Métriques collectées:                                      │
│  ┌──────────────────────────────────────────────────────┐  │
│  │ • CPU Usage: 0-100%                                  │  │
│  │ • Memory Usage: 0-100%                               │  │
│  │ • Free Heap: bytes                                   │  │
│  │ • Task Count: nombre de tâches actives               │  │
│  │ • Event Count: événements/seconde                    │  │
│  │ • Temperature: °C (si capteur)                       │  │
│  │ • Uptime: secondes depuis boot                       │  │
│  │ • WiFi RSSI: dBm                                     │  │
│  └──────────────────────────────────────────────────────┘  │
│                                                              │
│  Historique:                                                │
│  ┌──────────────────────────────────────────────────────┐  │
│  │ 60 points de données (1 minute d'historique)        │  │
│  │ Mise à jour toutes les secondes                      │  │
│  │ Calcul moyenne, min, max                             │  │
│  └──────────────────────────────────────────────────────┘  │
│                                                              │
│  Alertes:                                                   │
│  ┌──────────────────────────────────────────────────────┐  │
│  │ • CPU > 80%: WARNING                                 │  │
│  │ • CPU > 95%: CRITICAL                                │  │
│  │ • Memory > 90%: WARNING                              │  │
│  │ • Memory > 98%: CRITICAL                             │  │
│  │ • Heap < 10KB: WARNING                               │  │
│  │ • Heap < 5KB: CRITICAL                               │  │
│  │ • WiFi disconnected: WARNING                         │  │
│  │ • RTC not initialized: ERROR                         │  │
│  └──────────────────────────────────────────────────────┘  │
│                                                              │
│  Health Score:                                              │
│  ┌──────────────────────────────────────────────────────┐  │
│  │ 100: Excellent (tous les systèmes OK)               │  │
│  │ 75-99: Good (quelques avertissements)                │  │
│  │ 50-74: Fair (plusieurs problèmes)                    │  │
│  │ 25-49: Poor (problèmes sérieux)                      │  │
│  │ 0-24: Critical (système en danger)                   │  │
│  └──────────────────────────────────────────────────────┘  │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

---

## 8. HEARTBEAT LED PATTERNS

```
┌─────────────────────────────────────────────────────────────���
│              HEARTBEAT LED (GPIO 2) PATTERNS                 │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  BOOTING (Démarrage):                                       │
│  ▓░▓░▓░▓░▓░▓░▓░▓░▓░▓░  (100ms on/off)                      │
│  Signification: Système en cours de démarrage               │
│                                                              │
│  READY (Prêt):                                              │
│  ▓▓▓▓▓░░░░░  (1s on/off)                                    │
│  Signification: Système prêt, WiFi connecté                 │
│                                                              │
│  RUNNING (Actif):                                           │
│  ▓░▓░░░░░░░  (200ms on, 800ms off)                         │
│  Signification: Application active                          │
│                                                              │
│  WIFI_ERROR (Erreur WiFi):                                  │
│  ▓░▓░▓░▓░▓░  (50ms on/off)                                  │
│  Signification: Pas de connexion WiFi                       │
│                                                              │
│  ERROR (Erreur système):                                    │
│  ▓▓▓▓▓▓▓▓▓▓  (Fixe ON)                                      │
│  Signification: Erreur critique système                     │
│                                                              │
│  OFF (Arrêt):                                               │
│  ░░░░░░░░░░  (Fixe OFF)                                     │
│  Signification: Système arrêté                              │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

---

## 9. FLUX DE SYNCHRONISATION DU TEMPS

```
┌─────────────────────────────────────────────────────────────┐
│              TIME SYNCHRONIZATION FLOW                       │
├────────────────────────────��────────────────────────────────┤
│                                                              │
│  Boot:                                                      │
│  ┌──────────────────────────────────────────────────────┐  │
│  │ 1. System time = 0 (depuis boot)                     │  │
│  │ 2. Essayer NTP (si WiFi connecté)                    │  │
│  │    ├─ Succès: System time = NTP time                 │  │
│  │    │           RTC time = NTP time                   │  │
│  │    │           Source = NTP                          │  │
│  │    └─ Échec: Essayer RTC                             │  │
│  │ 3. Essayer RTC (si disponible)                       │  │
│  │    ├─ Succès: System time = RTC time                 │  │
│  │    │           Source = RTC                          │  │
│  │    └─ Échec: Garder system time                      │  │
│  │ 4. Garder system time (depuis boot)                  │  │
│  │    Source = SYSTEM                                   │  │
│  └───────────────────���──────────────────────────────────┘  │
│                                                              │
│  Runtime (toutes les 15 minutes):                           │
│  ┌───────────────────────────────────────────────���──────┐  │
│  │ 1. Essayer NTP (si WiFi connecté)                    │  │
│  │    ├─ Succès: Mettre à jour RTC et System time      │  │
│  │    └─ Échec: Garder RTC comme source                │  │
│  │ 2. Essayer RTC (si NTP échoué)                       │  │
│  │    ├─ Succès: Mettre à jour System time              │  │
│  │    └─ Échec: Garder System time                      │  │
│  └──────────────────────────────────────────────────────┘  │
│                                                              │
│  WiFi reconnect:                                            │
│  ┌──────────────────────────────────────────────────────┐  │
│  │ 1. WiFi connecté                                     │  │
│  │ 2. Déclencher sync immédiate (non-bloquante)         │  │
│  │ 3. Essayer NTP                                       │  │
│  │    ├─ Succès: Mettre à jour RTC et System time      │  │
│  │    └─ Échec: Garder RTC comme source                │  │
│  └──��───────────────────────────────────────────────────┘  │
│                                                              │
│  Hiérarchie des sources:                                    │
│  ┌──────────────────────────────────────────────────────┐  │
│  │ 1. NTP (priorité haute) - Précision: ±100ms         │  │
│  │ 2. RTC (priorité moyenne) - Précision: ±1s          │  │
│  │ 3. SYSTEM (priorité basse) - Précision: ±10s        │  │
│  └──────────────────────────────────────────────────────┘  │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

---

## 10. STRUCTURE DES FICHIERS

```
DO-Core-Os/
│
├── src/
│   ├── main.cpp                          # Point d'entrée
│   │
│   ├── kernel/
│   │   ├── core/
│   │   │   ├── kernel.h                  # Définitions
│   │   │   ├── task_manager.h/cpp        # Gestion tâches
│   │   │   ├── memory_manager.h/cpp      # Gestion mémoire
│   │   │   ├── log_system_optimized.h/cpp# Logging
│   │   │   ├── system_monitor.h/cpp      # Monitoring
│   │   │   └── event_system.h/cpp        # Événements
│   │   │
│   │   ├── hal/
│   │   │   ├── rtc_manager.h/cpp         # RTC DS3231
│   │   │   ├── time_sync_manager.h/cpp   # Sync temps
│   │   │   └── heartbeat_led.h/cpp       # LED status
│   │   │
│   │   ├── network/
│   │   │   ├── wifi_manager.h/cpp        # WiFi
│   │   │   ├── http_client.h/cpp         # HTTP client
│   │   │   ├── ntp_manager.h/cpp         # NTP
│   │   │   └── ota_manager.h/cpp         # OTA
│   │   │
│   │   ├── app/
│   │   │   └── app_manager.h/cpp         # App lifecycle
│   │   │
│   │   └── interface/
│   │       └── interface.h/cpp           # CLI shell
│   │
│   └── apps/
│       ├── irrig_app_master/
│       │   ├── irrig_app_master.h/cpp
│       │   └── irrig_app_master_http.h/cpp
│       │
│       ├── irrig_app_slave_sensors/
│       │   └── irrig_app_slave_sensors.h/cpp
│       │
│       ├── irrig_app_slave_relays/
│       │   └── irrig_app_slave_relays.h/cpp
│       │
│       └── irrig_common/
│           ├── irrig_types.h
│           ├── irrig_ipc.h
│           ├── irrig_communication.h/cpp
│           └── irrig_cli_commands.h/cpp
│
├── lib/
│   └── DMD32-main/                       # LED matrix library
│
├── platformio.ini                        # Configuration PlatformIO
├── README.md                             # Documentation principale
│
└── ANALYSE_*.md                          # Documents d'analyse
```

---

## 11. DÉPENDANCES ENTRE MODULES

```
main.cpp
├─ kernel/core/kernel.h
│  ├─ task_manager.h
│  ├─ memory_manager.h
│  ├─ log_system_optimized.h
│  └─ system_monitor.h
│
├─ kernel/hal/
│  ├─ rtc_manager.h
│  ├─ time_sync_manager.h
│  └─ heartbeat_led.h
│
├─ kernel/network/
│  ├─ wifi_manager.h
│  ├─ http_client.h
│  ├─ ntp_manager.h
│  └─ ota_manager.h
│
├─ kernel/app/app_manager.h
├─ kernel/interface/interface.h
│
└─ apps/
   ├─ irrig_app_master/irrig_app_master.h
   ├─ irrig_app_slave_sensors/irrig_app_slave_sensors.h
   ├─ irrig_app_slave_relays/irrig_app_slave_relays.h
   └─ irrig_common/
      ├─ irrig_types.h
      ├─ irrig_ipc.h
      ├─ irrig_communication.h
      └─ irrig_cli_commands.h
```

---

## 12. MATRICE DE COMMUNICATION

```
┌─────────────────────────────────────────────────────────────┐
│              COMMUNICATION MATRIX                            │
├──────────────┬──────────────┬──────────────┬────────────────┤
│ Source       │ Destination  │ Protocol     │ Données        │
├──────────────┼──────────────┼──────────────┼────────────────┤
│ Slave1       │ Master       │ ESP-NOW      │ SensorData     │
│ Master       │ Slave2       │ ESP-NOW      │ IrrigCommand   │
│ Slave2       │ Master       │ ESP-NOW      │ IrrigStatus    │
│ Master       │ Serveur      │ HTTP POST    │ SensorData     │
│ Master       │ Serveur      │ HTTP POST    │ IrrigStatus    │
│ Master       │ Serveur      │ HTTP GET     │ Config         │
│ Master       │ NTP Server   │ UDP NTP      │ Time Request   │
│ Master       │ RTC          │ I2C          │ Time Read/Write│
│ CLI          │ Kernel       │ Serial       │ Commands       │
│ Kernel       │ CLI          │ Serial       │ Responses      │
└──────────────┴──────────────┴──────────────┴────────────────┘
```

---

## 13. TIMELINE DE BOOT

```
T=0ms:     ESP32 boot
T=100ms:   Serial init (115200)
T=200ms:   NVS Flash init
T=300ms:   Task Manager init
T=400ms:   Memory Manager init
T=500ms:   Log System init
T=600ms:   System Monitor init
T=700ms:   WiFi Manager init
T=800ms:   NTP Manager init
T=900ms:   HTTP Client init
T=1000ms:  Heartbeat LED init (BOOTING pattern)
T=1100ms:  OTA Manager init
T=1200ms:  RTC Manager init
T=1300ms:  Time Sync Manager init
T=1400ms:  App Manager init
T=1500ms:  WiFi mode STA
T=1600ms:  Load WiFi credentials
T=1700ms:  WiFi auto-connect (si credentials)
T=2000ms:  WiFi connected (si credentials valides)
T=2100ms:  ESP-NOW init
T=2200ms:  Register 3 apps
T=2300ms:  Initial time sync
T=2400ms:  Create system tasks
T=2500ms:  Interface init
T=2600ms:  Display system logo
T=2700ms:  system_running = true
T=2800ms:  Ready for commands
```

---

## 14. UTILISATION DES RESSOURCES

```
┌─────────────────────────────────────────────────────────────┐
│              RESOURCE UTILIZATION                            │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  RAM (320 KB total):                                        │
│  ┌──────────────────────────────────────────────────────┐  │
│  │ Kernel:       70 KB (22%)  ████████░░░░░░░░░░░░░░  │  │
│  │ Applications: 33 KB (10%)  ███░░░░░░░░░░░░░░░░░░░  │  │
│  │ Free:        217 KB (68%)  ██████████████████░░░░░  │  │
│  └──────────────────────────────────────────────────────┘  │
│                                                              │
│  CPU (Dual Core 240 MHz):                                   │
│  ┌──────────────────────────────────────────────────────┐  │
│  │ Idle:        ~2%   ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  │  │
│  │ Active:     ~15%   ███░░░░░░░░░░░░░░░░░░░░░░░░░░░  │  │
│  │ Peak:       ~50%   ██████████░░░░░░░░░░░░░░░░░░░░  │  │
│  └──────────────────────────────────────────────────────┘  │
│                                                              │
│  Flash (4 MB total):                                        │
│  ┌──────────────────────────────────────────────────────┐  │
│  │ Firmware:   1.5 MB (37%)  ███████░░░░░░░░░░░░░░░░  │  │
│  │ NVS:        0.1 MB (2%)   ░░░░░░░░░░░░░░░░░░░░░░░  │  │
│  │ Free:       2.4 MB (61%)  ████████████░░░░░░░░░░░  │  │
│  └──────────────────────────────────────────────────────┘  │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

---

**Fin des diagrammes d'architecture**

