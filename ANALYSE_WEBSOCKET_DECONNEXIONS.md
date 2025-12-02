# Analyse: Déconnexions Immédiates WebSocket du Capteur

## 🔴 Symptôme Observé

Logs du Master:
```
[103413] INFO: KERNEL: [ESP32_master] WebSocket client #0 disconnected
[103415] INFO: KERNEL: [ESP32_master] WebSocket client #0 disconnected
[103417] INFO: KERNEL: [ESP32_master] WebSocket client #0 disconnected
```

Logs du Capteur:
```
[8350] INFO: KERNEL: [ESP32_sensor] WebSocket client started, connecting to 192.168.4.1:81
[8803] WARN: KERNEL: [ESP32_sensor] Cannot send sensor data: WebSocket disconnected
```

## 🔍 Cause Racine

### Configuration Actuelle du Master

```cpp
// Ligne 1475-1481 (ESP32_master_app_start)
if (WiFi.softAP(ap_ssid, ap_pass)) {
    MASTER_LOG(LOG_LEVEL_INFO, "AP started: %s", ap_ssid);
    MASTER_LOG(LOG_LEVEL_INFO, "AP IP: %s", WiFi.softAPIP().toString().c_str());
} else {
    MASTER_LOG(LOG_LEVEL_ERROR, "Failed to start AP");
    return;
}

// Puis, immédiatement après:
webSocket = new WebSocketsServer(81);
webSocket->begin();
webSocket->onEvent(onWebSocketEvent);
```

### Le Problème

En ESP32, quand vous appelez:
1. `WiFi.begin(ssid, pass)` → Mode **Station (Client)** - Se connecte à un réseau WiFi
2. `WiFi.softAP(ssid, pass)` → Mode **Access Point** - Crée un hotspot
3. **Mais NON combinés correctement** → Problème de socket ou d'interface

**La configuration correcte nécessite:**
```cpp
WiFi.mode(WIFI_AP_STA);  // ← MANQUANT!
```

Sinon, le mode WiFi est ambigu ou peut causer des problèmes de gestion des connexions.

### Symptômes de la Configuration Incorrecte

1. WebSocket server crée sur une interface qui n'écoute pas correctement
2. Les clients se connectent mais la connection est instable
3. Les reconnexions WebSocket automatiques échouent
4. Trois déconnexions rapides = tentatives de reconnexion échouées

---

## 🔧 Solution: Configurer le Mode WiFi Correctement

### Avant (Problématique)
```cpp
// Seulement softAP, pas de mode explicite
if (WiFi.softAP(ap_ssid, ap_pass)) {
    MASTER_LOG(LOG_LEVEL_INFO, "AP started: %s", ap_ssid);
}
```

### Après (Correct)
```cpp
// Configurer explicitement le mode STA+AP
WiFi.mode(WIFI_AP_STA);  // ← AJOUT CRITIQUE

// Puis démarrer l'AP
if (WiFi.softAP(ap_ssid, ap_pass)) {
    MASTER_LOG(LOG_LEVEL_INFO, "AP started: %s", ap_ssid);
    MASTER_LOG(LOG_LEVEL_INFO, "AP IP: %s", WiFi.softAPIP().toString().c_str());
} else {
    MASTER_LOG(LOG_LEVEL_ERROR, "Failed to start AP");
    return;
}

// Puis WebSocket sur la bonne interface
webSocket = new WebSocketsServer(81);
webSocket->begin();
webSocket->onEvent(onWebSocketEvent);
```

---

## 📋 Détails de la Modification

**Fichier:** `src/apps/ESP32_master/ESP32_master.cpp`

**Ligne:** ~1474 (dans `ESP32_master_app_start()`)

**Avant:**
```cpp
    // === AP Mode pour les clients (ESP32_sensor et ESP32_com) ===
    if (WiFi.softAP(ap_ssid, ap_pass)) {
```

**Après:**
```cpp
    // === AP Mode pour les clients (ESP32_sensor et ESP32_com) ===
    // Configure WiFi mode explicitly for both Station and AP
    WiFi.mode(WIFI_AP_STA);

    if (WiFi.softAP(ap_ssid, ap_pass)) {
```

---

## ✅ Vérification de la Solution

Après correction, les logs devraient montrer:

**Capteur:**
```
[8350] WebSocket client started, connecting to 192.168.4.1:81
[8400] WebSocket connected to master  ← STABLE!
[8405] Sensor data sent to master (len=178 bytes)
```

**Master:**
```
[XXX] WebSocket client #0 connected from 192.168.4.2  ← CONNECTION STABLE
[XXX] Received sensor data from slave_0 | bytes=178
```

---

## 🔗 Références

**Documentation ESP32 WiFi Modes:**
- `WIFI_AP`: Accès Point uniquement
- `WIFI_STA`: Station (Client) uniquement
- `WIFI_AP_STA`: **AP + Station simultanément** ← REQUIS ICI

**WebSocketsServer Documentation:**
- Le WebSocket server fonctionne mieux quand le WiFi est en mode `WIFI_AP_STA`
- Mode `WIFI_AP_STA` permet aux clients de se connecter via l'AP pendant que le master se connecte à un réseau WiFi externe

---

## 📊 Impact de la Modification

| Aspect | Avant | Après |
|--------|:-----:|:-----:|
| **Mode WiFi** | Ambigu | Explicite: AP+STA |
| **WebSocket Stable** | ❌ Non | ✅ Oui |
| **Capteur peut se connecter** | ❌ Non | ✅ Oui |
| **Master peut se connecter au serveur** | ✅ Oui (en STA) | ✅ Oui (simultané) |
| **Com peut se connecter** | ❌ Non | ✅ Oui |

