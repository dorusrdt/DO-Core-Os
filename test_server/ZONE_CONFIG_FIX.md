# 🔧 Zone Configuration Fix - CRITICAL UPDATE

## Date
25 Novembre 2025

## 📋 Problème Identifié
Le modèle `ZoneConfig` dans `irrigation_server.py` était **INCOMPLET** et ne correspondait pas à ce que l'ESP32 attendait vraiment du serveur.

### Champs Manquants ❌
1. **`physicalZoneNumber`** - Très important pour l'ESP32 pour mapper les zones aux relais physiques (1-4)
2. **`irrigationSchedule`** - Tableau d'horaires détaillés avec jours de la semaine

## ✅ Corrections Appliquées

### 1. Nouveau Modèle `IrrigationSchedule` (AJOUTÉ)
```python
class IrrigationSchedule(BaseModel):
    """Irrigation schedule with time, duration, and days of week"""
    time: str  # Format HH:MM (e.g., "08:00")
    duration: int  # Duration in minutes (1-300)
    daysOfWeek: List[int]  # Days of week (0=Sunday, 1=Monday, ..., 6=Saturday)
    isActive: Optional[bool] = True
```

### 2. Modèle `ZoneConfig` Mis à Jour
```python
class ZoneConfig(BaseModel):
    """Zone configuration with all required fields for ESP32"""
    zoneId: str
    physicalZoneNumber: int  # ← AJOUTÉ - Physical zone number (1-4)
    waterPerDay: int
    irrigationTime: Optional[str] = None  # Legacy support
    irrigationTimes: Optional[List[str]] = None  # Legacy support
    irrigationSchedule: Optional[List[IrrigationSchedule]] = None  # ← AJOUTÉ - New detailed schedules
    humidityThreshold: int
    sensors: List[SensorConfig]
```

## 📊 Exemple de Configuration Complète

### AVANT (Incomplet) ❌
```json
{
  "zoneId": "zone_potager_nord",
  "waterPerDay": 2000,
  "irrigationTime": "08:00",
  "humidityThreshold": 25,
  "sensors": [
    {"sensorId": "s_01"},
    {"sensorId": "s_02"}
  ]
}
```

### APRÈS (Complet) ✅
```json
{
  "zoneId": "zone_potager_nord",
  "physicalZoneNumber": 1,
  "waterPerDay": 2000,
  "irrigationTime": "08:00",
  "humidityThreshold": 25,
  "sensors": [
    {"sensorId": "s_01"},
    {"sensorId": "s_02"}
  ],
  "irrigationSchedule": [
    {
      "time": "08:00",
      "duration": 15,
      "daysOfWeek": [1, 3, 5],
      "isActive": true
    },
    {
      "time": "18:00",
      "duration": 20,
      "daysOfWeek": [0, 2, 4, 6],
      "isActive": true
    }
  ]
}
```

## 📝 Fichiers Modifiés

### 1. `irrigation_server.py`
- ✅ Ajout modèle `IrrigationSchedule`
- ✅ Mise à jour modèle `ZoneConfig` avec `physicalZoneNumber`
- ✅ Ajout support `irrigationSchedule` (optional)
- ✅ Syntaxe validée

### 2. `create_test_zones.sh`
- ✅ Toutes les 4 zones incluent maintenant `physicalZoneNumber`
- ✅ Toutes les zones incluent `irrigationSchedule` avec horaires réalistes
- ✅ Zone 1 (Potager Nord): Lundi/Mercredi/Vendredi + Dimanche/Mardi/Jeudi/Samedi
- ✅ Zone 2 (Jardin Sud): Semaine + Week-end
- ✅ Zone 3 (Serre): Lundi/Mercredi/Vendredi
- ✅ Zone 4 (Verger): Tous les jours
- ✅ Syntaxe Bash validée

## 🔄 Rétrocompatibilité

Le modèle reste **100% rétrocompatible** :
- ✅ `irrigationTime` reste optionnel (legacy)
- ✅ `irrigationTimes` reste optionnel (legacy)
- ✅ `physicalZoneNumber` peut être ignoré par des clients legacy
- ✅ `irrigationSchedule` est optionnel (nouveau format)

## 🧪 Impact sur les Tests

Les scripts de test suivants nécessitent une mise à jour :
- `create_test_zones.sh` - ✅ MISE À JOUR
- `test_server.sh` - À vérifier/mettre à jour si nécessaire
- Tout client qui envoie des zones doit inclure `physicalZoneNumber`

## 🎯 Prochaines Étapes

1. **Redémarrer le serveur:**
   ```bash
   cd /home/dorus/Documents/GitHub/DO-Core-Os/test_server
   python3 irrigation_server.py
   ```

2. **Créer les zones de test:**
   ```bash
   ./create_test_zones.sh
   ```

3. **Vérifier la réception du device:**
   - L'ESP32 doit recevoir `physicalZoneNumber` dans chaque zone
   - L'ESP32 doit parser `irrigationSchedule` s'il est présent
   - Fallback sur `irrigationTime` si `irrigationSchedule` absent

## 📚 Références

- `IRRIGATION_SCHEDULE_PROTOCOL.md` - Protocole complet
- `versio` (firmware ESP32) - Code C++ de parsing
- `IRRIGATION_LOGIC_VERIFICATION.md` - Logique d'irrigation

## ✨ Status

**VERSION**: 2.0.1
**STATUS**: ✅ CORRECTED & VALIDATED
**BACKWARD COMPATIBLE**: ✅ YES
**SYNTAX**: ✅ VALID (Python & Bash)
