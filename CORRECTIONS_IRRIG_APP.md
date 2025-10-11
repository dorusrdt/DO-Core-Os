# CORRECTIONS IRRIG_APP_MASTER - Adaptation Code Référence

## ✅ CORRECTIONS EFFECTUÉES

### 1. **Architecture Restructurée**

#### Avant (Incompatible)
```cpp
// Architecture fixe incompatible avec serveur
typedef struct {
    ZoneConfig_t config;
    ZoneStatus_t status;
} Zone_t;

Zone_t zones[4];  // Zones fixes 0-3
```

#### Après (Code Référence)
```cpp
// Architecture dynamique ZONE_STACK / SENSOR_STACK
struct ZoneSlot {
    int id;                // 1-4 (slot physique)
    bool configured;
    String zoneId;         // ID serveur dynamique
    int waterPerDay;
    String irrigationTime;
    int humidityThreshold;
};

struct SensorSlot {
    String id;             // "s_01" à "s_12"
    bool assigned;
    String zoneId;         // Référence zone
};

ZoneSlot ZONE_STACK[4];
SensorSlot SENSOR_STACK[12];
```

### 2. **Fonctions Critiques Ajoutées**

#### ✅ `handleZoneDeletion(String zoneId)`
- Suppression dynamique zones depuis serveur
- Arrêt irrigation d'urgence si zone active
- Libération capteurs assignés
- Mise à jour compteur zones

#### ✅ `parseConfiguration(String jsonResponse)`
- Gestion assignation dynamique zones
- Préservation capteurs zones existantes
- Calcul baseOffset pour mapping séquentiel
- Support commandes serveur (delete_zone)

#### ✅ `executeIrrigation(String zoneId, int durationSeconds)`
- Contrôle physique irrigation
- Activation relais pompe
- Timer irrigation
- Logs détaillés

#### ✅ `checkIrrigationSchedule()`
- Irrigation programmée selon heure
- Utilisation NTP (géré par DO-Core)
- Vérification horaire automatique

#### ✅ `checkMoistureThresholds()`
- Détection seuils critiques
- Irrigation d'urgence automatique
- Calcul moyennes par zone

#### ✅ `checkIrrigationTimer()`
- Surveillance timer actif
- Arrêt automatique irrigation
- Désactivation pompe

### 3. **Format JSON Serveur**

#### Enregistrement Device
```json
{
  "type": "register",
  "deviceId": "ESP32_IRRIGATION_11100454456464674",
  "capacity": {
    "zones": 4,
    "sensors": 12
  },
  "timestamp": "2024-01-15T10:30:00.000Z"
}
```

#### Envoi Données Capteurs
```json
{
  "type": "data",
  "deviceId": "ESP32_IRRIGATION_11100454456464674",
  "timestamp": "2024-01-15T10:30:00.000Z",
  "globalData": {
    "temperature": 24.5,
    "humidity": 60.0,
    "pressure": 1012.0,
    "batteryLevel": 85.0,
    "signalStrength": -45
  },
  "zonesData": [
    {
      "zoneId": "zone_abc123",
      "soilMoisture": [
        {"sensorId": "s_01", "value": 35.2},
        {"sensorId": "s_02", "value": 36.8}
      ]
    }
  ]
}
```

#### Réception Configuration
```json
{
  "type": "config",
  "zones": [
    {
      "zoneId": "zone_abc123",
      "waterPerDay": 2000,
      "irrigationTime": "08:00",
      "humidityThreshold": 25,
      "sensors": [
        {"sensorId": "s_01"},
        {"sensorId": "s_02"}
      ]
    }
  ],
  "commands": [
    {"action": "delete_zone", "zoneId": "zone_xyz789"}
  ]
}
```

### 4. **Intervalles Ajustés**

| Intervalle | Avant | Après | Raison |
|------------|-------|-------|--------|
| Config Poll | 30s | **10s** | Comme code référence |
| Sensor Read | 5s | 5s ✅ | Identique |
| Data Send | 15s | 15s ✅ | Identique |
| Irrigation Check | ❌ | **60s** | Ajouté |
| Threshold Check | ❌ | **30s** | Ajouté |

### 5. **Hardware Pins Définis**

```cpp
// Relais irrigation
#define ZONE_1_RELAY_PIN 2
#define ZONE_2_RELAY_PIN 4
#define ZONE_3_RELAY_PIN 16
#define ZONE_4_RELAY_PIN 17
#define PUMP_RELAY_PIN   5

// Indicateurs
#define STATUS_LED_PIN   18
#define BUZZER_PIN       19
```

### 6. **Données Environnementales Globales**

```cpp
// Ajouté pour compatibilité serveur
static float globalTemperature = 24.5;
static float globalHumidity = 60.0;
static float globalPressure = 1012.0;
```

### 7. **Simulation Réaliste**

#### Avant
```cpp
// Simulation uniforme
float base_value = 30 + (i * 1);
```

#### Après
```cpp
// Simulation réaliste par zone
switch (zoneIndex) {
    case 0: // Tomates
        simulatedMoisture[i] = 35 + random(-5, 10);
        break;
    case 1: // Laitue
        simulatedMoisture[i] = 55 + random(-5, 8);
        break;
    // ...
}
```

### 8. **Intégration DO-Core OS**

#### WiFi
```cpp
// WiFi géré par DO-Core au boot
if (WiFi.status() == WL_CONNECTED) {
    kernel_log(LOG_LEVEL_INFO, "WiFi connected: %s", 
               WiFi.localIP().toString().c_str());
}
```

#### NTP
```cpp
// NTP géré par DO-Core
struct tm timeinfo;
if (getLocalTime(&timeinfo)) {
    // Utiliser heure système
}
```

#### Logs
```cpp
// Utilisation kernel_log au lieu de Serial.printf
kernel_log(LOG_LEVEL_INFO, "Device ID: %s", app_config.device_id);
```

## 📊 RÉSUMÉ DES CHANGEMENTS

### ✅ Ajouté
- Architecture ZONE_STACK / SENSOR_STACK
- Gestion dynamique zones/capteurs
- Suppression zones depuis serveur
- Irrigation programmée (heure)
- Irrigation d'urgence (seuils)
- Contrôle physique irrigation
- Données environnementales globales
- Format JSON compatible serveur
- Pins hardware relais

### ✅ Modifié
- Callbacks adaptés DO-Core
- Intervalles ajustés (10s poll)
- Simulation réaliste par zone
- Configuration device_id/secret

### ❌ Supprimé
- Architecture Zone_t/ZoneManager_t (incompatible)
- Fonctions zone_* anciennes
- Module HTTP séparé (intégré)

## 🎯 COMPATIBILITÉ

### Code Référence
- ✅ Architecture identique (ZONE_STACK/SENSOR_STACK)
- ✅ Logique métier préservée à 100%
- ✅ Format JSON serveur compatible
- ✅ Gestion irrigation complète

### DO-Core OS
- ✅ Callbacks app_manager respectés
- ✅ WiFi/NTP géré par kernel
- ✅ Logs via kernel_log
- ✅ Tâche FreeRTOS dédiée

## 📝 FICHIERS MODIFIÉS

1. **irrig_app_master.h** - Restructuré complètement
2. **irrig_app_master.cpp** - Réécrit avec logique référence
3. **main.cpp** - Configuration ajustée (device_id, intervals)

## 🔧 BACKUP

Backup créé : `irrig_app_master.cpp.backup`

## ✅ PRÊT POUR COMPILATION

L'application est maintenant :
- ✅ Compatible avec le serveur de référence
- ✅ Intégrée dans DO-Core OS
- ✅ Fonctionnelle en simulation
- ✅ Prête pour hardware réel (avec #define USE_REAL_HARDWARE)

