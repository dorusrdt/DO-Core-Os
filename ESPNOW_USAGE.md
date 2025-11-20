# ESP-NOW Usage Guide

## Vue d'ensemble

Le système ESP-NOW implémente une auto-détection robuste basée sur des IDs uniques :
- **device_id** : Identifiant unique du device (basé sur MAC)
- **master_id** : Identifiant du groupe/master (1-255)
- **device_index** : Rôle du device (0=master, 1+=slave)

## Configuration initiale

### 1. Déployer un Master

```bash
# Compiler et flasher le firmware avec espnow_master activé
# Au premier démarrage, la config est initialisée automatiquement :
# - device_id = généré depuis MAC
# - master_id = 1 (défaut)
# - device_index = 0 (master)
```

### 2. Déployer des Slaves

```bash
# Compiler et flasher le firmware avec espnow_slave activé
# Au premier démarrage, la config est initialisée automatiquement :
# - device_id = généré depuis MAC
# - master_id = 1 (défaut, doit correspondre au master)
# - device_index = 1 (slave)
```

## Reconfiguration dynamique via CLI

### Afficher la configuration actuelle

```bash
espnow config show

# Résultat :
# ┌─ ESP-NOW Configuration ─────────────────────┐
# │ Device ID:      0xEEFF (MAC-based, read-only)
# │ Master ID:      1
# │ Device Index:   0 (MASTER)
# │ Config Version: 1
# └─────────────────────────────────────────────┘
```

### Changer le master_id

```bash
# Changer le master_id du device de 1 à 2
espnow config set-master-id 2

# Résultat :
# Master ID updated to 2. Restarting ESP-NOW app...
# ESP-NOW app restarted with new master_id
```

**Effet :**
- Master : Envoie maintenant des beacons avec master_id=2
- Slave : Écoute maintenant les beacons avec master_id=2

### Changer le device_index (rôle)

```bash
# Changer le rôle de master (0) à slave (1)
espnow config set-device-index 1

# Résultat :
# Device index updated to 1 (SLAVE). Restarting ESP-NOW app...
# ESP-NOW app restarted with new device_index
```

### Réinitialiser la configuration

```bash
# Réinitialiser à la config par défaut
espnow config reset

# Résultat :
# Config reset to defaults. Restarting ESP-NOW app...
# ESP-NOW app restarted with default config
```

## Scénarios d'utilisation

### Scénario 1 : Un master avec plusieurs slaves

```
Master (device_id=0xAABB, master_id=1, index=0)
├─ Slave 1 (device_id=0xCCDD, master_id=1, index=1)
├─ Slave 2 (device_id=0xEEFF, master_id=1, index=2)
└─ Slave 3 (device_id=0x1122, master_id=1, index=3)

Tous les devices ont master_id=1 → Ils forment un groupe cohérent
```

### Scénario 2 : Plusieurs masters indépendants

```
Master 1 (device_id=0xAABB, master_id=1, index=0)
├─ Slave 1A (device_id=0xCCDD, master_id=1, index=1)
└─ Slave 1B (device_id=0xEEFF, master_id=1, index=2)

Master 2 (device_id=0x1122, master_id=2, index=0)
├─ Slave 2A (device_id=0x3344, master_id=2, index=1)
└─ Slave 2B (device_id=0x5566, master_id=2, index=2)

Deux réseaux ESP-NOW complètement indépendants
```

### Scénario 3 : Reconfiguration dynamique

```
AVANT :
Master (master_id=1) → Slaves (master_id=1)

COMMANDE :
espnow config set-master-id 2

APRÈS :
Master (master_id=2) → Slaves (master_id=1) [déconnectés]

PUIS sur les slaves :
espnow config set-master-id 2

APRÈS :
Master (master_id=2) → Slaves (master_id=2) [reconnectés]
```

## Flux d'auto-détection

### Master

1. **Initialisation**
   - Charger config depuis NVS
   - Vérifier device_index == 0 (master)
   - Initialiser ESP-NOW
   - Démarrer beacon_task

2. **Beacon task** (toutes les 500ms)
   - Construire BeaconPacket avec master_id et device_id
   - Envoyer en broadcast
   - Incrémenter beacon_id

3. **Data reception**
   - Recevoir DataPacket du slave
   - Vérifier frame_type == FRAME_DATA
   - Vérifier data.master_id == config.master_id (filtrage)
   - Vérifier data.slave_id != config.device_id (pas soi-même)
   - Ajouter/mettre à jour SlaveInfo
   - Traiter les données

4. **Cleanup task** (toutes les 5s)
   - Pour chaque slave :
     - Si (now - last_seen_ticks) > 10s :
       - Marquer is_active = false
       - Log timeout

### Slave

1. **Initialisation**
   - Charger config depuis NVS
   - Vérifier device_index != 0 (slave)
   - Charger master_info depuis NVS (si disponible)
   - Initialiser ESP-NOW
   - Démarrer discovery_task et send_task

2. **Discovery task** (toutes les 1s)
   - Si master_found :
     - Vérifier beacon reçu dans les 5s
     - Si timeout : master_found = false
   - Si !master_found :
     - Scanner canaux 1-13 (200ms par canal)
     - Écouter les beacons
     - Vérifier beacon.master_id == config.master_id (filtrage)
     - Si beacon valide : master_found = true

3. **Beacon reception**
   - Recevoir BeaconPacket
   - Vérifier frame_type == FRAME_BEACON
   - Vérifier beacon.master_id == config.master_id (filtrage)
   - Vérifier beacon.device_id != config.device_id (pas soi-même)
   - Mettre à jour master_info
   - Sauvegarder en NVS
   - Aligner le canal

4. **Send task** (toutes les 2s)
   - Si master_found :
     - Construire DataPacket avec slave_id et master_id
     - Envoyer au master
     - Si envoi échoue 3x : master_found = false
   - Sinon :
     - Attendre discovery_task

## Logs de débogage

### Master startup

```
Master: App starting
Master: Config loaded - device_id=0xAABB, master_id=1
Master: Initializing ESP-NOW
Master: ESP-NOW initialized successfully
Master: Tasks created - ready to receive slave data
Master: Beacon task started
Master: Data task started
Master: Cleanup task started
```

### Slave startup

```
Slave: App starting
Slave: Config loaded - device_id=0xCCDD, master_id=1, device_index=1
Slave: Initializing ESP-NOW
Slave: ESP-NOW initialized successfully
Slave: Tasks created - starting auto-discovery
Slave: Discovery task started
Slave: Send task started
Slave: Scanning channels for master (master_id=1)
```

### Slave discovers master

```
Slave: Valid beacon from master 0xAABB on channel 6
Slave: Master discovered - device_id=0xAABB (AA:BB:CC:DD:EE:FF)
Slave: Master info saved to NVS
Slave: Data sent - seq=1, h0=512, h11=768
Master: Data from slave 0xCCDD - seq=1, h0=512, h11=768
Master: New slave discovered - 0xCCDD (CC:DD:EE:FF:11:22)
```

### Slave loses master

```
Slave: Master lost - no beacon for 5s, restarting discovery
Slave: Scanning channels for master (master_id=1)
```

## Troubleshooting

### Slave ne trouve pas le master

**Causes possibles :**
1. master_id différent entre master et slave
2. Master n'envoie pas de beacons
3. Problème de canal WiFi

**Solutions :**
```bash
# Vérifier la config du slave
espnow config show

# Vérifier que master_id correspond au master
espnow config set-master-id <correct_master_id>

# Vérifier les logs du master
# Master devrait afficher : "Beacon #X sent on channel Y"
```

### Master ne reçoit pas les données du slave

**Causes possibles :**
1. Slave n'a pas trouvé le master
2. master_id différent dans le DataPacket
3. Slave n'est pas dans la portée

**Solutions :**
```bash
# Vérifier que le slave a trouvé le master
# Logs du slave devraient afficher : "Master discovered"

# Vérifier la config du slave
espnow config show

# Vérifier que master_id correspond
espnow config set-master-id <correct_master_id>
```

### Reconfiguration ne prend pas effet

**Cause :**
L'app n'a pas été redémarrée correctement

**Solution :**
```bash
# Redémarrer manuellement l'app
app stop espnow_master
app start espnow_master

# Vérifier la nouvelle config
espnow config show
```

## Fichiers importants

```
src/apps/
├── espnow_common.h              # Structures de données
├── espnow_config.h              # API de configuration
├── espnow_config.cpp            # Implémentation config
├── espnow_cli.cpp               # Commandes CLI
├── espnow_master/
│   ├── espnow_master.h
│   └── espnow_master.cpp        # Implémentation master
└── espnow_slave/
    ├── espnow_slave.h
    └── espnow_slave.cpp         # Implémentation slave

ESPNOW_AUTO_DETECTION_DESIGN.md  # Documentation du design
ESPNOW_USAGE.md                  # Ce fichier
```

## Prochaines étapes

1. ✅ Implémenter le système d'IDs
2. ✅ Implémenter la reconfiguration dynamique
3. ⬜ Intégrer les commandes CLI au système CLI du kernel
4. ⬜ Ajouter des statistiques détaillées (RSSI, latence, etc.)
5. ⬜ Implémenter la sécurité (PMK, chiffrement)
6. ⬜ Tester avec plusieurs masters/slaves
