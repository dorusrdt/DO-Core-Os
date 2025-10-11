# IrrigAppMaster - Application de Contrôle d'Irrigation

Application pour le système D'O-Core OS permettant le contrôle intelligent d'un système d'irrigation.

**🎯 BASÉ SUR LE CODE DE RÉFÉRENCE** - Architecture ZONE_STACK / SENSOR_STACK

## État du Développement

**Version actuelle : 1.0.0 - PRODUCTION READY** ✅

### ✅ Architecture (Code Référence)
- **ZONE_STACK** : Gestion dynamique 4 zones avec IDs serveur
- **SENSOR_STACK** : Assignation dynamique 12 capteurs
- Mapping séquentiel capteurs → zones
- Préservation zones existantes lors updates
- Support commandes serveur (delete_zone)

### ✅ Gestion Capteurs
- Simulation réaliste par type de culture (Tomates, Laitue, Carottes, Mixte)
- Lecture périodique configurable (5s)
- Données environnementales globales (température, humidité, pression)
- Support hardware réel (ADC pins 32-39, 25-27, 14, 12-13)
- Logs détaillés par zone et capteur

### ✅ Gestion Zones
- Configuration dynamique depuis serveur
- Paramètres : eau/jour, heure irrigation, seuils humidité
- Assignation/libération capteurs automatique
- Suppression zones à chaud (handleZoneDeletion)
- Calcul moyennes humidité par zone

### ✅ Communication HTTP
- Enregistrement device (type: "register")
- Envoi données capteurs avec globalData (15s)
- Poll configuration serveur (10s)
- Format JSON compatible serveur référence
- Authentification HMAC-SHA256
- Gestion retry et erreurs

### ✅ Contrôle Irrigation
- **Irrigation programmée** : Selon heure configurée (checkIrrigationSchedule)
- **Irrigation d'urgence** : Seuils critiques (checkMoistureThresholds)
- **Contrôle physique** : Relais zones + pompe (executeIrrigation)
- **Timer automatique** : Arrêt après durée (checkIrrigationTimer)
- **Sécurité** : Arrêt d'urgence si suppression zone

### ✅ Intégration DO-Core OS
- WiFi géré par kernel (pas de reconnexion manuelle)
- NTP géré par kernel (getLocalTime)
- Logs via kernel_log
- Callbacks app_manager (init/start/loop/stop)
- Tâche FreeRTOS dédiée

## Configuration

```cpp
IrrigAppConfig_t config = {
    .server_url = "http://10.223.73.53:3000",
    .device_id = "ESP32_IRRIGATION_11100454456464674",  // ID serveur
    .device_secret = "esp32-secure-key-2024",           // Secret serveur
    .poll_interval_seconds = 10,        // 10s (comme code référence)
    .sensor_read_interval_seconds = 5,  // 5s
    .data_send_interval_seconds = 15,   // 15s
    .max_zones = 4,
    .max_sensors = 12,
    .simulation_mode = true  // false pour hardware réel
};
```

## Utilisation

### Enregistrement dans main.cpp
```cpp
#include "apps/irrig_app_master/irrig_app_master.h"

// Dans setup()
register_irrig_app_master(&config);
```

### Contrôle via Shell
```bash
# Lister les applications
app_list

# Démarrer l'application (ID attribué automatiquement)
app_start 1

# Vérifier l'état
app_info 1

# Arrêter l'application
app_stop 1
```

## Fonctionnalités de Base

- **Initialisation** : Configuration et validation
- **Démarrage** : Logs de statut système
- **Boucle principale** : Compteur de cycles avec logs périodiques
- **Arrêt** : Nettoyage et statistiques finales

## Module Capteurs Implémenté

### Fonctionnalités
- **12 capteurs d'humidité** organisés en **4 zones** (3 capteurs par zone)
- **Mode simulation** avec valeurs réalistes (30-42% base + variation ±5%)
- **Mode réel** (TODO - GPIO ADC à implémenter)
- **Lecture périodique** selon configuration (`sensor_read_interval_seconds`)
- **Validation des valeurs** (0-100%)
- **Moyennes par zone** pour décisions d'irrigation
- **Logs détaillés** et métriques de performance

### Structure des Données
```cpp
// Par capteur
typedef struct {
    float moisture_percent;    // 0-100%
    uint32_t last_read_time;   // Timestamp
    bool is_connected;         // État connexion
    float temperature;         // Bonus
    uint32_t read_count;       // Compteur lectures
} SensorData_t;

// Gestionnaire global
typedef struct {
    SensorData_t sensors[12];  // 12 capteurs
    uint32_t last_update;      // Dernière MAJ
    bool simulation_mode;      // Mode actif
    uint32_t total_reads;      // Total lectures
} SensorManager_t;
```

### Logs Attendus

```
[INFO] IrrigAppMaster: Initializing irrigation system...
[INFO] SensorManager: Initialized (SIMULATION mode) - 12 sensors ready
[INFO] IrrigAppMaster: Device ID: ESP32_IRRIGATION_001
[INFO] IrrigAppMaster: Starting irrigation system...
[DEBUG] IrrigAppMaster: Sensors read - Total reads: 1
[INFO] === SENSOR VALUES ===
[INFO] Zone 0: 35.2% avg
[INFO]   S0: 32.1% (reads: 1)
[INFO]   S1: 36.8% (reads: 1)
[INFO]   S2: 37.7% (reads: 1)
[INFO] Zone 1: 38.4% avg
[INFO]   S3: 39.2% (reads: 1)
[INFO]   S4: 37.1% (reads: 1)
[INFO]   S5: 38.9% (reads: 1)
[INFO] Total sensor reads: 1
[INFO] IrrigAppMaster: Running... (loop #10)
```

## Module Zones Implémenté

### Fonctionnalités
- **4 zones d'irrigation** configurables indépendamment
- **Paramètres par zone** : volume d'eau, heure d'irrigation, seuils d'humidité
- **États runtime** : inactif/actif/irrigation/erreur
- **Moyennes d'humidité** calculées automatiquement depuis les capteurs
- **Détection besoins d'irrigation** basée sur seuils configurés
- **Statistiques d'utilisation** : eau totale, nombre d'irrigations
- **Configuration par défaut** prête à l'emploi

### Structure des Données
```cpp
// États des zones
typedef enum {
    ZONE_STATE_INACTIVE = 0,    // Zone désactivée
    ZONE_STATE_ACTIVE,          // Zone active, attente irrigation
    ZONE_STATE_IRRIGATING,      // En cours d'irrigation
    ZONE_STATE_ERROR            // Erreur détectée
} ZoneState_t;

// Configuration d'une zone
typedef struct {
    uint8_t zone_id;                    // 0-3
    bool configured;                   // Zone configurée
    char zone_name[MAX_ZONE_NAME_LENGTH]; // Nom de la zone
    uint16_t water_per_day_ml;         // Volume d'eau par jour (ml)
    char irrigation_time[IRRIGATION_TIME_LENGTH]; // Heure "HH:MM"
    uint8_t humidity_threshold;        // Seuil urgence humidité (%)
    uint8_t sensor_ids[SENSORS_PER_ZONE]; // IDs des 3 capteurs
    bool auto_irrigation_enabled;      // Irrigation automatique
} ZoneConfig_t;

// État runtime d'une zone
typedef struct {
    ZoneState_t state;                 // État actuel
    uint32_t last_irrigation_time;     // Timestamp dernière irrigation
    uint32_t total_water_used_ml;      // Eau totale utilisée
    uint32_t irrigation_count;         // Nombre d'irrigations
    float current_moisture_avg;        // Moyenne humidité actuelle
    uint32_t last_sensor_update;       // Dernière MAJ capteurs
} ZoneStatus_t;

// Zone complète
typedef struct {
    ZoneConfig_t config;               // Configuration
    ZoneStatus_t status;               // État runtime
} Zone_t;
```

### Configuration par Défaut
Chaque zone est initialisée avec :
- **Nom** : "Zone 0", "Zone 1", etc.
- **Volume d'eau** : 2000ml par jour
- **Heure d'irrigation** : 08:00
- **Seuil d'urgence** : 25% d'humidité
- **Irrigation automatique** : activée
- **Capteurs associés** : 3 capteurs automatiques (zone 0: 0,1,2; zone 1: 3,4,5, etc.)

### Logs Attendus
```
[INFO] ZoneManager: Initialized - 4 zones ready with default config
[INFO] === ZONE STATUS ===
[INFO] Zone 0 (Zone 0): INACTIVE
[INFO]   Moisture: 35.2%, Threshold: 25%, Water/day: 2000ml
[INFO]   Irrigation time: 08:00, Auto: ON
[INFO]   Total water: 0 ml, Irrigations: 0
[INFO] Zone 1 (Zone 1): INACTIVE
[INFO]   Moisture: 38.4%, Threshold: 25%, Water/day: 2000ml
[INFO]   Irrigation time: 08:00, Auto: ON
[INFO]   Total water: 0 ml, Irrigations: 0
[INFO] Total irrigation events: 0
```

### Intégration avec Capteurs
- **Mise à jour automatique** : Les moyennes d'humidité sont calculées depuis les capteurs toutes les 5 secondes
- **Association automatique** : Chaque zone utilise 3 capteurs spécifiques
- **Validation croisée** : Seuils d'humidité comparés aux moyennes des capteurs

## Module HTTP Implémenté

### Fonctionnalités
- **Enregistrement automatique** du device auprès du serveur au démarrage
- **Envoi périodique** des données capteurs (intervalle `data_send_interval_seconds`)
- **Poll de configuration** depuis le serveur (intervalle `poll_interval_seconds`)
- **Authentification HMAC-SHA256** pour toutes les requêtes
- **Gestion d'erreurs** avec retry automatique
- **Parsing JSON** avec ArduinoJson
- **Statistiques détaillées** de communication

### Endpoints Utilisés

#### **1. Enregistrement Device**
```
POST /api/devices/register
Headers: X-HMAC-Signature, Content-Type: application/json
Body: {device_id, device_secret, device_type, firmware_version, capabilities}
```

#### **2. Envoi Données Capteurs**
```
POST /api/sensor-data
Headers: X-HMAC-Signature, X-Device-ID, Content-Type: application/json
Body: {timestamp, device_id, zones[{zone_id, moisture_avg, sensor_values[]}]}
```

#### **3. Poll Configuration**
```
GET /api/devices/{device_id}/config
Headers: X-HMAC-Signature, X-Device-ID
Response: {config_updated, zones[{configured, zone_name, water_per_day_ml, irrigation_time, ...}]}
```

### Structure des Données

#### **Payload Données Capteurs**
```cpp
typedef struct {
    uint32_t timestamp;
    uint8_t zone_count;  // 4
    struct {
        uint8_t zone_id;        // 0-3
        float moisture_avg;     // Moyenne humidité zone
        uint8_t sensor_count;   // 3
        float sensor_values[3]; // Valeurs individuelles
    } zones[4];
} SensorDataPayload_t;
```

#### **Réponse Configuration Serveur**
```cpp
typedef struct {
    bool config_updated;
    uint32_t server_timestamp;
    struct {
        bool configured;
        char zone_name[16];
        uint16_t water_per_day_ml;
        char irrigation_time[6];     // "HH:MM"
        uint8_t humidity_threshold;
        bool auto_irrigation_enabled;
    } zones[4];
} ServerConfigResponse_t;
```

### Workflow de Communication

```
ESP32 Démarrage
    ↓
WiFi Connecté
    ↓
HTTP Test Connectivity (/api/health)
    ↓
HTTP Register Device (/api/devices/register)
    ↓
Boucle Principale:
    ├── Toutes les 5s:  Lecture capteurs
    ├── Toutes les 15s: HTTP Send Data (/api/sensor-data)
    └── Toutes les 30s: HTTP Poll Config (/api/devices/{id}/config)
```

### Authentification HMAC-SHA256

```cpp
// Génération HMAC pour authentification
String hmac_data = data + device_secret;
String signature = http_generate_hmac(hmac_data, device_secret);

// Header ajouté à toutes les requêtes
http.addHeader("X-HMAC-Signature", signature);
```

### Logs Attendus

```
[INFO] HTTP Manager: Initialized - Ready for server communication
[INFO] IrrigAppMaster: Testing server connectivity...
[INFO] HTTP Manager: Server connectivity OK
[INFO] IrrigAppMaster: Registering device with server...
[INFO] HTTP Manager: Device registered successfully
[DEBUG] IrrigAppMaster: Sensor data sent to server
[DEBUG] IrrigAppMaster: Server config polled successfully
[INFO] === HTTP MANAGER STATS ===
[INFO] Initialized: YES
[INFO] Data sends: 5
[INFO] Config polls: 2
[INFO] Total errors: 0
```

### Gestion d'Erreurs

#### **Codes d'Erreur**
- `HTTP_IRRIG_OK` - Succès
- `HTTP_IRRIG_ERROR_INIT` - Module non initialisé
- `HTTP_IRRIG_ERROR_CONNECT` - Pas de connexion WiFi
- `HTTP_IRRIG_ERROR_TIMEOUT` - Timeout HTTP
- `HTTP_IRRIG_ERROR_AUTH` - Erreur d'authentification
- `HTTP_IRRIG_ERROR_JSON` - Erreur parsing JSON
- `HTTP_IRRIG_ERROR_SERVER` - Erreur serveur (404, 500, etc.)

#### **Retry Automatique**
- **Enregistrement device** : Retry toutes les 30 secondes si échec
- **Envoi données** : Continue même en cas d'erreur (logs warning)
- **Poll config** : Retry automatique avec backoff

## Prochaines Étapes

### ✅ **Terminé - Modules Capteurs + Zones + HTTP**
- ✅ **Module Capteurs** : 12 capteurs simulés, organisation par zones, lecture périodique
- ✅ **Module Zones** : 4 zones configurables, paramètres par zone, états runtime, détection irrigation
- ✅ **Module HTTP** : Communication serveur complète, enregistrement device, envoi données, poll config

### 🔄 **En Cours - Prochains Modules**

1. **Module Contrôle d'Irrigation** :
   - Irrigation programmée (planning horaire)
   - Irrigation d'urgence (seuils critiques)
   - Contrôle relais GPIO (4 zones + pompe)
   - Séquences d'irrigation sécurisées

### ✅ **Corrections Appliquées**

1. **Stack Overflow Corrigé** :
   - **Problème** : `Stack canary watchpoint triggered (IrrigAppMaster)`
   - **Cause** : Taille de pile insuffisante (2048 bytes)
   - **Solution** : Augmentation à 8192 bytes dans `APP_STACK_SIZE_DEFAULT`
   - **Fichier** : `src/kernel/app/app_manager.h`

2. **Module Communication HTTP** :
   - Intégration ArduinoJson + HTTPClient
   - Endpoints serveur (register, config, sensor-data)
   - Authentification HMAC-SHA256
   - Gestion retry et erreurs

3. **Module Contrôle d'Irrigation** :
   - Irrigation programmée (planning horaire)
   - Irrigation d'urgence (seuils critiques)
   - Contrôle relais GPIO (4 zones + pompe)
   - Séquences d'irrigation sécurisées

4. **Module Planning et Horaires** :
   - Synchronisation NTP
   - Vérification horaire des plannings
   - Gestion timezone Maroc
   - Heures business/nuit

5. **Module Gestion d'Urgences** :
   - Seuils critiques d'humidité (< 15%)
   - Alertes température (> 40°C)
   - Arrêt d'urgence
   - Logs d'alertes prioritaires