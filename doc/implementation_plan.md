# Plan d'Implémentation D'O-Core

## Directive d'Optimisation Mémoire Flash

**IMPORTANT** : À chaque implémentation, privilégier la minimisation de l'utilisation de la mémoire Flash :
- Utiliser des chaînes de caractères courtes et des macros
- Implémenter un mode minimaliste avec compilation conditionnelle
- Optimiser les structures de données et les buffers
- Réduire les logs verbeux en mode production
- Utiliser des build flags pour désactiver les fonctionnalités non essentielles

## Phase 1: Core System ✅ TERMINÉE

### Étape 1.1: Kernel Core ✅ TERMINÉE
- [x] Système de gestion d'erreurs
- [x] Types de base et constantes
- [x] Initialisation du système
- [x] Mode minimaliste avec optimisations mémoire

### Étape 1.2: Task Manager ✅ TERMINÉE
- [x] Gestion des tâches FreeRTOS
- [x] Création et suppression de tâches
- [x] Surveillance des performances
- [x] Version optimisée pour mémoire Flash

### Étape 1.3: Memory Manager ✅ TERMINÉE
- [x] Gestion de la mémoire heap
- [x] Surveillance de la fragmentation
- [x] Statistiques d'utilisation
- [x] Optimisations pour ESP32

### Étape 1.4: Log System ✅ TERMINÉE
- [x] Système de logs avec niveaux
- [x] Buffer circulaire optimisé
- [x] Affichage en temps réel
- [x] Validation d'intégrité

### Étape 1.5: System Monitor ✅ TERMINÉE
- [x] Surveillance de la santé système
- [x] Métriques de performance
- [x] Détection d'anomalies
- [x] Mode minimaliste

## Phase 2: HAL (Hardware Abstraction Layer) ⏸️ EN PAUSE

**NOTE** : Phase mise en pause pour optimiser la mémoire Flash. L'implémentation GPIO a été supprimée pour libérer de l'espace.

### Étape 2.1: GPIO Manager ❌ SUPPRIMÉE
- ~~[ ] Configuration des pins GPIO~~
- ~~[ ] Lecture/écriture des états~~
- ~~[ ] Gestion des interruptions~~
- ~~[ ] Commandes shell~~
- ~~[ ] Mode minimaliste~~

**Raison de suppression** : Conflits de noms avec ESP32 et utilisation excessive de mémoire Flash.

### Étape 2.2: ADC Manager ⏸️ EN ATTENTE
- [ ] Configuration des canaux ADC
- [ ] Lecture des valeurs analogiques
- [ ] Calibration et filtrage
- [ ] Commandes shell
- [ ] Mode minimaliste

### Étape 2.3: PWM Manager ⏸️ EN ATTENTE
- [ ] Configuration des canaux PWM
- [ ] Contrôle de fréquence et duty cycle
- [ ] Synchronisation des canaux
- [ ] Commandes shell
- [ ] Mode minimaliste

## Phase 3: Gestion Réseau ✅ TERMINÉE

### ✅ Étape 3.1: WiFi Manager
- **Statut**: ✅ TERMINÉE
- **Fonctionnalités**:
  - Connexion WiFi automatique et manuelle
  - Sauvegarde des credentials dans NVS
  - Scan des réseaux disponibles
  - Gestion des reconnexions
  - Statistiques WiFi détaillées

### ✅ Étape 3.2: NTP Manager
- **Statut**: ✅ TERMINÉE
- **Fonctionnalités**:
  - Synchronisation NTP automatique
  - Configuration timezone
  - Fonctions utilitaires temporelles
  - Diagnostic de connectivité
  - Gestion des heures de travail

### ⏸️ Étape 3.3: WebSocket Manager
- **Statut**: ⏸️ EN ATTENTE - À implémenter
- **Fonctionnalités prévues**:
  - Serveur WebSocket sur port 81
  - Gestion des clients connectés
  - Envoi de messages individuels et broadcast
  - Événements de connexion/déconnexion
  - Interface de commande complète

### ⏸️ Étape 3.4: HTTP Server Manager
- **Statut**: ⏸️ EN ATTENTE - À implémenter
- **Fonctionnalités prévues**:
  - Serveur HTTP sur port 80
  - Interface web responsive
  - API REST pour le monitoring
  - Routes par défaut (/api/status, /api/system, /api/network)
  - Gestion des routes personnalisées
  - Statistiques de requêtes

## Phase 4: Applications ✅ EN COURS

### ✅ Étape 4.1: Application Manager
- **Statut**: ✅ TERMINÉE
- **Fonctionnalités**:
  - Système de gestion d'applications
  - Enregistrement dynamique des applications
  - Gestion des états (UNLOADED, LOADING, RUNNING, PAUSED, STOPPED, ERROR)
  - Callbacks d'application (init, start, stop, pause, resume, loop)
  - Tâches FreeRTOS dédiées par application
  - Interface de commande complète

### 🔄 Étape 4.2: Applications LED
- **Statut**: 🔄 EN COURS - Problème d'enregistrement à résoudre
- **Applications créées**:
  - **Application LED Simple (ID 1)** ✅ FONCTIONNELLE
    - Pin 26, reste allumée
    - Callback start corrigé dans app_task_wrapper
  - **Application Dual LED (ID 2)** ❌ PROBLÈME D'ENREGISTREMENT
    - Pins 2 et 26, clignotement simultané
    - 2 tâches FreeRTOS épinglées sur cœurs différents
    - **Problème**: N'apparaît pas dans app_list

### Étape 4.3: Applications Futures
- [ ] Application Capteur - Lecture de capteurs et transmission de données
- [ ] Application Contrôle - Interface de contrôle à distance
- [ ] Application Affichage - Gestion d'écrans LCD/OLED

## Architecture Actuelle

```
D'O-Core System
├── Core System ✅
│   ├── Kernel Core
│   ├── Task Manager
│   ├── Memory Manager
│   ├── Log System
│   └── System Monitor
├── HAL (Hardware) ⏸️
│   ├── GPIO Manager ❌ (supprimé)
│   ├── ADC Manager
│   └── PWM Manager
├── Network ✅
│   ├── WiFi Manager
│   ├── NTP Manager
│   ├── WebSocket Manager ⏸️
│   └── HTTP Server Manager ⏸️
├── Applications ✅
│   ├── Application Manager
│   ├── LED App (ID 1) ✅
│   └── Dual LED App (ID 2) ❌
└── Interface ✅
    └── Command Shell
```

## Commandes Disponibles

### 🔧 Commandes Système
- `help`, `status`, `tasks`, `memory`, `dmesg`, `logs`, `log_echo`
- `test_log`, `debug_log`, `validate_log`, `neofetch`, `clear`, `uptime`, `version`

### 🌐 Commandes Réseau
- `wifi`, `wifi_enable`, `network`, `wifi_scan`, `wifi_reconnect`
- `wifi_stats`, `wifi_save`, `wifi_clear`, `wifi_auto`, `network_test`

### ⏰ Commandes NTP
- `ntp_status`, `ntp_sync`, `ntp_test`, `ntp_timezone`, `ntp_utilities`

### 🔌 Commandes WebSocket ⏸️
- `websocket`, `ws_start`, `ws_stop`, `ws_status`, `ws_send`, `ws_broadcast`, `ws_clients` (À implémenter)

### 🌍 Commandes HTTP Server ⏸️
- `http`, `http_start`, `http_stop`, `http_status`, `http_routes`, `http_stats` (À implémenter)

### 📱 Commandes Applications
- `app_status`, `app_list`, `app_start`, `app_stop`, `app_restart`
- `app_pause`, `app_resume`, `app_info`, `app_start_all`, `app_stop_all`

## Optimisations Mémoire Flash Implémentées

### Mode Minimaliste
- **Environnement** : `esp32dev_minimal` avec build flags optimisés
- **Chaînes** : Réduction via macros `STRING_OPTIMIZATION`
- **Logs** : Compilation conditionnelle avec `SERIAL_LOGS_MINIMAL`
- **FreeRTOS** : Désactivation des fonctionnalités de debug
- **WiFi** : Configuration ultra-minimale

### Optimisations Spécifiques
- **NTP** : Utilisation d'APIs Arduino standard
- **WebSocket** : À implémenter (limitation prévue : 4 clients, buffer 256 bytes)
- **GPIO** : Supprimé pour libérer mémoire
- **Logs** : Messages courts en mode minimaliste
- **Applications** : Maximum 4 applications simultanées

## Prochaines Actions

### 🎯 Priorité Immédiate
1. **Résoudre le problème d'enregistrement Dual LED App** - Diagnostiquer pourquoi l'application ID 2 n'apparaît pas
2. **Tester les applications LED** - Vérifier le fonctionnement des LEDs via les applications
3. **Intégrer les applications avec WebSocket/HTTP** - Contrôle des LEDs via interface web (À implémenter)

### 🔄 Phase 4: Applications (En Cours)
1. **✅ Application Manager** - Terminé et fonctionnel
2. **🔄 Applications LED** - LED Simple OK, Dual LED à corriger
3. **📋 Applications Capteur** - Lecture de capteurs et transmission de données
4. **🎮 Applications Contrôle** - Interface de contrôle à distance

### 🔧 Phase 2: HAL (Reprise Plus Tard)
1. **Étape 2.1: GPIO Manager** - Reprendre après optimisation mémoire
2. **Étape 2.2: I2C Manager** - Communication I2C
3. **Étape 2.3: SPI Manager** - Communication SPI

## Problèmes Actuels à Résoudre

### 🐛 Application Dual LED (ID 2)
- **Symptôme**: N'apparaît pas dans `app_list`
- **Cause possible**: Erreur d'enregistrement ou problème de compilation
- **Action**: Ajout de messages de debug pour diagnostiquer
- **Fichiers concernés**: `src/apps/dual_led_app.cpp`, `src/main.cpp`

## Notes Techniques

- **Mémoire Flash actuelle** : ~60.8% utilisée
- **Objectif** : Maintenir sous 70% pour les futures fonctionnalités
- **Priorité** : Résoudre les problèmes d'applications avant nouvelles fonctionnalités
- **Approche** : Implémentations minimalistes avec optimisations mémoire 
- **Applications actives** : 1/4 (LED Simple fonctionnelle) 