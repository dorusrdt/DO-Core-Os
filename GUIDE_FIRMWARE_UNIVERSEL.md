# 🚀 Guide : Firmware Universel avec Sélection de Rôle

## 🎯 Concept

**Un seul firmware** pour les 3 devices, avec **3 apps enregistrées**. Le rôle est choisi via CLI après le déploiement.

---

## 📋 Architecture

```
┌─────────────────────────────────────────────────────────┐
│  FIRMWARE UNIVERSEL (flashé sur tous les devices)      │
│                                                         │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐ │
│  │ Master App   │  │ Slave1 App   │  │ Slave2 App   │ │
│  │ (ID: 1)      │  │ (ID: 2)      │  │ (ID: 3)      │ │
│  │ STOPPED      │  │ STOPPED      │  │ STOPPED      │ │
│  └──────────────┘  └──────────────┘  └──────────────┘ │
│                                                         │
│  Configuration via CLI :                                │
│  - irrig_set_role <master|slave1|slave2>               │
│  - irrig_activate_role                                 │
│  - irrig_config_save                                   │
└─────────────────────────────────────────────────────────┘
```

---

## 🔄 Workflow de Déploiement

### Étape 1 : Compiler le Firmware Universel

```bash
cd /home/dorus/Documents/GitHub/DO-Core-Os

# Copier le firmware universel
cp DEVICE_CONFIGS/main_universal.cpp src/main.cpp

# Compiler
pio run
```

---

### Étape 2 : Flasher sur les 3 Devices

```bash
# Device 1
pio run --target upload --upload-port /dev/ttyUSB0

# Device 2
pio run --target upload --upload-port /dev/ttyUSB1

# Device 3
pio run --target upload --upload-port /dev/ttyUSB2
```

**Résultat** : Les 3 devices ont le **même firmware** !

---

### Étape 3 : Configurer Device 1 (Master)

```bash
# Connexion série
screen /dev/ttyUSB0 115200

# Vérifier les apps enregistrées
D'O-Core> app_list
=== Registered Applications ===
ID: 1 | IrrigAppMaster        | State: STOPPED
ID: 2 | IrrigAppSlaveSensors  | State: STOPPED
ID: 3 | IrrigAppSlaveRelays   | State: STOPPED

# Définir le rôle Master
D'O-Core> irrig_set_role master
Device role set to: MASTER
Use 'irrig_config_save' to persist this configuration
Use 'irrig_activate_role' to start the corresponding app

# Activer le rôle
D'O-Core> irrig_activate_role
Activating role: MASTER (App ID: 1)
SUCCESS: MASTER app started

# Vérifier
D'O-Core> app_list
=== Registered Applications ===
ID: 1 | IrrigAppMaster        | State: RUNNING   ← Actif !
ID: 2 | IrrigAppSlaveSensors  | State: STOPPED
ID: 3 | IrrigAppSlaveRelays   | State: STOPPED

# Sauvegarder la configuration
D'O-Core> irrig_config_save
Configuration saved successfully!
```

---

### Étape 4 : Configurer Device 2 (Slave Sensors)

```bash
# Connexion série
screen /dev/ttyUSB1 115200

# Définir le rôle Slave1
D'O-Core> irrig_set_role slave1
Device role set to: SLAVE1 (Sensors)

# Activer
D'O-Core> irrig_activate_role
Activating role: SLAVE1 (Sensors) (App ID: 2)
SUCCESS: SLAVE1 (Sensors) app started

# Vérifier
D'O-Core> app_list
=== Registered Applications ===
ID: 1 | IrrigAppMaster        | State: STOPPED
ID: 2 | IrrigAppSlaveSensors  | State: RUNNING   ← Actif !
ID: 3 | IrrigAppSlaveRelays   | State: STOPPED

# Sauvegarder
D'O-Core> irrig_config_save
```

---

### Étape 5 : Configurer Device 3 (Slave Relays)

```bash
# Connexion série
screen /dev/ttyUSB2 115200

# Définir le rôle Slave2
D'O-Core> irrig_set_role slave2
Device role set to: SLAVE2 (Relays)

# Activer
D'O-Core> irrig_activate_role
Activating role: SLAVE2 (Relays) (App ID: 3)
SUCCESS: SLAVE2 (Relays) app started

# Vérifier
D'O-Core> app_list
=== Registered Applications ===
ID: 1 | IrrigAppMaster        | State: STOPPED
ID: 2 | IrrigAppSlaveSensors  | State: STOPPED
ID: 3 | IrrigAppSlaveRelays   | State: RUNNING   ← Actif !

# Sauvegarder
D'O-Core> irrig_config_save
```

---

## 🎮 Commandes CLI Disponibles

### Gestion du Rôle

| Commande | Description | Exemple |
|----------|-------------|---------|
| `irrig_set_role <role>` | Définir le rôle du device | `irrig_set_role master` |
| `irrig_get_role` | Afficher le rôle actuel | `irrig_get_role` |
| `irrig_activate_role` | Activer l'app du rôle | `irrig_activate_role` |

**Rôles valides** : `master`, `slave1`, `slave2`

---

### Gestion des Apps

| Commande | Description | Exemple |
|----------|-------------|---------|
| `app_list` | Lister toutes les apps | `app_list` |
| `app_start <id>` | Démarrer une app | `app_start 1` |
| `app_stop <id>` | Arrêter une app | `app_stop 1` |
| `app_restart <id>` | Redémarrer une app | `app_restart 1` |

---

### Configuration Réseau

| Commande | Description | Exemple |
|----------|-------------|---------|
| `irrig_config_show` | Afficher config réseau | `irrig_config_show` |
| `irrig_config_master <ip> <port>` | Config Master | `irrig_config_master 192.168.1.100 8080` |
| `irrig_config_slave1 <ip> <port>` | Config Slave1 | `irrig_config_slave1 192.168.1.101 8081` |
| `irrig_config_slave2 <ip> <port>` | Config Slave2 | `irrig_config_slave2 192.168.1.102 8082` |
| `irrig_config_save` | Sauvegarder config | `irrig_config_save` |

---

## 🔄 Démarrage Automatique

Après configuration et sauvegarde, le rôle est **activé automatiquement** au redémarrage :

```
=== D'O-Core Ready - UNIVERSAL FIRMWARE ===

📋 All apps registered:
  ID 1: IrrigAppMaster
  ID 2: IrrigAppSlaveSensors
  ID 3: IrrigAppSlaveRelays

🚀 Auto-activating role: MASTER
Activating role: MASTER (App ID: 1)
SUCCESS: MASTER app started
```

---

## 📊 Exemple Complet : Configuration 3 Devices

### 1️⃣ Device 1 (Master)

```bash
# Flasher firmware
pio run --target upload --upload-port /dev/ttyUSB0

# Configurer
screen /dev/ttyUSB0 115200
D'O-Core> wifi_save MySSID MyPassword
D'O-Core> irrig_set_role master
D'O-Core> irrig_config_slave1 192.168.1.101 8081
D'O-Core> irrig_config_slave2 192.168.1.102 8082
D'O-Core> irrig_activate_role
D'O-Core> irrig_config_save

# Vérifier
D'O-Core> app_list
ID: 1 | IrrigAppMaster | State: RUNNING ✓
```

---

### 2️⃣ Device 2 (Slave Sensors)

```bash
# Flasher firmware (même firmware !)
pio run --target upload --upload-port /dev/ttyUSB1

# Configurer
screen /dev/ttyUSB1 115200
D'O-Core> wifi_save MySSID MyPassword
D'O-Core> irrig_set_role slave1
D'O-Core> irrig_config_master 192.168.1.100 8080
D'O-Core> irrig_activate_role
D'O-Core> irrig_config_save

# Vérifier
D'O-Core> app_list
ID: 2 | IrrigAppSlaveSensors | State: RUNNING ✓
```

---

### 3️⃣ Device 3 (Slave Relays)

```bash
# Flasher firmware (même firmware !)
pio run --target upload --upload-port /dev/ttyUSB2

# Configurer
screen /dev/ttyUSB2 115200
D'O-Core> wifi_save MySSID MyPassword
D'O-Core> irrig_set_role slave2
D'O-Core> irrig_config_master 192.168.1.100 8080
D'O-Core> irrig_activate_role
D'O-Core> irrig_config_save

# Vérifier
D'O-Core> app_list
ID: 3 | IrrigAppSlaveRelays | State: RUNNING ✓
```

---

## 🔧 Changer le Rôle d'un Device

Tu peux **changer le rôle** d'un device sans recompiler :

```bash
# Arrêter l'app actuelle
D'O-Core> app_stop 1

# Changer de rôle
D'O-Core> irrig_set_role slave1

# Activer le nouveau rôle
D'O-Core> irrig_activate_role

# Sauvegarder
D'O-Core> irrig_config_save
```

---

## 🐛 Dépannage

### Problème : "App for role not registered"

```bash
D'O-Core> irrig_activate_role
ERROR: App for role MASTER not registered!
```

**Cause** : Les apps ne sont pas enregistrées dans `main.cpp`

**Solution** : Vérifier que `main_universal.cpp` est bien utilisé

---

### Problème : Rôle non persistant après redémarrage

```bash
# Après reboot
⚠️  No role configured. Use 'irrig_set_role' to configure.
```

**Cause** : Configuration non sauvegardée

**Solution** :
```bash
D'O-Core> irrig_set_role master
D'O-Core> irrig_config_save  ← Important !
```

---

### Problème : Conflit GPIO entre apps

**Cause** : Plusieurs apps actives en même temps

**Solution** : `irrig_activate_role` arrête automatiquement les autres apps

---

## 📈 Avantages de cette Approche

| Avantage | Description |
|----------|-------------|
| ✅ **Un seul firmware** | Pas besoin de 3 compilations différentes |
| ✅ **Flexibilité** | Changer le rôle sans recompiler |
| ✅ **Déploiement rapide** | Flasher le même firmware partout |
| ✅ **Test facile** | Tester tous les rôles sur un seul device |
| ✅ **Maintenance** | Une seule version de firmware à maintenir |

---

## 📝 Checklist de Déploiement

### Avant le Déploiement
- [ ] Compiler `main_universal.cpp`
- [ ] Vérifier taille firmware < 1.5 MB
- [ ] Préparer credentials WiFi

### Device 1 (Master)
- [ ] Flasher firmware universel
- [ ] Configurer WiFi
- [ ] `irrig_set_role master`
- [ ] Configurer IPs Slaves
- [ ] `irrig_activate_role`
- [ ] `irrig_config_save`
- [ ] Vérifier : `app_list` → ID 1 RUNNING

### Device 2 (Slave Sensors)
- [ ] Flasher firmware universel
- [ ] Configurer WiFi
- [ ] `irrig_set_role slave1`
- [ ] Configurer IP Master
- [ ] `irrig_activate_role`
- [ ] `irrig_config_save`
- [ ] Vérifier : `app_list` → ID 2 RUNNING

### Device 3 (Slave Relays)
- [ ] Flasher firmware universel
- [ ] Configurer WiFi
- [ ] `irrig_set_role slave2`
- [ ] Configurer IP Master
- [ ] `irrig_activate_role`
- [ ] `irrig_config_save`
- [ ] Vérifier : `app_list` → ID 3 RUNNING

### Test Final
- [ ] Tous les devices connectés au WiFi
- [ ] Communication HTTP fonctionnelle
- [ ] Données capteurs reçues par Master
- [ ] Commandes irrigation exécutées par Slave2

---

## 🎓 Comparaison : Avant vs Après

### ❌ Avant (3 Firmwares Différents)

```bash
# Device 1
cp DEVICE_CONFIGS/main_device1_master.cpp src/main.cpp
pio run --target upload

# Device 2
cp DEVICE_CONFIGS/main_device2_slave_sensors.cpp src/main.cpp
pio run --target upload

# Device 3
cp DEVICE_CONFIGS/main_device3_slave_relays.cpp src/main.cpp
pio run --target upload
```

**Problèmes** :
- 3 compilations nécessaires
- Risque d'erreur (mauvais firmware sur mauvais device)
- Maintenance complexe

---

### ✅ Après (1 Firmware Universel)

```bash
# Compiler une seule fois
cp DEVICE_CONFIGS/main_universal.cpp src/main.cpp
pio run

# Flasher partout
pio run --target upload --upload-port /dev/ttyUSB0
pio run --target upload --upload-port /dev/ttyUSB1
pio run --target upload --upload-port /dev/ttyUSB2

# Configurer via CLI
# Device 1: irrig_set_role master
# Device 2: irrig_set_role slave1
# Device 3: irrig_set_role slave2
```

**Avantages** :
- 1 seule compilation
- Impossible de se tromper de firmware
- Configuration flexible via CLI

---

**Le firmware universel est prêt ! Compile et déploie ! 🚀**
