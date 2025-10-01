# Intégration Device ESP32 - Frontend SST

## Vue d'ensemble

Cette implémentation permet à un device ESP32 (DO-Core-OS) de se présenter et de se synchroniser avec le frontend SST. Le device fonctionne de manière autonome mais peut synchroniser ses données avec le système central quand il est connecté.

## Architecture

### Composants Implémentés

#### 1. Device ESP32 (DO-Core-OS)
- **Serveur de découverte**: HTTP server temporaire sur port 8080
- **Endpoint `/discover`**: Exposition des informations du device
- **Gestion d'état**: Suivi de l'état d'enregistrement (REGISTERED/UNREGISTERED)
- **Stockage JWT**: Token d'authentification pour les communications sécurisées

#### 2. Backend FastAPI
- **API Device**: Endpoints pour enregistrement, gestion et synchronisation
- **Base de données**: Table `devices` avec métadonnées et tokens
- **Authentification**: JWT tokens pour sécuriser les communications

#### 3. Frontend React
- **DeviceDiscoveryModal**: Interface de découverte et enregistrement
- **DeviceManager**: Gestion des devices enregistrés
- **DeviceService**: Service API pour les communications device

## Installation et Configuration

### 1. Base de Données

Exécutez le script SQL pour créer la table devices :

```bash
psql -d SST -f create_devices_table.sql
```

### 2. Démarrage du Système

1. **Device ESP32**:
   ```bash
   # Le device démarre automatiquement le serveur de découverte
   # s'il n'est pas enregistré
   ```

2. **Backend**:
   ```bash
   cd Fast-API
   python server_sst.py
   ```

3. **Frontend**:
   ```bash
   cd SST/frontend
   npm run dev
   ```

## Utilisation

### Découverte et Enregistrement d'un Device

1. **Allumez le device ESP32** sur le même réseau que le frontend
2. **Ouvrez le dashboard SST** et allez dans la section "Gestion des Devices"
3. **Cliquez sur "Découvrir un Device"**
4. **Le système scanne automatiquement** le réseau local (192.168.1.0/24)
5. **Sélectionnez le device** dans la liste des devices découverts
6. **Associez-le à une entreprise** et cliquez sur "Enregistrer"

### Fonctionnalités du DeviceManager

- **Visualisation**: Liste de tous les devices enregistrés
- **Statut temps réel**: En ligne/hors ligne, dernière synchronisation
- **Actions**: Synchronisation manuelle, configuration, désenregistrement
- **Monitoring**: Suivi des connexions et synchronisations

## API Endpoints

### Device Discovery
```
GET /discover
```
**Réponse**:
```json
{
  "device_id": "A1:B2:C3:D4:E5:F6_12345678",
  "device_name": "SST-Device-A1B2C3",
  "mac_address": "A1:B2:C3:D4:E5:F6",
  "ip_address": "192.168.1.100",
  "chip_id": 12345678,
  "firmware_version": "1.0.0",
  "state": "UNREGISTERED",
  "is_registered": false
}
```

### Device Management
```
POST /devices/register          # Enregistrer un device
GET  /devices/{corporate_id}    # Lister devices d'une entreprise
GET  /devices/device/{device_id} # Infos d'un device spécifique
POST /devices/{device_id}/sync  # Synchroniser un device
DELETE /devices/{device_id}     # Désenregistrer un device
```

## Flux de Synchronisation

### Enregistrement Initial
1. Device démarre → Serveur découverte actif (port 8080)
2. Frontend scanne réseau → Découvre device via `/discover`
3. Utilisateur associe device à entreprise
4. Backend enregistre device → Génère JWT token
5. Device reçoit token → Arrête serveur découverte

### Synchronisation Régulière
1. Device envoie données locales au backend
2. Backend met à jour base de données
3. Backend envoie nouvelles données/campagnes au device
4. Device met à jour son état local

## Sécurité

- **Authentification JWT**: Tokens uniques par device
- **Validation d'entreprise**: Devices liés à une entreprise spécifique
- **Timeout automatique**: Serveur découverte s'arrête après 5 minutes
- **Chiffrement**: Toutes les communications utilisent HTTPS en production

## Test

### Script de Test Automatique
```bash
cd Fast-API
python test_device_registration.py
```

### Test Manuel
1. Démarrez tous les composants
2. Utilisez le DeviceDiscoveryModal pour enregistrer un device
3. Vérifiez la synchronisation via DeviceManager
4. Testez les actions (sync, désenregistrement)

## Dépannage

### Device non découvert
- Vérifiez que le device est sur le même réseau
- Assurez-vous que le port 8080 n'est pas bloqué
- Vérifiez les logs du device ESP32

### Erreur d'enregistrement
- Vérifiez la connectivité avec le backend
- Assurez-vous que l'entreprise existe
- Vérifiez les permissions utilisateur

### Problèmes de synchronisation
- Vérifiez le token JWT du device
- Contrôlez la connectivité réseau
- Examinez les logs du backend

## Évolution Future

- **WebSocket**: Synchronisation temps réel
- **OTA Updates**: Mise à jour firmware à distance
- **Device Groups**: Gestion de groupes de devices
- **Analytics**: Statistiques avancées de synchronisation
- **Mobile App**: Interface mobile pour la gestion

## Support

Pour toute question ou problème, consultez :
- Les logs du device ESP32
- Les logs du backend FastAPI
- La console du navigateur (frontend)