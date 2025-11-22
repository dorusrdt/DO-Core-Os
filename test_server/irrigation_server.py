#!/usr/bin/env python3
"""
FastAPI Server for ESP32 Irrigation System
Simulates the external server (192.168.1.72:8000)
Handles device registration, configuration, and sensor data
"""

from fastapi import FastAPI, Request, HTTPException, Header, Response
from fastapi.responses import JSONResponse
from datetime import datetime
from typing import Optional, Dict, Any, List
import json
import uvicorn
import hashlib
from pydantic import BaseModel

app = FastAPI(title="ESP32 Irrigation Server", version="1.0.0")

# ============================================================================
# Data Models
# ============================================================================

class DeviceRegistration(BaseModel):
    type: str
    deviceId: str
    capacity: Dict[str, int]
    timestamp: str
    latitude: Optional[float] = None
    longitude: Optional[float] = None

class SensorData(BaseModel):
    type: str
    deviceId: str
    timestamp: str
    globalData: Dict[str, Any]
    zonesData: List[Dict[str, Any]]

# ============================================================================
# In-Memory Storage
# ============================================================================

# Registered devices
registered_devices: Dict[str, Dict[str, Any]] = {}

# Device configurations (deviceId -> config)
device_configs: Dict[str, Dict[str, Any]] = {}

# Config hashes for change detection
config_hashes: Dict[str, str] = {}

# Received sensor data
sensor_data_log: List[Dict[str, Any]] = []

# ============================================================================
# Helper Functions
# ============================================================================

def generate_config_hash(config: Dict[str, Any]) -> str:
    """Generate hash for configuration to detect changes"""
    config_str = json.dumps(config, sort_keys=True)
    return hashlib.md5(config_str.encode()).hexdigest()

def create_default_config() -> Dict[str, Any]:
    """Create a default empty configuration"""
    return {
        "type": "config",
        "zones": []
    }

# ============================================================================
# API Endpoints
# ============================================================================

@app.post("/api/devices/register")
async def register_device(registration: DeviceRegistration, request: Request):
    """
    Register a new ESP32 device
    Expected payload:
    {
        "type": "register",
        "deviceId": "ESP32_IRRIGATION_11100454456464674",
        "capacity": {"zones": 4, "sensors": 12},
        "timestamp": "...",
        "latitude": 35.6695,
        "longitude": -5.7857
    }
    """
    device_id = registration.deviceId

    # Store device registration
    registered_devices[device_id] = {
        "deviceId": device_id,
        "capacity": registration.capacity,
        "latitude": registration.latitude,
        "longitude": registration.longitude,
        "registeredAt": datetime.now().isoformat(),
        "lastSeen": datetime.now().isoformat()
    }

    # Initialize default config if not exists
    if device_id not in device_configs:
        device_configs[device_id] = create_default_config()
        config_hashes[device_id] = generate_config_hash(device_configs[device_id])

    print(f"[{datetime.now().isoformat()}] Device registered: {device_id}")
    print(f"  Capacity: {registration.capacity}")
    print(f"  Location: ({registration.latitude}, {registration.longitude})")

    return {
        "status": "registered",
        "deviceId": device_id,
        "message": "Device registered successfully"
    }

@app.get("/api/devices/{device_id}/config")
async def get_device_config(
    device_id: str,
    x_last_config_hash: Optional[str] = Header(None, alias="X-Last-Config-Hash")
):
    """
    Get device configuration
    Supports conditional request with X-Last-Config-Hash header
    Returns 304 Not Modified if config hasn't changed
    """
    # Check if device is registered
    if device_id not in registered_devices:
        raise HTTPException(status_code=404, detail="Device not registered")

    # Get current config
    config = device_configs.get(device_id, create_default_config())
    current_hash = config_hashes.get(device_id, "")

    # Check if config hasn't changed (conditional request)
    if x_last_config_hash and x_last_config_hash == current_hash:
        return Response(status_code=304)  # Not Modified

    # Update last seen
    registered_devices[device_id]["lastSeen"] = datetime.now().isoformat()

    # Return config with hash header
    response = JSONResponse(content=config)
    response.headers["X-Config-Hash"] = current_hash

    print(f"[{datetime.now().isoformat()}] Config requested for: {device_id}")
    print(f"  Hash: {current_hash}")
    print(f"  Zones: {len(config.get('zones', []))}")

    return response

@app.post("/api/devices/sensor-data")
async def receive_sensor_data(sensor_data: SensorData, request: Request):
    """
    Receive sensor data from ESP32 device
    Expected payload:
    {
        "type": "data",
        "deviceId": "...",
        "timestamp": "...",
        "globalData": {
            "temperature": 24.5,
            "humidity": 60.0,
            "pressure": 1012.0,
            "batteryLevel": 85.0,
            "signalStrength": -45
        },
        "zonesData": [
            {
                "zoneId": "zone_001",
                "soilMoisture": [
                    {"sensorId": "s01", "value": 45.5},
                    ...
                ]
            }
        ]
    }
    """
    device_id = sensor_data.deviceId

    # Check if device is registered
    if device_id not in registered_devices:
        raise HTTPException(status_code=404, detail="Device not registered")

    # Store sensor data
    data_entry = {
        "deviceId": device_id,
        "timestamp": datetime.now().isoformat(),
        "data": sensor_data.dict()
    }

    sensor_data_log.append(data_entry)

    # Keep only last 1000 entries
    if len(sensor_data_log) > 1000:
        sensor_data_log.pop(0)

    # Update last seen
    registered_devices[device_id]["lastSeen"] = datetime.now().isoformat()

    # Display received sensor data
    print(f"\n{'='*60}")
    print(f"[{datetime.now().isoformat()}] 📊 Sensor data received from: {device_id}")
    print(f"{'='*60}")

    # Global environmental data
    global_data = sensor_data.globalData
    print(f"\n🌡️  Global Environmental Data:")
    print(f"   Temperature: {global_data.get('temperature', 'N/A')}°C")
    print(f"   Humidity: {global_data.get('humidity', 'N/A')}%")
    print(f"   Pressure: {global_data.get('pressure', 'N/A')} hPa")
    print(f"   Battery Level: {global_data.get('batteryLevel', 'N/A')}%")
    print(f"   Signal Strength: {global_data.get('signalStrength', 'N/A')} dBm")

    # Zones data
    print(f"\n🌾 Zones Data ({len(sensor_data.zonesData)} zones):")
    for idx, zone_data in enumerate(sensor_data.zonesData, 1):
        zone_id = zone_data.get('zoneId', 'unknown')
        soil_moisture = zone_data.get('soilMoisture', [])

        print(f"\n   Zone {idx}: {zone_id}")
        print(f"   └─ Sensors ({len(soil_moisture)} sensors):")

        for sensor in soil_moisture:
            sensor_id = sensor.get('sensorId', 'unknown')
            sensor_value = sensor.get('value', 'N/A')
            print(f"      • {sensor_id}: {sensor_value}%")

    print(f"\n{'='*60}\n")

    return {
        "status": "received",
        "deviceId": device_id,
        "timestamp": datetime.now().isoformat()
    }

# ============================================================================
# Configuration Management Endpoints
# ============================================================================

@app.put("/api/devices/{device_id}/config")
async def update_device_config(device_id: str, config: Dict[str, Any]):
    """
    Update device configuration
    This endpoint is used by the test script to set configurations
    """
    if device_id not in registered_devices:
        raise HTTPException(status_code=404, detail="Device not registered")

    # Validate config structure
    if "type" not in config:
        config["type"] = "config"
    if "zones" not in config:
        config["zones"] = []

    # Update config
    device_configs[device_id] = config
    config_hashes[device_id] = generate_config_hash(config)

    print(f"[{datetime.now().isoformat()}] Config updated for: {device_id}")
    print(f"  New hash: {config_hashes[device_id]}")
    print(f"  Zones: {len(config.get('zones', []))}")

    return {
        "status": "updated",
        "deviceId": device_id,
        "configHash": config_hashes[device_id]
    }

@app.delete("/api/devices/{device_id}/zones/{zone_id}")
async def delete_zone(device_id: str, zone_id: str):
    """
    Delete a zone from device configuration
    """
    if device_id not in registered_devices:
        raise HTTPException(status_code=404, detail="Device not registered")

    config = device_configs.get(device_id, create_default_config())
    zones = config.get("zones", [])

    # Remove zone
    original_count = len(zones)
    config["zones"] = [z for z in zones if z.get("zoneId") != zone_id]

    # Update hash
    config_hashes[device_id] = generate_config_hash(config)
    device_configs[device_id] = config

    print(f"[{datetime.now().isoformat()}] Zone deleted: {device_id}/{zone_id}")
    print(f"  Zones before: {original_count}, after: {len(config['zones'])}")

    return {
        "status": "deleted",
        "deviceId": device_id,
        "zoneId": zone_id,
        "configHash": config_hashes[device_id]
    }

# ============================================================================
# Status & Debug Endpoints
# ============================================================================

@app.get("/api/devices")
async def list_devices():
    """List all registered devices"""
    return {
        "devices": list(registered_devices.values()),
        "count": len(registered_devices)
    }

@app.get("/api/devices/{device_id}/status")
async def get_device_status(device_id: str):
    """Get device status and last sensor data"""
    if device_id not in registered_devices:
        raise HTTPException(status_code=404, detail="Device not registered")

    # Get last sensor data for this device
    last_data = None
    for entry in reversed(sensor_data_log):
        if entry["deviceId"] == device_id:
            last_data = entry["data"]
            break

    return {
        "device": registered_devices[device_id],
        "config": device_configs.get(device_id),
        "configHash": config_hashes.get(device_id),
        "lastSensorData": last_data
    }

@app.get("/api/sensor-data")
async def get_sensor_data(device_id: Optional[str] = None, limit: int = 100):
    """Get sensor data log"""
    data = sensor_data_log

    if device_id:
        data = [entry for entry in sensor_data_log if entry["deviceId"] == device_id]

    return {
        "data": data[-limit:],
        "count": len(data[-limit:])
    }

@app.get("/health")
async def health_check():
    """Health check endpoint"""
    return {
        "status": "healthy",
        "service": "irrigation_server",
        "registered_devices": len(registered_devices),
        "sensor_data_entries": len(sensor_data_log),
        "timestamp": datetime.now().isoformat()
    }

@app.delete("/api/devices/{device_id}")
async def unregister_device(device_id: str):
    """Unregister a device (for testing)"""
    if device_id in registered_devices:
        del registered_devices[device_id]
    if device_id in device_configs:
        del device_configs[device_id]
    if device_id in config_hashes:
        del config_hashes[device_id]

    return {
        "status": "unregistered",
        "deviceId": device_id
    }

# ============================================================================
# Main
# ============================================================================

if __name__ == "__main__":
    print("=" * 60)
    print("🚀 ESP32 Irrigation Server")
    print("=" * 60)
    print("📡 Endpoints:")
    print("  POST   /api/devices/register          - Register device")
    print("  GET    /api/devices/{id}/config       - Get configuration")
    print("  PUT    /api/devices/{id}/config       - Update configuration")
    print("  POST   /api/devices/sensor-data       - Receive sensor data")
    print("  DELETE /api/devices/{id}/zones/{zid} - Delete zone")
    print("  GET    /api/devices                   - List devices")
    print("  GET    /api/devices/{id}/status       - Device status")
    print("  GET    /api/sensor-data               - Sensor data log")
    print("  GET    /health                        - Health check")
    print("=" * 60)
    print("🌐 Server: http://0.0.0.0:8000")
    print("📝 Docs: http://0.0.0.0:8000/docs")
    print("=" * 60)

    uvicorn.run(
        "irrigation_server:app",
        host="0.0.0.0",
        port=8000,
        reload=True,
        log_level="info"
    )

