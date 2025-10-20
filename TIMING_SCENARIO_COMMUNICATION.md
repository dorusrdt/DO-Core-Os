# ⏱️ Timing et Scénario de Communication - Système d'Irrigation

Ce document explique les intervalles de temps et le flux complet d'échange de données entre les 3 applications.

---

## 📊 Configuration des Intervalles de Temps

### **APP 1 : Master**
```cpp
poll_interval_seconds = 10;              // Interroge le serveur FastAPI toutes les 10s
sensor_read_interval_seconds = 5;        // (Non utilisé par Master)
data_send_interval_seconds = 15;         // Envoie les données au serveur toutes les 15s
```

### **APP 2 : Slave Sensors**
```cpp
read_interval_ms = 5000;                 // Lit les capteurs toutes les 5 secondes
samples_per_read = 5;                    // Fait 5 lectures et calcule la moyenne
// Envoie au Master : toutes les 15 secondes (configuré dans l'app)
```

### **APP 3 : Slave Relays**
```cpp
safety_timeout_ms = 3600000;             // Timeout de sécurité : 1 heure max
status_publish_interval_ms = 10000;      // Envoie le statut au Master toutes les 10s
```

---

## 🔄 Scénario Complet : Timeline sur 30 Secondes

```
T=0s    ┌─────────────────────────────────────────────────────────┐
        │ DÉMARRAGE DU SYSTÈME                                     │
        │ - Les 3 apps démarrent                                   │
        │ - Les 3 serveurs HTTP démarrent (ports 8080, 8081, 8082)│
        └─────────────────────────────────────────────────────────┘

T=0s    [SLAVE1] Lecture capteurs #1
        └─> Lit 12 capteurs d'humidité
        └─> Lit température, humidité, pression
        └─> Stocke en mémoire

T=5s    [SLAVE1] Lecture capteurs #2
        └─> Moyenne des 5 dernières lectures

T=10s   [MASTER] Poll serveur FastAPI
        └─> GET http://192.168.1.3:3000/api/devices/.../config
        └─> Récupère les commandes en attente

        [SLAVE1] Lecture capteurs #3
        [SLAVE2] Envoie statut au Master
        └─> POST http://192.168.1.61:8080/api/irrigation/status
        └─> {"zone_id":0, "is_irrigating":false, ...}

T=15s   [SLAVE1] Lecture capteurs #4
        [SLAVE1] Envoie données au Master ⭐
        └─> POST http://192.168.1.61:8080/api/sensors/data
        └─> {
              "timestamp": 1234567890,
              "moisture": [45, 52, 38, ...],
              "temperature": 23.5,
              "humidity": 65.2,
              "pressure": 1013.25,
              "battery_level": 85,
              "signal_strength": -45
            }

        [MASTER] Reçoit données Slave1 ⭐
        └─> Callback: on_sensor_data_received()
        └─> Stocke en mémoire locale
        └─> Log: "✅ Master: Received sensor data from Slave1"

        [MASTER] Envoie données au serveur FastAPI ⭐
        └─> POST http://192.168.1.3:3000/api/devices/sensor-data
        └─> Inclut les données capteurs + zones

T=20s   [SLAVE1] Lecture capteurs #5
        [SLAVE2] Envoie statut au Master
        └─> POST http://192.168.1.61:8080/api/irrigation/status

        [MASTER] Poll serveur FastAPI
        └─> GET http://192.168.1.3:3000/api/devices/.../config
        └─> Reçoit commande : "start_irrigation zone 1"

        [MASTER] Envoie commande au Slave2 ⭐
        └─> POST http://192.168.1.61:8082/api/irrigation/command
        └─> {
              "command": "start_irrigation",
              "zone_id": 1,
              "duration_seconds": 600
            }

        [SLAVE2] Reçoit commande ⭐
        └─> Callback: relays_execute_command()
        └─> Active le relais de la zone 1
        └─> Démarre le timer de 600 secondes
        └─> Log: "✅ Slave2: Received command 'start_irrigation'"

T=25s   [SLAVE1] Lecture capteurs #6

T=30s   [SLAVE1] Envoie données au Master
        [SLAVE2] Envoie statut au Master
        └─> {"zone_id":1, "is_irrigating":true, "remaining_seconds":590}

        [MASTER] Poll serveur FastAPI
        [MASTER] Envoie données au serveur FastAPI
```

---

## 📈 Diagramme de Séquence Détaillé

### **Scénario 1 : Envoi de Données Capteurs (Toutes les 15s)**

```
┌─────────┐          ┌─────────┐          ┌─────────┐          ┌──────────┐
│ SLAVE1  │          │ MASTER  │          │ SLAVE2  │          │ FastAPI  │
│(Sensors)│          │         │          │(Relays) │          │ Server   │
└────┬────┘          └────┬────┘          └────┬────┘          └────┬─────┘
     │                    │                    │                    │
     │ T=0s: Lit capteurs │                    │                    │
     │ (5 lectures/5s)    │                    │                    │
     ├───────────────────>│                    │                    │
     │                    │                    │                    │
     │ T=15s: POST        │                    │                    │
     │ /api/sensors/data  │                    │                    │
     ├───────────────────>│                    │                    │
     │                    │                    │                    │
     │ 200 OK             │                    │                    │
     │<───────────────────┤                    │                    │
     │                    │                    │                    │
     │                    │ Stocke données     │                    │
     │                    │ en mémoire         │                    │
     │                    │                    │                    │
     │                    │ POST /api/devices/sensor-data           │
     │                    ├────────────────────────────────────────>│
     │                    │                    │                    │
     │                    │ 200 OK             │                    │
     │                    │<────────────────────────────────────────┤
     │                    │                    │                    │
     │ T=30s: POST        │                    │                    │
     │ /api/sensors/data  │                    │                    │
     ├───────────────────>│                    │                    │
     │                    │                    │                    │
```

---

### **Scénario 2 : Commande d'Irrigation (Déclenchée par FastAPI)**

```
┌─────────┐          ┌─────────┐          ┌─────────┐          ┌──────────┐
│ SLAVE1  │          │ MASTER  │          │ SLAVE2  │          │ FastAPI  │
│(Sensors)│          │         │          │(Relays) │          │ Server   │
└────┬────┘          └────┬────┘          └────┬────┘          └────┬─────┘
     │                    │                    │                    │
     │                    │ T=10s: GET         │                    │
     │                    │ /api/devices/.../config                 │
     │                    ├────────────────────────────────────────>│
     │                    │                    │                    │
     │                    │ 200 OK             │                    │
     │                    │ {"pending_commands": [                  │
     │                    │   {"action":"start_irrigation",         │
     │                    │    "zone_id":1, "duration":600}         │
     │                    │ ]}                 │                    │
     │                    │<────────────────────────────────────────┤
     │                    │                    │                    │
     │                    │ POST /api/irrigation/command            │
     │                    ├───────────────────>│                    │
     │                    │                    │                    │
     │                    │                    │ Active relais zone 1
     │                    │                    │ Démarre timer 600s │
     │                    │                    │                    │
     │                    │ 200 OK             │                    │
     │                    │<───────────────────┤                    │
     │                    │                    │                    │
     │                    │                    │ T=20s: POST        │
     │                    │                    │ /api/irrigation/status
     │                    │<───────────────────┤                    │
     │                    │                    │                    │
     │                    │ 200 OK             │                    │
     │                    ├───────────────────>│                    │
     │                    │                    │                    │
```

---

## 🕐 Tableau Récapitulatif des Intervalles

| Action | Intervalle | App Concernée | Destination |
|--------|-----------|---------------|-------------|
| **Lecture capteurs** | 5s | Slave1 | Mémoire locale |
| **Envoi données capteurs** | 15s | Slave1 → Master | Master:8080 |
| **Envoi données au serveur** | 15s | Master → FastAPI | FastAPI:3000 |
| **Poll commandes** | 10s | Master → FastAPI | FastAPI:3000 |
| **Envoi statut irrigation** | 10s | Slave2 → Master | Master:8080 |
| **Exécution commande** | À la demande | Master → Slave2 | Slave2:8082 |

---

## 🔁 Cycle Complet : 1 Minute

```
0s  ─┬─ [SLAVE1] Lecture capteurs
     │
5s  ─┼─ [SLAVE1] Lecture capteurs
     │
10s ─┼─ [SLAVE1] Lecture capteurs
     ├─ [MASTER] Poll FastAPI
     └─ [SLAVE2] Envoie statut
     │
15s ─┼─ [SLAVE1] Lecture capteurs
     ├─ [SLAVE1] Envoie données → Master ⭐
     └─ [MASTER] Envoie données → FastAPI ⭐
     │
20s ─┼─ [SLAVE1] Lecture capteurs
     ├─ [MASTER] Poll FastAPI
     └─ [SLAVE2] Envoie statut
     │
25s ─┼─ [SLAVE1] Lecture capteurs
     │
30s ─┼─ [SLAVE1] Lecture capteurs
     ├─ [SLAVE1] Envoie données → Master ⭐
     ├─ [MASTER] Poll FastAPI
     ├─ [MASTER] Envoie données → FastAPI ⭐
     └─ [SLAVE2] Envoie statut
     │
35s ─┼─ [SLAVE1] Lecture capteurs
     │
40s ─┼─ [SLAVE1] Lecture capteurs
     ├─ [MASTER] Poll FastAPI
     └─ [SLAVE2] Envoie statut
     │
45s ─┼─ [SLAVE1] Lecture capteurs
     ├─ [SLAVE1] Envoie données → Master ⭐
     └─ [MASTER] Envoie données → FastAPI ⭐
     │
50s ─┼─ [SLAVE1] Lecture capteurs
     ├─ [MASTER] Poll FastAPI
     └─ [SLAVE2] Envoie statut
     │
55s ─┼─ [SLAVE1] Lecture capteurs
     │
60s ─┴─ [SLAVE1] Lecture capteurs
        [SLAVE1] Envoie données → Master ⭐
        [MASTER] Poll FastAPI
        [MASTER] Envoie données → FastAPI ⭐
        [SLAVE2] Envoie statut
```

---

## 📊 Fréquence des Communications

### **Slave1 → Master**
- **Fréquence** : Toutes les 15 secondes
- **Payload** : ~500 bytes (12 capteurs + métadonnées)
- **Par heure** : 240 requêtes
- **Par jour** : 5,760 requêtes

### **Master → FastAPI**
- **Fréquence** : Toutes les 15 secondes (données) + 10 secondes (poll)
- **Payload données** : ~1000 bytes
- **Payload poll** : ~200 bytes
- **Par heure** : 240 (données) + 360 (poll) = 600 requêtes
- **Par jour** : 14,400 requêtes

### **Slave2 → Master**
- **Fréquence** : Toutes les 10 secondes
- **Payload** : ~300 bytes
- **Par heure** : 360 requêtes
- **Par jour** : 8,640 requêtes

### **Master → Slave2**
- **Fréquence** : À la demande (commandes)
- **Payload** : ~200 bytes
- **Par heure** : Variable (0-100)
- **Par jour** : Variable (0-2,400)

---

## 🎯 Points Clés

### **1. Synchronisation**
- Les intervalles sont **indépendants** (pas de synchronisation stricte)
- Utilise `millis()` pour le timing (précision ±10ms)
- Pas de conflit car chaque app a son propre thread

### **2. Latence**
- **Slave1 → Master** : ~50-200ms (localhost)
- **Master → FastAPI** : ~100-500ms (réseau local)
- **Master → Slave2** : ~50-200ms (localhost)

### **3. Tolérance aux Pannes**
- Si Master ne répond pas : Slave1 réessaie 3 fois (délai 1s)
- Si FastAPI ne répond pas : Master réessaie au prochain poll (10s)
- Si Slave2 ne répond pas : Master réessaie 3 fois (délai 1s)

### **4. Mode Dégradé**
- Si FastAPI inaccessible : Le système local continue de fonctionner
- Les données sont stockées en mémoire (buffer limité)
- Pas de persistance des données non envoyées (perte possible)

---

## 🔧 Modification des Intervalles

Pour changer les intervalles, modifie `main.cpp` :

```cpp
// Master
irrig_config.poll_interval_seconds = 10;          // Poll FastAPI
irrig_config.data_send_interval_seconds = 15;     // Envoi données

// Slave1
sensor_config.read_interval_ms = 5000;            // Lecture capteurs

// Slave2
relay_config.status_publish_interval_ms = 10000;  // Envoi statut
```

**⚠️ Attention** :
- Intervalles trop courts → Surcharge réseau
- Intervalles trop longs → Données obsolètes
- Recommandé : 5-30 secondes pour la plupart des cas

---

## 📝 Logs Typiques sur 30 Secondes

```
[0s]   [INFO] Slave1: Reading sensors (sample 1/5)
[5s]   [INFO] Slave1: Reading sensors (sample 2/5)
[10s]  [INFO] Master: Polling FastAPI server
[10s]  [INFO] Slave1: Reading sensors (sample 3/5)
[10s]  [INFO] Slave2: Publishing status to Master
[15s]  [INFO] Slave1: Reading sensors (sample 4/5)
[15s]  [INFO] 📥 MasterHTTP: Incoming POST /api/sensors/data from 192.168.1.61
[15s]  [INFO] ✅ Master: Received sensor data from Slave1
[15s]  [INFO] Master: Sending sensor data to server
[15s]  [INFO] Master: Data sent to server successfully
[20s]  [INFO] Master: Polling FastAPI server
[20s]  [INFO] Slave1: Reading sensors (sample 5/5)
[20s]  [INFO] Slave2: Publishing status to Master
[20s]  [INFO] 📤 Master → Slave2: Sending 'start_irrigation' to http://192.168.1.61:8082
[20s]  [INFO] 📥 Slave2: Incoming POST /api/irrigation/command from 192.168.1.61
[20s]  [INFO] ✅ Slave2: Received command 'start_irrigation' for zone 1 (duration: 600s)
[20s]  [INFO] ✅ Master → Slave2: Command 'start_irrigation' sent successfully!
[25s]  [INFO] Slave1: Reading sensors (sample 1/5)
[30s]  [INFO] Master: Polling FastAPI server
[30s]  [INFO] Slave1: Reading sensors (sample 2/5)
[30s]  [INFO] 📥 MasterHTTP: Incoming POST /api/sensors/data from 192.168.1.61
[30s]  [INFO] ✅ Master: Received sensor data from Slave1
[30s]  [INFO] Master: Sending sensor data to server
[30s]  [INFO] Slave2: Publishing status to Master
```

---

**Dernière mise à jour : 2025-10-17**
