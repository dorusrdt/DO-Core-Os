# Application Auto-Start Configuration

## 📋 Description

Le fichier `app_autostart_config.h` permet de configurer quelles applications doivent démarrer automatiquement après le boot du système, de manière similaire à Kconfig dans Linux.

## 🔧 Utilisation

### Activer/Désactiver le démarrage automatique

Ouvrez le fichier `include/app_autostart_config.h` et modifiez les définitions :

```c
// Pour activer le démarrage automatique
#define CONFIG_APP_AUTOSTART_ESP32_MASTER    1

// Pour désactiver le démarrage automatique
// #define CONFIG_APP_AUTOSTART_ESP32_MASTER    1  // Commenté = désactivé
```

### Applications disponibles

- **ESP32 Master** (ID: 10)
  - Contrôle le système d'irrigation
  - Communique avec le serveur externe
  - Gère les zones et la planification

- **ESP32 Sensor** (ID: 11)
  - Lit les 12 capteurs d'humidité
  - Envoie les données au master via WebSocket

- **ESP32 Com** (ID: 12)
  - Contrôle les relais de zones et la pompe
  - Reçoit les commandes du master

## 📝 Exemples de configuration

### Configuration complète (toutes les apps)
```c
#define CONFIG_APP_AUTOSTART_ESP32_MASTER    1
#define CONFIG_APP_AUTOSTART_ESP32_SENSOR    1
#define CONFIG_APP_AUTOSTART_ESP32_COM       1
```

### Configuration minimale (master seulement)
```c
#define CONFIG_APP_AUTOSTART_ESP32_MASTER    1
// #define CONFIG_APP_AUTOSTART_ESP32_SENSOR    1
// #define CONFIG_APP_AUTOSTART_ESP32_COM       1
```

### Configuration test (sensor seulement)
```c
// #define CONFIG_APP_AUTOSTART_ESP32_MASTER    1
#define CONFIG_APP_AUTOSTART_ESP32_SENSOR    1
// #define CONFIG_APP_AUTOSTART_ESP32_COM       1
```

## 🔍 Logs

Après le boot, les logs affichent quelles applications ont été démarrées automatiquement :

```
[INFO] Checking auto-start configuration...
[INFO] Auto-started: ESP32_master (id=10)
[INFO] Auto-started: ESP32_sensor (id=11)
[INFO] Auto-started: ESP32_com (id=12)
```

Si une app est désactivée :
```
[DEBUG] ESP32_master auto-start disabled
```

## ⚙️ Fonctionnement technique

1. Le fichier `app_autostart_config.h` définit des macros de préprocesseur
2. `main.cpp` inclut ce fichier et vérifie les macros avec `#if`
3. Si une app est activée, `app_start(app_id)` est appelé automatiquement
4. Les apps désactivées peuvent toujours être démarrées manuellement via CLI : `app_start <id>`

## 🎯 Avantages

- ✅ Configuration simple et claire
- ✅ Pas de code à modifier dans `main.cpp`
- ✅ Compilation conditionnelle (les apps désactivées ne consomment pas de mémoire)
- ✅ Style Kconfig familier pour les développeurs Linux
- ✅ Facile à documenter et maintenir

## 📌 Notes

- Les modifications nécessitent une recompilation
- Les apps peuvent toujours être démarrées/arrêtées manuellement via CLI
- L'ordre de démarrage suit l'ordre dans le fichier de configuration

