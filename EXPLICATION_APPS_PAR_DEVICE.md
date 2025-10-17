# 📱 Explication : Une App par Device

## 🎯 Pourquoi Chaque Device N'Enregistre Qu'Une Seule App ?

### ✅ C'est Normal et Voulu !

Chaque ESP32 physique exécute **un seul rôle** dans le système d'irrigation :

```
┌─────────────────────────────────────────────────────────────┐
│  ESP32 #1 (Device 1)                                        │
│  ┌──────────────────────────────────────────────────────┐   │
│  │  App: IrrigAppMaster                                 │   │
│  │  Rôle: Orchestration, décisions, communication       │   │
│  └──────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│  ESP32 #2 (Device 2)                                        │
│  ┌──────────────────────────────────────────────────────┐   │
│  │  App: IrrigAppSlaveSensors                           │   │
│  │  Rôle: Lecture capteurs ADC, envoi données          │   │
│  └──────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│  ESP32 #3 (Device 3)                                        │
│  ┌──────────────────────────────────────────────────────┐   │
│  │  App: IrrigAppSlaveRelays                            │   │
│  │  Rôle: Contrôle relais GPIO, exécution irrigation   │   │
│  └──────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

---

## 🔍 Vérification : Commande `app_list`

### Device 1 (Master)
```bash
D'O-Core> app_list
=== Registered Applications ===
ID: 1 | IrrigAppMaster | Smart Irrigation Control System | State: RUNNING
```
✅ **Normal** : Le Master n'a besoin que de son app

---

### Device 2 (Slave Sensors)
```bash
D'O-Core> app_list
=== Registered Applications ===
ID: 1 | IrrigAppSlaveSensors | Sensor Reading System | State: RUNNING
```
✅ **Normal** : Le Slave Sensors n'a besoin que de son app

---

### Device 3 (Slave Relays)
```bash
D'O-Core> app_list
=== Registered Applications ===
ID: 1 | IrrigAppSlaveRelays | Relay Control System | State: RUNNING
```
✅ **Normal** : Le Slave Relays n'a besoin que de son app

---

## 🏗️ Architecture Distribuée

### Principe de Séparation des Responsabilités

Chaque ESP32 est **spécialisé** dans une tâche :

| Device | App | Responsabilité | Hardware |
|--------|-----|----------------|----------|
| **Device 1** | `IrrigAppMaster` | Orchestration, décisions, API | WiFi, HTTP |
| **Device 2** | `IrrigAppSlaveSensors` | Lecture capteurs | 12x ADC, BME280 |
| **Device 3** | `IrrigAppSlaveRelays` | Contrôle relais | 4x GPIO relais, 1x pompe |

---

## 📂 Configuration dans `main.cpp`

### Device 1 (Master) - `main_device1_master.cpp`

```cpp
// Enregistrer UNIQUEMENT le Master
if (register_irrig_app_master(&master_config) == SYS_OK) {
    Serial.println("Master app registered");
}
```

**Pourquoi pas les Slaves ?**
- Le Master ne lit pas les capteurs directement
- Le Master ne contrôle pas les relais directement
- Il **communique** avec les Slaves via HTTP

---

### Device 2 (Slave Sensors) - `main_device2_slave_sensors.cpp`

```cpp
// Enregistrer UNIQUEMENT le Slave Sensors
if (register_irrig_app_slave_sensors(&sensor_config) == SYS_OK) {
    Serial.println("Slave Sensors app registered");
}
```

**Pourquoi pas le Master ni Slave Relays ?**
- Ce device est dédié à la lecture des capteurs
- Il n'a pas besoin de logique d'irrigation
- Il n'a pas de relais connectés

---

### Device 3 (Slave Relays) - `main_device3_slave_relays.cpp`

```cpp
// Enregistrer UNIQUEMENT le Slave Relays
if (register_irrig_app_slave_relays(&relay_config) == SYS_OK) {
    Serial.println("Slave Relays app registered");
}
```

**Pourquoi pas le Master ni Slave Sensors ?**
- Ce device est dédié au contrôle des relais
- Il n'a pas de capteurs connectés
- Il exécute les commandes reçues du Master

---

## 🔄 Communication Entre Apps

Les apps communiquent via **HTTP**, pas via l'App Manager :

```
┌─────────────────┐
│  Master App     │
│  (Device 1)     │
└────┬────────┬───┘
     │        │
     │ HTTP   │ HTTP
     │ POST   │ POST
     │        │
┌────▼────┐  ┌▼──────────┐
│ Sensors │  │  Relays   │
│  App    │  │   App     │
│(Device2)│  │ (Device3) │
└─────────┘  └───────────┘
```

---

## ❌ Erreur Commune : Enregistrer Toutes les Apps sur Tous les Devices

### ❌ MAUVAIS (Ne PAS faire)
```cpp
// Sur Device 1
register_irrig_app_master(&master_config);
register_irrig_app_slave_sensors(&sensor_config);  // ❌ ERREUR !
register_irrig_app_slave_relays(&relay_config);    // ❌ ERREUR !
```

**Problèmes** :
1. Conflit GPIO (les apps essaient d'utiliser les mêmes pins)
2. Gaspillage de mémoire (apps inutiles)
3. Confusion dans les logs
4. Comportement imprévisible

---

### ✅ CORRECT
```cpp
// Sur Device 1 : UNIQUEMENT Master
register_irrig_app_master(&master_config);

// Sur Device 2 : UNIQUEMENT Slave Sensors
register_irrig_app_slave_sensors(&sensor_config);

// Sur Device 3 : UNIQUEMENT Slave Relays
register_irrig_app_slave_relays(&relay_config);
```

---

## 🧪 Test de Vérification

### Sur chaque device, vérifier qu'une seule app est enregistrée :

```bash
D'O-Core> app_list
```

**Résultat attendu** : **1 seule app** par device

---

## 🔧 Dépannage

### Problème : "Aucune app enregistrée"

```bash
D'O-Core> app_list
=== Registered Applications ===
(vide)
```

**Causes possibles** :
1. Mauvais fichier `main.cpp` compilé
2. Erreur dans `register_irrig_app_*()` (vérifier logs)
3. App Manager non initialisé

**Solution** :
```bash
# Vérifier que le bon fichier est copié
cp DEVICE_CONFIGS/main_device1_master.cpp src/main.cpp
pio run --target upload
```

---

### Problème : "App en état UNLOADED"

```bash
D'O-Core> app_list
ID: 1 | IrrigAppMaster | ... | State: UNLOADED
```

**Cause** : L'app est enregistrée mais pas démarrée

**Solution** :
```bash
D'O-Core> app_start 1
D'O-Core> app_list
```

---

## 📊 Tableau Récapitulatif

| Device | Fichier Config | App Enregistrée | État Attendu |
|--------|---------------|-----------------|--------------|
| Device 1 | `main_device1_master.cpp` | `IrrigAppMaster` | RUNNING |
| Device 2 | `main_device2_slave_sensors.cpp` | `IrrigAppSlaveSensors` | RUNNING |
| Device 3 | `main_device3_slave_relays.cpp` | `IrrigAppSlaveRelays` | RUNNING |

---

## 🎓 Analogie

Pense à une entreprise :

```
┌─────────────────────────────────────────────┐
│  PDG (Master)                               │
│  - Prend les décisions                      │
│  - Coordonne les équipes                    │
│  - Communique avec les clients (serveur)    │
└─────────────────────────────────────────────┘
         │
         ├─────────────────┬──────────────────┐
         │                 │                  │
┌────────▼────────┐  ┌─────▼──────────┐  ┌───▼──────────┐
│  Équipe Capteurs│  │ Équipe Relais  │  │   Serveur    │
│  (Device 2)     │  │  (Device 3)    │  │   FastAPI    │
│  - Lit données  │  │ - Exécute      │  │ - Stocke     │
│  - Envoie au PDG│  │   commandes    │  │ - Dashboard  │
└─────────────────┘  └────────────────┘  └──────────────┘
```

Le PDG (Master) ne fait pas le travail des équipes, il **coordonne** !

---

## ✅ Checklist de Validation

- [ ] Device 1 : `app_list` montre **1 app** (IrrigAppMaster)
- [ ] Device 2 : `app_list` montre **1 app** (IrrigAppSlaveSensors)
- [ ] Device 3 : `app_list` montre **1 app** (IrrigAppSlaveRelays)
- [ ] Chaque app est en état **RUNNING**
- [ ] Les commandes CLI `irrig_config_*` sont disponibles sur tous les devices
- [ ] Communication HTTP fonctionne entre devices

---

**C'est normal d'avoir une seule app par device ! C'est l'architecture distribuée ! 🚀**
