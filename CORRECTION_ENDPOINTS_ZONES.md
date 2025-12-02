# Correction: Endpoints Manquants pour la Création de Zones

## 🔴 Problème Identifié

Le script `create_test_zones.sh` essaie de créer des zones via:

```bash
curl -X POST "$SERVER/api/devices/$DEVICE_ID/zones" \
  -H "Content-Type: application/json" \
  -d '{ ... }'
```

Mais le serveur `irrigation_server.py` n'avait **pas ces endpoints**!

---

## ❌ Endpoints Manquants Identifiés

### 1. **POST `/api/devices/{device_id}/zones`**
- ❌ N'existait pas
- 🎯 Créer ou ajouter une zone

### 2. **GET `/api/devices/{device_id}/zones`**
- ❌ N'existait pas
- 🎯 Lister toutes les zones d'un device

---

## ✅ Solution Implémentée

### Ajouté deux nouveaux endpoints à `irrigation_server.py`:

#### 1. **POST `/api/devices/{device_id}/zones`** (Création)

```python
@app.post("/api/devices/{device_id}/zones")
async def create_zone(device_id: str, zone_data: Dict[str, Any]):
    """
    Create or add a zone to the device configuration
    Used by create_test_zones.sh
    """
    # Vérifier que le device est enregistré
    if device_id not in registered_devices:
        raise HTTPException(status_code=404, detail="Device not registered")

    # Initialiser la config si nécessaire
    if device_id not in device_configs:
        device_configs[device_id] = {"type": "config", "zones": []}

    config = device_configs[device_id]
    if "zones" not in config:
        config["zones"] = []

    # Vérifier si la zone existe déjà
    existing_zone = next((z for z in config["zones"]
                         if z.get("zoneId") == zone_data.get("zoneId")), None)

    if existing_zone:
        # Mettre à jour si elle existe
        existing_zone.update(zone_data)
        action = "updated"
    else:
        # Ajouter si elle n'existe pas
        config["zones"].append(zone_data)
        action = "created"

    # Mettre à jour le hash
    config_hashes[device_id] = generate_config_hash(config)

    return {
        "status": action,
        "deviceId": device_id,
        "zoneId": zone_data.get("zoneId"),
        "totalZones": len(config["zones"])
    }
```

#### 2. **GET `/api/devices/{device_id}/zones`** (Listing)

```python
@app.get("/api/devices/{device_id}/zones")
async def get_zones(device_id: str):
    """
    Get all zones for a device
    Used by create_test_zones.sh for verification
    """
    if device_id not in registered_devices:
        raise HTTPException(status_code=404, detail="Device not registered")

    config = device_configs.get(device_id, {})
    zones = config.get("zones", [])

    return {
        "deviceId": device_id,
        "zones": zones,
        "totalZones": len(zones)
    }
```

---

## 🔄 Flux de Création de Zone

### Avant (Échoue)
```
create_test_zones.sh
  ↓
POST /api/devices/{id}/zones
  ↓
❌ 404 Not Found (endpoint n'existe pas)
```

### Après (Fonctionne)
```
create_test_zones.sh
  ↓
POST /api/devices/{id}/zones
  ↓
✅ 200 OK - Zone créée
  ↓
GET /api/devices/{id}/config
  ↓
✅ Retourne zones[] rempli
```

---

## 📋 Endpoints Disponibles Mis à Jour

| Méthode | Endpoint | Statut |
|---------|----------|--------|
| POST | `/api/devices/register` | ✅ |
| GET | `/api/devices/{id}/config` | ✅ |
| PUT | `/api/devices/{id}/config` | ✅ |
| **POST** | **`/api/devices/{id}/zones`** | **✅ NEW** |
| **GET** | **`/api/devices/{id}/zones`** | **✅ NEW** |
| DELETE | `/api/devices/{id}/zones/{zid}` | ✅ |
| POST | `/api/devices/sensor-data` | ✅ |
| GET | `/api/devices` | ✅ |
| GET | `/api/devices/{id}/status` | ✅ |
| GET | `/health` | ✅ |

---

## 🚀 Utilisation

### Créer des zones
```bash
./create_test_zones.sh 4-zones
```

### Vérifier les zones
```bash
curl http://localhost:3000/api/devices/ESP32_IRRIGATION_11100454456464674/zones | jq
```

### Résultat
```json
{
  "deviceId": "ESP32_IRRIGATION_11100454456464674",
  "zones": [
    {
      "zoneId": "zone_potager_nord",
      "physicalZoneNumber": 1,
      ...
    }
  ],
  "totalZones": 1
}
```

---

## ✅ Vérification

Après redémarrage du serveur:

```bash
# Démarrer le serveur
python3 irrigation_server.py

# Dans un autre terminal
./create_test_zones.sh 4-zones

# Vérifier
curl http://localhost:3000/api/devices/ESP32_IRRIGATION_11100454456464674/config | jq '.zones | length'
# Résultat: 4 ✅
```

