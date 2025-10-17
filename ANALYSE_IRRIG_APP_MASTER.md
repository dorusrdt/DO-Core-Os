# Analyse de l'Application IrrigAppMaster

## Vue d'ensemble

**IrrigAppMaster** est une application de contrôle d'irrigation intelligente pour DO-Core OS, basée sur l'architecture **ZONE_STACK / SENSOR_STACK** du code de référence JavaScript.

**Version** : 1.0.0 - Production Ready ✅

---

## Architecture Globale

### Composants Principaux

```
IrrigAppMaster
├── Zone Management (4 zones physiques)
├── Sensor Management (12 capteurs d'humidité)
├── HTTP Communication (serveur FastAPI)
├── Irrigation Control (relais + pompe)
└── Time Management (NTP via DO-Core)
```

### Fichiers

- `irrig_app_master.h` (127 lignes) - Définitions et API
- `irrig_app_master.cpp` (819 lignes) - Implémentation
- `README.md` (405 lignes) - Documentation

---

## 1. Architecture ZONE_STACK / SENSOR_STACK

### Zone Stack (4 slots physiques)

```cpp
struct ZoneSlot {
    int id;                    // 1-4 (ID physique)
    bool configured;           // Zone configurée
    String zoneId;             // ID serveur (ex: "zone_abc123")
    int waterPerDay;           // Volume eau/jour (ml)
    String irrigationTime;     // Heure "HH:MM"
    int humidityThreshold;     // Seuil urgence (%)
};

static ZoneSlot ZONE_STACK[4];
```

**Fonctionnement** :
- 4 slots physiques fixes (matériel ESP32)
- Configuration dynamique depuis serveur
- Préservation zones existantes lors updates
- Support suppression à chaud (`handleZoneDeletion`)

### Sensor Stack (12 slots physiques)

```cpp
struct SensorSlot {
    String id;                 // "s_01" à "s_12"
    bool assigned;             // Assigné à une zone
    String zoneId;             // Référence zone
};

static SensorSlot SENSOR_STACK[12];
```

**Fonctionnement** :
- 12 capteurs physiques (pins ADC ESP32)
- Assignation dynamique aux zones
- Mapping séquentiel automatique
- Libération lors suppression zone

---

## 2. Configuration et Paramètres

### Structure de Configuration

```cpp
typedef struct {
    char server_url[128];                    // URL serveur FastAPI
    char device_id[64];                      // ID unique device
    char device_secret[32];                  // Secret HMAC
    uint16_t poll_interval_seconds;          // 10s (config serveur)
    uint16_t sensor_read_interval_seconds;   // 5s (lecture capteurs)
    uint16_t data_send_interval_seconds;     // 15s (envoi données)
    uint8_t max_zones;                       // 4
    uint8_t max_sensors;                     // 12
    bool simulation_mode;                    // true/false
} IrrigAppConfig_t;
```

### Configuration Actuelle (main.cpp)

```cpp
IrrigAppConfig_t config = {
    .server_url = "http://10.232.133.53:3000",
    .device_id = "ESP32_IRRIGATION_11100454456464674",
    .device_secret = "esp32-secure-key-2024",
    .poll_interval_seconds = 10,
    .sensor_read_interval_seconds = 5,
    .data_send_interval_seconds = 15,
    .max_zones = 4,
    .max_sensors = 12,
    .simulation_mode = true
};
```

---

## 3. Hardware et Pins

### Relais (Contrôle Irrigation)

```cpp
#define ZONE_1_RELAY_PIN 2
#define ZONE_2_RELAY_PIN 4
#define ZONE_3_RELAY_PIN 16
#define ZONE_4_RELAY_PIN 17
#define PUMP_RELAY_PIN   5
```

### Capteurs ADC (Humidité du Sol)

```cpp
#define MOISTURE_PIN_1  32
#define MOISTURE_PIN_2  33
#define MOISTURE_PIN_3  34
#define MOISTURE_PIN_4  35
#define MOISTURE_PIN_5  36
#define MOISTURE_PIN_6  39
#define MOISTURE_PIN_7  25
#define MOISTURE_PIN_8  26
#define MOISTURE_PIN_9  27
#define MOISTURE_PIN_10 14
#define MOISTURE_PIN_11 12
#define MOISTURE_PIN_12 13
```

### Indicateurs

```cpp
#define STATUS_LED_PIN   18
#define BUZZER_PIN       19
```

---

## 4. Cycle de Vie de l'Application

### Callbacks DO-Core

```cpp
SysError_t irrig_app_master_init(void);    // Initialisation
void irrig_app_master_start(void);         // Démarrage
void irrig_app_master_loop(void);          // Boucle principale
void irrig_app_master_stop(void);          // Arrêt
```

### Séquence d'Initialisation

```
1. Validation configuration
2. Initialisation ZONE_STACK (4 slots vides)
3. Initialisation SENSOR_STACK (12 slots "s_01" à "s_12")
4. Initialisation hardware (relais/capteurs)
5. Initialisation simulation (valeurs réalistes)
6. Enregistrement app_manager
```

### Séquence de Démarrage

```
1. Vérification WiFi (géré par DO-Core)
2. Enregistrement device auprès serveur
3. Log statut système
4. Prêt pour boucle principale
```

### Boucle Principale (10ms par cycle)

```cpp
void irrig_app_master_loop(void) {
    // Vérifier WiFi
    if (WiFi.status() != WL_CONNECTED) return;
    
    // Timer irrigation
    checkIrrigationTimer();
    
    // Poll config (10s)
    if (elapsed >= poll_interval) {
        pollConfiguration();
    }
    
    // Lecture capteurs (5s)
    if (elapsed >= sensor_interval) {
        readAllSensors();
    }
    
    // Envoi données (15s)
    if (elapsed >= data_interval) {
        sendSensorData();
    }
    
    // Planning irrigation (1 min)
    if (elapsed % 60000 < 1000) {
        checkIrrigationSchedule();
    }
    
    // Seuils humidité (30s)
    if (elapsed % 30000 < 1000) {
        checkMoistureThresholds();
    }
}
```

---

## 5. Communication HTTP

### Endpoints Serveur

**1. Enregistrement Device**
```
POST /api/devices/register
Body: {type, deviceId, capacity{zones, sensors}, timestamp}
Headers: X-Signature, X-Timestamp
```

**2. Poll Configuration**
```
GET /api/devices/{deviceId}/config
Response: {type: "config", zones: [...]}
```

**3. Envoi Données Capteurs**
```
POST /api/devices/sensor-data
Body: {type, deviceId, timestamp, globalData, zonesData}
```

### Format Données Envoyées

```json
{
  "type": "data",
  "deviceId": "ESP32_IRRIGATION_...",
  "timestamp": "2024-10-17T00:30:00.000Z",
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
        {"sensorId": "s_02", "value": 38.1}
      ]
    }
  ]
}
```

### Authentification HMAC

```cpp
String generateHMAC(String data) {
    // TODO: Implémenter HMAC-SHA256 avec mbedtls
    return "dummy_signature_" + String(millis());
}
```

**⚠️ Note** : HMAC actuellement en mode dummy, à implémenter avec mbedtls.

---

## 6. Gestion des Zones

### Parsing Configuration Serveur

```cpp
void parseConfiguration(String jsonResponse) {
    // 1. Vérifier commandes delete_zone
    // 2. Traiter zones reçues
    // 3. Préserver zones existantes (update config seulement)
    // 4. Assigner nouvelles zones à slots libres
    // 5. Mapper capteurs séquentiellement
}
```

### Assignation Capteurs

**Algorithme** :
```
Pour chaque nouvelle zone :
  1. Calculer baseOffset (capteurs zones précédentes)
  2. Pour chaque capteur de la zone :
     - Mapper vers ID hardware séquentiel
     - Trouver slot SENSOR_STACK libre
     - Assigner : id, zoneId, assigned=true
```

### Suppression Zone

```cpp
void handleZoneDeletion(String zoneId) {
    // 1. Arrêter irrigation si active
    // 2. Trouver slot zone
    // 3. Libérer capteurs (assigned=false)
    // 4. Nettoyer slot zone
    // 5. Retirer de assignedZones[]
    // 6. Mettre à jour deviceAssigned
}
```

---

## 7. Gestion des Capteurs

### Mode Simulation

```cpp
void initializeSimulatedSensors(void) {
    for (int i = 0; i < 12; i++) {
        int zoneIndex = i / 3;  // 3 capteurs par zone
        
        switch (zoneIndex) {
            case 0: // Tomates
                simulatedMoisture[i] = 35 + random(-5, 10);
                break;
            case 1: // Laitue
                simulatedMoisture[i] = 55 + random(-5, 8);
                break;
            case 2: // Carottes
                simulatedMoisture[i] = 45 + random(-8, 12);
                break;
            case 3: // Mixte
                simulatedMoisture[i] = 50 + random(-10, 10);
                break;
        }
    }
}
```

### Mise à Jour Simulation

```cpp
void updateSimulatedSensors(void) {
    // 1. Mettre à jour données environnementales globales
    globalTemperature += random(-100, 101) / 100.0;
    globalHumidity += random(-250, 251) / 100.0;
    globalPressure += random(-500, 501) / 100.0;
    
    // 2. Générer lectures réalistes par capteur
    for (int i = 0; i < 12; i++) {
        float baseValue = 30 + (i * 2);
        float variation = random(-500, 501) / 100.0;
        simulatedMoisture[i] = constrain(baseValue + variation, 15, 75);
    }
}
```

### Mode Réel (TODO)

```cpp
void readRealSensors(void) {
    // TODO: Implémenter lecture ADC
    // analogRead(MOISTURE_PIN_x)
    // Conversion 0-4095 → 0-100%
}
```

---

## 8. Contrôle d'Irrigation

### Irrigation Programmée

```cpp
void checkIrrigationSchedule(void) {
    // Obtenir heure actuelle (NTP via DO-Core)
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return;
    
    char currentTime[6];
    strftime(currentTime, 6, "%H:%M", &timeinfo);
    
    // Vérifier chaque zone
    for (int i = 0; i < 4; i++) {
        if (ZONE_STACK[i].configured && 
            ZONE_STACK[i].irrigationTime.equals(currentTime)) {
            
            int duration = ZONE_STACK[i].waterPerDay / 10;
            executeIrrigation(ZONE_STACK[i].zoneId, duration);
        }
    }
}
```

### Irrigation d'Urgence

```cpp
void checkMoistureThresholds(void) {
    for (int i = 0; i < 4; i++) {
        if (!ZONE_STACK[i].configured) continue;
        
        // Calculer moyenne humidité zone
        float avgMoisture = calculateZoneAverage(i);
        
        // Vérifier seuil critique
        if (avgMoisture < ZONE_STACK[i].humidityThreshold) {
            kernel_log(LOG_LEVEL_WARN, "Zone %d critical: %.1f%%", i, avgMoisture);
            executeIrrigation(ZONE_STACK[i].zoneId, 60); // 1 minute
        }
    }
}
```

### Exécution Irrigation

```cpp
void executeIrrigation(String zoneId, int durationSeconds) {
    // 1. Trouver slot zone
    // 2. Vérifier pas déjà en cours
    // 3. Activer pompe (GPIO HIGH)
    // 4. Définir timer arrêt
    
    digitalWrite(PUMP_RELAY_PIN, HIGH);
    pumpRunning = true;
    isIrrigating = true;
    activeIrrigationTimer = millis() + (durationSeconds * 1000);
}
```

### Timer Irrigation

```cpp
void checkIrrigationTimer(void) {
    if (isIrrigating && millis() >= activeIrrigationTimer) {
        // Arrêter irrigation
        digitalWrite(PUMP_RELAY_PIN, LOW);
        pumpRunning = false;
        isIrrigating = false;
        activeIrrigationTimer = 0;
    }
}
```

---

## 9. Intégration DO-Core OS

### Dépendances Kernel

```cpp
#include "../../kernel/core/log_system_optimized.h"  // Logs
#include "../../kernel/app/app_manager.h"            // App lifecycle
```

### Services Utilisés

**WiFi** : Géré par DO-Core
```cpp
if (WiFi.status() == WL_CONNECTED) {
    // WiFi prêt, pas de gestion manuelle
}
```

**NTP** : Géré par DO-Core
```cpp
struct tm timeinfo;
if (getLocalTime(&timeinfo)) {
    // Heure synchronisée via NTP
}
```

**Logs** : Via kernel
```cpp
kernel_log(LOG_LEVEL_INFO, "Message");
kernel_log(LOG_LEVEL_WARN, "Warning");
kernel_log(LOG_LEVEL_ERROR, "Error");
kernel_log(LOG_LEVEL_DEBUG, "Debug");
```

### Enregistrement Application

```cpp
SysError_t register_irrig_app_master(const IrrigAppConfig_t* config) {
    // 1. Copier configuration
    memcpy(&app_config, config, sizeof(IrrigAppConfig_t));
    
    // 2. Validation
    if (strlen(app_config.device_id) == 0) return SYS_INVALID_PARAM;
    
    // 3. Créer callbacks
    AppCallbacks_t callbacks = {
        .init = irrig_app_master_init,
        .start = irrig_app_master_start,
        .stop = irrig_app_master_stop,
        .loop = irrig_app_master_loop
    };
    
    // 4. Enregistrer
    return app_register("IrrigAppMaster", "Smart Irrigation", 
                       APP_TYPE_USER, &callbacks, &app_id);
}
```

---

## 10. Points Forts

✅ **Architecture propre** : ZONE_STACK / SENSOR_STACK bien défini
✅ **Configuration dynamique** : Zones assignées depuis serveur
✅ **Simulation réaliste** : Valeurs capteurs par type de culture
✅ **Communication HTTP** : Endpoints complets (register, config, data)
✅ **Contrôle irrigation** : Programmé + urgence
✅ **Intégration DO-Core** : WiFi, NTP, logs gérés par kernel
✅ **Gestion erreurs** : Retry, fallback, logs détaillés
✅ **Sécurité** : Arrêt d'urgence si suppression zone

---

## 11. Points à Améliorer

### ⚠️ Critique

**1. HMAC-SHA256 non implémenté**
```cpp
String generateHMAC(String data) {
    // TODO: Implémenter vrai HMAC-SHA256 avec mbedtls
    return "dummy_signature_" + String(millis());
}
```
**Impact** : Sécurité compromise, authentification factice

**2. Lecture capteurs réels non implémentée**
```cpp
void readRealSensors(void) {
    // TODO: Implémenter lecture réelle GPIO ADC
    for (int i = 0; i < 12; i++) {
        simulatedMoisture[i] = 50.0f; // Valeur fixe temporaire
    }
}
```
**Impact** : Mode hardware réel non fonctionnel

### ⚠️ Mineur

**3. Pas de gestion relais zones individuelles**
- Actuellement : Seule la pompe est contrôlée
- Manque : Activation relais zones spécifiques (ZONE_1_RELAY_PIN, etc.)

**4. Pas de validation timestamp serveur**
- Pas de vérification drift temporel
- Pas de détection replay attacks

**5. Pas de persistance configuration**
- Configuration perdue au reboot
- Pas de sauvegarde NVS

---

## 12. Recommandations

### Priorité 1 : Sécurité

**Implémenter HMAC-SHA256**
```cpp
#include <mbedtls/md.h>

String generateHMAC(String data) {
    mbedtls_md_context_t ctx;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
    
    const size_t payloadLength = data.length();
    const size_t keyLength = strlen(app_config.device_secret);
    
    byte hmacResult[32];
    
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1);
    mbedtls_md_hmac_starts(&ctx, (const unsigned char*)app_config.device_secret, keyLength);
    mbedtls_md_hmac_update(&ctx, (const unsigned char*)data.c_str(), payloadLength);
    mbedtls_md_hmac_finish(&ctx, hmacResult);
    mbedtls_md_free(&ctx);
    
    // Convertir en hex
    String signature = "";
    for (int i = 0; i < 32; i++) {
        char hex[3];
        sprintf(hex, "%02x", hmacResult[i]);
        signature += hex;
    }
    
    return signature;
}
```

### Priorité 2 : Hardware Réel

**Implémenter lecture ADC**
```cpp
void readRealSensors(void) {
    const int pins[] = {MOISTURE_PIN_1, MOISTURE_PIN_2, /* ... */};
    
    for (int i = 0; i < 12; i++) {
        // Lire ADC avec moyennage
        int sum = 0;
        for (int j = 0; j < MOISTURE_SAMPLES; j++) {
            sum += analogRead(pins[i]);
            delay(10);
        }
        int adcValue = sum / MOISTURE_SAMPLES;
        
        // Convertir ADC → % humidité
        // 4095 (sec) → 0%, 0 (mouillé) → 100%
        simulatedMoisture[i] = map(adcValue, MOISTURE_DRY_VALUE, MOISTURE_WET_VALUE, 0, 100);
        simulatedMoisture[i] = constrain(simulatedMoisture[i], 0, 100);
    }
}
```

### Priorité 3 : Contrôle Zones

**Activer relais zones individuelles**
```cpp
void executeIrrigation(String zoneId, int durationSeconds) {
    ZoneSlot* zone = findZoneSlot(zoneId);
    if (!zone) return;
    
    // Activer relais zone spécifique
    int relayPin = getZoneRelayPin(zone->id);
    digitalWrite(relayPin, HIGH);
    
    // Activer pompe
    digitalWrite(PUMP_RELAY_PIN, HIGH);
    
    // Timer
    activeIrrigationTimer = millis() + (durationSeconds * 1000);
    activeZoneId = zoneId;
}

int getZoneRelayPin(int zoneId) {
    switch (zoneId) {
        case 1: return ZONE_1_RELAY_PIN;
        case 2: return ZONE_2_RELAY_PIN;
        case 3: return ZONE_3_RELAY_PIN;
        case 4: return ZONE_4_RELAY_PIN;
        default: return -1;
    }
}
```

---

## 13. Conclusion

**IrrigAppMaster** est une application **bien architecturée** et **fonctionnelle** pour le contrôle d'irrigation intelligent. L'architecture ZONE_STACK / SENSOR_STACK est propre et extensible.

### État Actuel

✅ **Production Ready** en mode simulation
⚠️ **Nécessite implémentations** pour production réelle (HMAC, ADC, relais zones)

### Points Forts

- Architecture claire et maintenable
- Intégration DO-Core OS réussie
- Communication HTTP complète
- Gestion dynamique zones/capteurs

### Prochaines Étapes

1. Implémenter HMAC-SHA256 (sécurité)
2. Implémenter lecture ADC réelle (hardware)
3. Activer contrôle relais zones (irrigation)
4. Ajouter persistance NVS (configuration)
5. Tests hardware complets

L'application est prête pour tests en environnement contrôlé avec implémentation des points critiques.
