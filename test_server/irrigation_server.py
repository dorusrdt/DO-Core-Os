#!/usr/bin/env python3
"""
FastAPI Server for ESP32 Irrigation System - v2.0
Simulates the external server (192.168.1.72:8000)
Handles device registration, configuration, sensor data, and zone management
UPDATED: Now includes full zone management endpoints for web interface
"""

from fastapi import FastAPI, Request, HTTPException, Header, Response
from fastapi.responses import JSONResponse
from datetime import datetime
from typing import Optional, Dict, Any, List
import json
import uvicorn
import hashlib
from pydantic import BaseModel
from fastapi.middleware.cors import CORSMiddleware

app = FastAPI(title="ESP32 Irrigation Server", version="2.0.0")

# Enable CORS for web interface
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# ============================================================================
# Data Models
# ============================================================================

class DeviceCapacity(BaseModel):
    zones: int
    sensors: int

class DeviceRegistration(BaseModel):
    type: str
    deviceId: str
    capacity: DeviceCapacity
    timestamp: str
    latitude: Optional[float] = None
    longitude: Optional[float] = None

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

class IrrigationSchedule(BaseModel):
    """Irrigation schedule with time, duration, and days of week"""
    time: str  # Format HH:MM (e.g., "08:00")
    duration: int  # Duration in minutes (1-300)
    daysOfWeek: List[int]  # Days of week (0=Sunday, 1=Monday, ..., 6=Saturday)
    isActive: Optional[bool] = True

class ZoneConfig(BaseModel):
    """Zone configuration with all required fields for ESP32"""
    zoneId: str
    physicalZoneNumber: int  # Physical zone number (1-4 typically)
    waterPerDay: int  # Water per day in ml
    irrigationTime: Optional[str] = None  # Legacy: single time (HH:MM)
    irrigationTimes: Optional[List[str]] = None  # Legacy: list of times
    irrigationSchedule: Optional[List[IrrigationSchedule]] = None  # New: detailed schedules
    humidityThreshold: int  # Humidity threshold in percentage
    sensors: List[SensorConfig]

# ============================================================================
# In-Memory Storage
# ============================================================================

registered_devices: Dict[str, Dict[str, Any]] = {}
device_zones: Dict[str, List[Dict[str, Any]]] = {}
device_configs: Dict[str, Dict[str, Any]] = {}
config_hashes: Dict[str, str] = {}
sensor_data_log: List[Dict[str, Any]] = []
pending_commands: Dict[str, List[Dict[str, str]]] = {}

# ============================================================================
# Helper Functions
# ============================================================================

def generate_config_hash(config: Dict[str, Any]) -> str:
    """Generate hash for configuration to detect changes"""
    config_str = json.dumps(config, sort_keys=True)
    return hashlib.md5(config_str.encode()).hexdigest()

def create_default_config() -> Dict[str, Any]:
    """Create a default empty configuration"""
    return {"type": "config", "zones": [], "commands": []}

def build_config_from_zones(zones: List[Dict[str, Any]]) -> Dict[str, Any]:
    """Build a config response from zones list"""
    return {"type": "config", "zones": zones, "commands": []}

# ============================================================================
# API Endpoints - Device Registration & Core Operations
# ============================================================================

@app.post("/api/devices/register")
async def register_device(registration: DeviceRegistration, request: Request):
    """Register a new ESP32 device"""
    device_id = registration.deviceId

    registered_devices[device_id] = {
        "deviceId": device_id,
        "capacity": registration.capacity.dict() if hasattr(registration.capacity, 'dict') else registration.capacity,
        "latitude": registration.latitude,
        "longitude": registration.longitude,
        "registeredAt": datetime.now().isoformat(),
        "lastSeen": datetime.now().isoformat(),
        "status": "online"
    }

    if device_id not in device_zones:
        device_zones[device_id] = []
    if device_id not in device_configs:
        device_configs[device_id] = create_default_config()
        config_hashes[device_id] = generate_config_hash(device_configs[device_id])
    if device_id not in pending_commands:
        pending_commands[device_id] = []

    print(f"\n{'='*60}")
    print(f"[{datetime.now().isoformat()}] ✅ Device registered: {device_id}")
    print(f"  Capacity: {registration.capacity}")
    print(f"  Location: ({registration.latitude}, {registration.longitude})")
    print(f"{'='*60}\n")

    return {"status": "registered", "deviceId": device_id, "message": "Device registered successfully"}

@app.get("/api/devices/{device_id}/config")
async def get_device_config(
    device_id: str,
    x_last_config_hash: Optional[str] = Header(None, alias="X-Last-Config-Hash")
):
    """Get device configuration with optional hash validation"""
    if device_id not in registered_devices:
        raise HTTPException(status_code=404, detail="Device not registered")

    zones = device_zones.get(device_id, [])
    config = build_config_from_zones(zones)
    current_hash = generate_config_hash(config)

    if x_last_config_hash and x_last_config_hash == current_hash:
        return Response(status_code=304)

    registered_devices[device_id]["lastSeen"] = datetime.now().isoformat()
    response = JSONResponse(content=config)
    response.headers["X-Config-Hash"] = current_hash

    print(f"[{datetime.now().isoformat()}] ⚙️  Config requested for: {device_id}")
    print(f"  Hash: {current_hash}")
    print(f"  Zones: {len(zones)}")

    return response

@app.post("/api/devices/sensor-data")
async def receive_sensor_data(sensor_data: SensorDataRequest, request: Request):
    """Receive sensor data from ESP32 device"""
    device_id = sensor_data.deviceId

    if device_id not in registered_devices:
        raise HTTPException(status_code=404, detail="Device not registered")

    data_entry = {
        "deviceId": device_id,
        "timestamp": datetime.now().isoformat(),
        "data": sensor_data.dict()
    }

    sensor_data_log.append(data_entry)
    if len(sensor_data_log) > 1000:
        sensor_data_log.pop(0)

    registered_devices[device_id]["lastSeen"] = datetime.now().isoformat()

    print(f"\n{'='*60}")
    print(f"[{datetime.now().isoformat()}] 📊 Sensor data received from: {device_id}")
    print(f"{'='*60}")

    global_data = sensor_data.globalData
    print(f"\n🌡️  Global Environmental Data:")
    print(f"   Temperature: {global_data.temperature}°C")
    print(f"   Humidity: {global_data.humidity}%")
    print(f"   Pressure: {global_data.pressure} hPa")
    print(f"   Battery Level: {global_data.batteryLevel}%")
    print(f"   Signal Strength: {global_data.signalStrength} dBm")

    print(f"\n🌾 Zones Data ({len(sensor_data.zonesData)} zones):")
    for idx, zone_data in enumerate(sensor_data.zonesData, 1):
        zone_id = zone_data.zoneId
        soil_moisture = zone_data.soilMoisture

        print(f"\n   Zone {idx}: {zone_id}")
        print(f"   └─ Sensors ({len(soil_moisture)} sensors):")

        for sensor in soil_moisture:
            sensor_id = sensor.sensorId
            sensor_value = sensor.value
            print(f"      • {sensor_id}: {sensor_value}%")

    print(f"\n{'='*60}\n")

    return {"status": "received", "deviceId": device_id, "timestamp": datetime.now().isoformat()}

# ============================================================================
# Zone Management Endpoints
# ============================================================================

@app.post("/api/devices/{device_id}/zones")
async def add_zone(device_id: str, zone: ZoneConfig):
    """Add a zone to a device"""
    if device_id not in registered_devices:
        raise HTTPException(status_code=404, detail="Device not registered")

    zones = device_zones.get(device_id, [])
    existing_zone = next((z for z in zones if z["zoneId"] == zone.zoneId), None)

    if existing_zone:
        existing_zone.update(zone.dict())
        print(f"   ✅ Zone updated")
    else:
        zones.append(zone.dict())
        device_zones[device_id] = zones
        print(f"   ✅ Zone added")

    return {"status": "success", "message": f"Zone {zone.zoneId} configured", "zone": zone.dict()}

@app.get("/api/devices/{device_id}/zones")
async def get_device_zones(device_id: str):
    """Get all zones for a device"""
    if device_id not in registered_devices:
        raise HTTPException(status_code=404, detail="Device not registered")

    zones = device_zones.get(device_id, [])
    return {"deviceId": device_id, "zones": zones, "count": len(zones)}

@app.get("/api/devices/{device_id}/zones/{zone_id}")
async def get_zone(device_id: str, zone_id: str):
    """Get a specific zone configuration"""
    if device_id not in registered_devices:
        raise HTTPException(status_code=404, detail="Device not registered")

    zones = device_zones.get(device_id, [])
    zone = next((z for z in zones if z["zoneId"] == zone_id), None)

    if not zone:
        raise HTTPException(status_code=404, detail="Zone not found")

    return {"deviceId": device_id, "zone": zone}

@app.delete("/api/devices/{device_id}/zones/{zone_id}")
async def delete_zone(device_id: str, zone_id: str):
    """Delete a zone from device configuration"""
    if device_id not in registered_devices:
        raise HTTPException(status_code=404, detail="Device not registered")

    zones = device_zones.get(device_id, [])
    original_count = len(zones)
    device_zones[device_id] = [z for z in zones if z.get("zoneId") != zone_id]

    print(f"[{datetime.now().isoformat()}] 🗑️  Zone deleted: {device_id}/{zone_id}")
    print(f"  Zones before: {original_count}, after: {len(device_zones[device_id])}")

    return {"status": "deleted", "deviceId": device_id, "zoneId": zone_id}

# ============================================================================
# Configuration Management
# ============================================================================

@app.put("/api/devices/{device_id}/config")
async def update_device_config(device_id: str, config: Dict[str, Any]):
    """Update device configuration"""
    if device_id not in registered_devices:
        raise HTTPException(status_code=404, detail="Device not registered")

    if "type" not in config:
        config["type"] = "config"
    if "zones" not in config:
        config["zones"] = []

    device_zones[device_id] = config.get("zones", [])
    device_configs[device_id] = config
    config_hashes[device_id] = generate_config_hash(config)

    print(f"[{datetime.now().isoformat()}] ⚙️  Config updated for: {device_id}")
    print(f"  New zones: {len(config.get('zones', []))}")

    return {"status": "updated", "deviceId": device_id, "zones": len(config.get('zones', []))}

# ============================================================================
# Device Management Endpoints
# ============================================================================

@app.get("/api/devices")
async def list_devices():
    """List all registered devices"""
    return {"devices": list(registered_devices.values()), "count": len(registered_devices)}

@app.get("/api/devices/{device_id}")
async def get_device_info(device_id: str):
    """Get detailed device information"""
    if device_id not in registered_devices:
        raise HTTPException(status_code=404, detail="Device not found")

    return {
        "device": registered_devices[device_id],
        "zones": device_zones.get(device_id, []),
        "pendingCommands": pending_commands.get(device_id, [])
    }

@app.get("/api/devices/{device_id}/status")
async def get_device_status(device_id: str):
    """Get device status and last sensor data"""
    if device_id not in registered_devices:
        raise HTTPException(status_code=404, detail="Device not registered")

    last_data = None
    for entry in reversed(sensor_data_log):
        if entry["deviceId"] == device_id:
            last_data = entry["data"]
            break

    return {
        "device": registered_devices[device_id],
        "zones": device_zones.get(device_id, []),
        "lastSensorData": last_data
    }

@app.delete("/api/devices/{device_id}")
async def unregister_device(device_id: str):
    """Unregister a device"""
    if device_id in registered_devices:
        del registered_devices[device_id]
    if device_id in device_zones:
        del device_zones[device_id]
    if device_id in device_configs:
        del device_configs[device_id]
    if device_id in config_hashes:
        del config_hashes[device_id]
    if device_id in pending_commands:
        del pending_commands[device_id]

    print(f"[{datetime.now().isoformat()}] 🗑️  Device unregistered: {device_id}")
    return {"status": "unregistered", "deviceId": device_id}

# ============================================================================
# Sensor Data Endpoints
# ============================================================================

@app.get("/api/sensor-data")
async def get_sensor_data(device_id: Optional[str] = None, limit: int = 100):
    """Get sensor data log"""
    data = sensor_data_log if device_id is None else [entry for entry in sensor_data_log if entry["deviceId"] == device_id]
    return {"data": data[-limit:], "count": len(data[-limit:])}

@app.get("/api/sensor-data/history")
async def get_sensor_history(limit: int = 10):
    """Get sensor data history"""
    return {"data": sensor_data_log[-limit:], "count": len(sensor_data_log[-limit:])}

@app.get("/api/sensor-data/latest/{device_id}")
async def get_latest_sensor_data(device_id: str):
    """Get latest sensor data for a device"""
    if device_id not in registered_devices:
        raise HTTPException(status_code=404, detail="Device not registered")

    for entry in reversed(sensor_data_log):
        if entry["deviceId"] == device_id:
            return entry

    return {"message": "No sensor data yet", "deviceId": device_id}

# ============================================================================
# Health & Info Endpoints
# ============================================================================

@app.get("/api/health")
async def health_check_v1():
    """Health check endpoint (v1)"""
    return {"status": "healthy", "timestamp": datetime.now().isoformat()}

@app.get("/health")
async def health_check():
    """Health check endpoint"""
    return {
        "status": "healthy",
        "service": "irrigation_server",
        "version": "2.0.0",
        "registered_devices": len(registered_devices),
        "sensor_data_entries": len(sensor_data_log),
        "timestamp": datetime.now().isoformat()
    }

@app.get("/")
async def root():
    """Root endpoint - API info"""
    return {
        "service": "Irrigation Server",
        "version": "2.0.0",
        "status": "running",
        "devices": len(registered_devices),
        "timestamp": datetime.now().isoformat(),
        "docs": "/docs"
    }

# ============================================================================
# Main
# ============================================================================

if __name__ == "__main__":
    print("=" * 60)
    print("🚀 ESP32 Irrigation Server v2.0")
    print("=" * 60)
    print("📡 Endpoints:")
    print("  === ESP32 Device Endpoints ===")
    print("  POST   /api/devices/register          - Register device")
    print("  GET    /api/devices/{id}/config       - Get configuration")
    print("  POST   /api/devices/sensor-data       - Receive sensor data")
    print("  === Zone Management ===")
    print("  POST   /api/devices/{id}/zones        - Add zone")
    print("  GET    /api/devices/{id}/zones        - List zones")
    print("  GET    /api/devices/{id}/zones/{zid}  - Get zone")
    print("  DELETE /api/devices/{id}/zones/{zid} - Delete zone")
    print("  === Web Management ===")
    print("  GET    /api/devices                   - List devices")
    print("  GET    /api/devices/{id}              - Get device info")
    print("  GET    /api/devices/{id}/status       - Device status")
    print("  DELETE /api/devices/{id}              - Unregister device")
    print("  === Sensor Data ===")
    print("  GET    /api/sensor-data               - Sensor data log")
    print("  GET    /api/sensor-data/history       - Sensor history")
    print("  GET    /api/sensor-data/latest/{id}   - Latest sensor data")
    print("  === Health ===")
    print("  GET    /health                        - Health check")
    print("  GET    /api/health                    - Health check (v1)")
    print("=" * 60)
    print("🌐 Server: http://0.0.0.0:3000")
    print("📝 Docs: http://0.0.0.0:3000/docs")
    print("=" * 60)

    uvicorn.run(
        "irrigation_server:app",
        host="0.0.0.0",
        port=3000,
        reload=True,
        log_level="info"
    )
