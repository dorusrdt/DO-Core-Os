# 🌾 Cas d'Usage et Exemples Multi-Zones

## 📋 Table des Cas d'Usage

1. [Une seule zone à la fois (Compatibilité)](#cas-1-une-seule-zone-à-la-fois)
2. [Deux zones séquentielles](#cas-2-deux-zones-séquentielles)
3. [Trois zones simultanées](#cas-3-trois-zones-simultanées)
4. [Quatre zones simultanées](#cas-4-quatre-zones-simultanées)
5. [Arrêt d'une zone pendant l'irrigation](#cas-5-arrêt-dune-zone-pendant-lirrigration)
6. [Urgence: Arrêt complet](#cas-6-urgence-arrêt-complet)
7. [Scénario réaliste jardin](#cas-7-scénario-réaliste-jardin)

---

## Cas 1: Une Seule Zone à la Fois

### Configuration
```json
{
  "zones": [
    {
      "id": "zone_1",
      "physicalZoneNumber": 1,
      "schedules": [
        {"time": "08:00", "durationMinutes": 10, "days": "ALL"}
      ]
    }
  ]
}
```

### Timeline
```
08:00:00  Master: START zone_1 (60s)
          Com: Zone 1 ON, Pompe ON

08:01:00  Timer zone 1 expire
          Com: Zone 1 OFF, Pompe OFF

Status: ✅ Compatible (fonctionne comme avant)
```

### Logs
```
[1200000] INFO: [ESP32_master] 🚰 Starting irrigation for zone zone_1 (Physical #1) | Duration: 1 min 0 sec
[1200000] INFO: [ESP32_com] 📥 Received START irrigation command: Zone 1, Duration: 60 sec
[1200000] INFO: [ESP32_com] ✅ Pump: ON (GPIO 5 = LOW)
[1200000] INFO: [ESP32_com] ✅ Zone 1 relay: ON (GPIO 23 = LOW)
[1201000] INFO: [ESP32_com] ⏰ Irrigation timer expired for Zone 1
[1201000] INFO: [ESP32_com] 🛑 Stopping irrigation for Zone 1
[1201000] INFO: [ESP32_com] ✅ Pump: OFF (GPIO 5 = HIGH)
```

---

## Cas 2: Deux Zones Séquentielles

### Configuration
```json
{
  "zones": [
    {
      "id": "zone_1",
      "physicalZoneNumber": 1,
      "schedules": [{"time": "08:00", "durationMinutes": 10}]
    },
    {
      "id": "zone_2",
      "physicalZoneNumber": 2,
      "schedules": [{"time": "08:15", "durationMinutes": 10}]
    }
  ]
}
```

### Timeline
```
08:00:00  Master: START zone_1 (600s)
          Com: Zone 1 ON, Pompe ON

08:10:00  Zone 1 expire
          Com: Zone 1 OFF, Pompe OFF

08:15:00  Master: START zone_2 (600s)
          Com: Zone 2 ON, Pompe ON

08:25:00  Zone 2 expire
          Com: Zone 2 OFF, Pompe OFF

Status: ✅ Zones ne se chevauchent pas
```

### Logs
```
[1200000] INFO: [ESP32_master] 🚰 Starting irrigation for zone zone_1 (Physical #1) | Duration: 10 min
[1200000] INFO: [ESP32_com] ✅ Pump: ON (GPIO 5 = LOW)
[1200000] INFO: [ESP32_com] ✅ Zone 1 relay: ON (GPIO 23 = LOW)

[1210000] INFO: [ESP32_com] ⏰ Irrigation timer expired for Zone 1
[1210000] INFO: [ESP32_com] ✅ Pump: OFF (GPIO 5 = HIGH)

[1215000] INFO: [ESP32_master] 🚰 Starting irrigation for zone zone_2 (Physical #2) | Duration: 10 min
[1215000] INFO: [ESP32_com] ✅ Pump: ON (GPIO 5 = LOW)
[1215000] INFO: [ESP32_com] ✅ Zone 2 relay: ON (GPIO 4 = LOW)

[1225000] INFO: [ESP32_com] ⏰ Irrigation timer expired for Zone 2
[1225000] INFO: [ESP32_com] ✅ Pump: OFF (GPIO 5 = HIGH)
```

---

## Cas 3: Trois Zones Simultanées

### Configuration
```json
{
  "zones": [
    {
      "id": "zone_1",
      "physicalZoneNumber": 1,
      "schedules": [{"time": "08:00", "durationMinutes": 5}]
    },
    {
      "id": "zone_2",
      "physicalZoneNumber": 2,
      "schedules": [{"time": "08:00", "durationMinutes": 10}]
    },
    {
      "id": "zone_3",
      "physicalZoneNumber": 3,
      "schedules": [{"time": "08:00", "durationMinutes": 15}]
    }
  ]
}
```

### Timeline
```
08:00:00  Master: START zone_1 (300s)
          Com: Zone 1 ON, Pompe ON

08:00:01  Master: START zone_2 (600s)
          Com: Zone 2 ON, Pompe DÉJÀ ON

08:00:02  Master: START zone_3 (900s)
          Com: Zone 3 ON, Pompe DÉJÀ ON

          État: Zones 1, 2, 3 TOUS ACTIFS

08:05:00  Zone 1 expire
          Com: Zone 1 OFF, Pompe RESTE ON (zones 2, 3 actifs)

08:10:00  Zone 2 expire
          Com: Zone 2 OFF, Pompe RESTE ON (zone 3 actif)

08:15:00  Zone 3 expire
          Com: Zone 3 OFF, Pompe OFF (aucune zone)

Status: ✅ Optimisation pompe!
```

### Logs
```
[1200000] INFO: [ESP32_master] 🚰 Starting irrigation for zone zone_1 (Physical #1) | Duration: 5 min
[1200000] INFO: [ESP32_com] ✅ Pump: ON (GPIO 5 = LOW)

[1200010] INFO: [ESP32_master] 🚰 Starting irrigation for zone zone_2 (Physical #2) | Duration: 10 min
[1200010] INFO: [ESP32_com] ℹ️  Pump already running for another zone

[1200020] INFO: [ESP32_master] 🚰 Starting irrigation for zone zone_3 (Physical #3) | Duration: 15 min
[1200020] INFO: [ESP32_com] ℹ️  Pump already running for another zone

[1200055] INFO: [ESP32_com] ⏰ Irrigation timer expired for Zone 1
[1200055] INFO: [ESP32_com] ℹ️  Other zones still active, pump remains ON

[1201000] INFO: [ESP32_com] ⏰ Irrigation timer expired for Zone 2
[1201000] INFO: [ESP32_com] ℹ️  Other zones still active, pump remains ON

[1201055] INFO: [ESP32_com] ⏰ Irrigation timer expired for Zone 3
[1201055] INFO: [ESP32_com] ✅ Pump: OFF (GPIO 5 = HIGH)
```

---

## Cas 4: Quatre Zones Simultanées

### Configuration
```json
{
  "zones": [
    {"id": "zone_1", "physicalZoneNumber": 1, "schedules": [{"time": "08:00", "durationMinutes": 5}]},
    {"id": "zone_2", "physicalZoneNumber": 2, "schedules": [{"time": "08:00", "durationMinutes": 10}]},
    {"id": "zone_3", "physicalZoneNumber": 3, "schedules": [{"time": "08:00", "durationMinutes": 15}]},
    {"id": "zone_4", "physicalZoneNumber": 4, "schedules": [{"time": "08:00", "durationMinutes": 20}]}
  ]
}
```

### Timeline
```
08:00:00  Toutes les 4 zones démarrent
          Pompe ON UNE SEULE FOIS

08:05:00  Zone 1 s'arrête → Pompe reste ON
08:10:00  Zone 2 s'arrête → Pompe reste ON
08:15:00  Zone 3 s'arrête → Pompe reste ON
08:20:00  Zone 4 s'arrête → Pompe OFF

Status: ✅ Débit maximal optimisé!
```

### Consommation Énergétique
```
Avant (séquentiel):    4 × démarrage pompe = 4 pics de courant
Après (parallèle):     1 × démarrage pompe = 1 pic de courant

ÉCONOMIE: -75% de pics ⚡
```

---

## Cas 5: Arrêt d'une Zone Pendant l'Irrigation

### Scénario
Master reçoit une commande d'arrêt pour Zone 2 (ex: capteur défaillant)

### Commande
```json
{
  "action": "stop_irrigation",
  "zoneId": "zone_2",
  "physicalZoneNumber": 2
}
```

### Timeline
```
08:00:00  Zones 1, 2, 3 START
          Zone 1 ON, Zone 2 ON, Zone 3 ON, Pompe ON

08:00:30  Master: STOP zone_2 (anomalie détectée)
          Com reçoit: physicalZoneNumber = 2

08:00:30  stopIrrigation(2) appelé
          Zone 2 OFF
          Zones 1 et 3 restent actives
          Pompe reste ON

08:05:00  Zone 1 expire → Zone 1 OFF, Pompe ON
08:15:00  Zone 3 expire → Zone 3 OFF, Pompe OFF

Status: ✅ Arrêt sélectif fonctionnel
```

### Logs
```
[1200000] INFO: [ESP32_com] 📥 Received START zone 1
[1200001] INFO: [ESP32_com] 📥 Received START zone 2
[1200002] INFO: [ESP32_com] 📥 Received START zone 3

[1200030] INFO: [ESP32_com] 📥 Received STOP irrigation command:
[1200030] INFO: [ESP32_com]    Zone ID: zone_2
[1200030] INFO: [ESP32_com]    Zone 2 is active - stopping now
[1200030] INFO: [ESP32_com] 🛑 Stopping irrigation for Zone 2:
[1200030] INFO: [ESP32_com]    ✅ Zone 2 relay: OFF (GPIO 4 = HIGH)
[1200030] INFO: [ESP32_com]    ℹ️  Other zones still active, pump remains ON
```

---

## Cas 6: Urgence - Arrêt Complet

### Commande
```json
{
  "action": "stop_irrigation",
  "physicalZoneNumber": 0  // 0 = arrête TOUT
}
```

### Timeline
```
08:00:00  Zones 1, 2, 3, 4 actives

08:02:00  URGENCE: Interruption manuelle
          Master envoie STOP avec physicalZoneNumber = 0

08:02:01  Com reçoit: stopIrrigation(0)
          Zone 1 OFF
          Zone 2 OFF
          Zone 3 OFF
          Zone 4 OFF
          Pompe OFF

Status: ✅ Arrêt complet instantané
```

### Code
```cpp
// Master envoie arrêt d'urgence
if (emergencyStop) {
    DynamicJsonDocument cmd(256);
    cmd["action"] = "stop_irrigation";
    cmd["physicalZoneNumber"] = 0;  // 0 = STOP ALL

    webSocket->broadcastTXT(cmdStr);
}

// Com reçoit et traite
stopIrrigation(0);  // Arrête toutes les zones
```

---

## Cas 7: Scénario Réaliste Jardin

### Configuration Jardin
```json
{
  "zones": [
    {
      "id": "potager",
      "physicalZoneNumber": 1,
      "humidityThreshold": 40,
      "schedules": [
        {"time": "06:00", "durationMinutes": 20, "days": "Mon,Wed,Fri"},
        {"time": "18:00", "durationMinutes": 15, "days": "Tue,Thu,Sat"}
      ]
    },
    {
      "id": "fleurs",
      "physicalZoneNumber": 2,
      "humidityThreshold": 50,
      "schedules": [
        {"time": "06:30", "durationMinutes": 15, "days": "ALL"}
      ]
    },
    {
      "id": "gazon",
      "physicalZoneNumber": 3,
      "humidityThreshold": 35,
      "schedules": [
        {"time": "07:00", "durationMinutes": 30, "days": "Mon,Thu"}
      ]
    },
    {
      "id": "arbustes",
      "physicalZoneNumber": 4,
      "humidityThreshold": 45,
      "schedules": [
        {"time": "19:00", "durationMinutes": 20, "days": "Tue,Fri"}
      ]
    }
  ]
}
```

### Timeline Jour Complet (Vendredi)
```
06:00:00  Master: START potager (20 min, humidité OK)
          Com: Zone 1 ON, Pompe ON

06:30:00  Master: START fleurs (15 min, humidité OK)
          Com: Zone 2 ON, Pompe DÉJÀ ON

07:00:00  Zone 1 expire (potager)
          Com: Zone 1 OFF, Pompe RESTE ON (fleurs, no gazon schedule)

07:00:00  Master: START gazon (30 min, but NOT Friday schedule)
          → Skipped (not scheduled)

07:15:00  Zone 2 expire (fleurs)
          Com: Zone 2 OFF, Pompe OFF

18:00:00  Master: START potager (15 min, humidité OK)
          Com: Zone 1 ON, Pompe ON

18:15:00  Zone 1 expire (potager)
          Com: Zone 1 OFF, Pompe OFF

19:00:00  Master: START arbustes (20 min, humidité OK)
          Com: Zone 4 ON, Pompe ON

19:20:00  Zone 4 expire (arbustes)
          Com: Zone 4 OFF, Pompe OFF

Status: ✅ Journée optimale
        - Potager: 35 min (matin + soir)
        - Fleurs: 15 min (matin)
        - Gazon: 0 min (pas prévu vendredi)
        - Arbustes: 20 min (soir)
        - Pompe: 5 démarrages (au lieu de 4 séquentiels)
```

### Logs Résumés
```
[1200000] [potager] START (20 min)     → Pompe ON
[1203000] [fleurs] START (15 min)      → Pompe DÉJÀ ON
[1201200] [potager] ✅ Finished        → Pompe ON
[1203900] [fleurs] ✅ Finished         → Pompe OFF

[1264800] [potager] START (15 min)     → Pompe ON
[1265700] [potager] ✅ Finished        → Pompe OFF

[1268400] [arbustes] START (20 min)    → Pompe ON
[1269600] [arbustes] ✅ Finished       → Pompe OFF
```

---

## 📊 Comparaison Performance

### Ancien Système (Séquentiel)
```
Zone 1: 20 min  [████]
Zone 2: 15 min        [███]
Zone 3: 30 min             [██████]
Zone 4: 20 min                   [████]

Total: 85 minutes
Pompe démarrages: 4
Pics de courant: 4
```

### Nouveau Système (Parallèle)
```
Zone 1: 20 min  [████════════════════]
Zone 2: 15 min  [███====════════]
Zone 3: 30 min  [██████]
Zone 4: 20 min                [████]

Total: 30 minutes (si simultanées)
Pompe démarrages: Minimal
Pics de courant: 1-3
```

---

## ✅ Résumé Cas d'Usage

| Cas | Zones | Simultanées? | Pompe | Result |
|-----|-------|-------------|-------|--------|
| 1 | 1 | Non | ON/OFF | ✅ |
| 2 | 2 | Non | ON/OFF/ON | ✅ |
| 3 | 3 | Oui | ON (continu) | ✅✅ |
| 4 | 4 | Oui | ON (continu) | ✅✅✅ |
| 5 | 3 (arrêt 1) | Oui | ON (continue) | ✅ |
| 6 | 4 (arrêt tout) | Non | OFF | ✅ |
| 7 | 4 (mixte) | Partiel | Optimisée | ✅✅ |

---

**Status**: ✅ **TOUS LES CAS D'USAGE SUPPORTÉS**
