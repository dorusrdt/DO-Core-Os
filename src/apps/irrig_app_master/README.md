# IrrigAppMaster - Application de Contrôle d'Irrigation

Application pour le système D'O-Core OS permettant le contrôle intelligent d'un système d'irrigation.

## État du Développement

**Version actuelle : 0.3.0 - MODULE ZONES**
- ✅ Structure de base créée
- ✅ Callbacks d'application implémentés
- ✅ Enregistrement dans le système
- ✅ Tests de lancement possibles
- ✅ **Module Capteurs implémenté**
  - Simulation réaliste des 12 capteurs
  - Gestion des valeurs d'humidité (0-100%)
  - Organisation par zones (4 zones × 3 capteurs)
  - Lecture périodique configurable
  - Logs détaillés et debug
- ✅ **Module Zones implémenté**
  - Configuration des 4 zones d'irrigation
  - Paramètres par zone (eau/jour, heure irrigation, seuils)
  - États runtime (actif/inactif/irrigation/erreur)
  - Moyennes d'humidité par zone
  - Détection besoins d'irrigation
  - Statistiques d'utilisation
- ⏳ Logique métier d'irrigation (modules suivants)

## Configuration

```cpp
IrrigAppConfig_t config = {
    .server_url = "http://10.223.73.53:3000",
    .device_id = "ESP32_IRRIGATION_001",
    .device_secret = "esp32-secret-key",
    .poll_interval_seconds = 30,
    .sensor_read_interval_seconds = 5,
    .data_send_interval_seconds = 15,
    .max_zones = 4,
    .max_sensors = 12,
    .simulation_mode = true
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

## Prochaines Étapes

### ✅ **Terminé - Modules Capteurs + Zones**
- ✅ **Module Capteurs** : 12 capteurs simulés, organisation par zones, lecture périodique
- ✅ **Module Zones** : 4 zones configurables, paramètres par zone, états runtime, détection irrigation

### 🔄 **En Cours - Prochains Modules**

1. **Module Communication HTTP** :
   - Intégration ArduinoJson + HTTPClient
   - Endpoints serveur (register, config, sensor-data)
   - Authentification HMAC-SHA256
   - Gestion retry et erreurs

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