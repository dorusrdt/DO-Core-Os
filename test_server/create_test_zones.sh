#!/bin/bash

# Updated Test Zone Creation Script for Updated Irrigation Server v2.0
# Works with localhost:3000 for independent testing
# Usage: ./create_test_zones.sh [1-zone|2-zones|3-zones|4-zones]

SERVER="http://localhost:3000"
DEVICE_ID="ESP32_IRRIGATION_11100454456464674"

# Parse arguments - default to all 4 zones
ZONES_TO_CREATE="${1:-4-zones}"

# Validate argument
case "$ZONES_TO_CREATE" in
  1-zone|1|one)
    ZONES_TO_CREATE="1-zone"
    ZONE_COUNT=1
    ;;
  2-zones|2|two)
    ZONES_TO_CREATE="2-zones"
    ZONE_COUNT=2
    ;;
  3-zones|3|three)
    ZONES_TO_CREATE="3-zones"
    ZONE_COUNT=3
    ;;
  4-zones|4|four|*)
    ZONES_TO_CREATE="4-zones"
    ZONE_COUNT=4
    ;;
esac

echo "=========================================="
echo "🌱 Creating Test Zones (v2.0)"
echo "=========================================="
echo ""
echo "Server: $SERVER"
echo "Device: $DEVICE_ID"
echo "Zones to create: $ZONE_COUNT (mode: $ZONES_TO_CREATE)"
echo ""

# First, verify device is registered
echo "1️⃣  Checking device registration..."
DEVICE_CHECK=$(curl -s "$SERVER/api/devices/$DEVICE_ID")
if echo "$DEVICE_CHECK" | grep -q "Device not found"; then
    echo "❌ Device not registered. Register the device first:"
    echo "   curl -X POST $SERVER/api/devices/register -H 'Content-Type: application/json' -d '{...}'"
    exit 1
fi
echo "✅ Device found!"
echo ""

# Zone 1: Potager Nord (Tomates)
if [ "$ZONE_COUNT" -ge 1 ]; then
  echo "2️⃣  Creating Zone: Potager Nord (Tomates)..."
  curl -s -X POST "$SERVER/api/devices/$DEVICE_ID/zones" \
    -H "Content-Type: application/json" \
    -d '{
      "zoneId": "zone_potager_nord",
      "physicalZoneNumber": 1,
      "waterPerDay": 2000,
      "irrigationTime": "08:00",
      "humidityThreshold": 70,
      "sensors": [
        {"sensorId": "s01"},
        {"sensorId": "s02"},
        {"sensorId": "s04"}
      ],
      "irrigationSchedule": [
        {
          "time": "08:00",
          "duration": 15,
          "daysOfWeek": [1, 3, 5],
          "isActive": true
        },
        {
          "time": "03:39",
          "duration": 20,
          "daysOfWeek": [0, 2, 4, 6],
          "isActive": true
        }
      ]
    }' | jq '.'
  echo ""
fi

# Zone 2: Jardin Sud (Laitue)
if [ "$ZONE_COUNT" -ge 2 ]; then
  echo "3️⃣  Creating Zone: Jardin Sud (Laitue)..."
  curl -s -X POST "$SERVER/api/devices/$DEVICE_ID/zones" \
    -H "Content-Type: application/json" \
    -d '{
      "zoneId": "zone_jardin_sud",
      "physicalZoneNumber": 2,
      "waterPerDay": 3000,
      "irrigationTime": "18:00",
      "humidityThreshold": 70,
      "sensors": [
        {"sensorId": "s04"},
        {"sensorId": "s05"},
        {"sensorId": "s06"}
      ],
      "irrigationSchedule": [
        {
          "time": "06:00",
          "duration": 20,
          "daysOfWeek": [1, 2, 3, 4, 5],
          "isActive": true
        },
        {
          "time": "20:00",
          "duration": 15,
          "daysOfWeek": [0, 6],
          "isActive": true
        }
      ]
    }' | jq '.'
  echo ""
fi

# Zone 3: Serre (Carottes)
if [ "$ZONE_COUNT" -ge 3 ]; then
  echo "4️⃣  Creating Zone: Serre (Carottes)..."
  curl -s -X POST "$SERVER/api/devices/$DEVICE_ID/zones" \
    -H "Content-Type: application/json" \
    -d '{
      "zoneId": "zone_serre",
      "physicalZoneNumber": 3,
      "waterPerDay": 1500,
      "irrigationTime": "12:00",
      "humidityThreshold": 75,
      "sensors": [
        {"sensorId": "s07"},
        {"sensorId": "s08"},
        {"sensorId": "s09"}
      ],
      "irrigationSchedule": [
        {
          "time": "10:00",
          "duration": 10,
          "daysOfWeek": [1, 3, 5],
          "isActive": true
        }
      ]
    }' | jq '.'
  echo ""
fi

# Zone 4: Verger (Arbres Fruitiers)
if [ "$ZONE_COUNT" -ge 4 ]; then
  echo "5️⃣  Creating Zone: Verger (Arbres Fruitiers)..."
  curl -s -X POST "$SERVER/api/devices/$DEVICE_ID/zones" \
    -H "Content-Type: application/json" \
    -d '{
      "zoneId": "zone_verger",
      "physicalZoneNumber": 4,
      "waterPerDay": 1000,
      "irrigationTime": "06:00",
      "humidityThreshold": 60,
      "sensors": [
        {"sensorId": "s10"},
        {"sensorId": "s11"},
        {"sensorId": "s12"}
      ],
      "irrigationSchedule": [
        {
          "time": "07:00",
          "duration": 25,
          "daysOfWeek": [0, 1, 2, 3, 4, 5, 6],
          "isActive": true
        }
      ]
    }' | jq '.'
  echo ""
fi

echo "=========================================="
echo "✅ Test Zones Created! ($ZONE_COUNT zone(s))"
echo "=========================================="
echo ""
echo "📋 Verify zones:"
echo "  curl $SERVER/api/devices/$DEVICE_ID/zones | jq '.'"
echo ""
echo "📊 Device status:"
echo "  curl $SERVER/api/devices/$DEVICE_ID | jq '.'"
echo ""
echo "🌐 API Documentation:"
echo "  $SERVER/docs"
echo ""
echo "Wait 2 seconds for configuration update..."
echo ""

# Optional: Wait and show config update
sleep 2
echo "📈 Current configuration:"
curl -s "$SERVER/api/devices/$DEVICE_ID/config" | jq '.'
echo ""
