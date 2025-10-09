# IrrigAppMaster - Application de Contrôle d'Irrigation

Application pour le système D'O-Core OS permettant le contrôle intelligent d'un système d'irrigation.

## État du Développement

**Version actuelle : 0.1.0 - BASE**
- ✅ Structure de base créée
- ✅ Callbacks d'application implémentés
- ✅ Enregistrement dans le système
- ✅ Tests de lancement possibles
- ⏳ Logique métier d'irrigation (à implémenter)

## Configuration

```cpp
IrrigAppConfig_t config = {
    .server_url = "http://10.223.73.53:3000",
    .device_id = "ESP32_IRRIGATION_001",
    .device_secret = "esp32-secret-key",
    .poll_interval_seconds = 30,
    .sensor_read_interval_seconds = 5,
    .data_send_interval_seconds = 15,
    .max_zones = 4,
    .max_sensors = 12,
    .simulation_mode = true
};
```

## Utilisation

### Enregistrement dans main.cpp
```cpp
#include "apps/irrig_app_master/irrig_app_master.h"

// Dans setup()
register_irrig_app_master(&config);
```

### Contrôle via Shell
```bash
# Lister les applications
app_list

# Démarrer l'application (ID attribué automatiquement)
app_start 1

# Vérifier l'état
app_info 1

# Arrêter l'application
app_stop 1
```

## Fonctionnalités de Base

- **Initialisation** : Configuration et validation
- **Démarrage** : Logs de statut système
- **Boucle principale** : Compteur de cycles avec logs périodiques
- **Arrêt** : Nettoyage et statistiques finales

## Logs Attendus

```
[INFO] IrrigAppMaster: Initializing irrigation system...
[INFO] IrrigAppMaster: Device ID: ESP32_IRRIGATION_001
[INFO] IrrigAppMaster: Starting irrigation system...
[INFO] IrrigAppMaster: Running... (loop #10)
[INFO] IrrigAppMaster: Running... (loop #20)
```

## Prochaines Étapes

1. **Implémentation logique métier** :
   - Gestion des zones d'irrigation
   - Lecture des capteurs d'humidité
   - Contrôle des relais/pompes
   - Communication avec serveur

2. **Intégration services noyau** :
   - Utilisation du réseau (WiFi/HTTP)
   - Gestion du temps (NTP)
   - Persistance des données

3. **Fonctionnalités avancées** :
   - Mode simulation/réel
   - Gestion des urgences
   - Interface web
   - Monitoring temps réel