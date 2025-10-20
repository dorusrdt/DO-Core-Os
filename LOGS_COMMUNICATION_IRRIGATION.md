# 📊 Logs de Communication du Système d'Irrigation

Ce document liste tous les logs exposés pour suivre la communication entre les différents composants du système.

---

## 🔄 Communication Slave1 (Sensors) → Master

### **Côté Slave1 (Envoi)**

```
[INFO] IrrigComm: Sending sensor data to Master (attempt 1/3)
[DEBUG] IrrigComm: Sensor data sent successfully
```

**OU en cas d'erreur :**

```
[WARN] IrrigComm: HTTP error -1 (attempt 1/3)
[ERROR] IrrigComm: Failed to send sensor data after 3 attempts
```

### **Côté Master (Réception)**

```
[INFO] 📥 MasterHTTP: Incoming POST /api/sensors/data from 192.168.1.61
[INFO] ✅ Master: Received sensor data from Slave1
[INFO]    Temp: 23.5°C | Humidity: 65.2% | Pressure: 1013.2 hPa
[INFO]    Battery: 85% | Signal: -45 dBm
```

**OU en cas d'erreur :**

```
[ERROR] MasterHTTP: Missing body
[ERROR] MasterHTTP: Invalid JSON: ...
```

---

## 🔄 Communication Master → Slave2 (Relays)

### **Côté Master (Envoi)**

```
[INFO] 📤 Master → Slave2: Sending 'start_irrigation' to http://192.168.1.61:8082 (attempt 1/3)
[DEBUG]    Payload: {"command":"start_irrigation","zone_id":1,"duration_seconds":600,...}
[INFO] ✅ Master → Slave2: Command 'start_irrigation' sent successfully!
```

**OU en cas d'erreur :**

```
[WARN] ⚠️ Master → Slave2: HTTP error -1 (attempt 1/3)
[ERROR] IrrigComm: Failed to send command after 3 attempts
```

### **Côté Slave2 (Réception)**

```
[INFO] 📥 Slave2: Incoming POST /api/irrigation/command from 192.168.1.61
[DEBUG] Slave2: Body: {"command":"start_irrigation",...}
[INFO] ✅ Slave2: Received command 'start_irrigation' for zone 1 (duration: 600s)
[INFO] Relay: Starting irrigation on zone 1 for 600 seconds
```

**OU en cas d'erreur :**

```
[ERROR] Slave2: Missing body
[ERROR] Slave2: Invalid JSON: ...
```

---

## 🔄 Communication Slave2 (Relays) → Master (Statut)

### **Côté Slave2 (Envoi)**

```
[DEBUG] IrrigComm: Status sent successfully
```

**OU en cas d'erreur :**

```
[WARN] IrrigComm: Failed to send status (HTTP -1)
```

### **Côté Master (Réception)**

```
[DEBUG] Master: Received irrigation status from Slave2
[DEBUG]   Zone: 1, Irrigating: YES, Remaining: 580s
```

---

## 🔄 Communication Master → Serveur FastAPI

### **Côté Master (Envoi)**

```
[INFO] Master: Sending sensor data to server
[INFO] Master: Data sent to server successfully
```

**OU en cas d'erreur :**

```
[ERROR] Master: Failed to send data to server (HTTP -1)
```

---

## 📋 Commandes pour Voir les Logs

### **Voir tous les logs**

```bash
D'O-Core> log_show
```

### **Filtrer par niveau**

```bash
# Voir uniquement les erreurs
D'O-Core> log_filter error

# Voir info et au-dessus
D'O-Core> log_filter info

# Voir tout (debug inclus)
D'O-Core> log_filter debug
```

### **Voir les logs en temps réel**

```bash
# Afficher les logs toutes les 2 secondes
D'O-Core> log_show
# Attendre 2 secondes
D'O-Core> log_show
# Répéter...
```

### **Chercher un mot-clé**

```bash
# Chercher "Slave2"
D'O-Core> log_search Slave2

# Chercher "error"
D'O-Core> log_search error
```

---

## 🧪 Scénario de Test Complet

### **1. Démarrer les 3 apps**

```bash
D'O-Core> irrig_start_all_apps
```

**Logs attendus :**

```
[INFO] Master HTTP server started on port 8080
[INFO] HTTP server started on port 8081  (Slave1)
[INFO] HTTP server started on port 8082  (Slave2)
```

### **2. Slave1 envoie des données au Master**

**Logs attendus (toutes les 15 secondes) :**

```
[INFO] 📥 MasterHTTP: Incoming POST /api/sensors/data from 192.168.1.61
[INFO] ✅ Master: Received sensor data from Slave1
[INFO]    Temp: 23.5°C | Humidity: 65.2% | Pressure: 1013.2 hPa
```

### **3. Master envoie une commande au Slave2**

**Commande de test :**

```bash
# TODO: Ajouter une commande CLI pour tester
# Exemple: irrig_test_command start_irrigation 1 600
```

**Logs attendus :**

```
[INFO] 📤 Master → Slave2: Sending 'start_irrigation' to http://192.168.1.61:8082 (attempt 1/3)
[INFO] 📥 Slave2: Incoming POST /api/irrigation/command from 192.168.1.61
[INFO] ✅ Slave2: Received command 'start_irrigation' for zone 1 (duration: 600s)
[INFO] ✅ Master → Slave2: Command 'start_irrigation' sent successfully!
```

---

## ❌ Diagnostic des Erreurs

### **Erreur : "Failed to send sensor data after 3 attempts"**

**Causes possibles :**
- Master HTTP server pas démarré
- Mauvaise IP configurée dans `irrig_config_master`
- Port 8080 déjà utilisé
- WiFi déconnecté

**Solution :**
```bash
# Vérifier config
D'O-Core> irrig_config_show

# Vérifier que Master tourne
D'O-Core> app_list

# Vérifier WiFi
D'O-Core> wifi_status
```

### **Erreur : "Failed to send command after 3 attempts"**

**Causes possibles :**
- Slave2 HTTP server pas démarré
- Mauvaise IP configurée dans `irrig_config_slave2`
- Port 8082 déjà utilisé

**Solution :**
```bash
# Vérifier config
D'O-Core> irrig_config_show

# Vérifier que Slave2 tourne
D'O-Core> app_list

# Tester manuellement avec curl
curl -X POST http://192.168.1.61:8082/api/irrigation/command \
  -H "Content-Type: application/json" \
  -d '{"command":"test_relay","zone_id":1,"duration_seconds":5}'
```

---

## 📈 Statistiques de Communication

### **Voir les stats HTTP**

```bash
D'O-Core> http_stats
```

**Affiche :**
- Nombre de requêtes envoyées
- Nombre de succès/échecs
- Temps de réponse moyen

---

## 🎯 Résumé des Emojis dans les Logs

| Emoji | Signification |
|-------|---------------|
| 📥 | Réception de données (incoming) |
| 📤 | Envoi de données (outgoing) |
| ✅ | Succès |
| ⚠️ | Avertissement (retry) |
| ❌ | Erreur |

---

## 📝 Notes

- Les logs **DEBUG** ne s'affichent que si le niveau de log est configuré à DEBUG
- Les logs **INFO** s'affichent par défaut
- Les logs sont stockés dans un buffer circulaire de 1000 messages
- Utiliser `log_save` pour sauvegarder les logs en NVS

---

**Dernière mise à jour : 2025-10-17**
