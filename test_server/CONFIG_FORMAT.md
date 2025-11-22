# 📋 Format de Configuration - Serveur Externe → ESP32 Master

Ce document décrit le format exact du JSON de configuration envoyé par le serveur externe au ESP32 Master.

## 🔄 Endpoint

**GET** `/api/devices/{device_id}/config`

Le serveur retourne la configuration au format JSON suivant :

## 📦 Structure JSON

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
    },
    {
      "zoneId": "zone_002",
      "physicalZoneNumber": 2,
      "waterPerDay": 3000,
      "irrigationTime": "09:00",
      "humidityThreshold": 45,
      "sensors": [
        {"sensorId": "s04"},
        {"sensorId": "s05"},
        {"sensorId": "s06"}
      ]
    }
  ]
}
```

## 📝 Description des Champs

### Niveau Racine

| Champ | Type | Requis | Description |
|-------|------|--------|-------------|
| `type` | `string` | ✅ Oui | Doit être `"config"` |
| `zones` | `array` | ✅ Oui | Tableau des zones à configurer (peut être vide `[]`) |

### Objet Zone

Chaque élément du tableau `zones` est un objet avec les champs suivants :

| Champ | Type | Requis | Description | Exemple |
|-------|------|--------|-------------|---------|
| `zoneId` | `string` | ✅ Oui | Identifiant unique de la zone | `"zone_001"` |
| `physicalZoneNumber` | `integer` | ✅ Oui | Numéro physique de la zone (1-4) | `1`, `2`, `3`, `4` |
| `waterPerDay` | `integer` | ✅ Oui | Quantité d'eau en millilitres par jour | `5000` (5L) |
| `irrigationTime` | `string` | ✅ Oui | Heure d'irrigation au format HH:MM (24h) | `"08:00"`, `"14:30"` |
| `humidityThreshold` | `integer` | ✅ Oui | Seuil d'humidité en pourcentage (0-100) | `40`, `45`, `50` |
| `sensors` | `array` | ✅ Oui | Tableau des capteurs assignés à cette zone | Voir ci-dessous |

### Objet Capteur (dans `sensors`)

Chaque élément du tableau `sensors` est un objet avec :

| Champ | Type | Requis | Description | Exemple |
|-------|------|--------|-------------|---------|
| `sensorId` | `string` | ✅ Oui | Identifiant du capteur (s01-s12) | `"s01"`, `"s12"` |

## 📊 Exemples de Configuration

### Configuration Vide (Pas de zones)

```json
{
  "type": "config",
  "zones": []
}
```

### Configuration 1 Zone

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

### Configuration 4 Zones (Complète)

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
    },
    {
      "zoneId": "zone_002",
      "physicalZoneNumber": 2,
      "waterPerDay": 3000,
      "irrigationTime": "09:00",
      "humidityThreshold": 45,
      "sensors": [
        {"sensorId": "s04"},
        {"sensorId": "s05"},
        {"sensorId": "s06"}
      ]
    },
    {
      "zoneId": "zone_003",
      "physicalZoneNumber": 3,
      "waterPerDay": 4000,
      "irrigationTime": "10:00",
      "humidityThreshold": 50,
      "sensors": [
        {"sensorId": "s07"},
        {"sensorId": "s08"},
        {"sensorId": "s09"}
      ]
    },
    {
      "zoneId": "zone_004",
      "physicalZoneNumber": 4,
      "waterPerDay": 6000,
      "irrigationTime": "11:00",
      "humidityThreshold": 35,
      "sensors": [
        {"sensorId": "s10"},
        {"sensorId": "s11"},
        {"sensorId": "s12"}
      ]
    }
  ]
}
```

## 🔍 Validation

### Contraintes

- **`physicalZoneNumber`** : Doit être entre 1 et 4 (MAX_ZONES)
- **`waterPerDay`** : Doit être un entier positif (en millilitres)
- **`irrigationTime`** : Format HH:MM (24h), ex: `"08:00"`, `"14:30"`, `"23:59"`
- **`humidityThreshold`** : Doit être entre 0 et 100 (pourcentage)
- **`sensorId`** : Format `"s01"` à `"s12"` (s01, s02, ..., s09, s10, s11, s12)
- **`sensors`** : Maximum 12 capteurs au total (MAX_SENSORS)

### Comportement du Master

1. **Zones existantes** : Si une zone avec le même `zoneId` existe déjà, elle est mise à jour (sensors réassignés)
2. **Zones nouvelles** : Si `physicalZoneNumber` n'est pas occupé, une nouvelle zone est créée
3. **Zones occupées** : Si `physicalZoneNumber` est déjà occupé par une autre zone, la configuration est ignorée
4. **Capteurs** : Les capteurs sont assignés dans l'ordre d'apparition dans le tableau `sensors`

## 🔄 Headers HTTP

Le serveur peut retourner un header optionnel :

- **`X-Config-Hash`** : Hash MD5 de la configuration (pour détecter les changements)
  - Le master peut envoyer `X-Last-Config-Hash` dans la requête
  - Si le hash n'a pas changé, le serveur retourne `304 Not Modified`

## 📡 Exemple de Requête/Réponse

### Requête

```http
GET /api/devices/ESP32_IRRIGATION_11100454456464674/config HTTP/1.1
Host: 192.168.1.72:8000
X-Last-Config-Hash: abc123def456...
```

### Réponse (200 OK)

```http
HTTP/1.1 200 OK
Content-Type: application/json
X-Config-Hash: xyz789abc123...

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

### Réponse (304 Not Modified)

```http
HTTP/1.1 304 Not Modified
X-Config-Hash: xyz789abc123...
```

## 🛠️ Utilisation avec le Script de Test

Le script `send_zone_config.py` peut envoyer différentes configurations :

```bash
# Configuration 1 zone
python3 send_zone_config.py 1

# Configuration 2 zones
python3 send_zone_config.py 2

# Configuration 4 zones (complète)
python3 send_zone_config.py 4

# Configuration d'urgence (seuils bas)
python3 send_zone_config.py emergency
```

## 📌 Notes Importantes

1. **Ordre des zones** : L'ordre dans le tableau `zones` n'a pas d'importance, c'est `physicalZoneNumber` qui détermine l'emplacement
2. **Réassignation** : Si une zone existe déjà, ses capteurs sont d'abord désassignés puis réassignés selon la nouvelle config
3. **Capteurs multiples** : Une zone peut avoir plusieurs capteurs (jusqu'à 12 au total pour toutes les zones)
4. **Format des IDs** : Les IDs de capteurs doivent suivre le format `s01`-`s12` (avec zéro devant pour 01-09)

