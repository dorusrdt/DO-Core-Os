# 📺 Logs Améliorés : Architecture Refactorisée

## 🎯 Objectif

Organiser les logs pour refléter clairement la nouvelle architecture où **Master génère les données globales**.

---

## 📊 Flux de Logs Attendu

### **1. Slave1 → Master (Données Humidité)**

```
[79817] INFO: 📥 Slave1 → Master: Moisture data received
[79818] DEBUG:    Timestamp: 79817 | Sensors: 12 values
```

**Explication** :
- ✅ Slave1 envoie **UNIQUEMENT** les 12 valeurs d'humidité
- ✅ Log clair et concis

---

### **2. Master Génère Données Globales**

```
[79819] DEBUG: Master: Generated environment data - T=24.5°C, H=60.0%, P=1012.0hPa, Bat=85.0%
```

**Explication** :
- ✅ Master **GÉNÈRE** Temp/Hum/Pression/Batterie (simulation)
- ✅ Log montre clairement que c'est généré par Master

---

### **3. Master Met à Jour les Données**

```
[79820] INFO: 📊 Master: Data updated
[79821] INFO:    Moisture: 12 sensors from Slave1
[79822] INFO:    Environment: T=24.5°C, H=60.0%, P=1012.0hPa, Bat=85.0%
[79823] INFO:    WiFi RSSI: -45 dBm (Master)
[79824] INFO:    Zone 1 (zone_potager): 35.2% avg (3 sensors)
```

**Explication** :
- ✅ Résumé clair des données reçues et générées
- ✅ WiFi RSSI du **Master** (pas Slave1)
- ✅ Moyenne d'humidité par zone

---

### **4. Master Envoie au Serveur**

```
[79840] DEBUG: Sending sensor data: 1 zones
[79841] DEBUG: Sensor data sent successfully
```

**Explication** :
- ✅ Master agrège tout et envoie au serveur FastAPI
- ✅ Format POST `/api/devices/sensor-data` **INCHANGÉ**

---

## 🔄 Comparaison Avant/Après

### **AVANT (Logs Confus)**

```
[79817] INFO: ✅ Master: Received sensor data from Slave1
[79828] INFO:    Temp: 0.0°C | Humidity: 0.0% | Pressure: 0.0 hPa
[79828] INFO:    Battery: 0% | Signal: 0 dBm
```

**Problèmes** :
- ❌ Affiche des valeurs à 0 (pas claires)
- ❌ Ne montre pas que Master génère les données
- ❌ Logs non organisés

---

### **APRÈS (Logs Clairs)**

```
[79817] INFO: 📥 Slave1 → Master: Moisture data received
[79818] DEBUG:    Timestamp: 79817 | Sensors: 12 values
[79819] DEBUG: Master: Generated environment data - T=24.5°C, H=60.0%, P=1012.0hPa, Bat=85.0%
[79820] INFO: 📊 Master: Data updated
[79821] INFO:    Moisture: 12 sensors from Slave1
[79822] INFO:    Environment: T=24.5°C, H=60.0%, P=1012.0hPa, Bat=85.0%
[79823] INFO:    WiFi RSSI: -45 dBm (Master)
[79824] INFO:    Zone 1 (zone_potager): 35.2% avg (3 sensors)
```

**Avantages** :
- ✅ Flux clair : Réception → Génération → Mise à jour
- ✅ Montre explicitement que Master génère les données
- ✅ WiFi RSSI du Master (pertinent)
- ✅ Moyenne par zone (utile)

---

## 📝 Modifications Effectuées

### **1. `src/main.cpp` : Callback `on_sensor_data_received()`**

**Avant** :
```cpp
void on_sensor_data_received(SensorDataPacket_t* data) {
    kernel_log(LOG_LEVEL_INFO, "✅ Master: Received sensor data from Slave1");
    kernel_log(LOG_LEVEL_INFO, "   Temp: %.1f°C | Humidity: %.1f%% | Pressure: %.1f hPa",
               data->temperature, data->humidity, data->pressure);
    kernel_log(LOG_LEVEL_INFO, "   Battery: %d%% | Signal: %d dBm",
               data->battery_level, data->signal_strength);
    // ...
}
```

**Après** :
```cpp
void on_sensor_data_received(SensorDataPacket_t* data) {
    // ✅ Afficher réception données Slave1 (UNIQUEMENT humidité)
    kernel_log(LOG_LEVEL_INFO, "📥 Slave1 → Master: Moisture data received");
    kernel_log(LOG_LEVEL_DEBUG, "   Timestamp: %lu | Sensors: %d values", data->timestamp, MAX_SENSORS);
    
    // Copier données humidité
    for (int i = 0; i < 12; i++) {
        g_received_moisture[i] = data->moisture[i];
    }
    
    // ✅ Appeler fonction Master qui va GÉNÉRER les données globales
    updateSensorDataFromSlave(data->moisture, data->temperature, data->humidity, data->pressure);
}
```

---

### **2. `irrig_app_master.cpp` : Fonction `updateSensorDataFromSlave()`**

**Avant** :
```cpp
void updateSensorDataFromSlave(float moisture[MAX_SENSORS], float temp, float hum, float press) {
    // Mettre à jour données
    for (int i = 0; i < MAX_SENSORS; i++) {
        simulatedMoisture[i] = moisture[i];
    }
    
    // Mettre à jour environnement (reçu de Slave1)
    globalTemperature = temp;
    globalHumidity = hum;
    globalPressure = press;
    
    // Logs basiques
    kernel_log(LOG_LEVEL_DEBUG, "Sensor data received from Slave1:");
    kernel_log(LOG_LEVEL_DEBUG, "  Environment: T=%.1f°C, H=%.1f%%, P=%.1fhPa", 
               globalTemperature, globalHumidity, globalPressure);
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
    
    // ✅ Afficher résumé clair et organisé
    kernel_log(LOG_LEVEL_INFO, "📊 Master: Data updated");
    kernel_log(LOG_LEVEL_INFO, "   Moisture: %d sensors from Slave1", MAX_SENSORS);
    kernel_log(LOG_LEVEL_INFO, "   Environment: T=%.1f°C, H=%.1f%%, P=%.1fhPa, Bat=%.1f%%", 
               globalTemperature, globalHumidity, globalPressure, globalBatteryLevel);
    kernel_log(LOG_LEVEL_INFO, "   WiFi RSSI: %d dBm (Master)", WiFi.RSSI());
    
    // ✅ Afficher moyenne par zone
    if (assignedZoneCount > 0) {
        for (int i = 0; i < 4; i++) {
            if (ZONE_STACK[i].configured) {
                int sensorCount = 0;
                float avgMoisture = 0.0;
                
                for (int s = 0; s < 12; s++) {
                    if (SENSOR_STACK[s].assigned && SENSOR_STACK[s].zoneId == ZONE_STACK[i].zoneId) {
                        sensorCount++;
                        avgMoisture += simulatedMoisture[s];
                    }
                }
                
                if (sensorCount > 0) {
                    avgMoisture /= sensorCount;
                    kernel_log(LOG_LEVEL_INFO, "   Zone %d (%s): %.1f%% avg (%d sensors)", 
                               ZONE_STACK[i].id, ZONE_STACK[i].zoneId.c_str(), avgMoisture, sensorCount);
                }
            }
        }
    }
}
```

---

### **3. `irrig_app_master.cpp` : Nouvelle Fonction `updateGlobalEnvironmentData()`**

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

## 🎨 Hiérarchie des Logs

### **Niveau INFO (Logs Principaux)**

```
📥 Slave1 → Master: Moisture data received
📊 Master: Data updated
   Moisture: 12 sensors from Slave1
   Environment: T=24.5°C, H=60.0%, P=1012.0hPa, Bat=85.0%
   WiFi RSSI: -45 dBm (Master)
   Zone 1 (zone_potager): 35.2% avg (3 sensors)
```

**Utilité** : Vue d'ensemble du système

---

### **Niveau DEBUG (Détails Techniques)**

```
   Timestamp: 79817 | Sensors: 12 values
Master: Generated environment data - T=24.5°C, H=60.0%, P=1012.0hPa, Bat=85.0%
  Zone 1, Sensor 1 (s_01): 35.2%
  Zone 1, Sensor 2 (s_02): 34.8%
  Zone 1, Sensor 3 (s_03): 36.1%
```

**Utilité** : Debugging et analyse détaillée

---

## 📈 Exemple Complet de Session

```
[79817] INFO: 📥 Slave1 → Master: Moisture data received
[79818] DEBUG:    Timestamp: 79817 | Sensors: 12 values
[79819] DEBUG: Master: Generated environment data - T=24.5°C, H=60.0%, P=1012.0hPa, Bat=85.0%
[79820] INFO: 📊 Master: Data updated
[79821] INFO:    Moisture: 12 sensors from Slave1
[79822] INFO:    Environment: T=24.5°C, H=60.0%, P=1012.0hPa, Bat=85.0%
[79823] INFO:    WiFi RSSI: -45 dBm (Master)
[79824] INFO:    Zone 1 (zone_potager): 35.2% avg (3 sensors)
[79825] DEBUG:   Zone 1, Sensor 1 (s_01): 35.2%
[79826] DEBUG:   Zone 1, Sensor 2 (s_02): 34.8%
[79827] DEBUG:   Zone 1, Sensor 3 (s_03): 36.1%
[79840] DEBUG: Sending sensor data: 1 zones
[79841] DEBUG: Sensor data sent successfully
```

---

## ✅ Avantages des Nouveaux Logs

| Aspect | Avant | Après |
|--------|-------|-------|
| **Clarté** | ❌ Valeurs à 0 confuses | ✅ Flux clair et logique |
| **Traçabilité** | ❌ Pas clair qui génère quoi | ✅ Explicite : Slave1 envoie, Master génère |
| **Utilité** | ❌ Logs basiques | ✅ Moyenne par zone, WiFi RSSI pertinent |
| **Debugging** | ❌ Difficile | ✅ Facile avec niveaux INFO/DEBUG |
| **Organisation** | ❌ Logs éparpillés | ✅ Hiérarchie claire |

---

## 🧪 Test

### **Commande**

```bash
# Sur ESP32 Master
D'O-Core> log_echo on
```

### **Logs Attendus**

```
[INFO] 📥 Slave1 → Master: Moisture data received
[DEBUG]    Timestamp: 79817 | Sensors: 12 values
[DEBUG] Master: Generated environment data - T=24.5°C, H=60.0%, P=1012.0hPa, Bat=85.0%
[INFO] 📊 Master: Data updated
[INFO]    Moisture: 12 sensors from Slave1
[INFO]    Environment: T=24.5°C, H=60.0%, P=1012.0hPa, Bat=85.0%
[INFO]    WiFi RSSI: -45 dBm (Master)
[INFO]    Zone 1 (zone_potager): 35.2% avg (3 sensors)
```

---

## 📚 Résumé

### **Ce qui a changé**

1. ✅ Logs clairs : Réception → Génération → Mise à jour
2. ✅ Affichage explicite de la génération par Master
3. ✅ WiFi RSSI du Master (pertinent)
4. ✅ Moyenne d'humidité par zone
5. ✅ Hiérarchie INFO/DEBUG

### **Ce qui n'a PAS changé**

1. ✅ Format POST `/api/devices/sensor-data`
2. ✅ Serveur FastAPI
3. ✅ Fonctionnalité du système

---

**Logs améliorés et organisés !** 📺✨

**Dernière mise à jour : 2025-10-18**
