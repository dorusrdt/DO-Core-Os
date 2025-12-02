# Analyse: Dépendance WiFi/Serveur pour la Réception des Données de Capteurs

## 🔴 Problème Observé
L'utilisateur a rapporté que **les données des capteurs ne sont reçues que si le master est connecté au serveur externe**.

## 🔍 Analyse du Code

### 1. Flux de Réception des Données de Capteurs

**Fichier:** `src/apps/ESP32_master/ESP32_master.cpp`

#### A. Réception WebSocket (Indépendante)
```cpp
// Ligne 1398-1419
void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    case WStype_TEXT: {
        String clientMessage = String((char*)payload, length);

        // Reçoit les données du capteur (client #0)
        if (num == 0) {
            updateSlaveSensorData(num, clientMessage);  // ✅ PAS DE DÉPENDANCE WIFI ICI
        }
    }
}
```

**Conclusion:** La réception WebSocket est **INDÉPENDANTE** de WiFi.

#### B. Mise à Jour des Données
```cpp
// Ligne 205-227
static void updateSlaveSensorData(uint8_t slaveId, String data) {
    DeserializationError error = deserializeJson(slaveSensorData, data);

    if (error) {
        MASTER_LOG(LOG_LEVEL_WARN, "Failed to parse sensor data...");
        return;
    }

    slaveSensorDataLastUpdate = millis();  // ✅ MIS À JOUR LOCALEMENT
    MASTER_LOG(LOG_LEVEL_INFO, "Received sensor data from slave_%u | bytes=%u",
               slaveId, data.length());
}
```

**Conclusion:** La mise à jour des données locales est **INDÉPENDANTE** de WiFi.

---

### 2. Dépendances WiFi/Serveur

#### A. Enregistrement du Appareil
```cpp
// Ligne 250-256
static void registerDevice() {
    if (WiFi.status() != WL_CONNECTED) {
        MASTER_LOG(LOG_LEVEL_WARN, "Cannot register: WiFi not connected");
        return;  // ← BLOCAGE
    }
    // ... envoi HTTP à serveur
}
```

#### B. Récupération de Configuration
```cpp
// Ligne 334-344
static void pollConfiguration() {
    if (WiFi.status() != WL_CONNECTED) {
        MASTER_LOG(LOG_LEVEL_WARN, "Cannot poll config: WiFi not connected");
        return;  // ← BLOCAGE
    }

    if (!deviceRegistered) {
        MASTER_LOG(LOG_LEVEL_WARN, "Cannot poll config: Device not registered");
        return;  // ← BLOCAGE
    }
}
```

#### C. Envoi des Données de Capteurs vers Serveur
```cpp
// Ligne 891-893
static void sendSensorData() {
    if (WiFi.status() != WL_CONNECTED) {
        return;  // ← BLOCAGE
    }
    // ... envoi HTTP à serveur
}
```

---

### 3. Boucle Principale: Où Est Le Problème?

```cpp
// Ligne 1560-1600
static void ESP32_master_app_loop(void) {
    webSocket->loop();                    // ← Reçoit données capteurs

    // Poll configuration every 10 seconds
    if (currentTime - lastConfigPoll >= 10000) {
        pollConfiguration();               // ← DÉPEND de WiFi/Serveur
    }

    // Send sensor data every 15 seconds
    if (currentTime - lastDataSend >= 15000) {
        sendSensorData();                  // ← DÉPEND de WiFi/Serveur
    }

    // Check irrigation schedule every 10 seconds
    if (currentTime % 10000 < 1000) {
        checkIrrigationSchedule();         // ← PEUT DÉPENDRE DE CONFIG
    }

    // Check moisture thresholds every 30 seconds
    if (currentTime % 30000 < 1000) {
        checkMoistureThresholds();         // ← DÉPEND de données locales
    }
}
```

---

## 📊 Dépendance par Fonction

| Fonction | WiFi Requis? | Serveur Requis? | Données Locales? |
|----------|:--:|:--:|:--:|
| `onWebSocketEvent()` | ❌ | ❌ | ✅ |
| `updateSlaveSensorData()` | ❌ | ❌ | ✅ |
| `checkMoistureThresholds()` | ❌ | ❌ | ✅ |
| `checkIrrigationSchedule()` | ✅ (CONFIG) | ✅ (CONFIG) | ✅ |
| `checkIrrigationTimer()` | ❌ | ❌ | ✅ |
| `executeIrrigation()` | ❌ | ❌ | ✅ |
| `sendSensorData()` | ✅ | ✅ | ✅ |
| `pollConfiguration()` | ✅ | ✅ | - |
| `registerDevice()` | ✅ | ✅ | - |

---

## 🤔 Hypothèse: Pourquoi Cela Semble Dépendre?

### Scénario 1: Pas de Configuration Chargée
Si le master n'est pas enregistré/configuré, les zones ne sont **pas parsées** dans `ZONE_STACK`.

```cpp
// Ligne 561-610: handleZoneConfiguration()
for (int i = 0; i < MAX_ZONES; i++) {
    if (ZONE_STACK[i].configured) {
        // ... process zone
    }
}
```

**Si `ZONE_STACK` est vide**, les seuils d'humidité ne sont jamais vérifiés!

### Scénario 2: Logs Trompeurs
Les logs `sendSensorData()` et `pollConfiguration()` peuvent **masquer** les véritables opérations:

```cpp
// Les logs font SEMBLER que tout dépend du serveur
MASTER_LOG(LOG_LEVEL_WARN, "Cannot poll config: WiFi not connected");  // ← Visible
// MAIS: updateSlaveSensorData() continue silencieusement  // ← Invisible
```

---

## ✅ Conclusion

**La réception des données de capteurs est INDÉPENDANTE du WiFi/Serveur.**

Cependant, le système complet requiert WiFi/Serveur pour:
1. **Enregistrer le device** → `registerDevice()`
2. **Récupérer la configuration** → `pollConfiguration()`
3. **Parser les zones** → `parseConfiguration()`, `handleZoneConfiguration()`
4. **Appliquer les seuils** → `checkMoistureThresholds()`

**Le symptôme observé** n'est probablement PAS une dépendance de réception, mais plutôt:
- Sans configuration, aucune zone n'est enregistrée
- Sans zones, aucun seuil d'irrigation n'est appliqué
- Les données sont reçues mais ignorées

---

## 🔧 Recommandations

### 1. Ajouter un Mode "Offline" (Sans Serveur)
```cpp
// Permettre un fonctionnement minimal sans serveur
- Définir des zones par défaut
- Appliquer des seuils par défaut
- Autoriser l'irrigation manuelle par CLI
```

### 2. Découpler Configuration et Fonctionnement
```cpp
// Séparer:
- Réception des capteurs (toujours ON)
- Configuration du serveur (optionnelle)
- Irrigation basée sur seuils locaux
```

### 3. Logs Explicites
```cpp
// Clarifier où le blocage se produit:
if (!deviceRegistered) {
    MASTER_LOG(LOG_LEVEL_WARN,
               "⚠️ Not registered yet - configuration unavailable, but sensor reception works");
}
```

---

## 📝 Fichiers Impactés

- `src/apps/ESP32_master/ESP32_master.cpp` (1631 lignes)
  - `registerDevice()` - ligne 250
  - `pollConfiguration()` - ligne 334
  - `updateSlaveSensorData()` - ligne 205
  - `onWebSocketEvent()` - ligne 1398
  - `ESP32_master_app_loop()` - ligne 1560

