# 📟 Guide des Commandes CLI - Configuration Irrigation

## 🎯 Vue d'Ensemble

Ce guide explique comment utiliser les commandes CLI pour configurer les URLs de communication HTTP entre les devices du système d'irrigation.

---

## 🏗️ Architecture de Communication

```
┌─────────────────┐
│  Serveur FastAPI│  ← URL configurable via CLI
│  192.168.1.100  │
│     :8000       │
└────────▲────────┘
         │
         │ HTTP POST (sensor data)
         │
┌────────┴────────┐
│  Device 1       │
│  MASTER         │  ← Configure URLs des Slaves
│  192.168.1.101  │
│     :8080       │
└─────┬─────┬─────┘
      │     │
      │     └─────────────────┐
      │                       │
      │ HTTP POST             │ HTTP POST
      │ (sensor request)      │ (irrigation cmd)
      │                       │
┌─────▼─────────┐      ┌──────▼──────────┐
│  Device 2     │      │  Device 3       │
│  SLAVE1       │      │  SLAVE2         │
│  (Sensors)    │      │  (Relays)       │
│  192.168.1.102│      │  192.168.1.103  │
│     :8081     │      │     :8082       │
└───────────────┘      └─────────────────┘
```

---

## 📋 Liste des Commandes

### 1. **irrig_config_server** - Configurer l'URL du serveur FastAPI

**Usage** :
```bash
irrig_config_server <url>
```

**Exemple** :
```bash
irrig_config_server http://192.168.1.100:8000
```

**Description** :
- Configure l'URL du serveur FastAPI central
- Utilisé par le **Master** pour envoyer les doems - Love Me JeJenées capteurs
- Format : `http://IP:PORT` (sans slash final)

**Device concerné** : Master (Device 1)

---

### 2. **irrig_config_master** - Configurer l'IP/Port du Master

**Usage** :
```bash
irrig_config_master <ip> <port>
```

**Exemple** :
```bash
irrig_config_master 192.168.1.101 8080
```

**Description** :
- Configure l'adresse du Master
- Utilisé par les **Slaves** pour envoyer leurs données au Master
- Le Master écoute sur ce port pour recevoir les données des Slaves

**Devices concernés** : Slave1 (Device 2), Slave2 (Device 3)

---

### 3. **irrig_config_slave1** - Configurer l'IP/Port du Slave1 (Sensors)

**Usage** :
```bash
irrig_config_slave1 <ip> <port>
```

**Exemple** :
```bash
irrig_config_slave1 192.168.1.102 8081
```

**Description** :
- Configure l'adresse du Slave1 (capteurs)
- Utilisé par le **Master** pour demander les données capteurs
- Slave1 écoute sur ce port

**Device concerné** : Master (Device 1)

---

### 4. **irrig_config_slave2** - Configurer l'IP/Port du Slave2 (Relays)

**Usage** :
```bash
irrig_config_slave2 <ip> <port>
```

**Exemple** :
```bash
irrig_config_slave2 192.168.1.103 8082
```

**Description** :
- Configure l'adresse du Slave2 (relais)
- Utilisé par le **Master** pour envoyer les commandes d'irrigation
- Slave2 écoute sur ce port

**Device concerné** : Master (Device 1)

---

### 5. **irrig_config_show** - Afficher la configuration actuelle

**Usage** :
```bash
irrig_config_show
```

**Exemple de sortie** :
```
=== Irrigation System Configuration ===

Server (FastAPI):
  URL: http://192.168.1.100:8000

Master Device:
  IP: 192.168.1.101
  Port: 8080

Slave1 (Sensors):
  IP: 192.168.1.102
  Port: 8081

Slave2 (Relays):
  IP: 192.168.1.103
  Port: 8082

HTTP Settings:
  Timeout: 5000 ms
  Retry count: 3
  Retry delay: 1000 ms
=======================================
```

**Description** :
- Affiche toute la configuration actuelle
- Utile pour vérifier les paramètres avant de sauvegarder

**Devices concernés** : Tous

---

### 6. **irrig_config_save** - Sauvegarder la configuration

**Usage** :
```bash
irrig_config_save
```

**Description** :
- Sauvegarde la configuration actuelle dans la mémoire NVS (Non-Volatile Storage)
- La configuration persiste après un redémarrage
- **Important** : Toujours sauvegarder après avoir modifié la configuration !

**Devices concernés** : Tous

---

### 7. **irrig_config_load** - Charger la configuration sauvegardée

**Usage** :
```bash
irrig_config_load
```

**Description** :
- Charge la configuration depuis la mémoire NVS
- Utile après un reset ou pour restaurer une configuration
- Affiche automatiquement la configuration chargée

**Devices concernés** : Tous

---

### 8. **irrig_config_reset** - Réinitialiser aux valeurs par défaut

**Usage** :
```bash
irrig_config_reset
```

**Valeurs par défaut** :
```
Master:   192.168.1.101:8080
Slave1:   192.168.1.102:8081
Slave2:   192.168.1.103:8082
Server:   http://192.168.1.100:8000
Timeout:  5000 ms
Retry:    3 attempts
```

**Description** :
- Réinitialise toute la configuration aux valeurs par défaut
- N'écrase PAS la configuration NVS (utiliser `irrig_config_save` pour persister)

**Devices concernés** : Tous

---

## 🚀 Scénarios d'Usage

### Scénario 1 : Configuration Initiale du Master (Device 1)

```bash
# 1. Connecter en série au Master
# 2. Configurer le serveur FastAPI
D'O-Core> irrig_config_server http://192.168.1.100:8000

# 3. Configurer les Slaves
D'O-Core> irrig_config_slave1 192.168.1.102 8081
D'O-Core> irrig_config_slave2 192.168.1.103 8082

# 4. Vérifier la configuration
D'O-Core> irrig_config_show

# 5. Sauvegarder
D'O-Core> irrig_config_save
```

---

### Scénario 2 : Configuration Initiale du Slave1 (Device 2)

```bash
# 1. Connecter en série au Slave1
# 2. Configurer l'adresse du Master
D'O-Core> irrig_config_master 192.168.1.101 8080

# 3. Vérifier
D'O-Core> irrig_config_show

# 4. Sauvegarder
D'O-Core> irrig_config_save
```

---

### Scénario 3 : Configuration Initiale du Slave2 (Device 3)

```bash
# 1. Connecter en série au Slave2
# 2. Configurer l'adresse du Master
D'O-Core> irrig_config_master 192.168.1.101 8080

# 3. Vérifier
D'O-Core> irrig_config_show

# 4. Sauvegarder
D'O-Core> irrig_config_save
```

---

### Scénario 4 : Changer l'IP du Serveur FastAPI

```bash
# Sur le Master uniquement
D'O-Core> irrig_config_server http://192.168.1.200:8000
D'O-Core> irrig_config_save
```

---

### Scénario 5 : Restaurer Configuration Sauvegardée

```bash
# Après un reset ou changement manuel
D'O-Core> irrig_config_load
```

---

### Scénario 6 : Réinitialiser et Reconfigurer

```bash
# 1. Reset aux valeurs par défaut
D'O-Core> irrig_config_reset

# 2. Modifier selon besoin
D'O-Core> irrig_config_server http://192.168.1.150:8000

# 3. Sauvegarder
D'O-Core> irrig_config_save
```

---

## 🔧 Configuration Recommandée

### Réseau Local Standard (192.168.1.x)

| Device | Rôle | IP | Port | Commande |
|--------|------|----|----|----------|
| Serveur | FastAPI | 192.168.1.100 | 8000 | `irrig_config_server http://192.168.1.100:8000` |
| Device 1 | Master | 192.168.1.101 | 8080 | `irrig_config_master 192.168.1.101 8080` |
| Device 2 | Slave1 (Sensors) | 192.168.1.102 | 8081 | `irrig_config_slave1 192.168.1.102 8081` |
| Device 3 | Slave2 (Relays) | 192.168.1.103 | 8082 | `irrig_config_slave2 192.168.1.103 8082` |

---

## 📝 Checklist de Configuration

### ✅ Master (Device 1)
- [ ] Configurer URL serveur FastAPI : `irrig_config_server`
- [ ] Configurer IP/Port Slave1 : `irrig_config_slave1`
- [ ] Configurer IP/Port Slave2 : `irrig_config_slave2`
- [ ] Vérifier : `irrig_config_show`
- [ ] Sauvegarder : `irrig_config_save`

### ✅ Slave1 (Device 2)
- [ ] Configurer IP/Port Master : `irrig_config_master`
- [ ] Vérifier : `irrig_config_show`
- [ ] Sauvegarder : `irrig_config_save`

### ✅ Slave2 (Device 3)
- [ ] Configurer IP/Port Master : `irrig_config_master`
- [ ] Vérifier : `irrig_config_show`
- [ ] Sauvegarder : `irrig_config_save`

---

## 🐛 Dépannage

### Problème : Configuration non persistante après redémarrage

**Solution** : Vous avez oublié de sauvegarder !
```bash
D'O-Core> irrig_config_save
```

---

### Problème : Erreur "Failed to open NVS"

**Causes possibles** :
1. Partition NVS corrompue
2. Première utilisation

**Solution** :
```bash
# Réinitialiser et sauvegarder
D'O-Core> irrig_config_reset
D'O-Core> irrig_config_save
```

---

### Problème : Communication HTTP échoue

**Vérifications** :
1. Vérifier la configuration :
   ```bash
   D'O-Core> irrig_config_show
   ```

2. Vérifier la connectivité WiFi :
   ```bash
   D'O-Core> wifi
   D'O-Core> network
   ```

3. Tester la connectivité réseau :
   ```bash
   D'O-Core> network_test
   ```

4. Vérifier que les IPs sont correctes et accessibles

---

## 🔐 Sécurité

### Recommandations

1. **Réseau isolé** : Utiliser un réseau WiFi dédié pour les devices
2. **IPs statiques** : Configurer des IPs statiques sur le routeur pour éviter les changements
3. **Sauvegarde** : Toujours sauvegarder après configuration
4. **Documentation** : Noter les IPs utilisées dans un fichier externe

---

## 📚 Intégration avec le Code

### Utiliser l'URL du serveur dans le code

```cpp
#include "irrig_common/irrig_cli_commands.h"

void sendDataToServer() {
    const char* server_url = irrig_cli_get_server_url();

    if (server_url == NULL) {
        kernel_log(LOG_LEVEL_ERROR, "Server URL not configured!");
        return;
    }

    // Utiliser server_url pour HTTP POST
    HTTPClient http;
    http.begin(String(server_url) + "/api/sensors/data");
    // ...
}
```

### Vérifier si configuré

```cpp
if (!irrig_cli_is_server_configured()) {
    Serial.println("ERROR: Server not configured!");
    Serial.println("Use: irrig_config_server <url>");
}
```

---

## 🎓 Exemples Complets

### Configuration Complète d'un Système 3 Devices

#### 1. Master (Device 1)
```bash
# Connexion série au Master
D'O-Core> wifi_auto
D'O-Core> irrig_config_server http://192.168.1.100:8000
D'O-Core> irrig_config_slave1 192.168.1.102 8081
D'O-Core> irrig_config_slave2 192.168.1.103 8082
D'O-Core> irrig_config_show
D'O-Core> irrig_config_save
```

#### 2. Slave1 (Device 2)
```bash
# Connexion série au Slave1
D'O-Core> wifi_auto
D'O-Core> irrig_config_master 192.168.1.101 8080
D'O-Core> irrig_config_show
D'O-Core> irrig_config_save
```

#### 3. Slave2 (Device 3)
```bash
# Connexion série au Slave2
D'O-Core> wifi_auto
D'O-Core> irrig_config_master 192.168.1.101 8080
D'O-Core> irrig_config_show
D'O-Core> irrig_config_save
```

---

## 📊 Tableau Récapitulatif des Commandes

| Commande | Paramètres | Device | Persistant | Description |
|----------|-----------|--------|------------|-------------|
| `irrig_config_server` | `<url>` | Master | Non* | Configure serveur FastAPI |
| `irrig_config_master` | `<ip> <port>` | Slaves | Non* | Configure Master |
| `irrig_config_slave1` | `<ip> <port>` | Master | Non* | Configure Slave1 |
| `irrig_config_slave2` | `<ip> <port>` | Master | Non* | Configure Slave2 |
| `irrig_config_show` | - | Tous | - | Affiche config |
| `irrig_config_save` | - | Tous | **Oui** | Sauvegarde en NVS |
| `irrig_config_load` | - | Tous | - | Charge depuis NVS |
| `irrig_config_reset` | - | Tous | Non* | Reset aux défauts |

\* Devient persistant après `irrig_config_save`

---

**Configuration terminée ! Le système est prêt à communiquer ! 🚀**
