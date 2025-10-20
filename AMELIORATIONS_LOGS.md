# 📝 Améliorations des Logs - Système d'Irrigation

Ce document résume les améliorations apportées aux logs des 3 apps pour tracer toute la logique d'irrigation.

---

## 🎯 Objectif

Rendre **visible et traçable** chaque étape du flux d'irrigation :
1. Réception de configuration
2. Détection de l'heure programmée
3. Envoi de commande
4. Activation des relais
5. Envoi de statut
6. Arrêt automatique

---

## ✅ Améliorations par App

### **1. APP MASTER** (`irrig_app_master.cpp`)

#### **Vérification de l'Heure Programmée**

**Avant** :
```cpp
kernel_log(LOG_LEVEL_INFO, "Scheduled irrigation for zone %d", ZONE_STACK[i].id);
```

**Après** :
```cpp
kernel_log(LOG_LEVEL_INFO, "🎯 Master: SCHEDULED IRRIGATION TRIGGERED!");
kernel_log(LOG_LEVEL_INFO, "   Zone: %s (slot %d)", ZONE_STACK[i].zoneId.c_str(), ZONE_STACK[i].id);
kernel_log(LOG_LEVEL_INFO, "   Scheduled time: %s (MATCH!)", ZONE_STACK[i].irrigationTime.c_str());
kernel_log(LOG_LEVEL_INFO, "   Duration: %ds (based on %dml/day)", 
           ZONE_STACK[i].waterPerDay / 10, ZONE_STACK[i].waterPerDay);
```

**Ajout** :
```cpp
kernel_log(LOG_LEVEL_DEBUG, "⏰ Master: Checking irrigation schedule (current time: %s)", currentTime);
kernel_log(LOG_LEVEL_DEBUG, "   Zone %s: scheduled=%s, current=%s (no match)",
           ZONE_STACK[i].zoneId.c_str(), ZONE_STACK[i].irrigationTime.c_str(), currentTime);
```

---

#### **Envoi de Commande d'Irrigation**

**Avant** :
```cpp
kernel_log(LOG_LEVEL_INFO, "Sending irrigation command to Slave2: zone %s, duration %ds", 
           zoneId.c_str(), durationSeconds);
kernel_log(LOG_LEVEL_INFO, "Irrigation command sent successfully");
```

**Après** :
```cpp
kernel_log(LOG_LEVEL_INFO, "📤 Master: Preparing irrigation command...");
kernel_log(LOG_LEVEL_INFO, "   Target zone: %s", zoneId.c_str());
kernel_log(LOG_LEVEL_INFO, "   Duration: %ds", durationSeconds);
kernel_log(LOG_LEVEL_INFO, "   Found in slot %d (physical zone %d)", i, zoneSlot->id);

kernel_log(LOG_LEVEL_INFO, "📤 Master → Slave2: Sending irrigation command");
kernel_log(LOG_LEVEL_INFO, "   Command: START_IRRIGATION");
kernel_log(LOG_LEVEL_INFO, "   Zone ID (hardware): %d", cmd.zone_id);
kernel_log(LOG_LEVEL_INFO, "   Zone ID (server): %s", cmd.zone_server_id);
kernel_log(LOG_LEVEL_INFO, "   Duration: %ds", cmd.duration_seconds);

kernel_log(LOG_LEVEL_INFO, "✅ Master → Slave2: Command sent successfully!");
kernel_log(LOG_LEVEL_INFO, "   Irrigation timer set: %lus", activeIrrigationTimer / 1000);
```

---

#### **Fin d'Irrigation**

**Avant** :
```cpp
kernel_log(LOG_LEVEL_INFO, "Irrigation timer expired");
```

**Après** :
```cpp
kernel_log(LOG_LEVEL_INFO, "⏱️  Master: Irrigation timer expired");
kernel_log(LOG_LEVEL_INFO, "   Irrigation should be complete on Slave2");
```

---

### **2. APP SLAVE2 RELAYS** (`irrig_app_slave_relays.cpp`)

#### **Démarrage d'Irrigation**

**Avant** :
```cpp
kernel_log(LOG_LEVEL_INFO, "Starting irrigation: Zone %d, Duration %ds", zone_id, duration_seconds);
```

**Après** :
```cpp
kernel_log(LOG_LEVEL_INFO, "💧 Slave2: STARTING IRRIGATION");
kernel_log(LOG_LEVEL_INFO, "   Zone ID (hardware): %d", zone_id);
kernel_log(LOG_LEVEL_INFO, "   Duration: %ds", duration_seconds);

kernel_log(LOG_LEVEL_INFO, "🔌 Slave2: Activating zone %d relay (GPIO %d)", zone_id, gpio);
kernel_log(LOG_LEVEL_INFO, "🔌 Slave2: Activating pump (GPIO %d)", PUMP_RELAY_PIN);

kernel_log(LOG_LEVEL_INFO, "✅ Slave2: Irrigation started successfully!");
kernel_log(LOG_LEVEL_INFO, "   End time: %lus (in %ds)", irrigation_end_time / 1000, duration_seconds);
kernel_log(LOG_LEVEL_INFO, "   Total irrigations: %lu", total_irrigations);
```

---

#### **Arrêt d'Irrigation**

**Avant** :
```cpp
kernel_log(LOG_LEVEL_INFO, "Stopping irrigation for zone %d", active_zone_id);
```

**Après** :
```cpp
kernel_log(LOG_LEVEL_INFO, "🛑 Slave2: STOPPING IRRIGATION");
kernel_log(LOG_LEVEL_INFO, "   Zone: %d", active_zone_id);
kernel_log(LOG_LEVEL_INFO, "🔌 Slave2: Deactivating zone %d relay", active_zone_id);
kernel_log(LOG_LEVEL_INFO, "🔌 Slave2: Deactivating pump");
kernel_log(LOG_LEVEL_INFO, "✅ Slave2: Irrigation stopped successfully");
```

---

#### **Timer Expiré**

**Avant** :
```cpp
kernel_log(LOG_LEVEL_INFO, "Irrigation timer expired");
```

**Après** :
```cpp
kernel_log(LOG_LEVEL_INFO, "⏱️  Slave2: Irrigation timer EXPIRED");
kernel_log(LOG_LEVEL_INFO, "   Zone %d irrigation complete", active_zone_id);
```

---

### **3. COMMUNICATION** (`irrig_communication.cpp`)

Les logs de communication étaient déjà bons, mais ont été conservés :

```cpp
kernel_log(LOG_LEVEL_INFO, "📤 Master → Slave2: Sending '%s' to http://%s:%d (attempt %d/%d)", 
           cmd_str, g_comm_config.slave2_ip, g_comm_config.slave2_port,
           attempt + 1, g_comm_config.retry_count);

kernel_log(LOG_LEVEL_INFO, "✅ Master → Slave2: Command '%s' sent successfully!", cmd_str);

kernel_log(LOG_LEVEL_INFO, "📤 Slave2 → Master: Sending status");
kernel_log(LOG_LEVEL_INFO, "   Zone: %d | Irrigating: %s | Remaining: %lus | Pump: %s",
           status->zone_id, status->is_irrigating ? "YES" : "NO",
           status->remaining_seconds, status->pump_running ? "ON" : "OFF");
```

---

## 📊 Exemple de Logs Complets

### **Scénario : Irrigation Programmée à 17:47**

```
[17:47:00] [DEBUG] ⏰ Master: Checking irrigation schedule (current time: 17:47)
[17:47:00] [INFO] 🎯 Master: SCHEDULED IRRIGATION TRIGGERED!
[17:47:00] [INFO]    Zone: zone_test_auto (slot 1)
[17:47:00] [INFO]    Scheduled time: 17:47 (MATCH!)
[17:47:00] [INFO]    Duration: 150s (based on 1500ml/day)

[17:47:00] [INFO] 📤 Master: Preparing irrigation command...
[17:47:00] [INFO]    Target zone: zone_test_auto
[17:47:00] [INFO]    Duration: 150s
[17:47:00] [INFO]    Found in slot 0 (physical zone 1)

[17:47:00] [INFO] 📤 Master → Slave2: Sending irrigation command
[17:47:00] [INFO]    Command: START_IRRIGATION
[17:47:00] [INFO]    Zone ID (hardware): 0
[17:47:00] [INFO]    Zone ID (server): zone_test_auto
[17:47:00] [INFO]    Duration: 150s

[17:47:00] [INFO] 📤 Master → Slave2: Sending 'start_irrigation' to http://192.168.1.61:8082 (attempt 1/3)
[17:47:00] [INFO] ✅ Master → Slave2: Command 'start_irrigation' sent successfully!
[17:47:00] [INFO] ✅ Master → Slave2: Command sent successfully!
[17:47:00] [INFO]    Irrigation timer set: 150s

[17:47:00] [INFO] 📥 Slave2: Incoming POST /api/irrigation/command from 192.168.1.61
[17:47:00] [INFO] ✅ Slave2: Received command 'start_irrigation' for zone 0 (duration: 150s)

[17:47:00] [INFO] 💧 Slave2: STARTING IRRIGATION
[17:47:00] [INFO]    Zone ID (hardware): 1
[17:47:00] [INFO]    Duration: 150s
[17:47:00] [INFO] 🔌 Slave2: Activating zone 1 relay (GPIO 15)
[17:47:00] [DEBUG] Zone 1 relay: ON
[17:47:00] [INFO] 🔌 Slave2: Activating pump (GPIO 5)
[17:47:00] [INFO] Pump: ON
[17:47:00] [INFO] ✅ Slave2: Irrigation started successfully!
[17:47:00] [INFO]    End time: 270s (in 150s)
[17:47:00] [INFO]    Total irrigations: 1

[17:47:10] [INFO] 📤 Slave2: Publishing status (interval: 10000ms, elapsed: 10001ms)
[17:47:10] [INFO] 📤 Slave2 → Master: Sending status
[17:47:10] [INFO]    Zone: 0 | Irrigating: YES | Remaining: 140s | Pump: ON
[17:47:10] [INFO] ✅ Slave2: Status published successfully

[17:49:30] [INFO] ⏱️  Slave2: Irrigation timer EXPIRED
[17:49:30] [INFO]    Zone 1 irrigation complete

[17:49:30] [INFO] 🛑 Slave2: STOPPING IRRIGATION
[17:49:30] [INFO]    Zone: 1
[17:49:30] [INFO] 🔌 Slave2: Deactivating zone 1 relay
[17:49:30] [DEBUG] Zone 1 relay: OFF
[17:49:30] [INFO] 🔌 Slave2: Deactivating pump
[17:49:30] [INFO] Pump: OFF
[17:49:30] [INFO] ✅ Slave2: Irrigation stopped successfully

[17:49:30] [INFO] ⏱️  Master: Irrigation timer expired
[17:49:30] [INFO]    Irrigation should be complete on Slave2
```

---

## 🎨 Conventions de Logs

### **Emojis Utilisés**

| Emoji | Signification |
|-------|---------------|
| 🎯 | Déclenchement d'événement important |
| 📤 | Envoi de données/commande |
| 📥 | Réception de données/commande |
| ✅ | Succès d'opération |
| ❌ | Échec d'opération |
| ⚠️ | Avertissement |
| 💧 | Irrigation (démarrage) |
| 🛑 | Arrêt |
| 🔌 | Activation/désactivation GPIO |
| ⏰ | Vérification d'heure |
| ⏱️ | Timer/timeout |
| 🔄 | Loop/cycle |

---

### **Niveaux de Log**

| Niveau | Usage |
|--------|-------|
| **DEBUG** | Vérifications périodiques, détails techniques |
| **INFO** | Événements importants, changements d'état |
| **WARN** | Situations anormales mais gérées |
| **ERROR** | Erreurs nécessitant attention |

---

### **Format des Messages**

```
[Niveau] [Emoji] [App]: [Action]
         [Détails indentés avec 3 espaces]
```

**Exemple** :
```
[INFO] 📤 Master → Slave2: Sending irrigation command
       Command: START_IRRIGATION
       Zone ID (hardware): 0
       Duration: 150s
```

---

## 🔧 Utilisation

### **Activer les Logs en Temps Réel**

```bash
D'O-Core> log_echo on
```

### **Filtrer par Niveau**

```bash
D'O-Core> log_filter info   # Afficher INFO et supérieur
D'O-Core> log_filter debug  # Afficher DEBUG et supérieur
```

### **Afficher l'Historique**

```bash
D'O-Core> dmesg 50  # Afficher les 50 derniers messages
```

### **Rechercher dans les Logs**

```bash
D'O-Core> dmesg 100 | grep "IRRIGATION"
D'O-Core> dmesg 100 | grep "Slave2"
```

---

## 📈 Bénéfices

1. ✅ **Traçabilité complète** : Chaque étape est visible
2. ✅ **Debugging facile** : Identifier rapidement où ça bloque
3. ✅ **Compréhension du flux** : Voir l'enchaînement des événements
4. ✅ **Monitoring** : Surveiller le système en production
5. ✅ **Documentation vivante** : Les logs expliquent le comportement

---

**Dernière mise à jour : 2025-10-17**
