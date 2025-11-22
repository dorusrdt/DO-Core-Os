# 🔍 Vérification de la Logique d'Irrigation

Ce document vérifie la logique d'irrigation du système D'O-Core OS.

## 📊 1. Calcul de la Moyenne des Capteurs

### ✅ Vérification : Moyenne vs `humidityThreshold`

**Fonction** : `checkMoistureThresholds()` (ligne 859-897)

**Logique** :
1. Pour chaque zone configurée, le système :
   - Parse les données des capteurs depuis ESP32_sensor (via WebSocket)
   - Calcule la moyenne de tous les capteurs assignés à cette zone
   - Compare la moyenne au `humidityThreshold` de la zone

**Code** :
```cpp
// Check average moisture for this zone
float totalMoisture = 0;
int sensorCount = 0;

for (int s = 0; s < MAX_SENSORS; s++) {
    if (SENSOR_STACK[s].assigned && SENSOR_STACK[s].zoneId == ZONE_STACK[i].zoneId) {
        float moisture = 50.0; // Default fallback
        if (sensorDoc.containsKey(SENSOR_STACK[s].id)) {
            moisture = sensorDoc[SENSOR_STACK[s].id];
        }
        totalMoisture += moisture;
        sensorCount++;
    }
}

if (sensorCount > 0) {
    float avgMoisture = totalMoisture / sensorCount;

    if (avgMoisture < ZONE_STACK[i].humidityThreshold) {
        // Déclenche l'irrigation d'urgence
        executeIrrigation(ZONE_STACK[i].zoneId, 60);
    }
}
```

**✅ Résultat** : La moyenne est bien calculée et comparée au `humidityThreshold`.

**⚠️ Note** : Si un capteur n'a pas de données, la valeur par défaut est `50.0%`.

---

## ⏱️ 2. Calcul de la Durée d'Ouverture de la Vanne

### ✅ Vérification : Durée d'Irrigation

Il existe **deux types d'irrigation** avec des durées différentes :

### A. Irrigation Programmée (Schedule)

**Fonction** : `checkIrrigationSchedule()` (ligne 838-856)

**Déclenchement** : Quand l'heure actuelle correspond à `irrigationTime` de la zone

**Calcul de la durée** :
```cpp
executeIrrigation(ZONE_STACK[i].zoneId, ZONE_STACK[i].waterPerDay / 10);
```

**Formule** : `durationSeconds = waterPerDay / 10`

**Exemples** :
- `waterPerDay = 5000 ml` → `duration = 500 secondes` (8 minutes 20 secondes)
- `waterPerDay = 3000 ml` → `duration = 300 secondes` (5 minutes)
- `waterPerDay = 6000 ml` → `duration = 600 secondes` (10 minutes)

**⚠️ Note** : La conversion `ml / 10 = secondes` est une approximation.
- 1 ml/s = 3.6 L/h
- 5000 ml / 10 = 500s = 5 L en 8.33 minutes ≈ 36 L/h

### B. Irrigation d'Urgence (Emergency)

**Fonction** : `checkMoistureThresholds()` (ligne 888-893)

**Déclenchement** : Quand `avgMoisture < humidityThreshold`

**Durée fixe** :
```cpp
executeIrrigation(ZONE_STACK[i].zoneId, 10); // 10 seconds emergency irrigation
```

**Durée** : **10 secondes** - **FIXE**

---

## 📡 3. Format des Commandes vers ESP32_com

### ✅ Vérification : Type de Données

**Fonction** : `executeIrrigation()` (ligne 918-960)

**Protocole** : **WebSocket** (texte JSON)

**Format JSON** :

#### Commande START_IRRIGATION

```json
{
  "action": "start_irrigation",
  "zoneId": "zone_001",
  "physicalZoneNumber": 1,
  "durationSeconds": 60
}
```

**Champs** :
- `action` : `"start_irrigation"` (string)
- `zoneId` : Identifiant logique de la zone (string, ex: `"zone_001"`)
- `physicalZoneNumber` : Numéro physique de la zone (integer, 1-4)
- `durationSeconds` : Durée en secondes (integer)

**Code d'envoi** :
```cpp
DynamicJsonDocument cmd(256);
cmd["action"] = "start_irrigation";
cmd["zoneId"] = zoneId;
cmd["physicalZoneNumber"] = zoneSlot->physicalZoneNumber;
cmd["durationSeconds"] = durationSeconds;

String cmdStr;
serializeJson(cmd, cmdStr);
webSocket->broadcastTXT(cmdStr);
```

#### Commande STOP_IRRIGATION

```json
{
  "action": "stop_irrigation",
  "zoneId": "zone_001"
}
```

**Champs** :
- `action` : `"stop_irrigation"` (string)
- `zoneId` : Identifiant logique de la zone (string)

**Code d'envoi** :
```cpp
String stopCmd = "{\"action\":\"stop_irrigation\",\"zoneId\":\"" + zoneId + "\"}";
webSocket->broadcastTXT(stopCmd);
```

---

## 🔧 4. Réception et Traitement par ESP32_com

### ✅ Vérification : Parsing des Commandes

**Fonction** : `onWebSocketEvent_com()` (ligne 46-98)

**Logique** :
1. Reçoit le message JSON via WebSocket
2. Parse le JSON avec ArduinoJson
3. Extrait `action`, `zoneId`, `physicalZoneNumber`, `durationSeconds`
4. Appelle `startIrrigation()` ou `stopIrrigation()`

**Code** :
```cpp
String action = doc["action"].as<String>();

if (action == "start_irrigation") {
    String zoneId = doc["zoneId"].as<String>();
    int physicalZoneNumber = doc["physicalZoneNumber"] | 1;
    int durationSeconds = doc["durationSeconds"] | 60;

    startIrrigation(physicalZoneNumber, durationSeconds);
} else if (action == "stop_irrigation") {
    stopIrrigation();
}
```

---

## ⚙️ 5. Contrôle des Relais (ESP32_com)

### ✅ Vérification : Logique Active LOW

**Fonction** : `startIrrigation()` (ligne 101-131)

**Logique** :
- **Relais actifs à l'état LOW** (0V)
- **Relais inactifs à l'état HIGH** (3.3V)

**Séquence d'activation** :
1. **Pompe** : `digitalWrite(PUMP_RELAY_PIN, LOW)` → ON
2. **Zone** : `digitalWrite(zoneRelayPins[zoneIndex], LOW)` → ON
3. **Timer** : `irrigationEndTime = millis() + (durationSeconds * 1000)`

**Séquence de désactivation** :
1. **Zone** : `digitalWrite(zoneRelayPins[zoneIndex], HIGH)` → OFF
2. **Pompe** : `digitalWrite(PUMP_RELAY_PIN, HIGH)` → OFF

**Vérification du timer** : `checkIrrigationTimer()` (ligne 161-166)
- Vérifie toutes les itérations de la boucle si `millis() >= irrigationEndTime`
- Arrête automatiquement l'irrigation quand le temps est écoulé

---

## 📋 Résumé des Points Vérifiés

| Point | Statut | Détails |
|-------|--------|---------|
| **Moyenne des capteurs** | ✅ | Calculée correctement, comparée au `humidityThreshold` |
| **Durée programmée** | ✅ | `waterPerDay / 10` secondes |
| **Durée d'urgence** | ✅ | 60 secondes (fixe) |
| **Format commande** | ✅ | JSON via WebSocket avec `action`, `zoneId`, `physicalZoneNumber`, `durationSeconds` |
| **Contrôle relais** | ✅ | Active LOW (LOW = ON, HIGH = OFF) |
| **Timer automatique** | ✅ | Vérifié dans la boucle, arrêt automatique |

---

## ⚠️ Points d'Attention

### 1. Conversion Durée (waterPerDay / 10)

**Problème potentiel** : La formule `waterPerDay / 10` est une approximation.

**Exemple** :
- Si `waterPerDay = 5000 ml` → `500 secondes` (8.33 min)
- Débit supposé : ~36 L/h
- **Recommandation** : Vérifier si cette conversion correspond au débit réel de votre système

### 2. Valeur par Défaut des Capteurs

**Problème** : Si un capteur n'a pas de données, la valeur par défaut est `50.0%`.

**Impact** : Cela peut fausser la moyenne si certains capteurs ne répondent pas.

**Recommandation** : Exclure les capteurs sans données du calcul de moyenne.

### 3. Irrigation en Cours

**Problème** : Si une irrigation est déjà en cours, les nouvelles commandes sont ignorées.

**Code** :
```cpp
if (isIrrigating) {
    MASTER_LOG(LOG_LEVEL_WARN, "Irrigation already in progress, queuing command");
    return;
}
```

**Recommandation** : Implémenter une file d'attente pour les commandes d'irrigation.

### 4. Synchronisation Master/COM

**Problème** : Le master et ESP32_com gèrent chacun leur propre timer.

**Risque** : Désynchronisation si la communication WebSocket est interrompue.

**Recommandation** : Le master devrait recevoir un accusé de réception de ESP32_com.

---

## 🔄 Flux Complet d'Irrigation

### Irrigation Programmée

```
1. checkIrrigationSchedule() (appelée toutes les minutes)
   ↓
2. Compare l'heure actuelle avec irrigationTime
   ↓
3. executeIrrigation(zoneId, waterPerDay / 10)
   ↓
4. Envoie JSON via WebSocket à ESP32_com
   ↓
5. ESP32_com reçoit et parse la commande
   ↓
6. startIrrigation(physicalZoneNumber, durationSeconds)
   ↓
7. Active pompe (LOW) + zone (LOW)
   ↓
8. Timer démarre (irrigationEndTime)
   ↓
9. checkIrrigationTimer() vérifie le timer
   ↓
10. Arrêt automatique après durationSeconds
```

### Irrigation d'Urgence

```
1. checkMoistureThresholds() (appelée toutes les 30 secondes)
   ↓
2. Calcule la moyenne des capteurs de la zone
   ↓
3. Compare avgMoisture < humidityThreshold
   ↓
4. executeIrrigation(zoneId, 60) // 60 secondes
   ↓
5. [Même flux que l'irrigation programmée]
```

---

## 📝 Notes de Code

- **Fréquence de vérification** :
  - `checkIrrigationSchedule()` : Toutes les minutes (ligne 994)
  - `checkMoistureThresholds()` : Toutes les 30 secondes (ligne 999)
  - `checkIrrigationTimer()` : À chaque itération de la boucle (ESP32_com)

- **Protection** : Le système vérifie `isIrrigating` pour éviter les conflits

- **Logs** : Toutes les actions sont loggées avec `MASTER_LOG` et `COM_LOG`

