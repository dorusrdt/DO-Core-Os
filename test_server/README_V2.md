# 🌱 Serveur de Test FastAPI - IrrigAppMaster v2.0

## 📋 Vue d'ensemble

Serveur FastAPI mis à jour pour simuler le serveur externe de production `192.168.1.72:8000`.

**Nouvelle version 2.0 inclut:**
- ✅ Tous les endpoints ESP32 (registration, config, sensor data)
- ✅ **NEW:** Endpoints de gestion Web complets (zones, devices)
- ✅ **NEW:** Gestion complète des zones (CRUD)
- ✅ **NEW:** Support du CORS pour l'interface web
- ✅ Historique des données capteurs
- ✅ Commandes en attente pour les devices

## 🚀 Démarrage Rapide

### 1. Installation des dépendances

```bash
cd /home/dorus/Documents/GitHub/DO-Core-Os/test_server
pip install fastapi uvicorn pydantic python-multipart
```

### 2. Lancer le serveur

```bash
python irrigation_server.py
```

Le serveur démarre sur `http://localhost:8000`

### 3. Accéder à la documentation

- **Swagger UI:** http://localhost:8000/docs
- **ReDoc:** http://localhost:8000/redoc

## 📡 Endpoints API

### Device Registration (ESP32)

#### Register Device
```bash
POST /api/devices/register
Content-Type: application/json

{
    "type": "register",
    "deviceId": "ESP32_IRRIGATION_11100454456464674",
    "capacity": {"zones": 4, "sensors": 12},
    "timestamp": "2025-11-25T12:00:00",
    "latitude": 35.6695,
    "longitude": -5.7857
}
```

#### Get Device Configuration
```bash
GET /api/devices/{device_id}/config
X-Last-Config-Hash: <optional_hash>
```

Retourne 304 Not Modified si la config n'a pas changé (optimisation bande passante).

#### Send Sensor Data
```bash
POST /api/devices/sensor-data
Content-Type: application/json

{
    "type": "data",
    "deviceId": "...",
    "timestamp": "...",
    "globalData": {
        "temperature": 24.5,
        "humidity": 60.0,
        "pressure": 1012.0,
        "batteryLevel": 85.0,
        "signalStrength": -45
    },
    "zonesData": [...]
}
```

### Zone Management

#### Add/Update Zone
```bash
POST /api/devices/{device_id}/zones
Content-Type: application/json

{
    "zoneId": "zone_001",
    "waterPerDay": 2000,
    "irrigationTime": "08:00",
    "humidityThreshold": 25,
    "sensors": [
        {"sensorId": "s_01"},
        {"sensorId": "s_02"}
    ]
}
```

#### List Zones
```bash
GET /api/devices/{device_id}/zones
```

#### Get Specific Zone
```bash
GET /api/devices/{device_id}/zones/{zone_id}
```

#### Delete Zone
```bash
DELETE /api/devices/{device_id}/zones/{zone_id}
```

### Device Management

#### List All Devices
```bash
GET /api/devices
```

Response:
```json
{
    "devices": [...],
    "count": 1
}
```

#### Get Device Info
```bash
GET /api/devices/{device_id}
```

#### Get Device Status
```bash
GET /api/devices/{device_id}/status
```

#### Unregister Device
```bash
DELETE /api/devices/{device_id}
```

### Sensor Data Endpoints

#### Get Sensor Data Log
```bash
GET /api/sensor-data?device_id=...&limit=100
```

#### Get Sensor History
```bash
GET /api/sensor-data/history?limit=10
```

#### Get Latest Sensor Data
```bash
GET /api/sensor-data/latest/{device_id}
```

### Health & Info

#### Health Check
```bash
GET /health
GET /api/health
GET /
```

## 🧪 Tests

### Script de Test Automatisé

```bash
chmod +x test_server.sh
./test_server.sh
```

Teste tous les endpoints avec des résultats colorisés.

### Créer des Zones de Test

```bash
chmod +x create_test_zones.sh
./create_test_zones.sh
```

Crée 4 zones de test :
- Zone Potager Nord (Tomates)
- Zone Jardin Sud (Laitue)
- Zone Serre (Carottes)
- Zone Verger (Arbres Fruitiers)

## 📊 Structure des Données

### DeviceCapacity
```json
{
    "zones": 4,
    "sensors": 12
}
```

### ZoneConfig
```json
{
    "zoneId": "zone_001",
    "waterPerDay": 2000,
    "irrigationTime": "08:00",
    "irrigationTimes": ["08:00", "18:00"],
    "humidityThreshold": 25,
    "sensors": [
        {"sensorId": "s_01"},
        {"sensorId": "s_02"}
    ]
}
```

### SensorData
```json
{
    "sensorId": "s_01",
    "value": 45.5
}
```

### GlobalData
```json
{
    "temperature": 24.5,
    "humidity": 60.0,
    "pressure": 1012.0,
    "batteryLevel": 85.0,
    "signalStrength": -45
}
```

## 🔄 Workflow Typique

1. **Enregistrement Device**
   ```bash
   curl -X POST http://localhost:8000/api/devices/register \
     -H "Content-Type: application/json" \
     -d '{"type":"register","deviceId":"...","capacity":{...},...}'
   ```

2. **Ajouter Zones**
   ```bash
   ./create_test_zones.sh
   ```

3. **ESP32 Récupère Config**
   ```bash
   curl http://localhost:8000/api/devices/ESP32_ID/config
   ```

4. **ESP32 Envoie Données Capteurs**
   ```bash
   curl -X POST http://localhost:8000/api/devices/sensor-data \
     -H "Content-Type: application/json" \
     -d '{...}'
   ```

5. **Vérifier les Données**
   ```bash
   curl http://localhost:8000/api/sensor-data/latest/ESP32_ID
   ```

## 📈 Améliorations par rapport à v1.0

| Feature | v1.0 | v2.0 |
|---------|------|------|
| Device Registration | ✅ | ✅ |
| Config Management | ✅ | ✅ |
| Sensor Data Reception | ✅ | ✅ |
| Zone Management | ❌ | ✅ **NEW** |
| Web Interface Endpoints | ❌ | ✅ **NEW** |
| CORS Support | ❌ | ✅ **NEW** |
| Full CRUD Zones | ❌ | ✅ **NEW** |
| Sensor Data History | ✅ | ✅ Enhanced |
| Pending Commands | ❌ | ✅ **NEW** |

## 🔧 Configuration

### Port (défaut: 8000)
Modifier dans `irrigation_server.py`:
```python
uvicorn.run(
    "irrigation_server:app",
    host="0.0.0.0",
    port=8000,  # Changer ici
    ...
)
```

### Reload Mode
Désactiver le reload pour la production:
```python
uvicorn.run(
    ...
    reload=False,  # Changer en False
    ...
)
```

### Log Level
Options: `critical`, `error`, `warning`, `info`, `debug`
```python
uvicorn.run(
    ...
    log_level="info",  # Changer ici
    ...
)
```

## 🐛 Dépannage

### Le serveur ne démarre pas
```bash
# Vérifier les dépendances
pip list | grep -E "fastapi|uvicorn|pydantic"

# Réinstaller si nécessaire
pip install --upgrade fastapi uvicorn pydantic
```

### Port 8000 déjà utilisé
```bash
# Trouver et arrêter le processus
lsof -i :8000
kill -9 <PID>

# Ou changer le port dans le script
```

### Device not found error
- Enregistrer d'abord le device avec POST `/api/devices/register`
- Vérifier le `deviceId` dans les requests

## 📚 References

- [FastAPI Documentation](https://fastapi.tiangolo.com/)
- [Pydantic Models](https://docs.pydantic.dev/)
- [Uvicorn Server](https://www.uvicorn.org/)

## 📝 Changements v1.0 → v2.0

### Nouvelles Routes
- `POST /api/devices/{id}/zones` - Ajouter zone
- `GET /api/devices/{id}/zones` - Lister zones
- `DELETE /api/devices/{id}/zones/{zid}` - Supprimer zone
- `GET /api/devices/{id}` - Info device
- `GET /api/sensor-data/history` - Historique
- `GET /api/sensor-data/latest/{id}` - Dernières données

### Amélioration Models
- Ajout `DeviceCapacity` model pour meilleure validation
- Ajout `SensorDataRequest` pour validation complète
- Ajout `ZoneConfig` pour gestion zones

### Stockage Amélioré
- `device_zones` pour gestion directe zones
- `pending_commands` pour commandes en attente
- Structure optimisée pour scalabilité

## 🎯 Tests Recommandés

1. **Test de Base**
   ```bash
   ./test_server.sh
   ```

2. **Test Manuel des Zones**
   ```bash
   ./create_test_zones.sh
   ```

3. **Test de Load (stress test)**
   ```bash
   ab -n 1000 -c 10 http://localhost:8000/api/devices
   ```

4. **Test avec Device Réel**
   Connecter un vrai ESP32 et vérifier les données

## 📞 Support

Pour les problèmes, consulter :
- Log du serveur (terminal de démarrage)
- Documentation API: http://localhost:8000/docs
- Les fichiers de configuration dans le dossier

---

**Version:** 2.0.0
**Date:** Novembre 2025
**Status:** ✅ Production Ready
