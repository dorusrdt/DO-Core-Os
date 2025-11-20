# 🔧 Configuration : 1 Master + 2 Slaves

## 📋 Vue d'ensemble

Vous avez besoin de **3 ESP32** avec le **même firmware** :
- **ESP32 #1** → Master
- **ESP32 #2** → Slave 1
- **ESP32 #3** → Slave 2

Tous doivent avoir le **même `master_id`** pour communiquer ensemble.

---

## 🎯 Configuration du Master (ESP32 #1)

### Étape 1 : Uploader le firmware
```bash
pio run --target upload
pio device monitor
```

### Étape 2 : Configurer le WiFi (optionnel, pour CLI)
```
wifi_save MonWiFi MonMotDePasse
```

### Étape 3 : Vérifier la configuration actuelle
```
espnow config show
```

Devrait afficher quelque chose comme :
```
┌─ ESP-NOW Configuration ─────────────────────┐
│ Device ID:      0xAABB (MAC-based, read-only)
│ Master ID:      1
│ Device Index:   0 (MASTER)
│ Config Version: 1
└─────────────────────────────────────────────┘
```

### Étape 4 : Configurer le master_id (si nécessaire)
```
espnow config set-master-id 1
```

**Note :** 1 est la valeur par défaut, mais vous pouvez utiliser n'importe quelle valeur entre 1-255.

### Étape 5 : Vérifier que device_index = 0
```
espnow config set-device-index 0
```

### Étape 6 : Démarrer l'application Master
```
app_start 10
```

### Étape 7 : Vérifier les logs
Vous devriez voir :
```
Master: App starting
Master: Config loaded - device_id=0xAABB, master_id=1
Master: ESP-NOW initialized successfully
Master: Beacon task started
Master: Beacon #1 sent on channel 6 (master_id=1)
Master: Beacon #2 sent on channel 6 (master_id=1)
...
```

**✅ Le master est prêt !** Il envoie des beacons toutes les 500ms.

---

## 🎯 Configuration du Slave 1 (ESP32 #2)

### Étape 1 : Uploader le MÊME firmware
```bash
pio run --target upload
pio device monitor
```

### Étape 2 : Configurer le WiFi (optionnel)
```
wifi_save MonWiFi MonMotDePasse
```

### Étape 3 : Configurer le master_id (identique au master)
```
espnow config set-master-id 1
```

**⚠️ IMPORTANT :** Le `master_id` doit être **identique** à celui du master (1 dans cet exemple).

### Étape 4 : Configurer le device_index (unique pour ce slave)
```
espnow config set-device-index 1
```

**⚠️ IMPORTANT :** Chaque slave doit avoir un `device_index` unique (1, 2, 3, ...).

### Étape 5 : Vérifier la configuration
```
espnow config show
```

Devrait afficher :
```
┌─ ESP-NOW Configuration ─────────────────────┐
│ Device ID:      0xCCDD (différent du master)
│ Master ID:      1 (identique au master)
│ Device Index:   1 (SLAVE)
│ Config Version: 1
└─────────────────────────────────────────────┘
```

### Étape 6 : Démarrer l'application Slave
```
app_start 11
```

### Étape 7 : Vérifier les logs
Vous devriez voir :
```
Slave: App starting
Slave: Config loaded - device_id=0xCCDD, master_id=1, device_index=1
Slave: ESP-NOW initialized successfully
Slave: Discovery task started
Slave: Scanning channels for master (master_id=1)
...
Slave: Valid beacon from master 0xAABB on channel 6
Slave: Master discovered - device_id=0xAABB (AA:BB:CC:DD:EE:FF)
Slave: Master info saved to NVS
Slave: Aligned to master channel 6
Slave: Send task started
Slave: Data sent - seq=1, h0=123, h11=456
...
```

**✅ Le slave 1 est prêt !** Il a trouvé le master et envoie des données.

---

## 🎯 Configuration du Slave 2 (ESP32 #3)

### Étape 1 : Uploader le MÊME firmware
```bash
pio run --target upload
pio device monitor
```

### Étape 2 : Configurer le WiFi (optionnel)
```
wifi_save MonWiFi MonMotDePasse
```

### Étape 3 : Configurer le master_id (identique au master)
```
espnow config set-master-id 1
```

**⚠️ IMPORTANT :** Même `master_id` que le master et le slave 1.

### Étape 4 : Configurer le device_index (unique, différent du slave 1)
```
espnow config set-device-index 2
```

**⚠️ IMPORTANT :** `device_index = 2` (différent de 1).

### Étape 5 : Vérifier la configuration
```
espnow config show
```

Devrait afficher :
```
┌─ ESP-NOW Configuration ─────────────────────┐
│ Device ID:      0xEEFF (différent des autres)
│ Master ID:      1 (identique au master)
│ Device Index:   2 (SLAVE)
│ Config Version: 1
└─────────────────────────────────────────────┘
```

### Étape 6 : Démarrer l'application Slave
```
app_start 11
```

### Étape 7 : Vérifier les logs
Même chose que le slave 1, mais avec `device_id=0xEEFF` et `device_index=2`.

**✅ Le slave 2 est prêt !**

---

## ✅ Vérification côté Master

Sur le **master (ESP32 #1)**, vous devriez maintenant voir dans les logs :

```
Master: Data from slave 0xCCDD - seq=1, h0=123, h11=456 (RSSI: -45)
Master: New slave discovered - 0xCCDD (AA:BB:CC:DD:EE:FF)
Master: Data from slave 0xEEFF - seq=1, h0=789, h11=012 (RSSI: -50)
Master: New slave discovered - 0xEEFF (11:22:33:44:55:66)
Master: 2 slave(s) connected
...
Master: Slave 0xCCDD - seq=10, lost=0, dup=0, RSSI=-45
Master: Slave 0xEEFF - seq=10, lost=0, dup=0, RSSI=-50
```

**✅ Les deux slaves sont connectés au master !**

---

## 📊 Résumé de la Configuration

| Device | ESP32 | device_index | master_id | App ID | Commande |
|--------|-------|--------------|-----------|--------|----------|
| **Master** | #1 | `0` | `1` | `10` | `app_start 10` |
| **Slave 1** | #2 | `1` | `1` | `11` | `app_start 11` |
| **Slave 2** | #3 | `2` | `1` | `11` | `app_start 11` |

### Points clés :
- ✅ **Même firmware** sur les 3 devices
- ✅ **Même `master_id`** (1) pour tous
- ✅ **`device_index` unique** : 0 (master), 1 (slave 1), 2 (slave 2)
- ✅ **App ID différent** : 10 (master) ou 11 (slave)

---

## 🔄 Commandes Utiles

### Sur chaque device

```bash
# Voir la configuration
espnow config show

# Voir les apps enregistrées
app_list

# Voir l'état des apps
app_status

# Arrêter une app
app_stop 10   # Master
app_stop 11   # Slave

# Redémarrer une app
app_restart 10
app_restart 11
```

### Sur le Master uniquement

```bash
# Voir les statistiques des slaves (dans les logs)
# Les stats sont affichées toutes les 30s automatiquement
```

---

## 🚨 Dépannage

### Le slave ne trouve pas le master

1. **Vérifier que le master est démarré** :
   ```
   app_status
   ```
   L'app 10 doit être `RUNNING`.

2. **Vérifier le master_id** :
   ```
   # Sur le master
   espnow config show

   # Sur le slave
   espnow config show
   ```
   Les deux doivent avoir le **même master_id**.

3. **Vérifier les beacons** :
   Les logs du master doivent afficher :
   ```
   Master: Beacon #X sent on channel 6 (master_id=1)
   ```

4. **Vérifier le scan** :
   Les logs du slave doivent afficher :
   ```
   Slave: Scanning channels for master (master_id=1)
   ```

### Le master ne reçoit pas les données d'un slave

1. **Vérifier que le slave a trouvé le master** :
   Les logs du slave doivent afficher :
   ```
   Slave: Master discovered - device_id=0xAABB
   ```

2. **Vérifier l'envoi** :
   Les logs du slave doivent afficher :
   ```
   Slave: Data sent - seq=X, h0=XXX, h11=XXX
   ```

3. **Vérifier la réception** :
   Les logs du master doivent afficher :
   ```
   Master: Data from slave 0xXXXX - seq=X
   ```

### Les deux slaves ont le même device_index

**Erreur !** Chaque slave doit avoir un `device_index` unique :
- Slave 1 : `device_index = 1`
- Slave 2 : `device_index = 2`
- Slave 3 : `device_index = 3`
- etc.

**Solution :**
```
# Sur le slave 2
espnow config set-device-index 2
app_restart 11
```

---

## 🎓 Exemple Complet de Configuration

### Master (ESP32 #1)
```bash
# 1. Configuration
espnow config set-master-id 1
espnow config set-device-index 0
espnow config show

# 2. Démarrer
app_start 10

# 3. Vérifier
# Logs: "Master: Beacon #X sent on channel 6"
```

### Slave 1 (ESP32 #2)
```bash
# 1. Configuration
espnow config set-master-id 1
espnow config set-device-index 1
espnow config show

# 2. Démarrer
app_start 11

# 3. Vérifier
# Logs: "Slave: Master discovered - device_id=0xAABB"
# Logs: "Slave: Data sent - seq=1"
```

### Slave 2 (ESP32 #3)
```bash
# 1. Configuration
espnow config set-master-id 1
espnow config set-device-index 2
espnow config show

# 2. Démarrer
app_start 11

# 3. Vérifier
# Logs: "Slave: Master discovered - device_id=0xAABB"
# Logs: "Slave: Data sent - seq=1"
```

---

## 📈 Résultat Attendu

Une fois configuré, vous devriez avoir :

```
┌─────────────────────────────────────────┐
│           MASTER (ESP32 #1)             │
│  device_id=0xAABB, master_id=1          │
│  Envoie beacons toutes les 500ms        │
│  Reçoit données des 2 slaves            │
└───────────────┬─────────────────────────┘
                │
        ┌───────┴───────┐
        │               │
        ▼               ▼
┌──────────────┐  ┌──────────────┐
│ SLAVE 1      │  │ SLAVE 2      │
│ ESP32 #2     │  │ ESP32 #3     │
│ device_id=   │  │ device_id=   │
│ 0xCCDD       │  │ 0xEEFF       │
│ index=1      │  │ index=2      │
│ Envoie data  │  │ Envoie data  │
│ toutes les 2s│  │ toutes les 2s│
└──────────────┘  └──────────────┘
```

**✅ Configuration terminée !** Les 2 slaves communiquent avec le master.

