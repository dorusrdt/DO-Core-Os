# ESP32 FastAPI Data Collector

Serveur FastAPI pour collecter les données des ESP32 master/slave via WebSocket et HTTP.

## Installation

```bash
pip install fastapi uvicorn[standard]
```

## Démarrage

```bash
python fastapi_server.py
```

Le serveur sera accessible sur :
- API : http://localhost:8000
- Documentation : http://localhost:8000/docs

## Endpoints

### POST /data
Reçoit les données des ESP32 master.

**Payload exemple :**
```json
{
  "timestamp": 12345678,
  "master_ip": "192.168.1.100",
  "ap_ip": "192.168.4.1",
  "slaves": {
    "slave_0": "Hello Master! Slave IP: 192.168.4.2",
    "slave_1": "Hello Master! Slave IP: 192.168.4.3"
  }
}
```

### GET /data
Retourne toutes les données collectées.

### GET /health
Vérification de l'état du serveur.

### DELETE /data
Efface toutes les données stockées.

## Architecture

1. **ESP32 Master** : Crée AP WiFi + WebSocket server, collecte données slaves, envoie à FastAPI
2. **ESP32 Slave(s)** : Se connecte à l'AP master, envoie données via WebSocket
3. **FastAPI Server** : Reçoit et stocke les données via HTTP POST

## Données collectées

- Timestamp du master
- Adresses IP (master et AP)
- Données de chaque slave connecté
- Historique des communications