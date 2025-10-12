# ⚡ Quick Start - Serveur de Test

## 🚀 Démarrage Rapide (3 étapes)

### 1. Installation
```bash
cd test_server
./setup.sh
```

### 2. Démarrage Serveur
```bash
./start.sh
```

Le serveur démarre sur **http://192.168.1.3:3000**

### 3. Tester l'API
Dans un autre terminal :
```bash
cd test_server
./test_api.sh
```

---

## 📱 Tester avec ESP32

### 1. Vérifier Configuration ESP32

Dans `src/main.cpp` :
```cpp
strcpy(irrig_config.server_url, "http://192.168.1.3:3000");
strcpy(irrig_config.device_id, "ESP32_IRRIGATION_11100454456464674");
```

### 2. Compiler et Upload
```bash
cd /home/dorus/Documents/GitHub/DO-Core-Os
pio run --target upload
pio device monitor
```

### 3. Démarrer l'Application
Dans le shell DO-Core :
```
app_start 1
```

### 4. Observer les Logs

**Terminal 1 (Serveur)** :
```
📝 REGISTER DEVICE
   Device ID: ESP32_IRRIGATION_11100454456464674
   ✅ Device registered successfully

📊 SENSOR DATA RECEIVED
   Device ID: ESP32_IRRIGATION_11100454456464674
   Zones Data: 0 zones
   ✅ Data stored successfully
```

**Terminal 2 (ESP32)** :
```
IrrigAppMaster: Starting irrigation system...
WiFi connected: 192.168.X.X
Registering device with server...
Device registered successfully
Polling configuration...
Config received, parsing...
Sending sensor data: 0 zones
Sensor data sent successfully
```

---

## 🎮 Créer une Zone de Test

### Via Swagger UI (Recommandé)
1. Ouvrir http://192.168.1.3:3000/docs
2. Cliquer sur `POST /api/test/create-zone`
3. Cliquer "Try it out"
4. Cliquer "Execute"

### Via curl
```bash
curl -X POST "http://192.168.1.3:3000/api/test/create-zone"
```

### Résultat Attendu

**Serveur** :
```
➕ ADD ZONE
   Device ID: ESP32_IRRIGATION_11100454456464674
   Zone ID: zone_test_123456
   Water: 2000ml/day
   Time: 08:00
   Threshold: 25%
   Sensors: 3
   ✅ Zone added
```

**ESP32 (après 10s)** :
```
⚙️  CONFIG REQUEST
Received configuration for 1 zones
Zone 1: zone_test_123456
  Water: 2000ml/day, Time: 08:00, Threshold: 25%
  Sensors: 3
```

---

## 🗑️ Supprimer une Zone

### Via Swagger UI
1. Ouvrir http://192.168.1.3:3000/docs
2. Cliquer sur `DELETE /api/devices/{device_id}/zones/{zone_id}`
3. Entrer device_id et zone_id
4. Cliquer "Execute"

### Via curl
```bash
curl -X DELETE "http://192.168.1.3:3000/api/devices/ESP32_IRRIGATION_11100454456464674/zones/zone_test_123456"
```

### Résultat Attendu

**Serveur** :
```
🗑️  DELETE ZONE
   Device ID: ESP32_IRRIGATION_11100454456464674
   Zone ID: zone_test_123456
   ✅ Zone deleted, command queued
```

**ESP32 (après 10s)** :
```
Processing zone deletion: zone_test_123456
Found zone zone_test_123456 in slot 1
Freed sensor s_01
Freed sensor s_02
Freed sensor s_03
Zone zone_test_123456 removed from device
```

---

## 📊 Voir les Données

### Dernières Données Device
```bash
curl http://192.168.1.3:3000/api/sensor-data/latest/ESP32_IRRIGATION_11100454456464674 | jq '.'
```

### Historique (5 dernières)
```bash
curl "http://192.168.1.3:3000/api/sensor-data/history?limit=5" | jq '.'
```

### Info Device
```bash
curl http://192.168.1.3:3000/api/devices/ESP32_IRRIGATION_11100454456464674 | jq '.'
```

---

## 🔍 Vérifications

### ✅ Serveur Fonctionne
```bash
curl http://192.168.1.3:3000/api/health
```

Réponse attendue :
```json
{
  "status": "healthy",
  "timestamp": "2024-01-15T10:30:00.123456"
}
```

### ✅ ESP32 Enregistré
```bash
curl http://192.168.1.3:3000/api/devices
```

Doit afficher votre device dans la liste.

### ✅ Données Reçues
```bash
curl http://192.168.1.3:3000/api/sensor-data/history
```

Doit afficher l'historique des données capteurs.

---

## 🐛 Problèmes Courants

### Serveur ne démarre pas
```bash
# Vérifier si le port est libre
lsof -i :3000

# Si occupé, tuer le processus
kill -9 <PID>
```

### ESP32 ne se connecte pas
1. Vérifier IP : `192.168.1.3`
2. Vérifier port : `3000`
3. Ping serveur : `ping 192.168.1.3`
4. Vérifier firewall

### Pas de données reçues
1. Vérifier ESP32 démarré : `app_start 1`
2. Vérifier logs ESP32 : Erreurs HTTP ?
3. Vérifier logs serveur : Requêtes reçues ?

---

## 📚 Documentation Complète

- **README.md** - Documentation complète
- **Swagger UI** - http://192.168.1.3:3000/docs
- **ReDoc** - http://192.168.1.3:3000/redoc

---

## 🎯 Scénario Complet

### 1. Démarrer Serveur
```bash
cd test_server
./start.sh
```

### 2. Démarrer ESP32
```bash
# Terminal 2
pio run --target upload
pio device monitor
# Dans le shell: app_start 1
```

### 3. Créer Zone
```bash
# Terminal 3
curl -X POST "http://192.168.1.3:3000/api/test/create-zone"
```

### 4. Attendre 10s
ESP32 poll config et reçoit la zone

### 5. Observer Données
```bash
# Attendre 15s (envoi données)
curl http://192.168.1.3:3000/api/sensor-data/latest/ESP32_IRRIGATION_11100454456464674 | jq '.'
```

### 6. Supprimer Zone
```bash
curl -X DELETE "http://192.168.1.3:3000/api/devices/ESP32_IRRIGATION_11100454456464674/zones/zone_test_XXXXXX"
```

### 7. Attendre 10s
ESP32 reçoit commande delete_zone et libère capteurs

---

**Serveur prêt ! Bon test !** 🚀
