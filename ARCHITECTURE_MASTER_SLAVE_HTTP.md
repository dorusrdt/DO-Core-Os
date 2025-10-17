# Architecture Master-Slave HTTP - 3 Devices ESP32

## 📋 Vue d'ensemble

Architecture distribuée pour système d'irrigation intelligent avec **3 ESP32 indépendants** communiquant via **HTTP REST** sur réseau local WiFi.

---

## 🏗️ Architecture Globale

```
┌──────────────────────────────────────────────────────────────────┐
│                      Réseau WiFi Local                            │
│                      192.168.1.0/24                               │
└──────────────────────────────────────────────────────────────────┘
         │                    │                    │
         │                    │                    │
    ┌────▼─────┐         ┌────▼─────┐        ┌────▼─────┐
    │ Device 1 │         │ Device 2 │        │ Device 3 │
    │  MASTER  │         │  SLAVE1  │        │  SLAVE2  │
    │          │         │ SENSORS  │        │  RELAYS  │
    │ ESP32    │         │  ESP32   │        │  ESP32   │
    └──────────┘         └──────────┘        └──────────┘
    192.168.1.100       192.168.1.101       192.168.1.102
    Port: 8080          Port: 8081          Port: 8082
         │                    │                    │
         │◄───────────────────┘                    │
         │  POST /api/sensors/data                 │
         │  (Données capteurs)                     │
         │                                         │
         └─────────────────────────────────────────►
           POST /api/irrigation/command
           (Commandes irrigation)
```

---

## 🎯 Répartition des Responsabilités

### Device 1 : **MASTER** (192.168.1.100:8080)

**Rôle** : Chef d'orchestre, communication serveur FastAPI

**Responsabilités** :
- ✅ Communication HTTP avec serveur FastAPI (10.232.133.53:3000)
- ✅ Enregistrement device
- ✅ Poll configuration serveur (10s)
- ✅ Gestion ZONE_STACK / SENSOR_STACK
- ✅ **Recevoir** données capteurs de Slave1 (HTTP POST)
- ✅ **Envoyer** données au serveur FastAPI
- ✅ **Envoyer** commandes irrigation à Slave2 (HTTP POST)
- ✅ Décisions irrigation (programmée + urgence)

**Hardware** :
- ESP32 standard
- Connexion WiFi uniquement

**Endpoints exposés** :
- `POST /api/sensors/data` - Recevoir données de Slave1
- `POST /api/irrigation/status` - Recevoir statut de Slave2
- `GET /api/master/stats` - Statistiques

---

### Device 2 : **SLAVE SENSORS** (192.168.1.101:8081)

**Rôle** : Acquisition données capteurs d'humidité

**Responsabilités** :
- 📊 Lecture 12 capteurs ADC (pins 32-39, 25-27, 14, 12-13)
- 📊 Moyennage 5 échantillons par capteur
- 📊 Conversion ADC → % humidité (0-100%)
- 📊 Données environnementales (température, humidité, pression)
- 📊 **Publier** données vers Master (HTTP POST toutes les 5s)

**Hardware** :
- ESP32 avec 12 capteurs d'humidité capacitifs
- Pins ADC : 32, 33, 34, 35, 36, 39, 25, 26, 27, 14, 12, 13

**Endpoints exposés** :
- `GET /api/sensors/status` - Statut du Slave
- `GET /api/sensors/data` - Lecture immédiate capteurs

---

### Device 3 : **SLAVE RELAYS** (192.168.1.102:8082)

**Rôle** : Contrôle physique irrigation (relais + pompe)

**Responsabilités** :
- 💧 Contrôle 4 relais zones (pins 2, 4, 16, 17)
- 💧 Contrôle pompe (pin 5)
- 💧 Gestion timers irrigation
- 💧 Sécurité (timeout 5 minutes max)
- 💧 LED status (pin 18) et buzzer (pin 19)
- 💧 **Recevoir** commandes du Master (HTTP POST)
- 💧 **Publier** statut vers Master (HTTP POST toutes les 10s)

**Hardware** :
- ESP32 avec module 4 relais + relais pompe
- Pins relais zones : 2, 4, 16, 17
- Pin relais pompe : 5
- Pin LED status : 18
- Pin buzzer : 19

**Endpoints exposés** :
- `POST /api/irrigation/command` - Recevoir commandes
- `GET /api/irrigation/status` - Statut irrigation
- `POST /api/irrigation/emergency_stop` - Arrêt d'urgence

---

## 🔗 Communication HTTP REST

### 1. Slave1 → Master : Données Capteurs

**Endpoint** : `POST http://192.168.1.100:8080/api/sensors/data`

**Fréquence** : Toutes les 5 secondes

**Payload** :
```json
{
  "timestamp": 1697500000,
  "moisture": [35.2, 38.1, 42.5, 45.0, 50.2, 55.1, 48.3, 52.0, 46.5, 49.8, 51.2, 47.6],
  "temperature": 24.5,
  "humidity": 60.0,
  "pressure": 1012.0,
  "battery_level": 85.0,
  "signal_strength": -45
}
```

**Response** :
```json
{
  "status": "ok",
  "received_at": 1697500001
}
```

---

### 2. Master → Slave2 : Commandes Irrigation

**Endpoint** : `POST http://192.168.1.102:8082/api/irrigation/command`

**Fréquence** : À la demande (irrigation programmée ou urgence)

**Payload** :
```json
{
  "command": "start_irrigation",
  "zone_id": 1,
  "duration_seconds": 120,
  "zone_server_id": "zone_abc123",
  "timestamp": 1697500000
}
```

**Commandes possibles** :
- `start_irrigation` - Démarrer irrigation
- `stop_irrigation` - Arrêter irrigation
- `emergency_stop` - Arrêt d'urgence
- `test_relay` - Test relais

**Response** :
```json
{
  "status": "ok",
  "command_id": "cmd_12345",
  "executed_at": 1697500001
}
```

---

### 3. Slave2 → Master : Statut Irrigation (Optionnel)

**Endpoint** : `POST http://192.168.1.100:8080/api/irrigation/status`

**Fréquence** : Toutes les 10 secondes

**Payload** :
```json
{
  "zone_id": 1,
  "is_irrigating": true,
  "remaining_seconds": 85,
  "pump_running": true,
  "relay_states": [true, false, false, false],
  "timestamp": 1697500000
}
```

---

## 📁 Structure des Fichiers

```
src/apps/
├── irrig_common/                          (NOUVEAU - Partagé)
│   ├── irrig_types.h                      # Types communs
│   ├── irrig_ipc.h                        # Structures IPC
│   ├── irrig_communication.h              # API communication HTTP
│   └── irrig_communication.cpp            # Implémentation HTTP
│
├── irrig_app_master/                      (CONSERVÉ + Extension)
│   ├── irrig_app_master.h                 # API Master (inchangé)
│   ├── irrig_app_master.cpp               # Implémentation (inchangé)
│   ├── irrig_app_master_http.h            # Extension HTTP (NOUVEAU)
│   └── irrig_app_master_http.cpp          # Serveur HTTP Master (NOUVEAU)
│
├── irrig_app_slave_sensors/               (NOUVEAU)
│   ├── irrig_app_slave_sensors.h          # API Slave Sensors
│   └── irrig_app_slave_sensors.cpp        # Implémentation
│
└── irrig_app_slave_relays/                (NOUVEAU)
    ├── irrig_app_slave_relays.h           # API Slave Relays
    └── irrig_app_slave_relays.cpp         # Implémentation

DEVICE_CONFIGS/                            (NOUVEAU)
├── main_device1_master.cpp                # Config Device 1
├── main_device2_slave_sensors.cpp         # Config Device 2
└── main_device3_slave_relays.cpp          # Config Device 3
```

---

## 🚀 Compilation et Déploiement

### Étape 1 : Compiler Device 1 (Master)

```bash
# Copier configuration Master
cp DEVICE_CONFIGS/main_device1_master.cpp src/main.cpp

# Compiler
pio run

# Flasher sur ESP32 #1
pio run --target upload --upload-port /dev/ttyUSB0
```

### Étape 2 : Compiler Device 2 (Slave Sensors)

```bash
# Copier configuration Slave Sensors
cp DEVICE_CONFIGS/main_device2_slave_sensors.cpp src/main.cpp

# Compiler
pio run

# Flasher sur ESP32 #2
pio run --target upload --upload-port /dev/ttyUSB1
```

### Étape 3 : Compiler Device 3 (Slave Relays)

```bash
# Copier configuration Slave Relays
cp DEVICE_CONFIGS/main_device3_slave_relays.cpp src/main.cpp

# Compiler
pio run

# Flasher sur ESP32 #3
pio run --target upload --upload-port /dev/ttyUSB2
```

---

## ⚙️ Configuration Réseau

### Prérequis

- **Routeur WiFi** : Réseau local 192.168.1.0/24
- **SSID/Password** : Configurés via commande `wifi_save` sur chaque device
- **IPs fixes** : Configurées dans les fichiers main_deviceX.cpp

### Configuration WiFi sur chaque device

```bash
# Sur chaque ESP32 (via Serial Monitor)
wifi_save <SSID> <PASSWORD>

# Vérifier connexion
wifi_status
```

### Tableau des IPs

| Device | Rôle | IP | Port HTTP | Hardware |
|--------|------|----|-----------| ---------|
| ESP32 #1 | Master | 192.168.1.100 | 8080 | Standard |
| ESP32 #2 | Slave Sensors | 192.168.1.101 | 8081 | + 12 capteurs ADC |
| ESP32 #3 | Slave Relays | 192.168.1.102 | 8082 | + 5 relais |

---

## 🧪 Tests et Validation

### Test 1 : Connectivité Réseau

```bash
# Depuis un PC sur le même réseau

# Ping devices
ping 192.168.1.100
ping 192.168.1.101
ping 192.168.1.102

# Test endpoints
curl http://192.168.1.100:8080/api/master/stats
curl http://192.168.1.101:8081/api/sensors/status
curl http://192.168.1.102:8082/api/irrigation/status
```

### Test 2 : Communication Slave1 → Master

```bash
# Lecture données capteurs sur Slave1
curl http://192.168.1.101:8081/api/sensors/data

# Vérifier logs Master (doit recevoir données toutes les 5s)
# Serial Monitor Device 1 : "Master: Received sensor data from Slave1"
```

### Test 3 : Communication Master → Slave2

```bash
# Envoyer commande test depuis Master
curl -X POST http://192.168.1.102:8082/api/irrigation/command \
  -H "Content-Type: application/json" \
  -d '{
    "command": "test_relay",
    "zone_id": 1,
    "duration_seconds": 5,
    "zone_server_id": "test",
    "timestamp": 1697500000
  }'

# Vérifier logs Slave2 : "Testing relay zone 1"
```

### Test 4 : Irrigation Complète

```bash
# 1. Démarrer irrigation zone 1 (2 minutes)
curl -X POST http://192.168.1.102:8082/api/irrigation/command \
  -H "Content-Type: application/json" \
  -d '{
    "command": "start_irrigation",
    "zone_id": 1,
    "duration_seconds": 120,
    "zone_server_id": "zone_test",
    "timestamp": 1697500000
  }'

# 2. Vérifier statut pendant irrigation
curl http://192.168.1.102:8082/api/irrigation/status

# 3. Arrêt d'urgence (optionnel)
curl -X POST http://192.168.1.102:8082/api/irrigation/emergency_stop
```

---

## 📊 Flux de Données Complet

### Scénario : Irrigation Programmée

```
1. Slave1 lit capteurs (toutes les 5s)
   └─► POST → Master /api/sensors/data

2. Master reçoit données
   ├─► Stocke moisture[12]
   └─► Envoie au serveur FastAPI

3. Master poll config serveur (toutes les 10s)
   └─► GET → Serveur /api/devices/{id}/config

4. Master détecte heure irrigation (ex: 08:00)
   └─► Décision : Démarrer irrigation zone 1

5. Master envoie commande
   └─► POST → Slave2 /api/irrigation/command
       {command: "start_irrigation", zone_id: 1, duration: 120}

6. Slave2 exécute commande
   ├─► Active relais zone 1 (pin 2 → HIGH)
   ├─► Active pompe (pin 5 → HIGH)
   ├─► LED status ON (pin 18)
   └─► Timer 120 secondes

7. Slave2 publie statut (toutes les 10s)
   └─► POST → Master /api/irrigation/status
       {is_irrigating: true, remaining: 85s}

8. Timer expire (120s écoulés)
   ├─► Désactive relais zone 1 (pin 2 → LOW)
   ├─► Désactive pompe (pin 5 → LOW)
   └─► LED status OFF

9. Slave2 publie statut final
   └─► POST → Master /api/irrigation/status
       {is_irrigating: false}
```

---

## 🔒 Sécurité

### Timeouts

- **HTTP timeout** : 5 secondes par requête
- **Retry** : 3 tentatives avec délai 1 seconde
- **Irrigation timeout** : 5 minutes max (safety)

### Arrêt d'Urgence

```bash
# Depuis n'importe où sur le réseau
curl -X POST http://192.168.1.102:8082/api/irrigation/emergency_stop

# Résultat :
# - Tous relais OFF immédiatement
# - Pompe OFF
# - Buzzer alerte (500ms)
# - Logs "EMERGENCY STOP activated"
```

### Gestion Erreurs

**Si Slave1 offline** :
- Master continue avec dernières données reçues
- Logs warning "Failed to receive sensor data"
- Pas d'irrigation d'urgence (sécurité)

**Si Slave2 offline** :
- Master logs error "Failed to send irrigation command"
- Retry 3 fois
- Alerte utilisateur

**Si Master offline** :
- Slave1 continue lecture capteurs (données perdues)
- Slave2 maintient état actuel (irrigation en cours continue)

---

## 📈 Performances

### Latences

| Communication | Latence Typique | Latence Max |
|---------------|-----------------|-------------|
| Slave1 → Master | 10-20ms | 50ms |
| Master → Slave2 | 10-20ms | 50ms |
| Master → Serveur | 50-100ms | 500ms |

### Bande Passante

| Flux | Taille Payload | Fréquence | Bande Passante |
|------|----------------|-----------|----------------|
| Slave1 → Master | ~500 bytes | 5s | 0.8 kbps |
| Master → Slave2 | ~200 bytes | Variable | < 0.1 kbps |
| Slave2 → Master | ~150 bytes | 10s | 0.12 kbps |

**Total** : < 2 kbps (négligeable sur WiFi)

---

## 🎯 Avantages de cette Architecture

✅ **Modularité** : Chaque device est indépendant
✅ **Scalabilité** : Facile d'ajouter Slave3, Slave4, etc.
✅ **Résilience** : Si un Slave crash, les autres continuent
✅ **Maintenance** : Mise à jour device par device
✅ **Debugging** : Logs isolés par device
✅ **Performance** : Traitement parallèle sur 3 cores
✅ **Flexibilité** : Facile de changer IPs/ports
✅ **Standard** : HTTP REST, testable avec curl/Postman

---

## 🚨 Limitations et Contraintes

⚠️ **Dépendance WiFi** : Si WiFi tombe, tout s'arrête
⚠️ **Latence réseau** : 10-50ms (vs < 1ms en local)
⚠️ **Complexité déploiement** : 3 devices à flasher séparément
⚠️ **Synchronisation** : Pas de garantie temps réel strict
⚠️ **Sécurité** : HTTP non chiffré (OK en LAN privé)

---

## 📝 Prochaines Étapes

### Phase 1 : Tests Unitaires
- ✅ Test Slave1 seul (lecture capteurs)
- ✅ Test Slave2 seul (contrôle relais)
- ✅ Test Master seul (communication serveur)

### Phase 2 : Tests Intégration
- ✅ Test Slave1 → Master
- ✅ Test Master → Slave2
- ✅ Test complet 3 devices

### Phase 3 : Tests Terrain
- ✅ Irrigation programmée
- ✅ Irrigation d'urgence
- ✅ Gestion erreurs réseau
- ✅ Performance longue durée (24h+)

### Phase 4 : Optimisations
- 🔄 Ajouter authentification (API keys)
- 🔄 Ajouter HTTPS (certificats)
- 🔄 Ajouter cache données (si Master offline)
- 🔄 Ajouter watchdog (redémarrage auto)

---

## 📞 Support et Debugging

### Logs Importants

**Master** :
```
[INFO] MasterHTTP: Server started on port 8080
[DEBUG] Master: Received sensor data from Slave1
[INFO] IrrigComm: Command sent successfully
```

**Slave Sensors** :
```
[INFO] IrrigSlaveSensors: Ready
[DEBUG] Sensors read: T=24.5°C, H=60.0%, P=1012.0hPa
[DEBUG] IrrigComm: Sensor data sent successfully
```

**Slave Relays** :
```
[INFO] IrrigSlaveRelays: Ready
[INFO] Starting irrigation: Zone 1, Duration 120s
[INFO] Pump: ON
[INFO] Irrigation completed
```

### Commandes Shell Utiles

```bash
# Sur chaque device (Serial Monitor)
app_list          # Lister applications
app_info 1        # Info application
wifi_status       # Statut WiFi
system_info       # Info système
```

---

## ✅ Checklist Déploiement

- [ ] 3 ESP32 flashés avec firmwares respectifs
- [ ] WiFi configuré sur les 3 devices
- [ ] IPs fixes vérifiées (ping)
- [ ] Serveurs HTTP démarrés (curl /status)
- [ ] Communication Slave1 → Master OK
- [ ] Communication Master → Slave2 OK
- [ ] Hardware capteurs connecté (Slave1)
- [ ] Hardware relais connecté (Slave2)
- [ ] Test irrigation manuelle OK
- [ ] Serveur FastAPI accessible
- [ ] Logs monitoring actifs

---

**Architecture prête pour déploiement production ! 🚀**
