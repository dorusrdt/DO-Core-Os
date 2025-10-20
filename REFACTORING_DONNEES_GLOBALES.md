# 🔄 Refactoring : Master Génère les Données Globales

## 📋 Résumé du Changement

**Objectif** : Déplacer la génération des données environnementales globales de **Slave1** vers **Master**

---

## 🏗️ Architecture AVANT

```
┌─────────────────────────────────────────────────────────────┐
│  SLAVE1 (Capteurs)                                           │
│                                                              │
│  ✅ Lit 12x capteurs humidité sol (ADC)                     │
│  ✅ Génère Temp/Hum/Pression (simulation)                   │
│  ✅ Génère Batterie (simulation)                            │
│  ✅ Lit Signal WiFi (réel)                                  │
│  ✅ Envoie TOUT → Master                                    │
│                                                              │
└──────────────────────────┬───────────────────────────────────┘
                           │
                           │ HTTP POST
                           │ {moisture[12], temp, hum, press, bat, wifi}
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│  MASTER (Orchestration)                                      │
│                                                              │
│  ✅ Reçoit TOUTES les données de Slave1                     │
│  ✅ Agrège et envoie au serveur                             │
│                                                              │
└──────────────────────────┬───────────────────────────────────┘
                           │
                           │ HTTP POST
                           │ /api/devices/sensor-data
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│  SERVEUR FASTAPI                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 🏗️ Architecture APRÈS

```
┌─────────────────────────────────────────────────────────────┐
│  SLAVE1 (Capteurs)                                           │
│                                                              │
│  ✅ Lit 12x capteurs humidité sol (ADC)                     │
│  ✅ Envoie UNIQUEMENT moisture[12] → Master                 │
│  ❌ Ne génère PLUS Temp/Hum/Pression                        │
│  ❌ Ne génère PLUS Batterie                                 │
│  ❌ Ne lit PLUS Signal WiFi                                 │
│                                                              │
└──────────────────────────┬───────────────────────────────────┘
                           │
                           │ HTTP POST
                           │ {moisture[12]} UNIQUEMENT
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│  MASTER (Orchestration)                                      │
│                                                              │
│  ✅ Reçoit moisture[12] de Slave1                           │
│  ✅ GÉNÈRE Temp/Hum/Pression (simulation)                   │
│  ✅ GÉNÈRE Batterie (simulation)                            │
│  ✅ LIT Signal WiFi (réel - WiFi.RSSI())                    │
│  ✅ Agrège TOUT et envoie au serveur                        │
│                                                              │
└──────────────────────────┬───────────────────────────────────┘
                           │
                           │ HTTP POST
                           │ /api/devices/sensor-data
                           │ (FORMAT INCHANGÉ)
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│  SERVEUR FASTAPI                                             │
│  ✅ AUCUN CHANGEMENT                                         │
└─────────────────────────────────────────────────────────────┘
```

---

## 🔧 Modifications Effectuées

### **1. Slave1 : `irrig_app_slave_sensors.cpp`**

#### **Suppression Variables Globales**

**Avant** :
```cpp
static float simulated_moisture[MAX_SENSORS];
static float global_temperature = 24.5;
static float global_humidity = 60.0;
static float global_pressure = 1012.0;
```

**Après** :
```cpp
// Données capteurs (UNIQUEMENT humidité sol)
static float simulated_moisture[MAX_SENSORS];
```

---

#### **Modification `sensors_read_all()`**

**Avant** :
```cpp
void sensors_read_all(SensorDataPacket_t* packet) {
    // Lire capteurs d'humidité
    sensors_update_simulation(simulated_moisture);
    
    // Copier données
    for (int i = 0; i < MAX_SENSORS; i++) {
        packet->moisture[i] = simulated_moisture[i];
    }
    
    // ❌ Données environnementales
    packet->temperature = global_temperature;
    packet->humidity = global_humidity;
    packet->pressure = global_pressure;
    packet->battery_level = 85.0 + random(-10, 16);
    packet->signal_strength = WiFi.RSSI();
}
```

**Après** :
```cpp
void sensors_read_all(SensorDataPacket_t* packet) {
    // ✅ Lire UNIQUEMENT capteurs d'humidité sol
    sensors_update_simulation(simulated_moisture);
    
    // ✅ Copier UNIQUEMENT données d'humidité
    for (int i = 0; i < MAX_SENSORS; i++) {
        packet->moisture[i] = simulated_moisture[i];
    }
    
    // ✅ Données globales mises à 0 (seront générées par Master)
    packet->temperature = 0.0;
    packet->humidity = 0.0;
    packet->pressure = 0.0;
    packet->battery_level = 0.0;
    packet->signal_strength = 0;
    
    kernel_log(LOG_LEVEL_DEBUG, "Slave1: Read %d moisture sensors", MAX_SENSORS);
}
```

---

#### **Simplification `sensors_update_simulation()`**

**Avant** :
```cpp
void sensors_update_simulation(float moisture[MAX_SENSORS]) {
    // ❌ Mettre à jour données environnementales
    float tempVariation = (random(-100, 101) / 100.0);
    float humidityVariation = (random(-250, 251) / 100.0);
    float pressureVariation = (random(-500, 501) / 100.0);
    
    global_temperature = constrain(global_temperature + tempVariation, 15, 40);
    global_humidity = constrain(global_humidity + humidityVariation, 30, 90);
    global_pressure = constrain(global_pressure + pressureVariation, 990, 1030);
    
    // Mettre à jour capteurs
    for (int i = 0; i < MAX_SENSORS; i++) {
        float variation = (random(-200, 201) / 100.0);
        moisture[i] = constrain(moisture[i] + variation, 15, 85);
    }
}
```

**Après** :
```cpp
void sensors_update_simulation(float moisture[MAX_SENSORS]) {
    // ✅ Mettre à jour UNIQUEMENT capteurs d'humidité sol
    for (int i = 0; i < MAX_SENSORS; i++) {
        float variation = (random(-200, 201) / 100.0);
        moisture[i] = constrain(moisture[i] + variation, 15, 85);
    }
}
```

---

### **2. Master : `irrig_app_master.cpp`**

#### **Ajout Variable Batterie**

**Avant** :
```cpp
static float globalTemperature = 24.5;
static float globalHumidity = 60.0;
static float globalPressure = 1012.0;
```

**Après** :
```cpp
// ✅ Données environnementales globales (GÉNÉRÉES PAR MASTER)
static float globalTemperature = 24.5;
static float globalHumidity = 60.0;
static float globalPressure = 1012.0;
static float globalBatteryLevel = 85.0;
```

---

#### **Nouvelle Fonction `updateGlobalEnvironmentData()`**

```cpp
void updateGlobalEnvironmentData(void) {
    // ✅ GÉNÉRER données environnementales avec variation réaliste (simulation)
    float tempVariation = (random(-100, 101) / 100.0);      // ±1°C
    float humidityVariation = (random(-250, 251) / 100.0);  // ±2.5%
    float pressureVariation = (random(-500, 501) / 100.0);  // ±5 hPa
    float batteryVariation = (random(-50, 51) / 100.0);     // ±0.5%
    
    globalTemperature = constrain(globalTemperature + tempVariation, 15.0, 40.0);
    globalHumidity = constrain(globalHumidity + humidityVariation, 30.0, 90.0);
    globalPressure = constrain(globalPressure + pressureVariation, 990.0, 1030.0);
    globalBatteryLevel = constrain(globalBatteryLevel + batteryVariation, 70.0, 100.0);
    
    kernel_log(LOG_LEVEL_DEBUG, "Master: Generated environment data - T=%.1f°C, H=%.1f%%, P=%.1fhPa, Bat=%.1f%%",
               globalTemperature, globalHumidity, globalPressure, globalBatteryLevel);
}
```

---

#### **Modification `updateSensorDataFromSlave()`**

**Avant** :
```cpp
void updateSensorDataFromSlave(float moisture[MAX_SENSORS], float temp, float hum, float press) {
    // Mettre à jour données capteurs reçues de Slave1
    for (int i = 0; i < MAX_SENSORS; i++) {
        simulatedMoisture[i] = moisture[i];
    }
    
    // ❌ Mettre à jour données environnementales (reçues de Slave1)
    globalTemperature = temp;
    globalHumidity = hum;
    globalPressure = press;
}
```

**Après** :
```cpp
void updateSensorDataFromSlave(float moisture[MAX_SENSORS], float temp, float hum, float press) {
    // ✅ Mettre à jour UNIQUEMENT données d'humidité reçues de Slave1
    for (int i = 0; i < MAX_SENSORS; i++) {
        simulatedMoisture[i] = moisture[i];
    }
    
    // ✅ GÉNÉRER données environnementales localement (simulation)
    updateGlobalEnvironmentData();
}
```

---

#### **Modification `sendSensorData()`**

**Avant** :
```cpp
JsonObject globalData = doc.createNestedObject("globalData");
globalData["temperature"] = globalTemperature;
globalData["humidity"] = globalHumidity;
globalData["pressure"] = globalPressure;
globalData["batteryLevel"] = 85.0 + random(-10, 16);  // ❌ Aléatoire
globalData["signalStrength"] = WiFi.RSSI();
```

**Après** :
```cpp
// ✅ Données environnementales globales (GÉNÉRÉES PAR MASTER)
JsonObject globalData = doc.createNestedObject("globalData");
globalData["temperature"] = globalTemperature;      // Simulé
globalData["humidity"] = globalHumidity;            // Simulé
globalData["pressure"] = globalPressure;            // Simulé
globalData["batteryLevel"] = globalBatteryLevel;    // Simulé
globalData["signalStrength"] = WiFi.RSSI();         // ✅ RÉEL
```

---

### **3. Master : `irrig_app_master.h`**

**Ajout déclaration** :

```cpp
// Gestion capteurs (reçus des Slaves)
void updateGlobalEnvironmentData(void);  // ✅ Génère données globales (simulation)
void updateSensorDataFromSlave(float moisture[MAX_SENSORS], float temp, float hum, float press);
```

---

## 📊 Comparaison Avant/Après

| Aspect | Avant | Après |
|--------|-------|-------|
| **Génération Temp/Hum/Pression** | Slave1 | Master |
| **Génération Batterie** | Slave1 | Master |
| **Lecture WiFi RSSI** | Slave1 | Master |
| **Slave1 envoie** | 7 valeurs | 12 valeurs (moisture uniquement) |
| **Master génère** | 0 valeurs | 4 valeurs (temp, hum, press, bat) |
| **Format POST serveur** | Inchangé | Inchangé ✅ |
| **Serveur FastAPI** | Aucun changement | Aucun changement ✅ |

---

## 📺 Logs Attendus

### **Slave1 (Simplifié)**

**Avant** :
```
[DEBUG] Sensors read: T=24.5°C, H=60.0%, P=1012.0hPa
```

**Après** :
```
[DEBUG] Slave1: Read 12 moisture sensors
```

---

### **Master (Enrichi)**

**Avant** :
```
[DEBUG] Sensor data received from Slave1:
[DEBUG]   Environment: T=24.5°C, H=60.0%, P=1012.0hPa
```

**Après** :
```
[DEBUG] Master: Generated environment data - T=24.5°C, H=60.0%, P=1012.0hPa, Bat=85.0%
[DEBUG] Sensor data received from Slave1:
[DEBUG]   Moisture sensors: 12 values updated
[DEBUG]   Environment (generated): T=24.5°C, H=60.0%, P=1012.0hPa
```

---

## ✅ Avantages du Refactoring

### **1. Séparation des Responsabilités**

- ✅ **Slave1** : Spécialisé dans la lecture physique (ADC)
- ✅ **Master** : Responsable de la logique métier et simulation

---

### **2. Simplification Slave1**

- ✅ Code plus simple et focalisé
- ✅ Moins de variables globales
- ✅ Moins de calculs (pas de simulation environnementale)

---

### **3. Centralisation Logique**

- ✅ Toute la simulation est dans Master
- ✅ Plus facile à maintenir
- ✅ Plus facile à tester

---

### **4. WiFi RSSI Correct**

- ✅ **Avant** : RSSI de Slave1 (pas très utile)
- ✅ **Après** : RSSI de Master (device principal)

---

### **5. Rétrocompatibilité**

- ✅ Format POST `/api/devices/sensor-data` **INCHANGÉ**
- ✅ Serveur FastAPI **AUCUN CHANGEMENT**
- ✅ Dashboard **AUCUN CHANGEMENT**

---

## 🧪 Test

### **Étape 1 : Compiler**

```bash
cd /home/dorus/Documents/GitHub/DO-Core-Os
pio run
```

---

### **Étape 2 : Flasher Slave1**

```bash
# Copier config Slave1
cp DEVICE_CONFIGS/main_device2_slave_sensors.cpp src/main.cpp

# Compiler et flasher
pio run --target upload --upload-port /dev/ttyUSB1
```

---

### **Étape 3 : Flasher Master**

```bash
# Copier config Master
cp DEVICE_CONFIGS/main_device1_master.cpp src/main.cpp

# Compiler et flasher
pio run --target upload --upload-port /dev/ttyUSB0
```

---

### **Étape 4 : Vérifier Logs**

**Slave1** :
```
[DEBUG] Slave1: Read 12 moisture sensors
```

**Master** :
```
[DEBUG] Master: Generated environment data - T=24.5°C, H=60.0%, P=1012.0hPa, Bat=85.0%
[DEBUG] Sensor data received from Slave1:
[DEBUG]   Moisture sensors: 12 values updated
[DEBUG]   Environment (generated): T=24.5°C, H=60.0%, P=1012.0hPa
```

---

### **Étape 5 : Vérifier Serveur**

```bash
# Surveiller logs serveur
cd test_server
python3 server.py
```

**Logs attendus** :
```
📥 SENSOR DATA
   Device ID: ESP32_IRRIGATION_11100454456464674
   Global: T=24.5°C, H=60.0%, P=1012.0hPa, Bat=85.0%, WiFi=-45dBm
   Zones: 1
```

---

## 📝 Résumé

### **Ce qui a changé**

1. ✅ Slave1 envoie **UNIQUEMENT** `moisture[12]`
2. ✅ Master **GÉNÈRE** Temp/Hum/Pression/Batterie
3. ✅ Master **LIT** WiFi.RSSI() (réel)

---

### **Ce qui n'a PAS changé**

1. ✅ Format POST `/api/devices/sensor-data`
2. ✅ Serveur FastAPI
3. ✅ Dashboard
4. ✅ Structure `SensorDataPacket_t`

---

**Refactoring terminé avec succès !** 🚀

**Dernière mise à jour : 2025-10-18**
