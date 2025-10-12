# 🧪 Serveur de Test FastAPI - IrrigAppMaster

Serveur de test pour valider la logique métier de l'application IrrigAppMaster.

## 🎯 Fonctionnalités

### ✅ Endpoints ESP32 (Code Référence)
- `POST /api/devices/register` - Enregistrement device
- `POST /api/devices/sensor-data` - Réception données capteurs
- `GET /api/devices/{device_id}/config` - Configuration zones + commandes

### ✅ Endpoints Gestion (Interface Web)
- `GET /api/devices` - Liste des devices
- `GET /api/devices/{device_id}` - Info device
- `POST /api/devices/{device_id}/zones` - Ajouter zone
- `DELETE /api/devices/{device_id}/zones/{zone_id}` - Supprimer zone
- `GET /api/sensor-data/history` - Historique données
- `GET /api/sensor-data/latest/{device_id}` - Dernières données

### ✅ Fonctionnalités
- ✅ Format JSON compatible code référence
- ✅ Gestion dynamique zones/capteurs
- ✅ Commandes delete_zone
- ✅ Historique données capteurs
- ✅ Logs détaillés console
- ✅ Documentation Swagger automatique

---

## 🚀 Installation

### 1. Setup Environnement
```bash
cd test_server
chmod +x setup.sh start.sh
./setup.sh
```

Cela va :
- Créer un environnement virtuel Python
- Installer FastAPI, Uvicorn, Pydantic
- Configurer le serveur

### 2. Démarrer le Serveur
```bash
./start.sh
```

Ou manuellement :
```bash
source venv/bin/activate
python server.py
```

Le serveur démarre sur : **http://192.168.1.3:3000**

---

## 📚 Documentation

### Swagger UI (Interface Interactive)
```
http://192.168.1.3:3000/docs
```

### ReDoc (Documentation)
```
http://192.168.1.3:3000/redoc
```

### Health Check
```bash
curl http://192.168.1.3:3000/api/health
```

---

## 🧪 Tests avec ESP32

### 1. Configurer l'ESP32

Dans `main.cpp` :
```cpp
strcpy(irrig_config.server_url, "http://192.168.1.3:3000");
strcpy(irrig_config.device_id, "ESP32_IRRIGATION_11100454456464674");
```

### 2. Compiler et Upload
```bash
pio run --target upload
pio device monitor
```

### 3. Démarrer l'Application
Dans le shell DO-Core :
```
app_start 1
```

### 4. Observer les Logs Serveur

Le serveur affiche en temps réel :
```
📝 REGISTER DEVICE
   Device ID: ESP32_IRRIGATION_11100454456464674
   ✅ Device registered successfully

📊 SENSOR DATA RECEIVED
   Device ID: ESP32_IRRIGATION_11100454456464674
   Global Data:
      Temperature: 24.5°C
      Humidity: 60.0%
   Zones Data: 0 zones
   ✅ Data stored successfully

⚙️  CONFIG REQUEST
   Device ID: ESP32_IRRIGATION_11100454456464674
   Zones configured: 0
   ✅ Config sent successfully
```

---

## 🎮 Utilisation API

### 1. Créer une Zone de Test

**Via Swagger UI** : http://192.168.1.3:3000/docs

Ou **via curl** :
```bash
curl -X POST "http://192.168.1.3:3000/api/devices/ESP32_IRRIGATION_11100454456464674/zones" \
  -H "Content-Type: application/json" \
  -d '{
    "zoneId": "zone_potager_nord",
    "waterPerDay": 2000,
    "irrigationTime": "08:00",
    "humidityThreshold": 25,
    "sensors": [
      {"sensorId": "s_01"},
      {"sensorId": "s_02"},
      {"sensorId": "s_03"}
    ]
  }'
```

### 2. Créer Zone de Test Rapide

```bash
curl -X POST "http://192.168.1.3:3000/api/test/create-zone"
```

### 3. Lister les Devices

```bash
curl http://192.168.1.3:3000/api/devices
```

### 4. Voir Info Device

```bash
curl http://192.168.1.3:3000/api/devices/ESP32_IRRIGATION_11100454456464674
```

### 5. Supprimer une Zone

```bash
curl -X DELETE "http://192.168.1.3:3000/api/devices/ESP32_IRRIGATION_11100454456464674/zones/zone_potager_nord"
```

L'ESP32 recevra la commande `delete_zone` au prochain poll (10s).

### 6. Voir Historique Données

```bash
curl "http://192.168.1.3:3000/api/sensor-data/history?limit=5"
```

### 7. Dernières Données Device

```bash
curl http://192.168.1.3:3000/api/sensor-data/latest/ESP32_IRRIGATION_11100454456464674
```

---

## 📊 Scénarios de Test

### Scénario 1 : Enregistrement + Configuration

1. **Démarrer serveur** : `./start.sh`
2. **Démarrer ESP32** : `app_start 1`
3. **Vérifier enregistrement** : Logs serveur affichent "REGISTER DEVICE"
4. **Créer zone** : Via Swagger ou curl
5. **Attendre 10s** : ESP32 poll config
6. **Vérifier logs ESP32** : Zone reçue et configurée

### Scénario 2 : Envoi Données Capteurs

1. **Attendre 15s** après démarrage ESP32
2. **Observer logs serveur** : "SENSOR DATA RECEIVED"
3. **Vérifier données** :
   - globalData (température, humidité, pression)
   - zonesData avec sensorId et valeurs
4. **Consulter historique** : `/api/sensor-data/history`

### Scénario 3 : Suppression Zone

1. **Créer zone** : Via API
2. **Attendre poll ESP32** : Zone configurée
3. **Supprimer zone** : `DELETE /zones/{zone_id}`
4. **Attendre 10s** : ESP32 poll config
5. **Vérifier logs ESP32** : "Processing zone deletion"
6. **Vérifier capteurs libérés** : Logs "Freed sensor"

### Scénario 4 : Multiples Zones

1. **Créer 3 zones** avec capteurs différents
2. **Attendre poll** : ESP32 reçoit config
3. **Vérifier mapping** : Capteurs assignés séquentiellement
4. **Observer données** : 3 zones dans zonesData
5. **Supprimer zone 2** : Capteurs libérés
6. **Vérifier réassignation** : Zones 1 et 3 préservées

---

## 🔍 Logs Détaillés

Le serveur affiche des logs détaillés pour chaque requête :

### Enregistrement
```
📝 REGISTER DEVICE
   Device ID: ESP32_IRRIGATION_11100454456464674
   Type: register
   Capacity: 4 zones, 12 sensors
   Timestamp: 2024-01-15T10:30:00.000Z
   Signature: dummy_signature_123456
   ✅ Device registered successfully
```

### Données Capteurs
```
📊 SENSOR DATA RECEIVED
   Device ID: ESP32_IRRIGATION_11100454456464674
   Type: data
   Timestamp: 2024-01-15T10:30:15.000Z
   Global Data:
      Temperature: 24.5°C
      Humidity: 60.0%
      Pressure: 1012.0 hPa
      Battery: 85.0%
      Signal: -45 dBm
   Zones Data: 2 zones
      Zone zone_potager_nord: 3 sensors
         s_01: 35.2%
         s_02: 36.8%
         s_03: 37.1%
      Zone zone_jardin_sud: 3 sensors
         s_04: 55.3%
         s_05: 56.1%
         s_06: 54.8%
   ✅ Data stored successfully
```

### Configuration
```
⚙️  CONFIG REQUEST
   Device ID: ESP32_IRRIGATION_11100454456464674
   Signature: dummy_signature_123456
   Zones configured: 2
      zone_potager_nord: 2000ml/day, 08:00, 25%
      zone_jardin_sud: 3000ml/day, 18:00, 30%
   Pending commands: 0
   ✅ Config sent successfully
```

---

## 🛠️ Développement

### Structure
```
test_server/
├── server.py           # Serveur FastAPI
├── requirements.txt    # Dépendances Python
├── setup.sh           # Script installation
├── start.sh           # Script démarrage
├── README.md          # Ce fichier
└── venv/              # Environnement virtuel (créé par setup)
```

### Ajouter des Endpoints

Éditer `server.py` et ajouter :
```python
@app.get("/api/mon-endpoint")
async def mon_endpoint():
    return {"status": "ok"}
```

Le serveur redémarre automatiquement (hot reload).

### Modifier l'Adresse

Dans `server.py`, ligne finale :
```python
uvicorn.run(
    app,
    host="192.168.1.3",  # Changer ici
    port=3000,           # Ou changer le port
    log_level="info"
)
```

---

## 🐛 Dépannage

### Port déjà utilisé
```bash
# Trouver le processus
lsof -i :3000

# Tuer le processus
kill -9 <PID>
```

### Environnement virtuel non activé
```bash
source venv/bin/activate
```

### Dépendances manquantes
```bash
pip install -r requirements.txt
```

### ESP32 ne se connecte pas
1. Vérifier IP serveur : `192.168.1.3`
2. Vérifier port : `3000`
3. Vérifier firewall
4. Ping serveur depuis ESP32 : `ping 192.168.1.3`

---

## 📝 Notes

- Le serveur stocke les données **en mémoire** (perdu au redémarrage)
- Pour production, utiliser une vraie base de données (PostgreSQL, MongoDB)
- L'authentification HMAC est simplifiée (accepte toutes les signatures)
- Les logs sont affichés en console (pas de fichier log)

---

## 🚀 Prochaines Étapes

1. ✅ Tester enregistrement device
2. ✅ Tester envoi données capteurs
3. ✅ Tester configuration zones
4. ✅ Tester suppression zones
5. ✅ Tester irrigation programmée
6. ✅ Tester irrigation d'urgence

---

**Serveur prêt pour tests !** 🎉
