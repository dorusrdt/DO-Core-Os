# ESP-NOW Auto-Detection avec IDs Uniques - Design Détaillé

## 1. SYSTÈME D'IDENTIFICATION

### 1.1 Structure des IDs

```
┌─────────────────────────────────────────────────────────────┐
│ DEVICE_ID (16 bits) = Identifiant unique global             │
│ ├─ Généré depuis MAC address (2 derniers octets)            │
│ ├─ Unique par device (jamais deux devices avec même ID)     │
│ └─ Exemple: MAC AA:BB:CC:DD:EE:FF → device_id = 0xEEFF     │
├─────────────────────────────────────────────────────────────┤
│ MASTER_ID (8 bits) = Identifiant du groupe/master           │
│ ├─ Plage: 1-255 (0 réservé)                                 │
│ ├─ Tous les slaves d'un master ont le même master_id        │
│ ├─ Permet de supporter plusieurs masters indépendants       │
│ └─ Exemple: Master_Lab=1, Master_Garage=2                   │
├─────────────────────────────────────────────────────────────┤
│ DEVICE_INDEX (8 bits) = Rôle dans le groupe                 │
│ ├─ 0 = Master                                               │
│ ├─ 1-254 = Slaves (index du slave dans le groupe)           │
│ └─ Exemple: Master=0, Sensor1=1, Sensor2=2                  │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 Configuration stockée en NVS

```c
typedef struct {
    uint16_t device_id;         // Basé sur MAC (unique)
    uint8_t master_id;          // ID du master (1-255)
    uint8_t device_index;       // Rôle (0=master, 1+=slave)
    uint32_t config_version;    // Version de la config (pour migrations)
} DeviceConfig_t;
```

---

## 2. FLUX D'AUTO-DÉTECTION AVEC IDs

### 2.1 Scénario : Découverte d'un Slave par le Master

```
ÉTAPE 1 : MASTER ENVOIE BEACON
┌─────────────────────────────────────────────────────��───────┐
│ Master (device_id=0xAABB, master_id=1, index=0)             │
│ Envoie BeaconPacket en BROADCAST toutes les 500ms :         │
│                                                              │
│ {                                                            │
│   frame_type: FRAME_BEACON,                                 │
│   version: 1,                                                │
│   master_id: 1,              ← CLEF : ID du groupe          │
│   master_mac: [AA:BB:CC:DD:EE:FF],                          │
│   device_id: 0xAABB,         ← Identifiant unique du master │
│   channel: 6,                                                │
│   beacon_id: 12345,                                          │
│   uptime_ms: 123456                                          │
│ }                                                            │
└─────────────────────────────────────────────────────────────┘

ÉTAPE 2 : SLAVE REÇOIT ET FILTRE
┌─────────────────────────────────────────────────────────���───┐
│ Slave (device_id=0xCCDD, master_id=1, index=1)              │
│ Scanne les canaux et reçoit le beacon                       │
│                                                              │
│ Vérifications :                                              │
│ ✓ frame_type == FRAME_BEACON ?                              │
│ ✓ version == 1 ?                                            │
│ ✓ beacon.master_id == config.master_id ? (1 == 1)          │
│ ✓ beacon.device_id != config.device_id ? (0xAABB != 0xCCDD)│
│                                                              │
│ Résultat : BEACON VALIDE → Traiter                          │
└─────────────────────────────────────────────────────────────┘

ÉTAPE 3 : SLAVE AJOUTE MASTER COMME PEER
┌─────────────────────────────────────────────────────────────┐
│ Slave sauvegarde :                                           │
│ {                                                            │
│   master_mac: [AA:BB:CC:DD:EE:FF],                          │
│   master_device_id: 0xAABB,                                 │
│   channel: 6,                                                │
│   last_beacon_id: 12345,                                    │
│   last_beacon_ticks: now                                    │
│ }                                                            │
│                                                              │
│ Ajoute master comme peer ESP-NOW                            │
│ Aligne le canal à 6                                         │
│ Log: "Master discovered: device_id=0xAABB on channel 6"     │
└─────────────────────────────────────────────────────────────┘

ÉTAPE 4 : SLAVE ENVOIE DATA
┌─────────────────────────────────────────────────────────────┐
│ Slave envoie DataPacket au master :                         │
│                                                              │
│ {                                                            │
│   frame_type: FRAME_DATA,                                   │
│   version: 1,                                                │
│   slave_id: 0xCCDD,          ← Identifiant du slave         │
│   master_id: 1,              ← Vérification du groupe       │
│   seq: 1,                                                    │
│   timestamp: 456789,                                        │
│   humidity: [...]                                           │
│ }                                                            │
└─────────────────────────────────────────────────────────────┘

ÉTAPE 5 : MASTER REÇOIT ET VALIDE
┌─────────────────────────────────────────────────────────────┐
│ Master reçoit DataPacket                                    │
│                                                              │
│ Vérifications :                                              │
│ ✓ frame_type == FRAME_DATA ?                                │
│ ✓ version == 1 ?                                            │
│ ✓ data.master_id == config.master_id ? (1 == 1)            │
│ ✓ data.slave_id != config.device_id ? (0xCCDD != 0xAABB)   │
│                                                              │
│ Résultat : DATA VALIDE → Traiter                            │
│ Ajouter/mettre à jour SlaveInfo pour device_id=0xCCDD      │
│ Log: "Data from slave 0xCCDD: seq=1"                        │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 Filtrage par master_id

```
SCÉNARIO : Plusieurs masters indépendants

┌─────────────────────────────────────────────────────────────┐
│ MASTER_1 (master_id=1)                                      │
│ ├─ Envoie beacons avec master_id=1                          │
│ └─ Reçoit data avec master_id=1                             │
│                                                              │
│ MASTER_2 (master_id=2)                                      │
│ ├─ Envoie beacons avec master_id=2                          │
│ └─ Reçoit data avec master_id=2                             │
│                                                              │
│ SLAVE_1 (master_id=1)                                       │
│ ├─ Écoute beacons avec master_id=1 ✓                        │
│ ├─ Ignore beacons avec master_id=2 ✗                        │
│ └─ Envoie data avec master_id=1                             │
│                                                              │
│ SLAVE_2 (master_id=2)                                       │
│ ├─ Écoute beacons avec master_id=2 ✓                        │
│ ├─ Ignore beacons avec master_id=1 ✗                        │
│ └─ Envoie data avec master_id=2                             │
└─────────────────────────────────────────────────────────────┘

Résultat : Deux réseaux ESP-NOW complètement indépendants
```

---

## 3. RECONFIGURATION DYNAMIQUE VIA CLI

### 3.1 Commandes CLI proposées

```bash
# Afficher la configuration actuelle
espnow config show

# Changer le master_id (reconfiguration dynamique)
espnow config set-master-id <new_master_id>

# Changer le device_index (rôle)
espnow config set-device-index <new_index>

# Réinitialiser la configuration
espnow config reset

# Afficher les slaves connectés (master uniquement)
espnow slaves list

# Afficher le master découvert (slave uniquement)
espnow master show

# Afficher les statistiques
espnow stats
```

### 3.2 Implémentation de la reconfiguration

```c
// ============ espnow_config.h ============

typedef struct {
    uint16_t device_id;         // Basé sur MAC (read-only)
    uint8_t master_id;          // Configurable
    uint8_t device_index;       // Configurable
    uint32_t config_version;    // Version
} DeviceConfig_t;

// Charger la config depuis NVS
SysError_t espnow_config_load(DeviceConfig_t* config);

// Sauvegarder la config en NVS
SysError_t espnow_config_save(const DeviceConfig_t* config);

// Changer le master_id et recharger l'app
SysError_t espnow_config_set_master_id(uint8_t new_master_id);

// Changer le device_index et recharger l'app
SysError_t espnow_config_set_device_index(uint8_t new_index);

// Réinitialiser la config
SysError_t espnow_config_reset();

// ============ espnow_config.cpp ============

static const char* NVS_NAMESPACE = "espnow_cfg";

SysError_t espnow_config_load(DeviceConfig_t* config) {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, true);  // read-only
    
    if (!prefs.isKey("device_id")) {
        kernel_log(LOG_LEVEL_WARN, "Config not found, initializing...");
        prefs.end();
        
        // Générer device_id depuis MAC
        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        config->device_id = (mac[4] << 8) | mac[5];
        config->master_id = 1;      // Défaut
        config->device_index = 0;   // Défaut (master)
        config->config_version = 1;
        
        // Sauvegarder
        return espnow_config_save(config);
    }
    
    config->device_id = prefs.getUShort("device_id");
    config->master_id = prefs.getUChar("master_id");
    config->device_index = prefs.getUChar("device_index");
    config->config_version = prefs.getUInt("config_version");
    
    prefs.end();
    return SYS_OK;
}

SysError_t espnow_config_save(const DeviceConfig_t* config) {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);  // read-write
    
    prefs.putUShort("device_id", config->device_id);
    prefs.putUChar("master_id", config->master_id);
    prefs.putUChar("device_index", config->device_index);
    prefs.putUInt("config_version", config->config_version);
    
    prefs.end();
    
    kernel_log(LOG_LEVEL_INFO, "Config saved: master_id=%d, device_index=%d",
              config->master_id, config->device_index);
    
    return SYS_OK;
}

SysError_t espnow_config_set_master_id(uint8_t new_master_id) {
    if (new_master_id == 0 || new_master_id > 255) {
        kernel_log(LOG_LEVEL_ERROR, "Invalid master_id: %d (must be 1-255)", new_master_id);
        return SYS_ERROR;
    }
    
    DeviceConfig_t config;
    if (espnow_config_load(&config) != SYS_OK) {
        return SYS_ERROR;
    }
    
    uint8_t old_master_id = config.master_id;
    config.master_id = new_master_id;
    
    if (espnow_config_save(&config) != SYS_OK) {
        return SYS_ERROR;
    }
    
    kernel_log(LOG_LEVEL_INFO, "Master ID changed: %d → %d (restart required)",
              old_master_id, new_master_id);
    
    // Redémarrer l'app ESP-NOW pour appliquer la nouvelle config
    // (À implémenter selon votre système d'app management)
    
    return SYS_OK;
}
```

### 3.3 Commandes CLI

```c
// ============ espnow_cli.cpp ============

#include "espnow_config.h"

static int cli_espnow_config_show(int argc, char** argv) {
    DeviceConfig_t config;
    if (espnow_config_load(&config) != SYS_OK) {
        printf("ERROR: Failed to load config\n");
        return 1;
    }
    
    printf("┌─ ESP-NOW Configuration ─────────────────────┐\n");
    printf("│ Device ID:      0x%04X (MAC-based, read-only)\n", config.device_id);
    printf("│ Master ID:      %d\n", config.master_id);
    printf("│ Device Index:   %d (%s)\n", 
           config.device_index, 
           config.device_index == 0 ? "MASTER" : "SLAVE");
    printf("│ Config Version: %d\n", config.config_version);
    printf("└─────────────────────────────────────────────┘\n");
    
    return 0;
}

static int cli_espnow_config_set_master_id(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: espnow config set-master-id <master_id>\n");
        return 1;
    }
    
    uint8_t new_master_id = (uint8_t)atoi(argv[1]);
    
    if (espnow_config_set_master_id(new_master_id) != SYS_OK) {
        printf("ERROR: Failed to set master_id\n");
        return 1;
    }
    
    printf("Master ID updated to %d. Restarting ESP-NOW app...\n", new_master_id);
    
    // Redémarrer l'app
    app_stop_by_name("espnow_master");
    vTaskDelay(pdMS_TO_TICKS(500));
    app_start_by_name("espnow_master");
    
    return 0;
}

static int cli_espnow_config_set_device_index(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: espnow config set-device-index <index>\n");
        return 1;
    }
    
    uint8_t new_index = (uint8_t)atoi(argv[1]);
    
    if (new_index > 254) {
        printf("ERROR: Invalid index (must be 0-254)\n");
        return 1;
    }
    
    DeviceConfig_t config;
    if (espnow_config_load(&config) != SYS_OK) {
        printf("ERROR: Failed to load config\n");
        return 1;
    }
    
    config.device_index = new_index;
    if (espnow_config_save(&config) != SYS_OK) {
        printf("ERROR: Failed to save config\n");
        return 1;
    }
    
    printf("Device index updated to %d (%s). Restarting ESP-NOW app...\n",
           new_index, new_index == 0 ? "MASTER" : "SLAVE");
    
    // Redémarrer l'app
    app_stop_by_name("espnow_master");
    vTaskDelay(pdMS_TO_TICKS(500));
    app_start_by_name("espnow_master");
    
    return 0;
}

static int cli_espnow_config_reset(int argc, char** argv) {
    DeviceConfig_t config;
    
    // Générer device_id depuis MAC
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    config.device_id = (mac[4] << 8) | mac[5];
    config.master_id = 1;
    config.device_index = 0;
    config.config_version = 1;
    
    if (espnow_config_save(&config) != SYS_OK) {
        printf("ERROR: Failed to reset config\n");
        return 1;
    }
    
    printf("Config reset to defaults. Restarting ESP-NOW app...\n");
    
    // Redémarrer l'app
    app_stop_by_name("espnow_master");
    vTaskDelay(pdMS_TO_TICKS(500));
    app_start_by_name("espnow_master");
    
    return 0;
}

// Enregistrer les commandes CLI
void espnow_cli_register() {
    cli_register_command("espnow config show", cli_espnow_config_show);
    cli_register_command("espnow config set-master-id", cli_espnow_config_set_master_id);
    cli_register_command("espnow config set-device-index", cli_espnow_config_set_device_index);
    cli_register_command("espnow config reset", cli_espnow_config_reset);
}
```

---

## 4. FLUX DE RECONFIGURATION DYNAMIQUE

```
SCÉNARIO : Changer le master_id d'un master via CLI

AVANT :
┌─────────────────────────────────────────────────────────────┐
│ Master (device_id=0xAABB, master_id=1, index=0)             │
│ Envoie beacons avec master_id=1                             │
│ Slaves (master_id=1) écoutent et reçoivent les beacons      │
└─────────────────────────────────────────────────────────────┘

COMMANDE CLI :
$ espnow config set-master-id 2

ÉTAPES :
1. Charger config depuis NVS
2. Changer master_id: 1 → 2
3. Sauvegarder en NVS
4. Arrêter l'app espnow_master
5. Redémarrer l'app espnow_master
6. App recharge la config (master_id=2)
7. Envoie nouveaux beacons avec master_id=2

APRÈS :
┌─────────────────────────────────────────────────────────────┐
│ Master (device_id=0xAABB, master_id=2, index=0)             │
│ Envoie beacons avec master_id=2                             │
│ Slaves (master_id=1) n'écoutent plus (filtrage)             │
│ Slaves (master_id=2) écoutent et reçoivent les beacons      │
└─────────────────────────────────────────────────────────────┘
```

---

## 5. RÉSUMÉ : UTILISATION DES IDs DANS L'AUTO-DÉTECTION

| Aspect | Détail |
|--------|--------|
| **device_id** | Identifie UNIQUEMENT le device (MAC-based, read-only) |
| **master_id** | Filtre les beacons (slave n'écoute que son master) |
| **device_index** | Détermine le rôle (0=master, 1+=slave) |
| **Filtrage** | Beacon reçu valide si `beacon.master_id == config.master_id` |
| **Reconfiguration** | Changer master_id via CLI → redémarrage app → nouvelle config appliquée |
| **Scalabilité** | Support de plusieurs masters indépendants (master_id 1-255) |

---

## 6. FICHIERS À CRÉER/MODIFIER

```
src/apps/
├── espnow_common.h              (MODIFIER - ajouter structures avec IDs)
├── espnow_config.h              (CRÉER - gestion config)
├── espnow_config.cpp            (CRÉER - implémentation config)
├── espnow_cli.cpp               (CRÉER - commandes CLI)
├── espnow_master/
│   ├── espnow_master.h          (MODIFIER - inclure config)
│   └── espnow_master.cpp        (MODIFIER - utiliser IDs)
└── espnow_slave/
    ├── espnow_slave.h           (MODIFIER - inclure config)
    └── espnow_slave.cpp         (MODIFIER - utiliser IDs + filtrage)
```

---

## 7. PROCHAINES ÉTAPES

1. ✅ Valider ce design avec vous
2. ⬜ Implémenter espnow_common.h (structures avec IDs)
3. ⬜ Implémenter espnow_config.h/cpp (gestion config)
4. ⬜ Implémenter espnow_cli.cpp (commandes CLI)
5. ⬜ Implémenter espnow_master.cpp (utiliser IDs)
6. ⬜ Implémenter espnow_slave.cpp (filtrage + utiliser IDs)
7. ⬜ Tester avec plusieurs masters/slaves
