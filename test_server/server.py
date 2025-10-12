#!/usr/bin/env python3
"""
Serveur de Test FastAPI pour IrrigAppMaster
Compatible avec le code de référence ESP32
"""

from fastapi import FastAPI, HTTPException, Header
from pydantic import BaseModel
from typing import List, Optional, Dict, Any
from datetime import datetime
import uvicorn
import hashlib
import json

app = FastAPI(title="Irrigation Server", version="1.0.0")

# ===== STRUCTURES DE DONNÉES =====

class DeviceCapacity(BaseModel):
    zones: int
    sensors: int

class RegisterRequest(BaseModel):
    type: str
    deviceId: str
    capacity: DeviceCapacity
    timestamp: str

class SensorData(BaseModel):
    sensorId: str
    value: float

class ZoneData(BaseModel):
    zoneId: str
    soilMoisture: List[SensorData]

class GlobalData(BaseModel):
    temperature: float
    humidity: float
    pressure: float
    batteryLevel: float
    signalStrength: int

class SensorDataRequest(BaseModel):
    type: str
    deviceId: str
    timestamp: str
    globalData: GlobalData
    zonesData: List[ZoneData]

class SensorConfig(BaseModel):
    sensorId: str

class ZoneConfig(BaseModel):
    zoneId: str
    waterPerDay: int
    irrigationTime: str
    humidityThreshold: int
    sensors: List[SensorConfig]

class ConfigResponse(BaseModel):
    type: str
    zones: List[ZoneConfig]
    commands: Optional[List[Dict[str, str]]] = []

# ===== BASE DE DONNÉES EN MÉMOIRE =====

# Devices enregistrés
registered_devices: Dict[str, Dict[str, Any]] = {}

# Configurations des zones par device
device_zones: Dict[str, List[Dict[str, Any]]] = {}

# Historique des données capteurs
sensor_data_history: List[Dict[str, Any]] = []

# Commandes en attente
pending_commands: Dict[str, List[Dict[str, str]]] = {}

# ===== ENDPOINTS =====

@app.get("/")
async def root():
    """Page d'accueil"""
    return {
        "service": "Irrigation Server",
        "version": "1.0.0",
        "status": "running",
        "devices": len(registered_devices),
        "timestamp": datetime.now().isoformat()
    }

@app.get("/api/health")
async def health_check():
    """Health check pour test de connectivité"""
    return {
        "status": "healthy",
        "timestamp": datetime.now().isoformat()
    }

@app.post("/api/devices/register")
async def register_device(
    request: RegisterRequest,
    x_signature: Optional[str] = Header(None),
    x_timestamp: Optional[str] = Header(None)
):
    """
    Enregistrement d'un nouveau device
    Compatible avec le format du code référence
    """
    print(f"\n📝 REGISTER DEVICE")
    print(f"   Device ID: {request.deviceId}")
    print(f"   Type: {request.type}")
    print(f"   Capacity: {request.capacity.zones} zones, {request.capacity.sensors} sensors")
    print(f"   Timestamp: {request.timestamp}")
    print(f"   Signature: {x_signature}")
    
    # Enregistrer le device
    registered_devices[request.deviceId] = {
        "deviceId": request.deviceId,
        "capacity": request.capacity.model_dump(),
        "registeredAt": datetime.now().isoformat(),
        "lastSeen": datetime.now().isoformat(),
        "status": "online"
    }
    
    # Initialiser les zones vides
    if request.deviceId not in device_zones:
        device_zones[request.deviceId] = []
    
    # Initialiser les commandes vides
    if request.deviceId not in pending_commands:
        pending_commands[request.deviceId] = []
    
    print(f"   ✅ Device registered successfully")
    
    return {
        "status": "registered",
        "deviceId": request.deviceId,
        "message": "Device registered successfully",
        "timestamp": datetime.now().isoformat()
    }

@app.post("/api/devices/sensor-data")
async def receive_sensor_data(
    request: SensorDataRequest,
    x_signature: Optional[str] = Header(None),
    x_timestamp: Optional[str] = Header(None)
):
    """
    Réception des données capteurs
    Compatible avec le format du code référence
    """
    print(f"\n📊 SENSOR DATA RECEIVED")
    print(f"   Device ID: {request.deviceId}")
    print(f"   Type: {request.type}")
    print(f"   Timestamp: {request.timestamp}")
    print(f"   Global Data:")
    print(f"      Temperature: {request.globalData.temperature}°C")
    print(f"      Humidity: {request.globalData.humidity}%")
    print(f"      Pressure: {request.globalData.pressure} hPa")
    print(f"      Battery: {request.globalData.batteryLevel}%")
    print(f"      Signal: {request.globalData.signalStrength} dBm")
    
    print(f"   Zones Data: {len(request.zonesData)} zones")
    for zone in request.zonesData:
        print(f"      Zone {zone.zoneId}: {len(zone.soilMoisture)} sensors")
        for sensor in zone.soilMoisture:
            print(f"         {sensor.sensorId}: {sensor.value:.1f}%")
    
    # Stocker dans l'historique
    sensor_data_history.append({
        "deviceId": request.deviceId,
        "timestamp": request.timestamp,
        "globalData": request.globalData.model_dump(),
        "zonesData": [z.model_dump() for z in request.zonesData],
        "receivedAt": datetime.now().isoformat()
    })
    
    # Limiter l'historique à 100 entrées
    if len(sensor_data_history) > 100:
        sensor_data_history.pop(0)
    
    # Mettre à jour lastSeen
    if request.deviceId in registered_devices:
        registered_devices[request.deviceId]["lastSeen"] = datetime.now().isoformat()
    
    print(f"   ✅ Data stored successfully")
    
    return {
        "status": "received",
        "message": "Sensor data received successfully",
        "timestamp": datetime.now().isoformat()
    }

@app.get("/api/devices/{device_id}/config")
async def get_device_config(
    device_id: str,
    x_signature: Optional[str] = Header(None),
    x_timestamp: Optional[str] = Header(None)
):
    """
    Récupération de la configuration pour un device
    Compatible avec le format du code référence
    """
    print(f"\n⚙️  CONFIG REQUEST")
    print(f"   Device ID: {device_id}")
    print(f"   Signature: {x_signature}")
    
    # Vérifier si le device est enregistré
    if device_id not in registered_devices:
        print(f"   ❌ Device not registered")
        raise HTTPException(status_code=404, detail="Device not registered")
    
    # Récupérer les zones configurées
    zones = device_zones.get(device_id, [])
    
    # Récupérer les commandes en attente
    commands = pending_commands.get(device_id, [])
    
    print(f"   Zones configured: {len(zones)}")
    for zone in zones:
        print(f"      {zone['zoneId']}: {zone['waterPerDay']}ml/day, {zone['irrigationTime']}, {zone['humidityThreshold']}%")
    
    print(f"   Pending commands: {len(commands)}")
    for cmd in commands:
        print(f"      {cmd['action']}: {cmd.get('zoneId', 'N/A')}")
    
    # Vider les commandes après envoi
    if commands:
        pending_commands[device_id] = []
    
    # Mettre à jour lastSeen
    registered_devices[device_id]["lastSeen"] = datetime.now().isoformat()
    
    print(f"   ✅ Config sent successfully")
    
    return {
        "type": "config",
        "zones": zones,
        "commands": commands
    }

# ===== ENDPOINTS DE GESTION (Interface Web) =====

@app.get("/api/devices")
async def list_devices():
    """Liste tous les devices enregistrés"""
    return {
        "devices": list(registered_devices.values()),
        "count": len(registered_devices)
    }

@app.get("/api/devices/{device_id}")
async def get_device_info(device_id: str):
    """Informations détaillées sur un device"""
    if device_id not in registered_devices:
        raise HTTPException(status_code=404, detail="Device not found")
    
    return {
        "device": registered_devices[device_id],
        "zones": device_zones.get(device_id, []),
        "pendingCommands": pending_commands.get(device_id, [])
    }

@app.post("/api/devices/{device_id}/zones")
async def add_zone(device_id: str, zone: ZoneConfig):
    """
    Ajouter une zone à un device
    Format compatible avec le code référence
    """
    print(f"\n➕ ADD ZONE")
    print(f"   Device ID: {device_id}")
    print(f"   Zone ID: {zone.zoneId}")
    print(f"   Water: {zone.waterPerDay}ml/day")
    print(f"   Time: {zone.irrigationTime}")
    print(f"   Threshold: {zone.humidityThreshold}%")
    print(f"   Sensors: {len(zone.sensors)}")
    
    if device_id not in registered_devices:
        raise HTTPException(status_code=404, detail="Device not registered")
    
    # Vérifier si la zone existe déjà
    zones = device_zones.get(device_id, [])
    existing_zone = next((z for z in zones if z["zoneId"] == zone.zoneId), None)
    
    if existing_zone:
        # Mettre à jour la zone existante
        existing_zone.update(zone.model_dump())
        print(f"   ✅ Zone updated")
    else:
        # Ajouter nouvelle zone
        zones.append(zone.model_dump())
        device_zones[device_id] = zones
        print(f"   ✅ Zone added")
    
    return {
        "status": "success",
        "message": f"Zone {zone.zoneId} configured",
        "zone": zone.model_dump()
    }

@app.delete("/api/devices/{device_id}/zones/{zone_id}")
async def delete_zone(device_id: str, zone_id: str):
    """
    Supprimer une zone d'un device
    Ajoute une commande delete_zone pour le device
    """
    print(f"\n🗑️  DELETE ZONE")
    print(f"   Device ID: {device_id}")
    print(f"   Zone ID: {zone_id}")
    
    if device_id not in registered_devices:
        raise HTTPException(status_code=404, detail="Device not registered")
    
    # Retirer la zone de la configuration
    zones = device_zones.get(device_id, [])
    zones = [z for z in zones if z["zoneId"] != zone_id]
    device_zones[device_id] = zones
    
    # Ajouter commande delete_zone
    if device_id not in pending_commands:
        pending_commands[device_id] = []
    
    pending_commands[device_id].append({
        "action": "delete_zone",
        "zoneId": zone_id
    })
    
    print(f"   ✅ Zone deleted, command queued")
    
    return {
        "status": "success",
        "message": f"Zone {zone_id} deleted, command sent to device",
        "command": {
            "action": "delete_zone",
            "zoneId": zone_id
        }
    }

@app.get("/api/sensor-data/history")
async def get_sensor_history(limit: int = 10):
    """Récupérer l'historique des données capteurs"""
    return {
        "history": sensor_data_history[-limit:],
        "count": len(sensor_data_history)
    }

@app.get("/api/sensor-data/latest/{device_id}")
async def get_latest_sensor_data(device_id: str):
    """Récupérer les dernières données capteurs d'un device"""
    device_data = [d for d in sensor_data_history if d["deviceId"] == device_id]
    
    if not device_data:
        raise HTTPException(status_code=404, detail="No data found for this device")
    
    return device_data[-1]

@app.delete("/api/devices/{device_id}")
async def unregister_device(device_id: str):
    """Désenregistrer un device"""
    if device_id not in registered_devices:
        raise HTTPException(status_code=404, detail="Device not found")
    
    # Supprimer le device
    del registered_devices[device_id]
    
    # Supprimer ses zones
    if device_id in device_zones:
        del device_zones[device_id]
    
    # Supprimer ses commandes
    if device_id in pending_commands:
        del pending_commands[device_id]
    
    print(f"\n🗑️  Device {device_id} unregistered")
    
    return {
        "status": "success",
        "message": f"Device {device_id} unregistered"
    }

# ===== ENDPOINT DE TEST RAPIDE =====

@app.post("/api/test/create-zone")
async def test_create_zone(device_id: str = "ESP32_IRRIGATION_11100454456464674"):
    """
    Créer une zone de test rapidement
    """
    test_zone = ZoneConfig(
        zoneId=f"zone_test_{datetime.now().strftime('%H%M%S')}",
        waterPerDay=2000,
        irrigationTime="08:00",
        humidityThreshold=25,
        sensors=[
            SensorConfig(sensorId="s_01"),
            SensorConfig(sensorId="s_02"),
            SensorConfig(sensorId="s_03")
        ]
    )
    
    return await add_zone(device_id, test_zone)

# ===== DÉMARRAGE =====

if __name__ == "__main__":
    print("=" * 60)
    print("🚀 Irrigation Server Starting...")
    print("=" * 60)
    print(f"📍 Host: 192.168.1.3")
    print(f"🔌 Port: 3000")
    print(f"📚 Docs: http://192.168.1.3:3000/docs")
    print(f"🔍 Health: http://192.168.1.3:3000/api/health")
    print("=" * 60)
    
    uvicorn.run(
        app,
        host="192.168.1.3",
        port=3000,
        log_level="info"
    )
