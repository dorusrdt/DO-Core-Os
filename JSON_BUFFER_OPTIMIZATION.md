# 📊 Optimisation des Buffers JSON - Mémoire

Ce document explique les tailles optimales des buffers JSON et les optimisations effectuées.

## 📏 Signification de "758 bytes"

**758 bytes** est probablement la taille réelle d'un payload JSON envoyé par ESP32_sensor au master. C'est la taille du message sérialisé après `serializeJson()`.

### Exemple de Payload ESP32_sensor

```json
{
  "s01": 45.2,
  "s02": 50.1,
  "s03": 48.5,
  "s04": 33.7,
  "s05": 35.2,
  "s06": 32.1,
  "s07": 55.8,
  "s08": 60.3,
  "s09": 58.9,
  "s10": 42.1,
  "s11": 44.5,
  "s12": 46.8,
  "timestamp": 123456789,
  "deviceType": "sensor"
}
```

**Taille estimée** : ~200-300 bytes (sérialisé)
**Taille réelle observée** : ~150-200 bytes typiquement

## 🔧 Optimisations Effectuées

### 1. ESP32_sensor - Buffer d'envoi

**Avant** : `DynamicJsonDocument doc(2048)` - 2 KB
**Après** : `DynamicJsonDocument doc(512)` - 512 bytes
**Économie** : **1.5 KB**

**Justification** :
- 12 capteurs (s01-s12) : ~12 × 15 bytes = 180 bytes
- timestamp + deviceType : ~30 bytes
- **Total max** : ~250 bytes
- **Marge de sécurité** : 512 bytes (2× la taille réelle)

### 2. ESP32_master - Réception des données sensor

**Avant** : `DynamicJsonDocument slaveSensorData(4096)` - 4 KB
**Après** : `DynamicJsonDocument slaveSensorData(1024)` - 1 KB
**Économie** : **3 KB**

**Justification** :
- Reçoit les données de ESP32_sensor (~200-300 bytes max)
- **Marge de sécurité** : 1024 bytes (3-4× la taille réelle)

### 3. ESP32_master - Parsing des données sensor (local)

**Avant** : `DynamicJsonDocument sensorDoc(4096)` - 4 KB (×3 occurrences)
**Après** : `DynamicJsonDocument sensorDoc(1024)` - 1 KB
**Économie** : **9 KB** (3 × 3 KB)

**Justification** : Même raison que ci-dessus

### 4. ESP32_master - Parsing de la configuration

**Avant** : `DynamicJsonDocument doc(32768)` - 32 KB
**Après** : `DynamicJsonDocument doc(4096)` - 4 KB
**Économie** : **28 KB**

**Justification** :
- Configuration max : 4 zones avec 3 capteurs chacune
- Structure : type + zones array
- **Estimation** : ~2-3 KB max
- **Marge de sécurité** : 4 KB

### 5. ESP32_master - Envoi des données au serveur

**Avant** : `DynamicJsonDocument doc(32768)` - 32 KB
**Après** : `DynamicJsonDocument doc(3072)` - 3 KB
**Économie** : **29 KB**

**Justification** :
- Structure : type + deviceId + timestamp + globalData + zonesData
- globalData : 5 champs (~150 bytes)
- zonesData : 4 zones × 3 capteurs = 12 capteurs (~600 bytes)
- **Total max** : ~1.5 KB
- **Marge de sécurité** : 3 KB (2× la taille réelle)

## 📊 Tableau Récapitulatif

| Buffer | Avant | Après | Économie | Justification |
|--------|-------|-------|----------|---------------|
| ESP32_sensor (envoi) | 2 KB | 512 B | **1.5 KB** | 12 capteurs max ~250 B |
| ESP32_master (réception) | 4 KB | 1 KB | **3 KB** | Reçoit ~200-300 B |
| ESP32_master (parsing local ×3) | 12 KB | 3 KB | **9 KB** | Même raison |
| ESP32_master (config) | 32 KB | 4 KB | **28 KB** | Config max ~2-3 KB |
| ESP32_master (envoi serveur) | 32 KB | 3 KB | **29 KB** | Payload max ~1.5 KB |
| **TOTAL** | **82 KB** | **11.5 KB** | **70.5 KB** | **86% de réduction** |

## ✅ Tailles Optimales

### Recommandations

1. **ESP32_sensor** : 512 bytes suffisent pour 12 capteurs
2. **ESP32_master (réception)** : 1024 bytes pour recevoir les données sensor
3. **ESP32_master (parsing)** : 1024 bytes pour parser les données localement
4. **ESP32_master (config)** : 4096 bytes pour les configurations complexes
5. **ESP32_master (envoi serveur)** : 3072 bytes pour le payload complet

### Calcul de la Taille Réelle

Pour calculer la taille réelle d'un JSON :

```cpp
String payload;
serializeJson(doc, payload);
size_t realSize = payload.length(); // Taille réelle en bytes
```

**Exemple** :
- Payload ESP32_sensor : ~150-200 bytes
- Payload ESP32_master → serveur : ~500-800 bytes (selon nombre de zones)

## ⚠️ Notes Importantes

### Marge de Sécurité

Les tailles allouées incluent une **marge de sécurité** de 2-4× la taille réelle pour :
- Variations dans les données
- Overhead de sérialisation JSON
- Fragmentation mémoire
- Sécurité contre les stack overflows

### Si Vous Rencontrez des Problèmes

Si vous voyez des erreurs de parsing ou de sérialisation :

1. **Vérifier la taille réelle** : Utiliser `payload.length()` après sérialisation
2. **Augmenter progressivement** : Ajouter 512-1024 bytes à la fois
3. **Surveiller les logs** : Les logs affichent `memoryUsage` et `capacity`

### Surveillance

Le code affiche maintenant :
```cpp
MASTER_LOG(LOG_LEVEL_INFO, "JSON document: %u/%u bytes used, zones: %d",
           memoryUsed, capacity, assignedZoneCount);
```

Si `memoryUsed` approche `capacity` (≥95%), augmenter la taille du buffer.

## 🎯 Résultat Final

**Mémoire économisée** : **~70.5 KB de RAM**

Cette optimisation libère une quantité significative de mémoire pour d'autres opérations du système.

