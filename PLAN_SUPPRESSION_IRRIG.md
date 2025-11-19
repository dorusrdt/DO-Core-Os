# 🗑️ PLAN DE SUPPRESSION COMPLÈTE DES APPLICATIONS D'IRRIGATION

## 📋 FICHIERS À SUPPRIMER

### 1. Répertoires complets

```
src/apps/irrig_app_master/              (Complètement)
src/apps/irrig_app_slave_sensors/       (Complètement)
src/apps/irrig_app_slave_relays/        (Complètement)
src/apps/irrig_common/                  (Complètement)
DEVICE_CONFIGS/                         (Complètement - configs spécifiques irrig)
```

### 2. Fichiers à modifier

```
src/main.cpp                            (Supprimer includes et code irrig)
src/kernel/interface/interface.h        (Supprimer commandes irrig)
src/kernel/interface/interface.cpp      (Supprimer commandes irrig)
```

---

## 🔍 ANALYSE DES DÉPENDANCES

### Dans src/main.cpp

**Includes à supprimer** (lignes 24-29):
```cpp
#include "apps/irrig_app_master/irrig_app_master.h"
#include "apps/irrig_app_master/irrig_app_master_http.h"
#include "apps/irrig_app_slave_sensors/irrig_app_slave_sensors.h"
#include "apps/irrig_app_slave_relays/irrig_app_slave_relays.h"
#include "apps/irrig_common/irrig_communication.h"
#include "apps/irrig_common/irrig_cli_commands.h"
```

**Variables globales à supprimer**:
```cpp
static uint8_t g_master_app_id = 0;
static uint8_t g_slave1_app_id = 0;
static uint8_t g_slave2_app_id = 0;
static float g_received_moisture[12];
static float g_received_temperature = 0;
static float g_received_humidity = 0;
static float g_received_pressure = 0;
```

**Callbacks à supprimer**:
```cpp
void on_sensor_data_received(SensorDataPacket_t* data)
void on_irrigation_status_received(IrrigationStatusPacket_t* status)
void on_command_received_slave(IrrigationCommandPacket_t* cmd)
```

**Code dans setup() à supprimer**:
- Lignes ~414-422: Callbacks
- Lignes ~782-793: ESP-NOW init
- Lignes ~799-857: Enregistrement des 3 apps
- Lignes ~882-883: Enregistrement des callbacks
- Lignes ~900-950: Activation des rôles

**Code dans setup() à modifier**:
- Lignes ~759: Rôles hardcodés (#define DEVICE_ROLE_*)
- Lignes ~770-780: Logs de rôles

**Sections à supprimer**:
- "===== CALLBACKS POUR MASTER HTTP ====="
- "===== LOGO SYSTÈME ====="
- "===== INITIALISER COMMUNICATION ESP-NOW ====="
- "===== VERSION SIMPLIFIÉE : RÔLES CODÉS EN DUR ====="
- "===== ENREGISTRER LES 3 APPS ====="
- "===== DÉMARRER LES APPS ET ENREGISTRER LES CALLBACKS SELON LE RÔLE ====="

### Dans src/kernel/interface/interface.h

**Commandes à supprimer**:
```cpp
SysError_t cmd_irrig_config_server(int argc, char* argv[]);
SysError_t cmd_irrig_config_show(int argc, char* argv[]);
SysError_t cmd_irrig_config_save(int argc, char* argv[]);
SysError_t cmd_irrig_config_load(int argc, char* argv[]);
SysError_t cmd_irrig_config_reset(int argc, char* argv[]);
SysError_t cmd_irrig_set_role(int argc, char* argv[]);
SysError_t cmd_irrig_get_role(int argc, char* argv[]);
SysError_t cmd_irrig_activate_role(int argc, char* argv[]);
```

### Dans src/kernel/interface/interface.cpp

**Commandes à supprimer**:
- Implémentations de toutes les commandes irrig_*
- Enregistrements dans interface_register_command()

---

## 🗂️ STRUCTURE APRÈS SUPPRESSION

```
src/
├── main.cpp                          (Nettoyé)
├── kernel/
│   ├── core/                         (Inchangé)
│   ├── hal/                          (Inchangé)
│   ├── network/                      (Inchangé)
│   ├── app/                          (Inchangé)
│   └── interface/                    (Nettoyé)
└── apps/
    └── example_app/                  (Exemple conservé)
```

---

## 📊 IMPACT

### Avant suppression
- 4 répertoires d'apps (1 master + 2 slaves + 1 common)
- ~2000 lignes de code irrig
- ~50 commandes CLI irrig
- ~500 KB de code

### Après suppression
- 1 répertoire d'apps (example_app)
- ~0 lignes de code irrig
- ~0 commandes CLI irrig
- ~100 KB de code

### Réduction
- 75% du code supprimé
- 80% des commandes CLI supprimées
- 80% de l'espace disque libéré

---

## ✅ CHECKLIST DE SUPPRESSION

### Phase 1: Sauvegarde
- [ ] Créer branche git: `git checkout -b cleanup/remove-irrig-apps`
- [ ] Créer archive: `tar -czf irrig_apps_backup.tar.gz src/apps/irrig_*`

### Phase 2: Suppression des répertoires
- [ ] `rm -rf src/apps/irrig_app_master/`
- [ ] `rm -rf src/apps/irrig_app_slave_sensors/`
- [ ] `rm -rf src/apps/irrig_app_slave_relays/`
- [ ] `rm -rf src/apps/irrig_common/`
- [ ] `rm -rf DEVICE_CONFIGS/`

### Phase 3: Nettoyage src/main.cpp
- [ ] Supprimer includes irrig (lignes 24-29)
- [ ] Supprimer variables globales irrig
- [ ] Supprimer callbacks irrig
- [ ] Supprimer code ESP-NOW init
- [ ] Supprimer enregistrement des 3 apps
- [ ] Supprimer activation des rôles
- [ ] Supprimer #define DEVICE_ROLE_*
- [ ] Simplifier setup()

### Phase 4: Nettoyage interface
- [ ] Supprimer commandes irrig de interface.h
- [ ] Supprimer implémentations de interface.cpp
- [ ] Supprimer enregistrements de commandes

### Phase 5: Vérification
- [ ] Compiler: `pio run`
- [ ] Vérifier pas d'erreurs de compilation
- [ ] Vérifier pas de références irrig restantes: `grep -r "irrig" src/`
- [ ] Vérifier structure des fichiers

### Phase 6: Commit
- [ ] `git add -A`
- [ ] `git commit -m "cleanup: remove irrigation apps and dependencies"`
- [ ] `git push origin cleanup/remove-irrig-apps`

---

## 🔧 COMMANDES DE SUPPRESSION

### Supprimer répertoires
```bash
cd /home/dorus/Documents/GitHub/DO-Core-Os
rm -rf src/apps/irrig_app_master/
rm -rf src/apps/irrig_app_slave_sensors/
rm -rf src/apps/irrig_app_slave_relays/
rm -rf src/apps/irrig_common/
rm -rf DEVICE_CONFIGS/
```

### Vérifier références restantes
```bash
grep -r "irrig" src/ --include="*.h" --include="*.cpp"
grep -r "DEVICE_ROLE" src/ --include="*.h" --include="*.cpp"
grep -r "SensorDataPacket" src/ --include="*.h" --include="*.cpp"
grep -r "IrrigationCommand" src/ --include="*.h" --include="*.cpp"
```

### Vérifier compilation
```bash
pio run
```

---

## 📝 NOTES

1. **Sauvegarde**: Les fichiers seront supprimés définitivement. Créer une sauvegarde avant.
2. **Git**: Utiliser une branche pour pouvoir revenir en arrière si nécessaire.
3. **Compilation**: Vérifier que le projet compile après chaque phase.
4. **Tests**: Tester les commandes CLI restantes après suppression.
5. **Documentation**: Mettre à jour README.md après suppression.

---

## 🎯 RÉSULTAT FINAL

Un projet D'O-Core OS **propre et minimaliste** avec:
- ✅ Kernel robuste et modulaire
- ✅ Framework d'applications générique
- ✅ Pas de code spécifique à l'irrigation
- ✅ Base solide pour nouvelles applications
- ✅ Facile à maintenir et étendre

