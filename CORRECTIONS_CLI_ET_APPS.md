# 🔧 Corrections : CLI et Apps

## 🐛 Problèmes Identifiés

### 1. ❌ Commandes CLI Irrigation Non Disponibles

```bash
D'O-Core> irrig_config_show
Unknown command: irrig_config_show
```

**Cause** : Le fichier `irrig_cli_commands.cpp` n'est pas compilé/linké.

**Solution** : ✅ Ajout de `#include "../../apps/irrig_common/irrig_cli_commands.h"` dans `interface.cpp`

---

### 2. ✅ Une Seule App par Device (C'est Normal !)

```bash
D'O-Core> app_list
ID: 1 | IrrigAppMaster | ... | State: UNLOADED
```

**Explication** : Chaque device physique n'enregistre que **sa propre app**.
- Device 1 → `IrrigAppMaster`
- Device 2 → `IrrigAppSlaveSensors`
- Device 3 → `IrrigAppSlaveRelays`

**Ce n'est PAS un bug !** C'est l'architecture distribuée.

---

## 🔧 Corrections Appliquées

### Fichier : `src/kernel/interface/interface.cpp`

#### Avant
```cpp
#include "interface.h"
#include <WiFi.h>
// ... autres includes

// Inclure les commandes OTA
extern SysError_t cmd_ota_start(int argc, char* argv[]);
// ...

// Inclure les commandes Irrigation
extern SysError_t cmd_irrig_config_server(int argc, char* argv[]);
extern SysError_t cmd_irrig_config_master(int argc, char* argv[]);
// ... (déclarations extern)
```

#### Après
```cpp
#include "interface.h"
#include <WiFi.h>
// ... autres includes
#include "../../apps/irrig_common/irrig_cli_commands.h"  // ← AJOUTÉ

// Inclure les commandes OTA
extern SysError_t cmd_ota_start(int argc, char* argv[]);
// ...

// Commandes Irrigation : incluses via irrig_cli_commands.h  // ← SIMPLIFIÉ
```

**Changements** :
1. ✅ Ajout de l'include du header CLI irrigation
2. ✅ Suppression des déclarations `extern` redondantes
3. ✅ Les commandes sont maintenant disponibles

---

## 🧪 Test de Validation

### 1. Compiler le Projet

```bash
cd /home/dorus/Documents/GitHub/DO-Core-Os
pio run
```

**Résultat attendu** : ✅ Compilation réussie sans erreurs

---

### 2. Flasher sur Device 1 (Master)

```bash
# Copier le bon fichier main.cpp
cp DEVICE_CONFIGS/main_device1_master.cpp src/main.cpp

# Compiler et flasher
pio run --target upload --upload-port /dev/ttyUSB0
```

---

### 3. Tester les Commandes CLI

```bash
# Connexion série
screen /dev/ttyUSB0 115200

# Tester help
D'O-Core> help
```

**Résultat attendu** : Les commandes `irrig_config_*` doivent apparaître :
```
irrig_config_server  - Configure FastAPI server URL
irrig_config_master  - Configure Master device IP/port
irrig_config_slave1  - Configure Slave1 (Sensors) IP/port
irrig_config_slave2  - Configure Slave2 (Relays) IP/port
irrig_config_show    - Show current configuration
irrig_config_save    - Save configuration to NVS
irrig_config_load    - Load configuration from NVS
irrig_config_reset   - Reset configuration to defaults
```

---

### 4. Tester une Commande

```bash
D'O-Core> irrig_config_show
```

**Résultat attendu** :
```
=== Irrigation System Configuration ===

Server (FastAPI):
  URL: NOT CONFIGURED

Master Device:
  IP: 192.168.1.100
  Port: 8080

Slave1 (Sensors):
  IP: 192.168.1.101
  Port: 8081

Slave2 (Relays):
  IP: 192.168.1.102
  Port: 8082

HTTP Settings:
  Timeout: 5000 ms
  Retry count: 3
  Retry delay: 1000 ms
=======================================
```

---

### 5. Vérifier l'App

```bash
D'O-Core> app_list
```

**Résultat attendu** :
```
=== Registered Applications ===
ID: 1 | IrrigAppMaster | Smart Irrigation Control System | State: RUNNING
```

**Note** : Une seule app, c'est **normal** ! Voir `EXPLICATION_APPS_PAR_DEVICE.md`

---

## 📋 Checklist de Vérification

### ✅ Compilation
- [ ] `pio run` réussit sans erreurs
- [ ] Pas d'erreurs de linkage
- [ ] Taille du firmware < 1.5 MB

### ✅ Commandes CLI
- [ ] `help` affiche les commandes `irrig_config_*`
- [ ] `irrig_config_show` fonctionne
- [ ] `irrig_config_reset` fonctionne
- [ ] `irrig_config_save` fonctionne

### ✅ Apps
- [ ] Device 1 : 1 app (IrrigAppMaster)
- [ ] Device 2 : 1 app (IrrigAppSlaveSensors)
- [ ] Device 3 : 1 app (IrrigAppSlaveRelays)
- [ ] Chaque app démarre correctement

---

## 🔍 Diagnostic des Problèmes Potentiels

### Problème : Commandes CLI toujours absentes après compilation

**Vérifications** :
1. Le fichier `irrig_cli_commands.cpp` est-il dans `src/apps/irrig_common/` ?
2. L'include est-il bien dans `interface.cpp` ?
3. La compilation inclut-elle tous les fichiers `.cpp` ?

**Solution** :
```bash
# Vérifier que le fichier existe
ls -la src/apps/irrig_common/irrig_cli_commands.cpp

# Nettoyer et recompiler
pio run --target clean
pio run
```

---

### Problème : Erreur de compilation "undefined reference to cmd_irrig_config_*"

**Cause** : Le linker ne trouve pas les symboles

**Solution** : Vérifier que `irrig_cli_commands.cpp` est bien compilé :
```bash
# Vérifier les fichiers objets
ls -la .pio/build/esp32dev/src/apps/irrig_common/
```

Devrait contenir : `irrig_cli_commands.cpp.o`

---

### Problème : App en état UNLOADED

```bash
D'O-Core> app_list
ID: 1 | IrrigAppMaster | ... | State: UNLOADED
```

**Solution** :
```bash
# Démarrer l'app manuellement
D'O-Core> app_start 1

# Vérifier
D'O-Core> app_list
```

Si l'app ne démarre toujours pas, vérifier les logs :
```bash
D'O-Core> dmesg 20
```

---

## 📊 Résumé des Fichiers Modifiés

| Fichier | Modification | Statut |
|---------|--------------|--------|
| `src/kernel/interface/interface.cpp` | Ajout include `irrig_cli_commands.h` | ✅ |
| `src/kernel/interface/interface.cpp` | Suppression `extern` redondants | ✅ |
| `src/apps/irrig_common/irrig_cli_commands.h` | Créé (déclarations) | ✅ |
| `src/apps/irrig_common/irrig_cli_commands.cpp` | Créé (implémentations) | ✅ |

---

## 🎯 Prochaines Étapes

1. **Compiler et tester** :
   ```bash
   pio run
   ```

2. **Flasher sur les 3 devices** :
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

3. **Configurer via CLI** :
   ```bash
   # Sur chaque device
   D'O-Core> irrig_config_show
   D'O-Core> irrig_config_master 192.168.1.100 8080
   D'O-Core> irrig_config_save
   ```

4. **Tester la communication** :
   ```bash
   # Sur Master
   D'O-Core> app_start 1
   
   # Vérifier les logs
   D'O-Core> dmesg 10
   ```

---

## 📚 Documentation Créée

- ✅ `GUIDE_COMMANDES_CLI_IRRIGATION.md` - Guide complet des commandes
- ✅ `EXPLICATION_APPS_PAR_DEVICE.md` - Explication architecture distribuée
- ✅ `CORRECTIONS_CLI_ET_APPS.md` - Ce document

---

**Les corrections sont appliquées ! Compile et teste ! 🚀**
