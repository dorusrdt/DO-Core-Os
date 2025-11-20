# 🚀 GUIDE DE DÉMARRAGE RAPIDE - D'O-CORE OS

## ⚡ 5 minutes pour démarrer

### Étape 1: Compilation
```bash
# Installer PlatformIO (si nécessaire)
pip install platformio

# Compiler le projet
cd /home/dorus/Documents/GitHub/DO-Core-Os
pio run

# Uploader sur ESP32
pio run -t upload
```

### Étape 2: Connexion série
```bash
# Ouvrir le moniteur série
pio device monitor --baud 115200

# Vous devriez voir:
# === D'O-Core Init ===
# Ver: 1.0.0
# ...
# === D'O-Core Ready ===
```

### Étape 3: Premiers tests
```bash
# Afficher l'aide
help

# Voir l'état du système
status

# Afficher les logs
dmesg

# Voir les tâches
tasks
```

---

## 📡 Connexion WiFi

### Sauvegarder les credentials
```bash
wifi_save MyNetwork MyPassword123
```

### Connexion automatique
```bash
wifi_auto
```

### Vérifier la connexion
```bash
wifi_stats
```

### Scanner les réseaux disponibles
```bash
wifi_scan
```

---

## 🕐 Synchronisation temps

### Synchroniser avec NTP (après WiFi)
```bash
ntp_sync
```

### Vérifier le statut
```bash
ntp_status
```

### Configurer le fuseau horaire
```bash
# UTC+1 (Europe centrale)
ntp_timezone 1

# UTC-5 (Est USA)
ntp_timezone -5

# UTC+1 avec heure d'été
ntp_timezone 1 1
```

### Diagnostic complet
```bash
ntp_test
```

---

## 📊 Monitoring système

### État global
```bash
status
```

### Utilisation mémoire
```bash
memory
```

### Liste des tâches
```bash
tasks
```

### Uptime
```bash
uptime
```

### Logs système
```bash
dmesg
dmesg 10        # Afficher les 10 derniers logs
```

---

## 🔍 Debugging

### Afficher les logs en temps réel
```bash
log_echo on
```

### Désactiver l'affichage des logs
```bash
log_echo off
```

### Filtrer les logs par niveau
```bash
logs info       # Afficher les INFO
logs error      # Afficher les ERREURS
logs warn       # Afficher les AVERTISSEMENTS
```

### Filtrer par source
```bash
logs WiFi       # Logs du WiFi
logs NTP        # Logs du NTP
```

### Valider l'intégrité du système de logs
```bash
validate_log
```

---

## 🎮 Gestion des applications

### Lister les applications
```bash
app_list
```

### Démarrer une application
```bash
app_start 1
```

### Arrêter une application
```bash
app_stop 1
```

### Redémarrer une application
```bash
app_restart 1
```

### Mettre en pause
```bash
app_pause 1
```

### Reprendre
```bash
app_resume 1
```

### Informations détaillées
```bash
app_info 1
```

---

## 🌐 Tests réseau

### Test de connectivité
```bash
network_test
```

### Afficher les informations réseau
```bash
network
```

### Statistiques WiFi détaillées
```bash
wifi_stats
```

---

## 📝 Créer votre première application

### 1. Créer le fichier `my_app.cpp`
```cpp
#include "kernel/core/kernel.h"

void my_app_task(void* parameter) {
    kernel_log(LOG_LEVEL_INFO, "Mon app démarre!");
    
    int counter = 0;
    while(1) {
        kernel_log(LOG_LEVEL_INFO, "Compteur: %d", counter++);
        vTaskDelay(pdMS_TO_TICKS(5000));  // Attendre 5 secondes
    }
}
```

### 2. Enregistrer dans `main.cpp`
```cpp
// Dans setup(), après app_manager_init()
app_register(1, "MyApp", "Ma première application", APP_TYPE_USER);
```

### 3. Créer la tâche
```cpp
// Dans setup(), après app_manager_init()
uint8_t task_id;
task_create_pinned_to_core(
    "MyApp",
    my_app_task,
    NULL,
    PRIORITY_NORMAL,
    STACK_SIZE_NORMAL,
    0,
    &task_id
);
```

### 4. Compiler et tester
```bash
pio run -t upload
pio device monitor --baud 115200

# Dans le CLI:
app_list      # Voir votre app
app_start 1   # Démarrer
```

---

## 🔧 Configuration personnalisée

### Fichier: `src/kernel/core/minimal_config.h`

```cpp
// Tailles de stack (en bytes)
#define STACK_SIZE_SMALL 2048
#define STACK_SIZE_NORMAL 4096
#define STACK_SIZE_LARGE 8192
#define STACK_SIZE_SHELL 8192

// Limites système
#define MAX_TASKS 32
#define MAX_APPS 16
#define MAX_LOG_MESSAGES 256
#define MAX_COMMANDS 100

// Priorités des tâches
#define PRIORITY_LOW 5
#define PRIORITY_NORMAL 10
#define PRIORITY_HIGH 15

// Timeouts (en millisecondes)
#define NTP_SYNC_TIMEOUT_MS 30000
#define WIFI_CONNECT_TIMEOUT_MS 30000
#define HTTP_TIMEOUT_MS 10000

// Fuseau horaire par défaut (en secondes)
#define NTP_GMT_OFFSET_SEC 3600      // UTC+1
#define NTP_DAYLIGHT_OFFSET_SEC 3600 // +1h en été
```

---

## 🐛 Troubleshooting

### Le système ne démarre pas
```bash
# Vérifier les logs
dmesg

# Vérifier la mémoire
memory

# Vérifier les tâches
tasks
```

### WiFi ne se connecte pas
```bash
# Vérifier les credentials
wifi_stats

# Rescanner les réseaux
wifi_scan

# Réessayer
wifi_auto
```

### NTP ne synchronise pas
```bash
# Diagnostic complet
ntp_test

# Vérifier la connexion WiFi
network_test

# Vérifier le fuseau horaire
ntp_timezone
```

### Mémoire insuffisante
```bash
# Vérifier l'utilisation
memory

# Réduire les tailles de stack
# Éditer minimal_config.h
```

---

## 📚 Commandes complètes

### Système
| Commande | Description |
|----------|-------------|
| `help` | Affiche l'aide |
| `status` | État du système |
| `tasks` | Liste des tâches |
| `memory` | Utilisation mémoire |
| `dmesg [n]` | Logs système |
| `logs [filter]` | Logs filtrés |
| `uptime` | Uptime système |
| `version` | Version |
| `clear` | Effacer l'écran |
| `neofetch` | Afficher le logo |

### WiFi
| Commande | Description |
|----------|-------------|
| `wifi_save <ssid> <pwd>` | Sauvegarder credentials |
| `wifi_auto` | Connexion auto |
| `wifi_scan` | Scanner réseaux |
| `wifi_stats` | Statistiques WiFi |
| `wifi_reconnect` | Forcer reconnexion |
| `wifi_clear` | Effacer credentials |

### NTP
| Commande | Description |
|----------|-------------|
| `ntp_sync` | Synchronisation manuelle |
| `ntp_status` | État NTP |
| `ntp_test` | Diagnostic NTP |
| `ntp_timezone <offset>` | Configurer fuseau |
| `ntp_utilities` | Utilitaires temps |

### Applications
| Commande | Description |
|----------|-------------|
| `app_list` | Liste applications |
| `app_start <id>` | Démarrer app |
| `app_stop <id>` | Arrêter app |
| `app_restart <id>` | Redémarrer app |
| `app_pause <id>` | Mettre en pause |
| `app_resume <id>` | Reprendre |
| `app_info <id>` | Infos détaillées |

### Logs
| Commande | Description |
|----------|-------------|
| `log_echo [on\|off]` | Affichage temps réel |
| `test_log` | Test système logs |
| `debug_log` | Debug logs |
| `validate_log` | Valider intégrité |

### Réseau
| Commande | Description |
|----------|-------------|
| `network` | Vue d'ensemble réseau |
| `network_test` | Test connectivité |
| `http_get <url>` | Requête GET |
| `http_post <url> <data>` | Requête POST |

---

## 💡 Astuces

### 1. Afficher les logs en temps réel
```bash
log_echo on
# Maintenant tous les logs s'affichent immédiatement
```

### 2. Sauvegarder les credentials WiFi
```bash
wifi_save MyNetwork MyPassword
# Les credentials sont sauvegardés en flash
# Ils seront utilisés au redémarrage
```

### 3. Créer un alias pour les commandes longues
```bash
# Dans votre terminal (pas dans le CLI)
alias do-monitor='pio device monitor --baud 115200'
```

### 4. Monitorer en continu
```bash
# Terminal 1: Compiler et uploader
pio run -t upload

# Terminal 2: Monitorer
pio device monitor --baud 115200
```

### 5. Déboguer les problèmes de mémoire
```bash
memory          # Voir l'utilisation
tasks           # Voir les tâches et leur stack
dmesg           # Voir les erreurs
```

---

## 🎯 Prochaines étapes

### Niveau 1: Débutant
- ✅ Compiler et uploader
- ✅ Tester les commandes CLI
- ✅ Connecter WiFi
- ✅ Synchroniser l'heure

### Niveau 2: Intermédiaire
- ✅ Créer une application simple
- ✅ Utiliser le logging
- ✅ Monitorer les ressources
- ✅ Configurer le fuseau horaire

### Niveau 3: Avancé
- ✅ Créer des applications complexes
- ✅ Utiliser les tâches multi-core
- ✅ Implémenter des commandes CLI personnalisées
- ✅ Optimiser la mémoire

---

## 📖 Documentation complète

Pour plus de détails, consultez:
- `README.md` - Documentation générale
- `ANALYSE_ARCHITECTURE_COMPLETE.md` - Architecture détaillée
- `RESUME_ANALYSE_EXECUTIVE.md` - Vue d'ensemble

---

## ✨ Bon développement!

Vous êtes maintenant prêt à utiliser D'O-Core OS. Amusez-vous bien! 🎉

*Pour toute question, consultez la documentation ou explorez le code source.*
