# Tests IrrigAppMaster - Checklist de Validation

## 🎯 Objectif
Valider que l'application IrrigAppMaster fonctionne correctement après les corrections.

## ✅ Tests de Compilation

### 1. Compilation PlatformIO
```bash
cd /home/dorus/Documents/GitHub/DO-Core-Os
pio run
```

**Attendu** :
- ✅ Compilation sans erreurs
- ✅ Pas de warnings critiques
- ✅ Taille firmware < 1.3MB

### 2. Vérification Dépendances
```bash
pio lib list
```

**Attendu** :
- ✅ ArduinoJson présent
- ✅ WiFi (intégré ESP32)
- ✅ HTTPClient (intégré ESP32)

## ✅ Tests de Démarrage

### 1. Boot Système
**Logs attendus** :
```
=== D'O-Core Init ===
Ver: 0.3.0
Heap: XXXXX
NVS init...
NVS OK
Init components...
Init App Manager
App Manager OK
IrrigApp registered
App %s registered (ID: 1)
WiFi connected
NTP sync OK
```

### 2. Initialisation Application
**Commande shell** :
```
app_start 1
```

**Logs attendus** :
```
IrrigAppMaster: Initializing irrigation system...
Device ID: ESP32_IRRIGATION_11100454456464674
Server: http://10.223.73.53:3000
Initializing hardware (SIMULATION MODE)...
Initialized 12 simulated moisture sensors
IrrigAppMaster: Initialization complete
IrrigAppMaster: Starting irrigation system...
Running in SIMULATION mode
WiFi connected: 192.168.X.X
Registering device with server...
Device registered successfully
IrrigAppMaster: System ready
```

## ✅ Tests Fonctionnels

### 1. Enregistrement Device
**Vérifier** :
- ✅ Requête POST `/api/devices/register` envoyée
- ✅ Payload contient `type: "register"`
- ✅ Payload contient `deviceId` et `capacity`
- ✅ Header `X-Signature` présent
- ✅ Réponse 200/201 reçue

**Logs attendus** :
```
Registering device with server...
Sending registration request...
Device registered successfully
```

### 2. Poll Configuration
**Attendre 10 secondes**

**Vérifier** :
- ✅ Requête GET `/api/devices/{id}/config` envoyée
- ✅ Intervalle = 10 secondes (pas 30s)
- ✅ Parsing JSON si zones reçues

**Logs attendus** :
```
Polling configuration...
Config received, parsing...
Received configuration for X zones
Zone 1: zone_abc123
  Water: 2000ml/day, Time: 08:00, Threshold: 25%
  Sensors: 3
```

### 3. Lecture Capteurs
**Attendre 5 secondes**

**Vérifier** :
- ✅ Lecture toutes les 5 secondes
- ✅ 12 capteurs simulés
- ✅ Valeurs réalistes (15-75%)
- ✅ Variation par zone (Tomates, Laitue, Carottes, Mixte)

**Logs attendus** :
```
Current sensor readings:
  Zone 1, Sensor 1 (s_01): 35.2% moisture
  Zone 1, Sensor 2 (s_02): 36.8% moisture
  Zone 1, Sensor 3 (s_03): 37.1% moisture
```

### 4. Envoi Données Capteurs
**Attendre 15 secondes**

**Vérifier** :
- ✅ Requête POST `/api/devices/sensor-data` envoyée
- ✅ Payload contient `type: "data"`
- ✅ `globalData` présent (temperature, humidity, pressure, batteryLevel, signalStrength)
- ✅ `zonesData` avec `zoneId` et `soilMoisture[]`
- ✅ Chaque capteur a `sensorId` (s_01, s_02...) et `value`

**Logs attendus** :
```
Sending sensor data: X zones
Sensor data sent successfully
```

**Exemple payload** :
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

### 5. Assignation Zone Dynamique
**Serveur envoie config avec nouvelle zone**

**Vérifier** :
- ✅ Zone ajoutée dans ZONE_STACK
- ✅ Capteurs assignés dans SENSOR_STACK
- ✅ Mapping séquentiel (baseOffset calculé)
- ✅ assignedZoneCount incrémenté

**Logs attendus** :
```
Received configuration for 1 zones
Zone 1: zone_abc123
  Water: 2000ml/day, Time: 08:00, Threshold: 25%
  Sensors: 3
```

### 6. Suppression Zone
**Serveur envoie commande delete_zone**

**Vérifier** :
- ✅ Zone retirée de ZONE_STACK
- ✅ Capteurs libérés dans SENSOR_STACK
- ✅ Irrigation arrêtée si active
- ✅ assignedZoneCount décrémenté

**Logs attendus** :
```
Processing zone deletion: zone_abc123
Found zone zone_abc123 in slot 1
Freed sensor s_01
Freed sensor s_02
Freed sensor s_03
Zone zone_abc123 removed from device
```

### 7. Irrigation Programmée
**Configurer zone avec heure actuelle + 1 minute**

**Attendre l'heure programmée**

**Vérifier** :
- ✅ checkIrrigationSchedule() détecte heure
- ✅ executeIrrigation() appelé
- ✅ Pompe activée (GPIO HIGH)
- ✅ Timer démarré
- ✅ Arrêt automatique après durée

**Logs attendus** :
```
Scheduled irrigation for zone 1
Starting irrigation for zone zone_abc123
Pump: ON - Duration: 200s
[après durée]
Irrigation completed
Pump: OFF
```

### 8. Irrigation d'Urgence
**Simuler humidité < seuil (ex: 20% < 25%)**

**Vérifier** :
- ✅ checkMoistureThresholds() détecte seuil
- ✅ Irrigation d'urgence déclenchée (60s)
- ✅ Log warning

**Logs attendus** :
```
Zone 1 moisture critical: 20.5% < 25%
Starting irrigation for zone zone_abc123
Pump: ON - Duration: 60s
```

### 9. Arrêt Application
**Commande shell** :
```
app_stop 1
```

**Vérifier** :
- ✅ Irrigation arrêtée si active
- ✅ Pompe désactivée
- ✅ Cleanup complet

**Logs attendus** :
```
IrrigAppMaster: Stopping irrigation system...
Emergency stop: irrigation halted
IrrigAppMaster: Cleanup completed
App IrrigAppMaster stopped
```

## ✅ Tests Hardware Réel

### Configuration
```cpp
irrig_config.simulation_mode = false;
```

**Compiler avec** :
```cpp
#define USE_REAL_HARDWARE
```

### Vérifications
- ✅ Pins relais initialisés (2, 4, 16, 17, 5)
- ✅ Pins ADC configurés (32-39, 25-27, 14, 12-13)
- ✅ Lecture ADC fonctionnelle
- ✅ Contrôle relais fonctionnel
- ✅ Pompe activée/désactivée correctement

## ✅ Tests de Robustesse

### 1. Perte WiFi
**Débrancher WiFi**

**Vérifier** :
- ✅ Application continue de tourner
- ✅ Pas de crash
- ✅ Logs "WiFi not connected"
- ✅ Reprise automatique après reconnexion

### 2. Serveur Indisponible
**Arrêter serveur**

**Vérifier** :
- ✅ Erreurs HTTP loggées
- ✅ Pas de crash
- ✅ Retry automatique
- ✅ Reprise après redémarrage serveur

### 3. JSON Invalide
**Serveur envoie JSON malformé**

**Vérifier** :
- ✅ Erreur parsing détectée
- ✅ Pas de crash
- ✅ Log erreur

### 4. Mémoire
**Vérifier heap** :
```
sys_info
```

**Vérifier** :
- ✅ Heap stable (pas de fuite)
- ✅ Stack app < 8192 bytes
- ✅ Pas de stack overflow

## 📊 Métriques de Succès

| Métrique | Cible | Statut |
|----------|-------|--------|
| Compilation | ✅ Sans erreur | ⏳ |
| Boot système | ✅ < 5s | ⏳ |
| Enregistrement device | ✅ < 30s | ⏳ |
| Poll config | ✅ Toutes les 10s | ⏳ |
| Lecture capteurs | ✅ Toutes les 5s | ⏳ |
| Envoi données | ✅ Toutes les 15s | ⏳ |
| Assignation zone | ✅ Instantanée | ⏳ |
| Suppression zone | ✅ < 1s | ⏳ |
| Irrigation programmée | ✅ À l'heure exacte | ⏳ |
| Irrigation urgence | ✅ < 30s après détection | ⏳ |
| Heap libre | ✅ > 100KB | ⏳ |
| Uptime | ✅ > 24h sans crash | ⏳ |

## 🔍 Commandes Shell Utiles

```bash
# Lister applications
app_list

# Démarrer application
app_start 1

# Arrêter application
app_stop 1

# Info système
sys_info

# Logs
log_level 3  # DEBUG
log_level 2  # INFO
```

## 📝 Notes de Test

### Date : ___________
### Testeur : ___________

**Résultats** :
- [ ] Tous les tests passent
- [ ] Tests partiels (préciser) : ___________
- [ ] Bugs trouvés : ___________

**Observations** :
___________________________________________
___________________________________________
___________________________________________
