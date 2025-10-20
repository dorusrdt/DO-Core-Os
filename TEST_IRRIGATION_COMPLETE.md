# 🧪 Test Complet du Système d'Irrigation

Ce document décrit le test complet de la logique d'irrigation programmée entre les 3 apps.

---

## 🎯 Objectif du Test

Vérifier que **toute la chaîne de communication fonctionne** :

1. ✅ Serveur FastAPI → Master : Envoi de configuration avec zone programmée
2. ✅ Master : Détection de l'heure programmée
3. ✅ Master → Slave2 : Envoi de commande d'irrigation
4. ✅ Slave2 : Activation des relais (zone + pompe)
5. ✅ Slave2 → Master : Envoi de statut (irrigation active)
6. ✅ Slave2 : Arrêt automatique après durée programmée

---

## 📋 Prérequis

### **1. Serveur FastAPI**
```bash
cd /home/dorus/Documents/GitHub/DO-Core-Os/test_server
python3 server.py
```

**Vérifier** : `http://192.168.1.3:3000/api/health`

### **2. ESP32 avec les 3 Apps**
```bash
# Compiler et flasher
pio run --target upload --upload-port /dev/ttyUSB0

# Moniteur série
pio device monitor --port /dev/ttyUSB0 --baud 115200
```

### **3. Configuration ESP32**
```bash
D'O-Core> irrig_config_show
# Vérifier que les IPs sont correctes :
# - Master: 192.168.1.61:8080
# - Slave1: 192.168.1.61:8081
# - Slave2: 192.168.1.61:8082
# - Server: http://192.168.1.3:3000

# Démarrer les 3 apps
D'O-Core> app_start 1  # Master
D'O-Core> app_start 2  # Slave1
D'O-Core> app_start 3  # Slave2

# Activer les logs en temps réel
D'O-Core> log_echo on
```

---

## 🚀 Scénario de Test : Irrigation Programmée

### **Étape 1 : Créer une Zone avec Irrigation dans 2 Minutes**

```bash
cd /home/dorus/Documents/GitHub/DO-Core-Os/test_server
./test_irrigation_schedule.sh
```

**Ce script va** :
1. Supprimer toutes les zones existantes
2. Créer une zone `zone_test_auto` avec irrigation programmée dans **2 minutes**
3. Afficher l'heure de déclenchement

**Sortie attendue** :
```
==========================================
🧪 Test Irrigation Programmée
==========================================
📅 Heure actuelle : 17:45
⏰ Irrigation programmée à : 17:47
...
✅ Zone créée avec succès !
==========================================
📍 Zone ID: zone_test_auto
💧 Eau par jour: 1500ml
⏰ Heure d'irrigation: 17:47
📊 Seuil humidité: 20%
🔬 Capteurs: s_01, s_02, s_03

⏳ L'irrigation démarrera automatiquement à 17:47
📺 Surveillez les logs de l'ESP32 avec 'log_echo on'
==========================================
```

---

### **Étape 2 : Surveiller les Logs ESP32**

**Logs attendus sur l'ESP32** :

#### **T+0s : Master reçoit la configuration**
```
[INFO] Received configuration for 1 zones
[INFO] Zone 1: zone_test_auto
[INFO]   Water: 1500ml/day, Time: 17:47, Threshold: 20%
[INFO]   Sensors: 3
```

#### **T+10s : Master poll le serveur (toutes les 10s)**
```
[INFO] Received configuration for 1 zones
[INFO] Zone 1: zone_test_auto (existing - preserving sensors)
[INFO]   Water: 1500ml/day, Time: 17:47, Threshold: 20% (updated)
```

#### **T+60s : Master vérifie l'heure (toutes les 60s)**
```
[DEBUG] ⏰ Master: Checking irrigation schedule (current time: 17:46)
[DEBUG]    Zone zone_test_auto: scheduled=17:47, current=17:46 (no match)
```

#### **T+120s : DÉCLENCHEMENT DE L'IRRIGATION ! 🎯**
```
[DEBUG] ⏰ Master: Checking irrigation schedule (current time: 17:47)
[INFO] 🎯 Master: SCHEDULED IRRIGATION TRIGGERED!
[INFO]    Zone: zone_test_auto (slot 1)
[INFO]    Scheduled time: 17:47 (MATCH!)
[INFO]    Duration: 150s (based on 1500ml/day)

[INFO] 📤 Master: Preparing irrigation command...
[INFO]    Target zone: zone_test_auto
[INFO]    Duration: 150s
[INFO]    Found in slot 0 (physical zone 1)

[INFO] 📤 Master → Slave2: Sending irrigation command
[INFO]    Command: START_IRRIGATION
[INFO]    Zone ID (hardware): 0
[INFO]    Zone ID (server): zone_test_auto
[INFO]    Duration: 150s

[INFO] 📤 Master → Slave2: Sending 'start_irrigation' to http://192.168.1.61:8082 (attempt 1/3)
[INFO] ✅ Master → Slave2: Command 'start_irrigation' sent successfully!
[INFO] ✅ Master → Slave2: Command sent successfully!
[INFO]    Irrigation timer set: 150s
```

#### **Slave2 reçoit la commande**
```
[INFO] 📥 Slave2: Incoming POST /api/irrigation/command from 192.168.1.61
[INFO] ✅ Slave2: Received command 'start_irrigation' for zone 0 (duration: 150s)

[INFO] 💧 Slave2: STARTING IRRIGATION
[INFO]    Zone ID (hardware): 1
[INFO]    Duration: 150s
[INFO] 🔌 Slave2: Activating zone 1 relay (GPIO 15)
[DEBUG] Zone 1 relay: ON
[INFO] 🔌 Slave2: Activating pump (GPIO 5)
[INFO] Pump: ON
[INFO] ✅ Slave2: Irrigation started successfully!
[INFO]    End time: 270s (in 150s)
[INFO]    Total irrigations: 1
```

#### **T+130s : Slave2 envoie son statut (toutes les 10s)**
```
[INFO] 📤 Slave2: Publishing status (interval: 10000ms, elapsed: 10001ms)
[INFO] 📤 Slave2 → Master: Sending status
[INFO]    Zone: 0 | Irrigating: YES | Remaining: 140s | Pump: ON
[INFO] ✅ Slave2: Status published successfully
```

#### **T+270s : FIN DE L'IRRIGATION**
```
[INFO] ⏱️  Slave2: Irrigation timer EXPIRED
[INFO]    Zone 1 irrigation complete

[INFO] 🛑 Slave2: STOPPING IRRIGATION
[INFO]    Zone: 1
[INFO] 🔌 Slave2: Deactivating zone 1 relay
[DEBUG] Zone 1 relay: OFF
[INFO] 🔌 Slave2: Deactivating pump
[INFO] Pump: OFF
[INFO] ✅ Slave2: Irrigation stopped successfully
```

#### **Master détecte la fin**
```
[INFO] ⏱️  Master: Irrigation timer expired
[INFO]    Irrigation should be complete on Slave2
```

---

## 📊 Timeline Complète (2 minutes 30 secondes)

```
T=0s    [Master] Reçoit config zone_test_auto (irrigation à 17:47)
        [Master] Stocke en ZONE_STACK[0]
        [Master] Assigne capteurs s_01, s_02, s_03

T=10s   [Master] Poll serveur (config inchangée)
        [Slave2] Envoie statut → Master (irrigating: NO)

T=20s   [Master] Poll serveur
        [Slave2] Envoie statut → Master

T=60s   [Master] Vérifie heure (17:46 ≠ 17:47) → Pas de match

T=120s  [Master] Vérifie heure (17:47 = 17:47) → MATCH ! 🎯
        [Master] Envoie commande → Slave2
        [Slave2] Reçoit commande
        [Slave2] Active relais zone 1 (GPIO 15)
        [Slave2] Active pompe (GPIO 5)

T=130s  [Slave2] Envoie statut (irrigating: YES, remaining: 140s)

T=140s  [Slave2] Envoie statut (irrigating: YES, remaining: 130s)

...

T=270s  [Slave2] Timer expire
        [Slave2] Désactive relais zone 1
        [Slave2] Désactive pompe
        [Slave2] Envoie statut (irrigating: NO)
```

---

## ✅ Critères de Succès

| Critère | Vérification |
|---------|--------------|
| **Config reçue** | Zone apparaît dans les logs Master |
| **Heure détectée** | Log "SCHEDULED IRRIGATION TRIGGERED!" |
| **Commande envoyée** | Log "Master → Slave2: Command sent successfully!" |
| **Relais activé** | Log "Slave2: Activating zone 1 relay (GPIO 15)" |
| **Pompe activée** | Log "Pump: ON" |
| **Statut envoyé** | Log "Slave2 → Master: Sending status" avec "Irrigating: YES" |
| **Arrêt automatique** | Log "Slave2: Irrigation timer EXPIRED" après 150s |
| **Relais désactivé** | Log "Zone 1 relay: OFF" |
| **Pompe désactivée** | Log "Pump: OFF" |

---

## 🐛 Dépannage

### **Problème : Irrigation ne démarre pas**

**Vérifier** :
1. L'heure système de l'ESP32 : `date` (doit être synchronisée via NTP)
2. La zone est bien configurée : `dmesg | grep "Zone 1:"`
3. Le Master vérifie bien l'heure : Chercher `"Checking irrigation schedule"`

**Solution** :
```bash
# Forcer une synchronisation NTP
D'O-Core> time_sync
```

---

### **Problème : Commande non reçue par Slave2**

**Vérifier** :
1. Slave2 est bien démarré : `app_list` (doit montrer "IrrigSlaveRelay: RUNNING")
2. Serveur HTTP de Slave2 écoute : `netstat -an | grep 8082`
3. IP configurée correctement : `irrig_config_show`

**Solution** :
```bash
# Redémarrer Slave2
D'O-Core> app_stop 3
D'O-Core> app_start 3
```

---

### **Problème : Relais ne s'active pas**

**Vérifier** :
1. Les logs montrent "Activating zone 1 relay (GPIO 15)"
2. Le GPIO est bien configuré (voir `irrig_types.h`)
3. Pas de conflit avec d'autres périphériques

**Test manuel** :
```bash
# Tester le relais directement
D'O-Core> irrig_start 1 10
# Devrait activer le relais pendant 10 secondes
```

---

## 🔧 Tests Alternatifs

### **Test 1 : Irrigation Immédiate**

```bash
./test_irrigation_now.sh
```

Crée une zone avec l'heure actuelle → Irrigation démarre dans les 10 secondes.

---

### **Test 2 : Irrigation Manuelle (CLI)**

```bash
D'O-Core> irrig_start 1 60
```

Démarre l'irrigation de la zone 1 pendant 60 secondes (bypass du serveur).

---

### **Test 3 : Vérifier le Statut**

```bash
# Pendant l'irrigation
curl http://192.168.1.61:8082/api/irrigation/status | jq '.'
```

**Sortie attendue** :
```json
{
  "device_id": "ESP32_SLAVE_RELAYS",
  "zone_id": 0,
  "is_irrigating": true,
  "remaining_seconds": 120,
  "pump_running": true,
  "relay_states": [true, false, false, false],
  "timestamp": 150000,
  "total_commands": 1,
  "total_irrigations": 1
}
```

---

## 📈 Logs Complets Attendus

Voir le fichier `LOGS_EXEMPLE_IRRIGATION.txt` pour un exemple complet de logs sur 3 minutes.

---

**Dernière mise à jour : 2025-10-17**
