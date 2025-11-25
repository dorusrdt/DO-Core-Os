#!/bin/bash

# Test script for updated Irrigation Server v2.0
# Validates all endpoints are working correctly

SERVER="http://localhost:8000"
DEVICE_ID="ESP32_IRRIGATION_11100454456464674"

echo "🧪 Testing Updated Irrigation Server v2.0"
echo "=========================================="
echo ""

# Color codes
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test counter
TESTS_PASSED=0
TESTS_FAILED=0

# Function to test endpoint
test_endpoint() {
    local method=$1
    local endpoint=$2
    local data=$3
    local expected_code=$4
    local description=$5

    echo -n "Testing: $description ... "

    if [ "$method" = "GET" ]; then
        RESPONSE=$(curl -s -w "\n%{http_code}" "$SERVER$endpoint")
    elif [ "$method" = "POST" ]; then
        RESPONSE=$(curl -s -w "\n%{http_code}" -X POST "$SERVER$endpoint" \
            -H "Content-Type: application/json" \
            -d "$data")
    elif [ "$method" = "DELETE" ]; then
        RESPONSE=$(curl -s -w "\n%{http_code}" -X DELETE "$SERVER$endpoint")
    elif [ "$method" = "PUT" ]; then
        RESPONSE=$(curl -s -w "\n%{http_code}" -X PUT "$SERVER$endpoint" \
            -H "Content-Type: application/json" \
            -d "$data")
    fi

    HTTP_CODE=$(echo "$RESPONSE" | tail -n1)
    BODY=$(echo "$RESPONSE" | sed '$d')

    if [ "$HTTP_CODE" = "$expected_code" ]; then
        echo -e "${GREEN}✅ PASS${NC} (HTTP $HTTP_CODE)"
        ((TESTS_PASSED++))
    else
        echo -e "${RED}❌ FAIL${NC} (Expected $expected_code, got $HTTP_CODE)"
        echo "Response: $BODY"
        ((TESTS_FAILED++))
    fi
}

# 1. Health check
echo "═══ Health Endpoints ═══"
test_endpoint "GET" "/health" "" "200" "Health check"
test_endpoint "GET" "/api/health" "" "200" "API health check"
test_endpoint "GET" "/" "" "200" "Root endpoint"
echo ""

# 2. Device Registration
echo "═══ Device Registration ═══"
REGISTER_DATA='{
    "type": "register",
    "deviceId": "'$DEVICE_ID'",
    "capacity": {"zones": 4, "sensors": 12},
    "timestamp": "2025-11-25T12:00:00",
    "latitude": 35.6695,
    "longitude": -5.7857
}'

test_endpoint "POST" "/api/devices/register" "$REGISTER_DATA" "200" "Register device"
test_endpoint "GET" "/api/devices" "" "200" "List devices"
test_endpoint "GET" "/api/devices/$DEVICE_ID" "" "200" "Get device info"
test_endpoint "GET" "/api/devices/$DEVICE_ID/status" "" "200" "Get device status"
echo ""

# 3. Zone Management
echo "═══ Zone Management ═══"
ZONE1_DATA='{
    "zoneId": "zone_test_1",
    "waterPerDay": 2000,
    "irrigationTime": "08:00",
    "humidityThreshold": 25,
    "sensors": [{"sensorId": "s_01"}, {"sensorId": "s_02"}]
}'

ZONE2_DATA='{
    "zoneId": "zone_test_2",
    "waterPerDay": 3000,
    "irrigationTime": "18:00",
    "humidityThreshold": 30,
    "sensors": [{"sensorId": "s_03"}]
}'

test_endpoint "POST" "/api/devices/$DEVICE_ID/zones" "$ZONE1_DATA" "200" "Add zone 1"
test_endpoint "POST" "/api/devices/$DEVICE_ID/zones" "$ZONE2_DATA" "200" "Add zone 2"
test_endpoint "GET" "/api/devices/$DEVICE_ID/zones" "" "200" "List zones"
test_endpoint "GET" "/api/devices/$DEVICE_ID/zones/zone_test_1" "" "200" "Get specific zone"
test_endpoint "DELETE" "/api/devices/$DEVICE_ID/zones/zone_test_1" "" "200" "Delete zone"
echo ""

# 4. Configuration
echo "═══ Configuration Endpoints ═══"
CONFIG_DATA='{
    "type": "config",
    "zones": [],
    "commands": []
}'

test_endpoint "GET" "/api/devices/$DEVICE_ID/config" "" "200" "Get device config"
test_endpoint "PUT" "/api/devices/$DEVICE_ID/config" "$CONFIG_DATA" "200" "Update device config"
echo ""

# 5. Sensor Data
echo "═══ Sensor Data Endpoints ═══"
SENSOR_DATA='{
    "type": "data",
    "deviceId": "'$DEVICE_ID'",
    "timestamp": "2025-11-25T12:00:00",
    "globalData": {
        "temperature": 24.5,
        "humidity": 60.0,
        "pressure": 1012.0,
        "batteryLevel": 85.0,
        "signalStrength": -45
    },
    "zonesData": [
        {
            "zoneId": "zone_test_2",
            "soilMoisture": [{"sensorId": "s_03", "value": 45.5}]
        }
    ]
}'

test_endpoint "POST" "/api/devices/sensor-data" "$SENSOR_DATA" "200" "Receive sensor data"
test_endpoint "GET" "/api/sensor-data" "" "200" "Get sensor data log"
test_endpoint "GET" "/api/sensor-data/history" "" "200" "Get sensor history"
test_endpoint "GET" "/api/sensor-data/latest/$DEVICE_ID" "" "200" "Get latest sensor data"
echo ""

# 6. Cleanup
echo "═══ Cleanup ═══"
test_endpoint "DELETE" "/api/devices/$DEVICE_ID" "" "200" "Unregister device"
echo ""

# Summary
echo "=========================================="
echo "Test Results:"
echo -e "  ${GREEN}Passed: $TESTS_PASSED${NC}"
echo -e "  ${RED}Failed: $TESTS_FAILED${NC}"
echo "=========================================="

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "${GREEN}✅ All tests passed!${NC}"
    exit 0
else
    echo -e "${RED}❌ Some tests failed!${NC}"
    exit 1
fi
