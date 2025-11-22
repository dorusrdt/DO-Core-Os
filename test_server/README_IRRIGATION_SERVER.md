# 🌊 Irrigation Server - Guide d'utilisation

Serveur de test pour simuler le serveur externe (192.168.1.72:8000) et tester le système d'irrigation ESP32.

## 📋 Prérequis

```bash
cd test_server
source venv/bin/activate  # ou: . venv/bin/activate
pip install -r requirements.txt
```

## 🚀 Démarrer le serveur

```bash
# Option 1: Directement
python irrigation_server.py

# Option 2: Avec uvicorn
uvicorn irrigation_server:app --host 0.0.0.0 --port 8000 --reload
```

Le serveur démarre sur `http://0.0.0.0:8000`

## 📡 Endpoints disponibles

### Device Management
- `POST /api/devices/register` - Enregistrer un device ESP32
- `GET /api/devices/{device_id}/config` - Récupérer la configuration
- `PUT /api/devices/{device_id}/config` - Mettre à jour la configuration
- `DELETE /api/devices/{device_id}/zones/{zone_id}` - Supprimer une zone
- `GET /api/devices` - Lister tous les devices
- `GET /api/devices/{device_id}/status` - Statut d'un device

### Sensor Data
- `POST /api/devices/sensor-data` - Recevoir les données de capteurs

### Utilities
- `GET /health` - Health check
- `GET /api/sensor-data` - Log des données de capteurs

## 📤 Envoyer des configurations

Le script `send_zone_config.sh` permet d'envoyer différentes configurations de test :

### Configurations disponibles

```bash
# Configuration simple - 1 zone
./send_zone_config.sh 1-zone

# Configuration - 2 zones
./send_zone_config.sh 2-zones

# Configuration complète - 4 zones (tous les capteurs)
./send_zone_config.sh 4-zones

# Configuration avec seuils bas (déclenche irrigation d'urgence)
./send_zone_config.sh emergency

# Configuration avec plusieurs capteurs par zone
./send_zone_config.sh multiple-sensors

# Mettre à jour une zone existante
./send_zone_config.sh update

# Configuration vide (supprimer toutes les zones)
./send_zone_config.sh empty
```

### Gestion des zones

```bash
# Supprimer une zone
./send_zone_config.sh delete zone_001

# Voir le statut du device
./send_zone_config.sh status
```

**Note:** Le script utilise `curl` et optionnellement `jq` pour un meilleur affichage JSON. Si `jq` n'est pas installé, le script fonctionne quand même.

## 🧪 Scénarios de test

### Test 1: Configuration initiale
```bash
# 1. Démarrer le serveur
./start_irrigation_server.sh

# 2. Dans un autre terminal, envoyer la config
./send_zone_config.sh 4-zones

# 3. L'ESP32 master va poller la config automatiquement
```

### Test 2: Mise à jour de configuration
```bash
# 1. Envoyer config initiale
./send_zone_config.sh 2-zones

# 2. Attendre que l'ESP32 récupère la config

# 3. Mettre à jour
./send_zone_config.sh update

# L'ESP32 détectera le changement via le hash
```

### Test 3: Suppression de zone
```bash
# 1. Configurer 4 zones
./send_zone_config.sh 4-zones

# 2. Supprimer une zone
./send_zone_config.sh delete zone_002

# L'ESP32 recevra la commande delete_zone
```

### Test 4: Irrigation d'urgence
```bash
# 1. Configurer avec seuils bas
./send_zone_config.sh emergency

# 2. L'ESP32 va déclencher l'irrigation d'urgence si l'humidité < seuil
```

## 📊 Vérifier les données

### Via l'API
```bash
# Voir tous les devices
curl http://192.168.1.72:8000/api/devices

# Voir le statut d'un device
curl http://192.168.1.72:8000/api/devices/ESP32_IRRIGATION_11100454456464674/status

# Voir les données de capteurs
curl http://192.168.1.72:8000/api/sensor-data?device_id=ESP32_IRRIGATION_11100454456464674
```

### Via l'interface web
Ouvrir dans le navigateur:
- Documentation API: http://192.168.1.72:8000/docs
- Interface interactive: http://192.168.1.72:8000/redoc

## 🔍 Structure d'une configuration

```json
{
  "type": "config",
  "zones": [
    {
      "zoneId": "zone_001",
      "physicalZoneNumber": 1,
      "waterPerDay": 5000,
      "irrigationTime": "08:00",
      "humidityThreshold": 40,
      "sensors": [
        {"sensorId": "s01"},
        {"sensorId": "s02"},
        {"sensorId": "s03"}
      ]
    }
  ]
}
```

### Paramètres
- `zoneId`: Identifiant unique de la zone
- `physicalZoneNumber`: Numéro physique du relais (1-4)
- `waterPerDay`: Quantité d'eau par jour en ml
- `irrigationTime`: Heure d'irrigation (format HH:MM)
- `humidityThreshold`: Seuil d'humidité en % (déclenche irrigation d'urgence si <)
- `sensors`: Liste des capteurs assignés (IDs: s01-s12)

## 🐛 Debug

### Vérifier que le serveur fonctionne
```bash
curl http://192.168.1.72:8000/health
```

### Voir les logs du serveur
Le serveur affiche dans la console:
- Enregistrements de devices
- Requêtes de configuration
- Données de capteurs reçues

### Problèmes courants

1. **Le serveur ne démarre pas**
   - Vérifier que le port 8000 n'est pas utilisé: `lsof -i :8000`
   - Vérifier que le venv est activé

2. **L'ESP32 ne reçoit pas la config**
   - Vérifier que le serveur est accessible: `curl http://192.168.1.72:8000/health`
   - Vérifier que le device est enregistré: `curl http://192.168.1.72:8000/api/devices`

3. **La config ne change pas**
   - Le serveur utilise un hash pour détecter les changements
   - Vérifier le hash dans les logs du serveur
   - L'ESP32 envoie `X-Last-Config-Hash` dans les requêtes

## 📝 Notes

- Le serveur stocke les données en mémoire (perdu au redémarrage)
- Les configurations sont stockées par device ID
- Le hash de configuration permet d'éviter les rechargements inutiles
- Le serveur supporte les requêtes conditionnelles (304 Not Modified)

