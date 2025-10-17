# Configuration des 3 Devices ESP32

Ce dossier contient les fichiers de configuration `main.cpp` pour chaque device de l'architecture Master-Slave.

---

## 📁 Fichiers

| Fichier | Device | Rôle | IP |
|---------|--------|------|-----|
| `main_device1_master.cpp` | ESP32 #1 | Master (HTTP + Serveur) | 192.168.1.100:8080 |
| `main_device2_slave_sensors.cpp` | ESP32 #2 | Slave Sensors (12 capteurs ADC) | 192.168.1.101:8081 |
| `main_device3_slave_relays.cpp` | ESP32 #3 | Slave Relays (4 zones + pompe) | 192.168.1.102:8082 |

---

## 🚀 Procédure de Compilation

### Device 1 : Master

```bash
# 1. Copier configuration
cp DEVICE_CONFIGS/main_device1_master.cpp src/main.cpp

# 2. Compiler
pio run

# 3. Flasher
pio run --target upload --upload-port /dev/ttyUSB0

# 4. Monitorer
pio device monitor --port /dev/ttyUSB0
```

### Device 2 : Slave Sensors

```bash
# 1. Copier configuration
cp DEVICE_CONFIGS/main_device2_slave_sensors.cpp src/main.cpp

# 2. Compiler
pio run

# 3. Flasher
pio run --target upload --upload-port /dev/ttyUSB1

# 4. Monitorer
pio device monitor --port /dev/ttyUSB1
```

### Device 3 : Slave Relays

```bash
# 1. Copier configuration
cp DEVICE_CONFIGS/main_device3_slave_relays.cpp src/main.cpp

# 2. Compiler
pio run

# 3. Flasher
pio run --target upload --upload-port /dev/ttyUSB2

# 4. Monitorer
pio device monitor --port /dev/ttyUSB2
```

---

## ⚙️ Configuration WiFi

Sur chaque device, après le premier boot :

```bash
# Via Serial Monitor
wifi_save <SSID> <PASSWORD>

# Exemple
wifi_save MonReseauWiFi MotDePasse123

# Vérifier
wifi_status
```

---

## 🔧 Personnalisation

### Modifier les IPs

Éditer les fichiers `main_deviceX.cpp` :

```cpp
// Device 1 (Master)
#define DEVICE_IP "192.168.1.100"
#define SLAVE1_IP "192.168.1.101"
#define SLAVE2_IP "192.168.1.102"

// Device 2 (Slave Sensors)
#define DEVICE_IP "192.168.1.101"
#define MASTER_IP "192.168.1.100"

// Device 3 (Slave Relays)
#define DEVICE_IP "192.168.1.102"
#define MASTER_IP "192.168.1.100"
```

### Modifier les Ports

```cpp
// Device 1
#define MASTER_HTTP_PORT 8080

// Device 2
#define SLAVE1_HTTP_PORT 8081

// Device 3
#define SLAVE2_HTTP_PORT 8082
```

### Modifier le Serveur FastAPI

Dans `main_device1_master.cpp` :

```cpp
strcpy(master_config.server_url, "http://10.232.133.53:3000");
```

---

## 🧪 Tests de Connectivité

### Test 1 : Ping

```bash
ping 192.168.1.100
ping 192.168.1.101
ping 192.168.1.102
```

### Test 2 : Endpoints HTTP

```bash
# Master
curl http://192.168.1.100:8080/api/master/stats

# Slave Sensors
curl http://192.168.1.101:8081/api/sensors/status
curl http://192.168.1.101:8081/api/sensors/data

# Slave Relays
curl http://192.168.1.102:8082/api/irrigation/status
```

### Test 3 : Communication Inter-Device

```bash
# Vérifier logs Master (doit recevoir données toutes les 5s)
# Serial Monitor Device 1 :
# [DEBUG] Master: Received sensor data from Slave1

# Vérifier logs Slave1 (doit envoyer données)
# Serial Monitor Device 2 :
# [DEBUG] IrrigComm: Sensor data sent successfully
```

---

## 📊 Logs Attendus

### Device 1 (Master)

```
=== D'O-Core Init - DEVICE 1 (MASTER) ===
WiFi connected
IP: 192.168.1.100
IrrigComm: HTTP mode initialized
  Master: 192.168.1.100:8080
  Slave1: 192.168.1.101:8081
  Slave2: 192.168.1.102:8082
Master app registered
MasterHTTP: Server started on port 8080
=== D'O-Core Ready - MASTER ===
IP: 192.168.1.100:8080
Slave1: 192.168.1.101:8081
Slave2: 192.168.1.102:8082
```

### Device 2 (Slave Sensors)

```
=== D'O-Core Init - DEVICE 2 (SLAVE SENSORS) ===
WiFi connected
IP: 192.168.1.101
IrrigComm: HTTP mode initialized
  Master: 192.168.1.100:8080
  Slave1: 192.168.1.101:8081
Slave Sensors app registered
IrrigSlaveSensors: Initializing...
  Mode: REAL
  Read interval: 5000ms
  Samples per read: 5
HTTP server started on port 8081
=== D'O-Core Ready - SLAVE SENSORS ===
IP: 192.168.1.101:8081
Master: 192.168.1.100:8080
Hardware: 12 ADC moisture sensors
```

### Device 3 (Slave Relays)

```
=== D'O-Core Init - DEVICE 3 (SLAVE RELAYS) ===
WiFi connected
IP: 192.168.1.102
IrrigComm: HTTP mode initialized
  Master: 192.168.1.100:8080
  Slave2: 192.168.1.102:8082
Slave Relays app registered
IrrigSlaveRelays: Initializing...
  Safety timeout: 300000ms
Relay pins initialized:
  Zones: 2, 4, 16, 17
  Pump: 5
HTTP server started on port 8082
=== D'O-Core Ready - SLAVE RELAYS ===
IP: 192.168.1.102:8082
Master: 192.168.1.100:8080
Hardware: 4 zone relays + pump relay
```

---

## 🚨 Troubleshooting

### Problème : WiFi ne se connecte pas

```bash
# Vérifier credentials
wifi_status

# Reconfigurer
wifi_save <SSID> <PASSWORD>

# Redémarrer
# Appuyer sur bouton RESET
```

### Problème : IP incorrecte

```bash
# Vérifier IP actuelle
wifi_status

# Si DHCP au lieu de fixe, vérifier code :
# WiFi.config(local_IP, gateway, subnet) doit être AVANT WiFi.begin()
```

### Problème : Devices ne communiquent pas

```bash
# 1. Vérifier ping
ping 192.168.1.100
ping 192.168.1.101
ping 192.168.1.102

# 2. Vérifier firewall (désactiver temporairement)
sudo ufw disable

# 3. Vérifier serveurs HTTP démarrés
curl http://192.168.1.100:8080/api/master/stats
curl http://192.168.1.101:8081/api/sensors/status
curl http://192.168.1.102:8082/api/irrigation/status

# 4. Vérifier logs Serial Monitor
# Device 1 : "MasterHTTP: Server started on port 8080"
# Device 2 : "HTTP server started on port 8081"
# Device 3 : "HTTP server started on port 8082"
```

### Problème : Timeout HTTP

```bash
# Augmenter timeout dans irrig_communication.cpp
.http_timeout_ms = 10000,  // 10 secondes au lieu de 5
```

---

## 📝 Checklist Déploiement

### Avant Compilation

- [ ] IPs configurées correctement dans les 3 fichiers
- [ ] Ports HTTP configurés (8080, 8081, 8082)
- [ ] URL serveur FastAPI correcte (Device 1)
- [ ] Gateway et subnet corrects

### Après Flash

- [ ] Device 1 : WiFi connecté, IP = 192.168.1.100
- [ ] Device 2 : WiFi connecté, IP = 192.168.1.101
- [ ] Device 3 : WiFi connecté, IP = 192.168.1.102
- [ ] Ping OK entre tous les devices
- [ ] Serveurs HTTP démarrés (curl OK)
- [ ] Logs "Received sensor data" sur Device 1
- [ ] Logs "Sensor data sent" sur Device 2

### Tests Fonctionnels

- [ ] Lecture capteurs OK (Device 2)
- [ ] Données reçues par Master (Device 1)
- [ ] Commande test relais OK (Device 3)
- [ ] Irrigation complète OK (120s)
- [ ] Arrêt d'urgence OK
- [ ] Communication serveur FastAPI OK

---

## 🎯 Ordre de Démarrage Recommandé

1. **Device 1 (Master)** - Démarrer en premier
2. **Device 2 (Slave Sensors)** - Démarrer 10s après
3. **Device 3 (Slave Relays)** - Démarrer 10s après

Cet ordre permet au Master d'être prêt à recevoir les données des Slaves dès leur démarrage.

---

## 📞 Support

Pour toute question ou problème, consulter :
- `ARCHITECTURE_MASTER_SLAVE_HTTP.md` - Documentation complète
- Logs Serial Monitor de chaque device
- Tests curl pour vérifier endpoints

**Bonne chance avec le déploiement ! 🚀**
