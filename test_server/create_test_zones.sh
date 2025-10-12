#!/bin/bash

SERVER="http://192.168.1.3:3000"
DEVICE_ID="ESP32_IRRIGATION_11100454456464674"

echo "=========================================="
echo "🌱 Creating Test Zones"
echo "=========================================="
echo ""

# Zone 1: Potager Nord (Tomates)
echo "1️⃣  Creating Zone: Potager Nord (Tomates)..."
curl -s -X POST "$SERVER/api/devices/$DEVICE_ID/zones" \
  -H "Content-Type: application/json" \
  -d '{
    "zoneId": "zone_potager_nord",
    "waterPerDay": 2000,
    "irrigationTime": "08:00",
    "humidityThreshold": 25,
    "sensors": [
      {"sensorId": "s_01"},
      {"sensorId": "s_02"},
      {"sensorId": "s_03"}
    ]
  }' | jq '.'
echo ""

# Zone 2: Jardin Sud (Laitue)
echo "2️⃣  Creating Zone: Jardin Sud (Laitue)..."
curl -s -X POST "$SERVER/api/devices/$DEVICE_ID/zones" \
  -H "Content-Type: application/json" \
  -d '{
    "zoneId": "zone_jardin_sud",
    "waterPerDay": 3000,
    "irrigationTime": "18:00",
    "humidityThreshold": 30,
    "sensors": [
      {"sensorId": "s_04"},
      {"sensorId": "s_05"},
      {"sensorId": "s_06"}
    ]
  }' | jq '.'
echo ""

# Zone 3: Serre (Carottes)
echo "3️⃣  Creating Zone: Serre (Carottes)..."
curl -s -X POST "$SERVER/api/devices/$DEVICE_ID/zones" \
  -H "Content-Type: application/json" \
  -d '{
    "zoneId": "zone_serre",
    "waterPerDay": 1500,
    "irrigationTime": "12:00",
    "humidityThreshold": 35,
    "sensors": [
      {"sensorId": "s_07"},
      {"sensorId": "s_08"},
      {"sensorId": "s_09"}
    ]
  }' | jq '.'
echo ""

echo "=========================================="
echo "✅ Test Zones Created!"
echo "=========================================="
echo ""
echo "Wait 10 seconds for ESP32 to poll config..."
echo ""
echo "Check device config:"
echo "  curl $SERVER/api/devices/$DEVICE_ID | jq '.'"
echo ""
