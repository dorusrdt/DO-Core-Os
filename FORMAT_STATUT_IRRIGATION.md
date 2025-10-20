# 📊 Format du Statut d'Irrigation (Slave2 → Master)

Ce document décrit le contenu exact du statut envoyé par Slave2 (Relays) au Master toutes les 10 secondes.

---

## 🔄 Fréquence d'Envoi

- **Intervalle** : 10 secondes (configurable via `status_publish_interval_ms`)
- **Endpoint** : `POST http://192.168.1.61:8080/api/irrigation/status`
- **Content-Type** : `application/json`

---

## 📦 Structure du Payload JSON

```json
{
  "zone_id": 0,
  "is_irrigating": false,
  "remaining_seconds": 0,
  "pump_running": false,
  "relay_states": [false, false, false, false],
  "timestamp": 1234567890
}
```

---

## 📋 Description des Champs

### **1. `zone_id`** (uint8_t)
- **Type** : Entier (0-3)
- **Description** : ID de la zone actuellement en irrigation
- **Valeurs** :
  - `0` : Zone 1
  - `1` : Zone 2
  - `2` : Zone 3
  - `3` : Zone 4
  - Si aucune irrigation en cours, contient la dernière zone irriguée

**Exemple** :
```json
"zone_id": 1  // Zone 2 en cours d'irrigation
```

---

### **2. `is_irrigating`** (bool)
- **Type** : Booléen
- **Description** : Indique si une irrigation est actuellement en cours
- **Valeurs** :
  - `true` : Irrigation active
  - `false` : Aucune irrigation

**Exemple** :
```json
"is_irrigating": true  // Irrigation en cours
```

---

### **3. `remaining_seconds`** (uint32_t)
- **Type** : Entier (secondes)
- **Description** : Temps restant avant la fin de l'irrigation
- **Valeurs** :
  - `> 0` : Nombre de secondes restantes
  - `0` : Aucune irrigation en cours

**Exemple** :
```json
"remaining_seconds": 540  // 9 minutes restantes
```

---

### **4. `pump_running`** (bool)
- **Type** : Booléen
- **Description** : État de la pompe principale
- **Valeurs** :
  - `true` : Pompe activée
  - `false` : Pompe arrêtée

**Exemple** :
```json
"pump_running": true  // Pompe en marche
```

---

### **5. `relay_states`** (array[4])
- **Type** : Tableau de 4 booléens
- **Description** : État de chaque relais de zone
- **Index** :
  - `[0]` : Zone 1
  - `[1]` : Zone 2
  - `[2]` : Zone 3
  - `[3]` : Zone 4
- **Valeurs** :
  - `true` : Relais activé (zone en irrigation)
  - `false` : Relais désactivé

**Exemple** :
```json
"relay_states": [false, true, false, false]  // Seule la zone 2 est active
```

---

### **6. `timestamp`** (uint32_t)
- **Type** : Entier (millisecondes)
- **Description** : Timestamp du statut (millis() de l'ESP32)
- **Utilité** : Détecter les messages obsolètes ou perdus

**Exemple** :
```json
"timestamp": 1234567890  // 1234567 secondes depuis le boot
```

---

## 📊 Exemples de Statuts

### **Exemple 1 : Aucune Irrigation**

```json
{
  "zone_id": 0,
  "is_irrigating": false,
  "remaining_seconds": 0,
  "pump_running": false,
  "relay_states": [false, false, false, false],
  "timestamp": 95557
}
```

**Interprétation** :
- Aucune irrigation en cours
- Tous les relais sont OFF
- Pompe arrêtée
- Système au repos

---

### **Exemple 2 : Irrigation Zone 2 en Cours**

```json
{
  "zone_id": 1,
  "is_irrigating": true,
  "remaining_seconds": 540,
  "pump_running": true,
  "relay_states": [false, true, false, false],
  "timestamp": 105566
}
```

**Interprétation** :
- Zone 2 en irrigation (zone_id = 1)
- 540 secondes restantes (9 minutes)
- Pompe activée
- Seul le relais de la zone 2 est ON

---

### **Exemple 3 : Irrigation Zone 4 Presque Terminée**

```json
{
  "zone_id": 3,
  "is_irrigating": true,
  "remaining_seconds": 15,
  "pump_running": true,
  "relay_states": [false, false, false, true],
  "timestamp": 115566
}
```

**Interprétation** :
- Zone 4 en irrigation (zone_id = 3)
- 15 secondes restantes
- Pompe activée
- Seul le relais de la zone 4 est ON

---

## 🔍 Logs Associés

### **Côté Slave2 (Envoi)**

```
[75538] INFO: KERNEL: 📤 Slave2: Publishing status (interval: 10000ms, elapsed: 10007ms)
[75538] INFO: KERNEL: 📤 Slave2 → Master: Sending status
[75538] INFO: KERNEL:    Zone: 0 | Irrigating: NO | Remaining: 0s | Pump: OFF
[75723] INFO: KERNEL: ✅ Slave2: Status published successfully
```

### **Côté Master (Réception)**

```
[75723] DEBUG: KERNEL: Master: Received irrigation status from Slave2
[75723] DEBUG: KERNEL:   Zone: 0, Irrigating: NO, Remaining: 0s
```

---

## 🎯 Utilisation du Statut par le Master

Le Master utilise ce statut pour :

1. **Surveiller l'état du système**
   - Vérifier que Slave2 est toujours actif (heartbeat)
   - Détecter les pannes (pas de statut reçu pendant 30s)

2. **Afficher l'état en temps réel**
   - Dashboard web
   - Interface CLI (`irrig_status`)

3. **Envoyer au serveur FastAPI**
   - Le Master peut inclure ce statut dans les données envoyées au serveur
   - Permet un monitoring centralisé

4. **Logs et historique**
   - Enregistrer les événements d'irrigation
   - Calculer les statistiques (temps total d'irrigation, etc.)

---

## 🔧 Modification du Contenu

Pour ajouter des champs au statut, modifie :

### **1. Structure de données** (`irrig_types.h`)

```cpp
typedef struct {
    uint8_t zone_id;
    bool is_irrigating;
    uint32_t remaining_seconds;
    bool pump_running;
    bool relay_states[MAX_ZONES];
    uint32_t timestamp;
    
    // Nouveaux champs
    float water_flow_rate;      // Débit d'eau (L/min)
    uint32_t total_water_used;  // Eau totale utilisée (L)
} IrrigationStatusPacket_t;
```

### **2. Sérialisation JSON** (`irrig_communication.cpp`)

```cpp
doc["water_flow_rate"] = status->water_flow_rate;
doc["total_water_used"] = status->total_water_used;
```

### **3. Mise à jour du statut** (`irrig_app_slave_relays.cpp`)

```cpp
void relays_get_status(IrrigationStatusPacket_t* status) {
    // ... code existant ...
    status->water_flow_rate = read_flow_sensor();
    status->total_water_used = g_total_water_used;
}
```

---

## 📊 Taille du Payload

- **Taille moyenne** : ~150-200 bytes
- **Fréquence** : Toutes les 10 secondes
- **Bande passante** : ~15-20 bytes/seconde
- **Par heure** : ~54-72 KB
- **Par jour** : ~1.3-1.7 MB

**Impact réseau** : Négligeable sur un réseau local

---

## 🔐 Sécurité

**Note** : Actuellement, aucune authentification n'est implémentée.

Pour sécuriser :
1. Ajouter un token d'authentification dans le header
2. Utiliser HTTPS au lieu de HTTP
3. Valider l'IP source côté Master
4. Ajouter un HMAC pour vérifier l'intégrité

---

## 🧪 Test Manuel

Pour tester l'envoi de statut manuellement :

```bash
# Depuis un terminal Linux
curl -X POST http://192.168.1.61:8080/api/irrigation/status \
  -H "Content-Type: application/json" \
  -d '{
    "zone_id": 1,
    "is_irrigating": true,
    "remaining_seconds": 600,
    "pump_running": true,
    "relay_states": [false, true, false, false],
    "timestamp": 123456
  }'
```

**Réponse attendue** :
```json
{
  "status": "ok",
  "received_at": 123456
}
```

---

**Dernière mise à jour : 2025-10-17**
