# ⚡ ESP-NOW : Guide de Déploiement Rapide

## 🎯 Déploiement en 5 minutes

### 📱 Étape 1 : Préparer le Master

```
1. Uploader le firmware sur ESP32 #1
2. Ouvrir Serial Monitor (115200)
3. Attendre le boot
4. Configurer (si première fois) :
   espnow config set-master-id 1
   espnow config set-device-index 0
5. Démarrer :
   app_start 10
```

**✅ Vérification :** Vous devriez voir `Master: Beacon #X sent on channel 6`

---

### 📱 Étape 2 : Préparer le Slave

```
1. Uploader le MÊME firmware sur ESP32 #2
2. Ouvrir Serial Monitor (115200)
3. Attendre le boot
4. Configurer :
   espnow config set-master-id 1        ← MÊME que le master
   espnow config set-device-index 1     ← Unique par slave
5. Démarrer :
   app_start 11
```

**✅ Vérification :** Vous devriez voir `Slave: Master discovered - device_id=0xXXXX`

---

## 🔑 Points Clés

| Élément | Master | Slave |
|---------|--------|-------|
| **Firmware** | Identique | Identique |
| **device_index** | `0` | `1, 2, 3, ...` |
| **master_id** | `1` (exemple) | `1` (identique) |
| **App ID** | `10` | `11` |
| **Commande** | `app_start 10` | `app_start 11` |

---

## 📊 Flux de Découverte

```
┌─────────────┐
│   MASTER    │
│ device_id=0 │
│ master_id=1 │
└──────┬──────┘
       │
       │ Envoie BEACON toutes les 500ms
       │ (broadcast sur canal 6)
       │
       ▼
┌─────────────┐
│    SLAVE    │
│ device_id=1 │
│ master_id=1 │
└──────┬──────┘
       │
       │ 1. Scan canaux 1-13
       │ 2. Reçoit BEACON
       │ 3. Filtre par master_id
       │ 4. Sauvegarde master_mac
       │ 5. Aligne canal
       │
       ▼
┌─────────────┐
│ CONNEXION   │
│ ÉTABLIE     │
└─────────────┘
       │
       │ Slave envoie DATA toutes les 2s
       │
       ▼
┌─────────────┐
│   MASTER    │
│ Reçoit DATA │
└─────────────┘
```

---

## 🚨 Erreurs Courantes

### ❌ "Slave: Scanning channels - no master found"

**Cause :** Le master n'est pas démarré ou `master_id` différent

**Solution :**
```bash
# Sur le master
app_start 10

# Vérifier master_id
espnow config show  # Doit être identique sur master et slave
```

### ❌ "Master: Data from wrong master_id"

**Cause :** Le slave a un `master_id` différent

**Solution :**
```bash
# Sur le slave
espnow config set-master-id 1  # Même que le master
app_restart 11
```

### ❌ "Slave: Received beacon from self"

**Cause :** Le slave a `device_index = 0` (configuré comme master)

**Solution :**
```bash
# Sur le slave
espnow config set-device-index 1
app_restart 11
```

---

## 📋 Checklist Rapide

### Master
- [ ] `device_index = 0`
- [ ] `master_id = 1` (ou votre choix)
- [ ] `app_start 10` exécuté
- [ ] Beacons visibles dans logs

### Slave
- [ ] `device_index = 1, 2, 3, ...` (unique)
- [ ] `master_id = 1` (identique au master)
- [ ] `app_start 11` exécuté
- [ ] Master découvert dans logs
- [ ] Données envoyées dans logs

---

## 🔧 Commandes Essentielles

```bash
# Voir la configuration
espnow config show

# Configurer master_id
espnow config set-master-id 1

# Configurer device_index
espnow config set-device-index 0  # Master
espnow config set-device-index 1  # Slave 1
espnow config set-device-index 2  # Slave 2

# Démarrer/Arrêter
app_start 10   # Master
app_start 11    # Slave
app_stop 10     # Arrêter master
app_stop 11     # Arrêter slave

# Voir les apps
app_list
app_status
```

---

## 🎓 Exemple Complet

### Configuration d'un réseau avec 1 Master + 2 Slaves

#### Master (ESP32 #1)
```bash
espnow config show
# Device ID: 0xAABB
# Master ID: 1
# Device Index: 0

app_start 10
# Master: Beacon #1 sent on channel 6 (master_id=1)
```

#### Slave 1 (ESP32 #2)
```bash
espnow config set-master-id 1
espnow config set-device-index 1
espnow config show
# Device ID: 0xCCDD
# Master ID: 1
# Device Index: 1

app_start 11
# Slave: Master discovered - device_id=0xAABB
# Slave: Data sent - seq=1
```

#### Slave 2 (ESP32 #3)
```bash
espnow config set-master-id 1
espnow config set-device-index 2
espnow config show
# Device ID: 0xEEFF
# Master ID: 1
# Device Index: 2

app_start 11
# Slave: Master discovered - device_id=0xAABB
# Slave: Data sent - seq=1
```

#### Vérification côté Master
```bash
# Sur le master, vous devriez voir :
# Master: Data from slave 0xCCDD - seq=1
# Master: Data from slave 0xEEFF - seq=1
# Master: 2 slave(s) connected
```

---

## 💡 Astuces

1. **Premier déploiement** : Commencez avec 1 master + 1 slave pour valider
2. **Plusieurs slaves** : Donnez un `device_index` unique à chaque slave
3. **Plusieurs réseaux** : Utilisez des `master_id` différents (1, 2, 3, ...)
4. **Dépannage** : Vérifiez toujours les logs des deux côtés (master et slave)
5. **Persistance** : Les infos du master sont sauvegardées en NVS, le slave les retrouve au redémarrage

## ⚠️ Note importante : Canal ESP-NOW

**ESP-NOW utilise toujours le même canal que le WiFi actif :**
- Si le master est connecté à un WiFi → ESP-NOW utilise le canal de ce WiFi
- Si le WiFi change de canal → ESP-NOW change aussi automatiquement
- Le master annonce son canal dans chaque beacon
- Le slave doit s'aligner sur le canal annoncé

**Conséquence :** Ne pas coder le canal en dur. Le système gère automatiquement les changements de canal via les beacons.

---

## 📞 Support

Si vous rencontrez des problèmes :
1. Vérifiez les logs des deux devices
2. Vérifiez que `master_id` est identique
3. Vérifiez que `device_index` est correct (0 pour master, 1+ pour slave)
4. Vérifiez que les apps sont démarrées (`app_status`)

