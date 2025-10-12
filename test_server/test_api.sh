#!/bin/bash

SERVER="http://192.168.1.3:3000"
DEVICE_ID="ESP32_IRRIGATION_11100454456464674"

echo "=========================================="
echo "🧪 Testing Irrigation Server API"
echo "=========================================="
echo ""

# Test 1: Health Check
echo "1️⃣  Testing Health Check..."
curl -s "$SERVER/api/health" | jq '.'
echo ""

# Test 2: List Devices (should be empty)
echo "2️⃣  Listing Devices (before registration)..."
curl -s "$SERVER/api/devices" | jq '.'
echo ""

# Test 3: Create Test Zone
echo "3️⃣  Creating Test Zone..."
curl -s -X POST "$SERVER/api/test/create-zone?device_id=$DEVICE_ID" | jq '.'
echo ""

# Test 4: List Devices (should have 1 device)
echo "4️⃣  Listing Devices (after zone creation)..."
curl -s "$SERVER/api/devices" | jq '.'
echo ""

# Test 5: Get Device Info
echo "5️⃣  Getting Device Info..."
curl -s "$SERVER/api/devices/$DEVICE_ID" | jq '.'
echo ""

# Test 6: Create Another Zone
echo "6️⃣  Creating Another Zone..."
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

# Test 7: Get Config (should have 2 zones)
echo "7️⃣  Getting Device Config..."
curl -s "$SERVER/api/devices/$DEVICE_ID/config" | jq '.'
echo ""

# Test 8: Delete Zone
echo "8️⃣  Deleting Zone..."
curl -s -X DELETE "$SERVER/api/devices/$DEVICE_ID/zones/zone_jardin_sud" | jq '.'
echo ""

# Test 9: Get Config (should have delete command)
echo "9️⃣  Getting Device Config (with delete command)..."
curl -s "$SERVER/api/devices/$DEVICE_ID/config" | jq '.'
echo ""

# Test 10: Sensor Data History
echo "🔟 Getting Sensor Data History..."
curl -s "$SERVER/api/sensor-data/history?limit=5" | jq '.'
echo ""

echo "=========================================="
echo "✅ API Tests Complete!"
echo "=========================================="
echo ""
echo "Check the server console for detailed logs"
echo ""
