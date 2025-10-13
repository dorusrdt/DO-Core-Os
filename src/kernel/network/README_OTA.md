# 🚀 OTA Manager - DO-Core OS

## Vue d'Ensemble

Le **OTA Manager** permet la mise à jour Over-The-Air (OTA) du firmware DO-Core OS via une interface web élégante fournie par la librairie **ElegantOTA**.

## 🎯 Fonctionnalités

- ✅ Interface web moderne et intuitive
- ✅ Upload firmware (.bin) via navigateur
- ✅ Upload filesystem (SPIFFS/LittleFS)
- ✅ Barre de progression en temps réel
- ✅ Authentification optionnelle
- ✅ Statistiques OTA (succès/échecs)
- ✅ Intégration harmonieuse avec DO-Core OS
- ✅ Mode dégradé si échec

## 📦 Architecture

```
OTA Manager
├── ota_manager.h         Interface OTA
├── ota_manager.cpp       Implémentation
└── README_OTA.md         Documentation (ce fichier)

Dépendances:
├── ElegantOTA            Interface web OTA
├── WebServer             Serveur HTTP
├── WiFi                  Connectivité réseau
└── Kernel Services       Logs, NVS, etc.
```

## 🚀 Utilisation Rapide

### 1. Démarrer le Serveur OTA

```bash
# Dans le shell DO-Core
D'O-Core> ota_start

✅ OTA Server started on port 8080
🌐 OTA URL: http://192.168.1.100:8080/update
```

### 2. Accéder à l'Interface Web

Ouvrez votre navigateur et allez à l'URL affichée:
```
http://[IP_ESP32]:8080/update
```

### 3. Uploader le Firmware

1. Compilez votre nouveau firmware:
   ```bash
   pio run
   ```

2. Le fichier `.bin` se trouve dans:
   ```
   .pio/build/esp32dev/firmware.bin
   ```

3. Dans l'interface web:
   - Cliquez sur **"Choose File"**
   - Sélectionnez `firmware.bin`
   - Cliquez sur **"Update"**
   - Attendez la fin de l'upload (barre de progression)
   - L'ESP32 redémarre automatiquement

## 📋 Commandes CLI

### `ota_start`
Démarre le serveur web OTA.

```bash
D'O-Core> ota_start
✅ OTA Server started on port 8080
🌐 OTA URL: http://192.168.1.100:8080/update
```

### `ota_stop`
Arrête le serveur web OTA.

```bash
D'O-Core> ota_stop
OTA Server stopped
```

### `ota_status`
Affiche le statut du serveur OTA.

```bash
D'O-Core> ota_status
OTA Status: RUNNING
http://192.168.1.100:8080/update
```

### `ota_url`
Affiche l'URL d'accès OTA.

```bash
D'O-Core> ota_url
http://192.168.1.100:8080/update
```

### `ota_info`
Affiche les informations de version du firmware.

```bash
D'O-Core> ota_info

=== OTA Version Info ===
Version: 1.0.0
Build Date: Oct 12 2025 22:56:00
Chip: ESP32
Free Heap: 245632 bytes
Sketch Size: 1048576 bytes
Free Space: 2097152 bytes
========================
```

### `ota_stats`
Affiche les statistiques OTA.

```bash
D'O-Core> ota_stats

=== OTA Statistics ===
Total Updates: 5
Successful: 4
Failed: 1
State: IDLE
Last Progress: 100%
======================
```

## 🔧 Configuration

### Configuration par Défaut

```cpp
Port: 8080
Hostname: do-core-ota
Username: admin
Password: admin
Auth: Désactivée
Auto Reboot: Activé
```

### Modifier le Port

```cpp
// Dans votre code
ota_manager.set_port(3000);
ota_manager.start();
```

### Activer l'Authentification

```cpp
// Dans votre code
ota_manager.set_auth("admin", "secure_password");
ota_manager.enable_auth(true);
ota_manager.start();
```

## 📊 Workflow Complet

### Scénario: Mise à Jour du Firmware

```
1. Développement
   ├─► Modifier le code
   ├─► Tester localement
   └─► Compiler: pio run

2. Préparation
   ├─► ESP32 connecté au WiFi
   ├─► Démarrer OTA: ota_start
   └─► Noter l'URL affichée

3. Upload
   ├─► Ouvrir navigateur
   ├─► Aller à http://[IP]:8080/update
   ├─► Sélectionner firmware.bin
   ├─► Cliquer "Update"
   └─► Attendre progression 100%

4. Redémarrage
   ├─► ESP32 redémarre automatiquement
   ├─► Nouveau firmware chargé
   └─► Vérifier version: ota_info
```

## 🔐 Sécurité

### Recommandations

1. **Réseau Local Uniquement**
   - N'exposez pas le port OTA sur Internet
   - Utilisez uniquement sur réseau local sécurisé

2. **Authentification**
   - Activez l'authentification en production
   - Utilisez un mot de passe fort

3. **Firewall**
   - Limitez l'accès au port OTA
   - Utilisez un VPN si accès distant nécessaire

4. **Validation**
   - Testez toujours le firmware localement avant OTA
   - Gardez une copie du firmware stable

## 🐛 Dépannage

### Problème: "OTA Server not running"

**Solution:**
```bash
# Vérifier WiFi
D'O-Core> wifi

# Démarrer OTA
D'O-Core> ota_start
```

### Problème: "Failed to start OTA: WIFI_ERROR"

**Solution:**
```bash
# Connecter WiFi d'abord
D'O-Core> wifi_save MySSID MyPassword
D'O-Core> wifi_auto

# Puis démarrer OTA
D'O-Core> ota_start
```

### Problème: Upload échoue à 50%

**Causes possibles:**
- Signal WiFi faible
- Fichier .bin corrompu
- Pas assez d'espace mémoire

**Solutions:**
1. Rapprocher l'ESP32 du routeur
2. Recompiler le firmware
3. Vérifier l'espace disponible: `ota_info`

### Problème: ESP32 ne redémarre pas après OTA

**Solution:**
```bash
# Redémarrage manuel
D'O-Core> reboot

# Ou bouton RESET physique
```

## 📈 Logs OTA

Les logs OTA sont intégrés au système de logs DO-Core:

```bash
# Voir les logs OTA
D'O-Core> dmesg | grep OTA

# Exemples de logs
[INFO] OTA Manager init
[INFO] OTA Manager initialized successfully
[INFO] ✅ OTA Server started on port 8080
[INFO] 🔄 OTA Update Started
[INFO] 📥 OTA Progress: 10%
[INFO] 📥 OTA Progress: 50%
[INFO] 📥 OTA Progress: 100%
[INFO] ✅ OTA Update Successful!
```

## 🔄 Intégration avec CI/CD

### Exemple: Upload Automatique

```bash
#!/bin/bash
# Script: deploy_ota.sh

ESP32_IP="192.168.1.100"
OTA_PORT="8080"
FIRMWARE=".pio/build/esp32dev/firmware.bin"

# Compiler
pio run

# Upload via curl
curl -F "update=@${FIRMWARE}" \
     http://${ESP32_IP}:${OTA_PORT}/update

echo "OTA Update sent!"
```

## 📚 Références

- **ElegantOTA**: https://github.com/ayushsharma82/ElegantOTA
- **ESP32 OTA**: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/ota.html
- **PlatformIO OTA**: https://docs.platformio.org/en/latest/platforms/espressif32.html#over-the-air-ota-update

## 🎯 Prochaines Étapes

- [ ] Tester l'OTA avec un firmware simple
- [ ] Activer l'authentification
- [ ] Intégrer dans votre workflow de déploiement
- [ ] Configurer le port selon vos besoins
- [ ] Documenter votre processus OTA spécifique

---

**Version**: 1.0.0
**Date**: 2025-10-12
**Auteur**: DO-Core OS Team
**Licence**: MIT
