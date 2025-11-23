# 📊 Analyse : Serveur Réel vs Serveur de Simulation

## 🎯 Objectif

Comparer la structure de configuration envoyée par le **serveur réel** (`irrigation-ai-v-beta`) avec celle du **serveur de simulation** actuel pour identifier les différences et proposer les mises à jour nécessaires dans `ESP32_master`.

---

## 📋 Comparaison des Structures de Configuration

### 🔴 Serveur de Simulation Actuel (`test_server/irrigation_server.py`)

**Format JSON envoyé :**
```json
{
  "type": "config",
  "deviceId": "ESP32_IRRIGATION_...",
  "zones": [
    {
      "zoneId": "zone_001",
      "physicalZoneNumber": 1,
      "waterPerDay": 5000,
      "irrigationTime": "08:00",          // ⚠️ UN SEUL HORAIRE
      "humidityThreshold": 40,
      "sensors": [
        {"sensorId": "s01"},
        {"sensorId": "s02"},
        {"sensorId": "s03"}
      ]
    }
  ],
  "timestamp": "2025-11-22T..."
}
```

**Caractéristiques :**
- ✅ `irrigationTime` : String simple "HH:MM" (un seul horaire par zone)
- ✅ `waterPerDay` : En millilitres
- ✅ `humidityThreshold` : Pourcentage (0-100)
- ❌ Pas de support pour plusieurs horaires
- ❌ Pas de support pour jours spécifiques
- ❌ Durée fixe calculée : `waterPerDay / 10` (secondes)

---

### 🟢 Serveur Réel (`irrigation-ai-v-beta`)

**Format JSON envoyé :**
```json
{
  "type": "config",
  "deviceId": "ESP32_IRRIGATION_...",
  "zones": [
    {
      "zoneId": "zone_1763489053352_60413tgyw",
      "physicalZoneNumber": 1,
      "waterPerDay": 12000,                // En millilitres (converti depuis L)
      "humidityThreshold": 35,
      "sensors": [
        {"sensorId": "s01"},
        {"sensorId": "s02"},
        {"sensorId": "s03"},
        {"sensorId": "s04"}
      ],
      "irrigationSchedule": [              // ✅ NOUVEAU : Array d'horaires
        {
          "time": "08:00",
          "duration": 15,                   // ✅ NOUVEAU : Durée en minutes
          "daysOfWeek": [1, 3, 5]          // ✅ NOUVEAU : Jours de la semaine
        },
        {
          "time": "22:44",
          "duration": 2,
          "daysOfWeek": [1, 2, 3, 4, 5]
        }
      ],
      "scheduleCount": 2                   // ✅ NOUVEAU : Nombre d'horaires
    }
  ],
  "timestamp": "2025-11-18T18:05:33.592Z"
}
```

**Caractéristiques :**
- ✅ `irrigationSchedule` : **Array d'objets** (support multiple horaires)
- ✅ `scheduleCount` : Nombre d'horaires actifs
- ✅ `duration` : Durée spécifique par horaire (en minutes, 1-300)
- ✅ `daysOfWeek` : Jours spécifiques (0=dimanche, 1=lundi, ..., 6=samedi)
- ✅ `waterPerDay` : En millilitres (converti depuis L × 1000)
- ✅ `humidityThreshold` : Depuis `aiConfig.irrigationThreshold`
- ⚠️ `irrigationTime` : **DÉPRÉCIÉ** (utilisé comme fallback si pas de schedule)

---

## 🔍 Différences Clés Identifiées

### 1. **Système d'Horaires**

| Aspect | Simulation | Serveur Réel |
|--------|------------|--------------|
| **Format** | `irrigationTime: "08:00"` (string) | `irrigationSchedule: [...]` (array) |
| **Nombre** | 1 horaire par zone | **Illimité** (dynamique) |
| **Durée** | Calculée : `waterPerDay / 10` (secondes) | **Spécifique** : `duration` (minutes) |
| **Jours** | Tous les jours | **Sélectifs** : `daysOfWeek` array |
| **Flexibilité** | ❌ Rigide | ✅ Très flexible |

### 2. **Structure des Données**

**Simulation :**
```json
{
  "irrigationTime": "08:00"  // Simple string
}
```

**Réel :**
```json
{
  "irrigationSchedule": [
    {
      "time": "08:00",
      "duration": 15,
      "daysOfWeek": [1, 3, 5]
    }
  ],
  "scheduleCount": 1
}
```

### 3. **Champs Manquants dans le Master Actuel**

Le master actuel ne gère **PAS** :
- ❌ `irrigationSchedule` (array)
- ❌ `scheduleCount`
- ❌ `duration` par horaire
- ❌ `daysOfWeek` (filtrage par jour)

---

## 💡 Analyse des Potentielles Utilisations

### 1. **`irrigationSchedule` (Array d'Horaires)**

**Utilisation actuelle :** ❌ Non supporté

**Potentiel :**
- ✅ **Plusieurs arrosages par jour** : Ex. 08:00 et 18:00
- ✅ **Arrosages différenciés** : Durée différente selon l'heure
- ✅ **Optimisation de l'eau** : Arrosages courts le matin, longs le soir
- ✅ **Adaptation saisonnière** : Plus d'horaires en été, moins en hiver

**Impact sur la logique :**
- Remplacer `checkIrrigationSchedule()` qui vérifie un seul horaire
- Implémenter une boucle sur tous les horaires de toutes les zones
- Vérifier `daysOfWeek` avant d'exécuter

### 2. **`duration` (Durée par Horaire)**

**Utilisation actuelle :** ❌ Non supporté (utilise `waterPerDay / 10`)

**Potentiel :**
- ✅ **Durée précise** : 15 min le matin, 30 min le soir
- ✅ **Économie d'eau** : Durées optimisées par l'IA
- ✅ **Adaptation au besoin** : Plus long si sol très sec

**Impact sur la logique :**
- Remplacer `executeIrrigation(zoneId, waterPerDay / 10)`
- Utiliser `schedule.duration * 60` (convertir minutes → secondes)

### 3. **`daysOfWeek` (Jours Spécifiques)**

**Utilisation actuelle :** ❌ Non supporté (arrose tous les jours)

**Potentiel :**
- ✅ **Arrosage sélectif** : Lundi, Mercredi, Vendredi uniquement
- ✅ **Économie d'eau** : Pas d'arrosage le dimanche
- ✅ **Adaptation météo** : Arrosage selon prévisions météo

**Impact sur la logique :**
- Vérifier le jour actuel (`tm_wday`) avant d'exécuter
- Comparer avec `daysOfWeek` array
- Ne pas arroser si le jour n'est pas dans la liste

### 4. **`scheduleCount` (Nombre d'Horaires)**

**Utilisation actuelle :** ❌ Non supporté

**Potentiel :**
- ✅ **Validation** : Vérifier qu'on a reçu tous les horaires
- ✅ **Logs** : Afficher "X horaires chargés"
- ✅ **Debug** : Détecter si des horaires manquent

**Impact sur la logique :**
- Validation après parsing
- Logs informatifs
- Pas critique pour le fonctionnement

---

## 📝 Rapport : Mises à Jour Nécessaires dans ESP32_master

### 🔴 **Priorité HAUTE**

#### 1. **Parser `irrigationSchedule` au lieu de `irrigationTime`**

**Fichier :** `src/apps/ESP32_master/ESP32_master.cpp`

**Fonction :** `parseConfiguration()`

**Changements nécessaires :**

```cpp
// AVANT (actuel)
String irrigationTime = zone["irrigationTime"];  // "08:00"
ZONE_STACK[i].irrigationTime = irrigationTime;

// APRÈS (nouveau)
if (zone.containsKey("irrigationSchedule")) {
    JsonArray schedules = zone["irrigationSchedule"];
    // Parser chaque horaire
    for (JsonObject schedule : schedules) {
        String time = schedule["time"];           // "08:00"
        int duration = schedule["duration"];      // 15 (minutes)
        JsonArray daysOfWeek = schedule["daysOfWeek"];  // [1, 3, 5]

        // Stocker dans une structure
        // ...
    }
} else if (zone.containsKey("irrigationTime")) {
    // Fallback pour compatibilité
    String irrigationTime = zone["irrigationTime"];
    // Créer un schedule par défaut (tous les jours)
}
```

**Structure de données à ajouter :**

```cpp
struct IrrigationSchedule {
    String time;              // "08:00"
    int durationMinutes;      // 15
    bool daysOfWeek[7];       // [false, true, false, true, false, true, false]
    bool isActive;
};

struct ZoneSlot {
    // ... champs existants ...
    IrrigationSchedule schedules[10];  // Max 10 horaires par zone
    int scheduleCount;
    // irrigationTime devient DEPRECATED
};
```

#### 2. **Modifier `checkIrrigationSchedule()` pour gérer plusieurs horaires**

**Fichier :** `src/apps/ESP32_master/ESP32_master.cpp`

**Fonction :** `checkIrrigationSchedule()`

**Changements nécessaires :**

```cpp
// AVANT (actuel)
static void checkIrrigationSchedule() {
    // Vérifie UN SEUL horaire : irrigationTime
    for (int i = 0; i < MAX_ZONES; i++) {
        if (ZONE_STACK[i].configured &&
            ZONE_STACK[i].irrigationTime.equals(currentTime)) {
            executeIrrigation(ZONE_STACK[i].zoneId,
                            ZONE_STACK[i].waterPerDay / 10);
        }
    }
}

// APRÈS (nouveau)
static void checkIrrigationSchedule() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return;

    int currentDayOfWeek = timeinfo.tm_wday;  // 0=dimanche, 1=lundi, etc.
    char currentTime[6];
    strftime(currentTime, sizeof(currentTime), "%H:%M", &timeinfo);

    for (int i = 0; i < MAX_ZONES; i++) {
        if (!ZONE_STACK[i].configured) continue;

        // Vérifier TOUS les horaires de cette zone
        for (int s = 0; s < ZONE_STACK[i].scheduleCount; s++) {
            IrrigationSchedule& schedule = ZONE_STACK[i].schedules[s];

            // Vérifier le jour
            if (!schedule.daysOfWeek[currentDayOfWeek]) {
                continue;  // Pas aujourd'hui
            }

            // Vérifier l'heure
            if (String(currentTime).equals(schedule.time)) {
                // Exécuter avec la durée spécifique
                int durationSeconds = schedule.durationMinutes * 60;
                executeIrrigation(ZONE_STACK[i].zoneId, durationSeconds);
            }
        }
    }
}
```

#### 3. **Vérifier `daysOfWeek` avant exécution**

**Fichier :** `src/apps/ESP32_master/ESP32_master.cpp`

**Fonction :** `checkIrrigationSchedule()`

**Logique :**
- Obtenir le jour actuel : `tm_wday` (0=dimanche, 1=lundi, ..., 6=samedi)
- Vérifier si `daysOfWeek[currentDayOfWeek] == true`
- Ne pas exécuter si le jour n'est pas dans la liste

---

### 🟡 **Priorité MOYENNE**

#### 4. **Utiliser `duration` au lieu de `waterPerDay / 10`**

**Fichier :** `src/apps/ESP32_master/ESP32_master.cpp`

**Fonction :** `checkIrrigationSchedule()`

**Changements :**
- Remplacer `waterPerDay / 10` par `schedule.durationMinutes * 60`
- `duration` est en minutes, convertir en secondes

#### 5. **Gérer le fallback `irrigationTime` (compatibilité)**

**Fichier :** `src/apps/ESP32_master/ESP32_master.cpp`

**Fonction :** `parseConfiguration()`

**Logique :**
- Si `irrigationSchedule` existe → utiliser
- Sinon, si `irrigationTime` existe → créer un schedule par défaut (tous les jours, durée = `waterPerDay / 10`)
- Sinon → pas d'irrigation programmée

#### 6. **Valider `scheduleCount`**

**Fichier :** `src/apps/ESP32_master/ESP32_master.cpp`

**Fonction :** `parseConfiguration()`

**Logique :**
- Après parsing, vérifier que `scheduleCount` correspond au nombre réel d'horaires parsés
- Logger un avertissement si différence

---

### 🟢 **Priorité BASSE (Améliorations)**

#### 7. **Logs améliorés pour les horaires**

**Fichier :** `src/apps/ESP32_master/ESP32_master.cpp`

**Fonction :** `parseConfiguration()`

**Logs à ajouter :**
```
Zone 1: zone_001 configured with 3 schedules:
  Schedule 1: 08:00, 15 min, days: [Mon, Wed, Fri]
  Schedule 2: 18:00, 30 min, days: [Tue, Thu, Sat]
  Schedule 3: 22:00, 10 min, days: [All]
```

#### 8. **Gestion de la mémoire pour les horaires**

**Fichier :** `src/apps/ESP32_master/ESP32_master.cpp`

**Considérations :**
- Limiter le nombre d'horaires par zone (ex: 10 max)
- Vérifier la mémoire disponible avant d'ajouter un horaire
- Logger si limite atteinte

---

## 🎯 Plan d'Implémentation Recommandé

### **Phase 1 : Support Basique (Compatibilité)**

1. ✅ Parser `irrigationSchedule` si présent
2. ✅ Fallback sur `irrigationTime` si absent
3. ✅ Utiliser `duration` au lieu de `waterPerDay / 10`
4. ✅ Vérifier `daysOfWeek` avant exécution

### **Phase 2 : Support Complet**

1. ✅ Support de plusieurs horaires par zone
2. ✅ Validation de `scheduleCount`
3. ✅ Logs détaillés
4. ✅ Gestion de la mémoire

### **Phase 3 : Optimisations**

1. ✅ Cache des horaires actifs
2. ✅ Pré-calcul des prochains horaires
3. ✅ Détection des conflits (même heure, même zone)

---

## 📊 Tableau Récapitulatif des Changements

| Champ | Simulation | Serveur Réel | Action Requise |
|-------|------------|--------------|----------------|
| `irrigationTime` | ✅ String "HH:MM" | ⚠️ DÉPRÉCIÉ | Fallback uniquement |
| `irrigationSchedule` | ❌ Absent | ✅ Array d'objets | **PARSER** |
| `scheduleCount` | ❌ Absent | ✅ Integer | **VALIDER** |
| `duration` | ❌ Calculé | ✅ Minutes | **UTILISER** |
| `daysOfWeek` | ❌ Tous les jours | ✅ Array [0-6] | **VÉRIFIER** |
| `waterPerDay` | ✅ Millilitres | ✅ Millilitres | ✅ OK |
| `humidityThreshold` | ✅ Pourcentage | ✅ Pourcentage | ✅ OK |
| `sensors` | ✅ Array | ✅ Array | ✅ OK |

---

## ⚠️ Points d'Attention

### 1. **Compatibilité Rétrograde**

Le serveur réel peut encore envoyer `irrigationTime` comme fallback. Le master doit gérer les deux formats :
- Format nouveau : `irrigationSchedule` (prioritaire)
- Format ancien : `irrigationTime` (fallback)

### 2. **Limite de Mémoire**

Avec plusieurs horaires par zone, la mémoire peut être un problème :
- Limiter à 10 horaires par zone max
- Utiliser des structures statiques (pas de malloc)

### 3. **Validation des Données**

- `duration` : Entre 1 et 300 minutes
- `daysOfWeek` : Array de 0-6, pas de doublons
- `time` : Format "HH:MM" valide

### 4. **Performance**

Vérifier tous les horaires toutes les minutes peut être coûteux :
- Vérifier seulement les horaires actifs
- Pré-calculer les prochains horaires

---

## ✅ Checklist de Migration

- [ ] Ajouter structure `IrrigationSchedule`
- [ ] Modifier `ZoneSlot` pour inclure `schedules[]` et `scheduleCount`
- [ ] Parser `irrigationSchedule` dans `parseConfiguration()`
- [ ] Implémenter fallback `irrigationTime`
- [ ] Modifier `checkIrrigationSchedule()` pour boucler sur tous les horaires
- [ ] Vérifier `daysOfWeek` avant exécution
- [ ] Utiliser `duration` au lieu de `waterPerDay / 10`
- [ ] Ajouter logs détaillés
- [ ] Tester avec serveur réel
- [ ] Tester compatibilité avec serveur de simulation

---

## 📌 Conclusion

Le serveur réel utilise un **système d'horaires dynamique et flexible** (`irrigationSchedule`) qui permet :
- ✅ Plusieurs arrosages par jour
- ✅ Durées spécifiques par horaire
- ✅ Jours sélectifs
- ✅ Configuration illimitée

Le master actuel doit être **mis à jour** pour supporter ce nouveau format tout en gardant la **compatibilité** avec l'ancien format (`irrigationTime`).

**Impact :** Modifications importantes dans `parseConfiguration()` et `checkIrrigationSchedule()`, mais bénéfices significatifs en flexibilité et optimisation de l'irrigation.

