# 🌍 ANALYSE DE L'ÉCOSYSTÈME D'O-CORE OS

## 📦 DÉPENDANCES EXTERNES

### 1. Framework et OS

| Dépendance | Version | Rôle | Source |
|-----------|---------|------|--------|
| **Arduino Framework** | 2.x | Framework de base | Arduino IDE / PlatformIO |
| **ESP-IDF** | 4.4+ | SDK ESP32 | Espressif Systems |
| **FreeRTOS** | 10.x | Kernel temps réel | Intégré ESP-IDF |

### 2. Bibliothèques Arduino

```ini
# platformio.ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino

lib_deps =
    # WiFi et réseau
    WiFi                    # Intégré Arduino
    HTTPClient              # Intégré Arduino
    WebServer               # Intégré Arduino
    
    # OTA
    ElegantOTA              # Web-based OTA updates
    
    # JSON
    ArduinoJson             # JSON parsing/generation
    
    # Capteurs (optionnel)
    DHT sensor library      # Capteurs DHT
    Adafruit BME280         # Capteur BME280
    
    # Affichage (optionnel)
    DMD32                   # LED matrix display
```

### 3. Bibliothèques système ESP32

```cpp
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>
#include <esp_system.h>
#include <esp_event.h>
#include <nvs_flash.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/timers.h>
#include <esp_heap_caps.h>
#include <esp_now.h>
#include <time.h>
```

---

## 🔌 INTÉGRATIONS MATÉRIELLES

### 1. Capteurs

| Capteur | Type | Interface | Pins | Rôle |
|---------|------|-----------|------|------|
| **Humidité sol** | Capacitif | ADC | 32-36, 39, 25-27, 14, 12-13 | Slave1 |
| **Température** | DHT22/BME280 | I2C/1-Wire | 21 (SDA), 22 (SCL) | Slave1 |
| **Humidité air** | DHT22/BME280 | I2C/1-Wire | 21 (SDA), 22 (SCL) | Slave1 |
| **Pression** | BME280 | I2C | 21 (SDA), 22 (SCL) | Slave1 |

### 2. Actuateurs

| Actuateur | Type | Interface | Pins | Rôle |
|-----------|------|-----------|------|------|
| **Relais Zone 1** | GPIO | Digital | 15 | Slave2 |
| **Relais Zone 2** | GPIO | Digital | 4 | Slave2 |
| **Relais Zone 3** | GPIO | Digital | 18 | Slave2 |
| **Relais Zone 4** | GPIO | Digital | 19 | Slave2 |
| **Pompe** | GPIO | Digital | 5 | Slave2 |

### 3. Horloge temps réel

| Composant | Type | Interface | Pins | Rôle |
|-----------|------|-----------|------|------|
| **RTC DS3231** | I2C | I2C | 25 (SDA), 26 (SCL) | HAL |

### 4. Indicateurs

| Indicateur | Type | Interface | Pins | Rôle |
|-----------|------|-----------|------|------|
| **Heartbeat LED** | GPIO | Digital | 2 | HAL |
| **Status LED** | GPIO | Digital | 18 | Optionnel |
| **Buzzer** | GPIO | Digital | 19 | Optionnel |

---

## 🌐 INTÉGRATIONS RÉSEAU

### 1. WiFi

**Mode**: Station (STA)
**Fréquence**: 2.4 GHz
**Standard**: 802.11 b/g/n
**Portée**: ~100m (intérieur), ~300m (extérieur)

**Utilisation**:
- Connexion au serveur HTTP
- Synchronisation NTP
- OTA updates
- Fallback pour ESP-NOW

### 2. ESP-NOW

**Protocole**: Propriétaire Espressif
**Portée**: ~250m (ligne de vue)
**Débit**: ~250 kbps
**Latence**: ~10-100ms
**Fiabilité**: Retry automatique

**Utilisation**:
- Communication Master ↔ Slave1 (données capteurs)
- Communication Master ↔ Slave2 (commandes irrigation)
- Fonctionne sans WiFi

### 3. HTTP/REST

**Protocole**: HTTP 1.1
**Port**: 80 (configurable)
**Méthodes**: GET, POST, PUT, DELETE, PATCH, HEAD, OPTIONS
**Format**: JSON

**Endpoints serveur**:
```
GET    /api/config                 # Récupérer configuration
POST   /api/sensor-data            # Envoyer données capteurs
POST   /api/irrigation-status      # Envoyer statut irrigation
POST   /api/device-register        # Enregistrer device
```

### 4. NTP

**Serveur**: pool.ntp.org
**Port**: 123 (UDP)
**Intervalle**: 15 minutes
**Timezone**: UTC+1 (Maroc)

---

## 📊 FLUX DE DONNÉES

### 1. Données capteurs (Slave1 → Master)

```
Slave1 (Sensors App)
  ├─ Lit ADC (12 capteurs)
  ├─ Lit DHT/BME280
  ├─ Crée SensorDataPacket_t
  │  ├─ timestamp
  │  ├─ moisture[12]
  │  ├─ temperature
  │  ├─ humidity
  │  ├─ pressure
  │  ├─ battery_level
  │  └─ signal_strength
  │
  └─ Envoie via ESP-NOW
     └─ Master reçoit
        ├─ Agrège données
        ├─ Détecte seuils
        └─ Envoie serveur (HTTP POST)
```

### 2. Commandes irrigation (Master → Slave2)

```
Master (Irrigation App)
  ├─ Polling serveur (GET /api/config)
  ├─ Parse schedules
  ├─ Détecte irrigation à faire
  ├─ Crée IrrigationCommandPacket_t
  │  ├─ command (START/STOP/EMERGENCY_STOP)
  │  ├─ zone_id
  │  ├─ duration_seconds
  │  ├─ zone_server_id
  │  └─ timestamp
  │
  └─ Envoie via ESP-NOW
     └─ Slave2 reçoit
        ├─ Valide commande
        ├─ Contrôle GPIO relais
        ├─ Démarre timer
        └─ Publie statut
```

### 3. Statut irrigation (Slave2 → Master)

```
Slave2 (Relays App)
  ��─ Exécute commande
  ├─ Contrôle GPIO
  ├─ Gère timer
  ├─ Crée IrrigationStatusPacket_t
  │  ├─ zone_id
  │  ├─ is_irrigating
  │  ├─ remaining_seconds
  │  ├─ pump_running
  │  ├─ relay_states[4]
  │  └─ timestamp
  │
  └─ Envoie via ESP-NOW
     └─ Master reçoit
        └─ Envoie serveur (HTTP POST)
```

---

## 🔄 CYCLE DE VIE DES DONNÉES

### Scénario complet: Irrigation programmée

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

T=28800s: Slave2
  ├─ Reçoit commande
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

## 🔐 SÉCURITÉ ET AUTHENTIFICATION

### 1. WiFi

**Sécurité**: WPA2/WPA3
**Credentials**: Stockés en NVS (chiffré par ESP32)
**Persistance**: Automatique après connexion réussie

### 2. HTTP

**Authentification**: Device ID + Secret (optionnel)
**Headers**:
```
Authorization: Bearer <device_secret>
Content-Type: application/json
User-Agent: D'O-Core/1.0.0
```

### 3. ESP-NOW

**Sécurité**: Pas de chiffrement natif
**Mitigation**: MAC addresses codées en dur (test)
**À faire**: Implémenter chiffrement AES-128

### 4. OTA

**Authentification**: Optionnelle (username/password)
**Port**: 3232 (non standard)
**Checksum**: Validation après upload

---

## 📈 SCALABILITÉ

### Limites actuelles

| Ressource | Limite | Utilisation |
|-----------|--------|-------------|
| **Tâches** | 32 | ~8 (système + apps) |
| **Applications** | 4 | 3 (Master, Slave1, Slave2) |
| **Zones irrigation** | 4 | 4 |
| **Capteurs** | 12 | 12 |
| **Logs** | 1000 messages | ~100-500 |
| **RAM** | 320 KB | ~103 KB (32%) |
| **Flash** | 4 MB | ~1.5 MB (37%) |

### Possibilités d'extension

1. **Plus de zones**: Augmenter MAX_ZONES (actuellement 4)
2. **Plus de capteurs**: Augmenter MAX_SENSORS (actuellement 12)
3. **Plus d'apps**: Augmenter MAX_APPS (actuellement 4)
4. **Plus de tâches**: Augmenter MAX_TASKS (actuellement 32)
5. **Plus de logs**: Augmenter LOG_BUFFER_SIZE (actuellement 1000)

---

## 🔧 CONFIGURATION SYSTÈME

### platformio.ini

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
upload_speed = 921600

# Optimisations
build_flags =
    -O2
    -DCORE_DEBUG_LEVEL=0
    -DLOG_LEVEL_DEFAULT=LOG_LEVEL_INFO

# Partitions
board_build.partitions = partitions.csv

# Taille sketch
board_upload.maximum_size = 1966080
```

### Partitions Flash

```csv
# partitions.csv
Name,   Type, SubType, Offset,  Size, Flags
nvs,    data, nvs,     0x9000,  0x5000,
otadata,data, ota,     0xe000,  0x2000,
app0,   app,  ota_0,   0x10000, 0xf0000,
app1,   app,  ota_1,   0x100000,0xf0000,
spiffs, data, spiffs,  0x1f0000,0x10000,
```

---

## 🚀 DÉPLOIEMENT

### 1. Compilation

```bash
# Build
pio run

# Build avec optimisations
pio run -e esp32dev

# Clean build
pio run --target clean
pio run
```

### 2. Upload

```bash
# Upload via USB
pio run --target upload --upload-port /dev/ttyUSB0

# Upload via OTA
pio run --target upload --upload-port 192.168.1.100
```

### 3. Monitoring

```bash
# Serial monitor
pio device monitor --port /dev/ttyUSB0 --baud 115200

# Avec filtrage
pio device monitor --port /dev/ttyUSB0 --baud 115200 | grep "ERROR"
```

---

## 📚 STRUCTURE DES FICHIERS

### Répertoires clés

```
src/
├── main.cpp                          # Point d'entrée
├── kernel/
│   ├── core/                         # Services kernel
│   │   ├── kernel.h                  # Définitions
│   │   ├── task_manager.*            # Gestion tâches
│   │   ├── memory_manager.*          # Gestion mémoire
│   │   ├── log_system_optimized.*    # Logging
│   │   └── system_monitor.*          # Monitoring
│   ├── hal/                          # Hardware abstraction
│   │   ├── rtc_manager.*             # RTC DS3231
│   │   ├── time_sync_manager.*       # Sync temps
│   │   └── heartbeat_led.*           # LED status
│   ├── network/                      # Réseau
│   │   ├── wifi_manager.*            # WiFi
│   │   ├── http_client.*             # HTTP client
│   │   ├── ntp_manager.*             # NTP
│   │   └── ota_manager.*             # OTA updates
│   ├── app/                          # App framework
│   │   └── app_manager.*             # Lifecycle
│   └── interface/                    # CLI
│       └── interface.*               # Shell
└── apps/
    ├── irrig_app_master/             # Master controller
    ├── irrig_app_slave_sensors/      # Slave sensors
    ├── irrig_app_slave_relays/       # Slave relays
    └── irrig_common/                 # Shared code
        ├── irrig_types.h             # Types communs
        ├── irrig_ipc.h               # Structures IPC
        ├── irrig_communication.*      # ESP-NOW
        └── irrig_cli_commands.*       # CLI irrigation
```

---

## 🔍 POINTS D'INTÉGRATION

### 1. Ajouter une nouvelle app

```cpp
// 1. Créer fichier my_app.h/cpp
// 2. Implémenter callbacks
SysError_t my_app_init(void) { ... }
void my_app_start(void) { ... }
void my_app_loop(void) { ... }
void my_app_stop(void) { ... }

// 3. Enregistrer dans main.cpp
AppCallbacks_t callbacks = {
    .init = my_app_init,
    .start = my_app_start,
    .loop = my_app_loop,
    .stop = my_app_stop
};
app_register("MyApp", "Description", APP_TYPE_USER, &callbacks, &app_id);

// 4. Démarrer
app_start(app_id);
```

### 2. Ajouter une nouvelle commande CLI

```cpp
// 1. Implémenter handler
SysError_t cmd_my_command(int argc, char* argv[]) {
    if (argc < 2) {
        Serial.println("Usage: my_command <param>");
        return SYS_INVALID_PARAM;
    }
    // Logique
    return SYS_OK;
}

// 2. Enregistrer dans interface.cpp
interface_register_command("my_command", "Description", cmd_my_command);
```

### 3. Ajouter un nouveau capteur

```cpp
// 1. Définir pin ADC
#define MY_SENSOR_PIN 32

// 2. Lire dans Slave1
void sensors_read_all(SensorDataPacket_t* packet) {
    int raw = analogRead(MY_SENSOR_PIN);
    float value = raw_to_value(raw);
    packet->custom_sensor = value;
}

// 3. Envoyer au Master
irrig_comm_publish_sensor_data(&packet);
```

---

## 🎯 INTÉGRATIONS POSSIBLES

### 1. Base de données

- **SQLite**: Stockage local des historiques
- **InfluxDB**: Time-series database pour métriques
- **MongoDB**: Cloud storage pour configurations

### 2. Cloud

- **AWS IoT Core**: MQTT + REST API
- **Azure IoT Hub**: Device management
- **Google Cloud IoT**: BigQuery integration

### 3. Interfaces utilisateur

- **Web Dashboard**: React/Vue.js
- **Mobile App**: Flutter/React Native
- **Desktop App**: Electron

### 4. Protocoles additionnels

- **MQTT**: Pub/Sub messaging
- **CoAP**: Constrained Application Protocol
- **LoRaWAN**: Long-range communication

### 5. Capteurs additionnels

- **Débitmètre**: Mesure débit eau
- **Capteur pluie**: Détection pluie
- **Anémomètre**: Vitesse vent
- **Pyranomètre**: Rayonnement solaire

---

## 📊 MÉTRIQUES DE PERFORMANCE

### Temps de réponse

| Opération | Temps |
|-----------|-------|
| Lecture ADC | ~10ms |
| Envoi ESP-NOW | ~50-100ms |
| Requête HTTP | ~500-2000ms |
| Sync NTP | ~1-5s |
| Changement état app | ~10-50ms |

### Consommation énergétique

| Mode | Courant |
|------|---------|
| Idle (WiFi off) | ~80 mA |
| WiFi connected | ~150 mA |
| WiFi transmit | ~300-400 mA |
| ESP-NOW transmit | ~200-300 mA |
| Irrigation active | ~500-1000 mA (relais) |

---

## 🔗 DÉPENDANCES ENTRE MODULES

```
main.cpp
├─ kernel/core/kernel.h
│  ├─ task_manager.h
│  ├─ memory_manager.h
│  ├─ log_system_optimized.h
│  └─ system_monitor.h
├─ kernel/hal/
│  ├─ rtc_manager.h
│  ├─ time_sync_manager.h
│  └─ heartbeat_led.h
├─ kernel/network/
│  ├─ wifi_manager.h
│  ├─ http_client.h
│  ├─ ntp_manager.h
│  └─ ota_manager.h
├─ kernel/app/app_manager.h
├─ kernel/interface/interface.h
└─ apps/
   ├─ irrig_app_master/
   │  ├─ irrig_app_master.h
   │  └─ irrig_app_master_http.h
   ├─ irrig_app_slave_sensors/
   │  └─ irrig_app_slave_sensors.h
   ├─ irrig_app_slave_relays/
   │  └─ irrig_app_slave_relays.h
   └─ irrig_common/
      ├─ irrig_types.h
      ├─ irrig_ipc.h
      ├─ irrig_communication.h
      └─ irrig_cli_commands.h
```

---

## 📋 RÉSUMÉ ÉCOSYSTÈME

| Aspect | Détail |
|--------|--------|
| **Framework** | Arduino + ESP-IDF + FreeRTOS |
| **Capteurs** | 12 ADC + DHT/BME280 |
| **Actuateurs** | 5 relais GPIO |
| **Communication** | WiFi + ESP-NOW + HTTP + NTP |
| **Persistance** | NVS Flash |
| **Monitoring** | System Monitor + Heartbeat LED |
| **Logging** | 1000 messages circulaire |
| **OTA** | ElegantOTA (port 3232) |
| **Scalabilité** | 32 tâches, 4 apps, 12 capteurs, 4 zones |
| **Performance** | ~2% CPU idle, ~15-40% CPU active |
| **Mémoire** | ~103 KB / 320 KB (32%) |

