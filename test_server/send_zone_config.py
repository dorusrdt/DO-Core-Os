#!/usr/bin/env python3
"""
Script to send different zone configurations to the irrigation server
Supports multiple configuration scenarios for testing
"""

import requests
import json
import sys
from datetime import datetime
from typing import Dict, Any, List

# Server configuration
SERVER_URL = "http://192.168.1.72:8000"
DEVICE_ID = "ESP32_IRRIGATION_11100454456464674"

# ============================================================================
# Configuration Templates
# ============================================================================

def create_config_1_zone() -> Dict[str, Any]:
    """Single zone configuration - Zone 1 with 3 sensors"""
    return {
        "type": "config",
        "zones": [
            {
                "zoneId": "zone_001",
                "physicalZoneNumber": 1,
                "waterPerDay": 5000,  # 5L per day
                "irrigationTime": "08:00",
                "humidityThreshold": 40,
                "sensors": [
                    {"sensorId": "s01"},
                    {"sensorId": "s02"},
                    {"sensorId": "s03"}
                ]
            }
        ]
    }

def create_config_2_zones() -> Dict[str, Any]:
    """Two zones configuration"""
    return {
        "type": "config",
        "zones": [
            {
                "zoneId": "zone_001",
                "physicalZoneNumber": 1,
                "waterPerDay": 5000,
                "irrigationTime": "08:00",
                "humidityThreshold": 40,
                "sensors": [
                    {"sensorId": "s01"},
                    {"sensorId": "s02"},
                    {"sensorId": "s03"}
                ]
            },
            {
                "zoneId": "zone_002",
                "physicalZoneNumber": 2,
                "waterPerDay": 3000,
                "irrigationTime": "09:00",
                "humidityThreshold": 45,
                "sensors": [
                    {"sensorId": "s04"},
                    {"sensorId": "s05"},
                    {"sensorId": "s06"}
                ]
            }
        ]
    }

def create_config_4_zones_full() -> Dict[str, Any]:
    """Full 4 zones configuration using all 12 sensors"""
    return {
        "type": "config",
        "zones": [
            {
                "zoneId": "zone_001",
                "physicalZoneNumber": 1,
                "waterPerDay": 5000,
                "irrigationTime": "08:00",
                "humidityThreshold": 40,
                "sensors": [
                    {"sensorId": "s01"},
                    {"sensorId": "s02"},
                    {"sensorId": "s03"}
                ]
            },
            {
                "zoneId": "zone_002",
                "physicalZoneNumber": 2,
                "waterPerDay": 3000,
                "irrigationTime": "09:00",
                "humidityThreshold": 45,
                "sensors": [
                    {"sensorId": "s04"},
                    {"sensorId": "s05"},
                    {"sensorId": "s06"}
                ]
            },
            {
                "zoneId": "zone_003",
                "physicalZoneNumber": 3,
                "waterPerDay": 4000,
                "irrigationTime": "10:00",
                "humidityThreshold": 50,
                "sensors": [
                    {"sensorId": "s07"},
                    {"sensorId": "s08"},
                    {"sensorId": "s09"}
                ]
            },
            {
                "zoneId": "zone_004",
                "physicalZoneNumber": 4,
                "waterPerDay": 6000,
                "irrigationTime": "11:00",
                "humidityThreshold": 35,
                "sensors": [
                    {"sensorId": "s10"},
                    {"sensorId": "s11"},
                    {"sensorId": "s12"}
                ]
            }
        ]
    }

def create_config_emergency_low_threshold() -> Dict[str, Any]:
    """Configuration with very low humidity thresholds (triggers emergency irrigation)"""
    return {
        "type": "config",
        "zones": [
            {
                "zoneId": "zone_001",
                "physicalZoneNumber": 1,
                "waterPerDay": 5000,
                "irrigationTime": "08:00",
                "humidityThreshold": 20,  # Very low - will trigger emergency
                "sensors": [
                    {"sensorId": "s01"},
                    {"sensorId": "s02"},
                    {"sensorId": "s03"}
                ]
            },
            {
                "zoneId": "zone_002",
                "physicalZoneNumber": 2,
                "waterPerDay": 3000,
                "irrigationTime": "09:00",
                "humidityThreshold": 25,  # Very low
                "sensors": [
                    {"sensorId": "s04"},
                    {"sensorId": "s05"},
                    {"sensorId": "s06"}
                ]
            }
        ]
    }

def create_config_multiple_sensors_per_zone() -> Dict[str, Any]:
    """Configuration with multiple sensors per zone (testing sensor assignment)"""
    return {
        "type": "config",
        "zones": [
            {
                "zoneId": "zone_001",
                "physicalZoneNumber": 1,
                "waterPerDay": 5000,
                "irrigationTime": "08:00",
                "humidityThreshold": 40,
                "sensors": [
                    {"sensorId": "s01"},
                    {"sensorId": "s02"},
                    {"sensorId": "s03"},
                    {"sensorId": "s04"},
                    {"sensorId": "s05"}
                ]
            },
            {
                "zoneId": "zone_002",
                "physicalZoneNumber": 2,
                "waterPerDay": 3000,
                "irrigationTime": "09:00",
                "humidityThreshold": 45,
                "sensors": [
                    {"sensorId": "s06"},
                    {"sensorId": "s07"},
                    {"sensorId": "s08"},
                    {"sensorId": "s09"},
                    {"sensorId": "s10"},
                    {"sensorId": "s11"},
                    {"sensorId": "s12"}
                ]
            }
        ]
    }

def create_config_update_existing() -> Dict[str, Any]:
    """Update existing zone configuration (testing zone update logic)"""
    return {
        "type": "config",
        "zones": [
            {
                "zoneId": "zone_001",  # Same zoneId, different config
                "physicalZoneNumber": 1,
                "waterPerDay": 8000,  # Changed from 5000
                "irrigationTime": "07:00",  # Changed from 08:00
                "humidityThreshold": 35,  # Changed from 40
                "sensors": [
                    {"sensorId": "s01"},
                    {"sensorId": "s02"},
                    {"sensorId": "s03"},
                    {"sensorId": "s04"}  # Added sensor
                ]
            }
        ]
    }

def create_config_empty() -> Dict[str, Any]:
    """Empty configuration (no zones)"""
    return {
        "type": "config",
        "zones": []
    }

# ============================================================================
# Helper Functions
# ============================================================================

def send_config(config: Dict[str, Any], device_id: str = DEVICE_ID) -> bool:
    """Send configuration to server"""
    url = f"{SERVER_URL}/api/devices/{device_id}/config"

    try:
        response = requests.put(url, json=config, timeout=10)

        if response.status_code == 200:
            result = response.json()
            print(f"✅ Configuration sent successfully")
            print(f"   Config hash: {result.get('configHash', 'N/A')}")
            return True
        else:
            print(f"❌ Error: {response.status_code}")
            print(f"   Response: {response.text}")
            return False

    except requests.exceptions.RequestException as e:
        print(f"❌ Connection error: {e}")
        return False

def delete_zone(zone_id: str, device_id: str = DEVICE_ID) -> bool:
    """Delete a zone from configuration"""
    url = f"{SERVER_URL}/api/devices/{device_id}/zones/{zone_id}"

    try:
        response = requests.delete(url, timeout=10)

        if response.status_code == 200:
            print(f"✅ Zone {zone_id} deleted successfully")
            return True
        else:
            print(f"❌ Error: {response.status_code}")
            print(f"   Response: {response.text}")
            return False

    except requests.exceptions.RequestException as e:
        print(f"❌ Connection error: {e}")
        return False

def get_device_status(device_id: str = DEVICE_ID) -> Dict[str, Any]:
    """Get device status"""
    url = f"{SERVER_URL}/api/devices/{device_id}/status"

    try:
        response = requests.get(url, timeout=10)

        if response.status_code == 200:
            return response.json()
        else:
            print(f"❌ Error: {response.status_code}")
            return {}

    except requests.exceptions.RequestException as e:
        print(f"❌ Connection error: {e}")
        return {}

# ============================================================================
# Main
# ============================================================================

def main():
    """Main function"""
    if len(sys.argv) < 2:
        print("Usage: python send_zone_config.py <config_type> [options]")
        print("\nAvailable configurations:")
        print("  1-zone              - Single zone with 3 sensors")
        print("  2-zones             - Two zones with 6 sensors")
        print("  4-zones             - Full 4 zones with 12 sensors")
        print("  emergency           - Low thresholds (triggers emergency)")
        print("  multiple-sensors    - Multiple sensors per zone")
        print("  update              - Update existing zone config")
        print("  empty               - Empty configuration (no zones)")
        print("\nZone management:")
        print("  delete <zone_id>    - Delete a zone")
        print("  status              - Show device status")
        print("\nExample:")
        print("  python send_zone_config.py 4-zones")
        print("  python send_zone_config.py delete zone_001")
        print("  python send_zone_config.py status")
        sys.exit(1)

    command = sys.argv[1].lower()

    print("=" * 60)
    print(f"📤 Sending Configuration: {command}")
    print(f"🌐 Server: {SERVER_URL}")
    print(f"📱 Device: {DEVICE_ID}")
    print("=" * 60)

    config_map = {
        "1-zone": create_config_1_zone,
        "2-zones": create_config_2_zones,
        "4-zones": create_config_4_zones_full,
        "emergency": create_config_emergency_low_threshold,
        "multiple-sensors": create_config_multiple_sensors_per_zone,
        "update": create_config_update_existing,
        "empty": create_config_empty
    }

    if command == "delete":
        if len(sys.argv) < 3:
            print("❌ Error: Zone ID required")
            print("Usage: python send_zone_config.py delete <zone_id>")
            sys.exit(1)
        zone_id = sys.argv[2]
        success = delete_zone(zone_id)
        sys.exit(0 if success else 1)

    elif command == "status":
        status = get_device_status()
        if status:
            print("\n📊 Device Status:")
            print(json.dumps(status, indent=2))
        sys.exit(0)

    elif command in config_map:
        config = config_map[command]()
        print(f"\n📋 Configuration:")
        print(json.dumps(config, indent=2))
        print()
        success = send_config(config)
        sys.exit(0 if success else 1)

    else:
        print(f"❌ Unknown configuration type: {command}")
        print("Run without arguments to see available options")
        sys.exit(1)

if __name__ == "__main__":
    main()

