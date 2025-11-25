#!/bin/bash

# Quick Test Script - Validate All Changes
# Tests the complete flow before actual deployment

set -e  # Exit on error

SERVER="http://localhost:8000"
DEVICE_ID="ESP32_IRRIGATION_11100454456464674"

echo "╔════════════════════════════════════════════════════════════════════════════╗"
echo "║                    🧪 QUICK VALIDATION TEST                               ║"
echo "║              Vérifie tous les changements avant déploiement                ║"
echo "╚════════════════════════════════════════════════════════════════════════════╝"
echo ""

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Counter
TESTS_PASSED=0
TESTS_FAILED=0

test_case() {
    local name="$1"
    echo ""
    echo -e "${YELLOW}Testing: $name${NC}"
}

test_pass() {
    echo -e "${GREEN}✅ PASS${NC}"
    ((TESTS_PASSED++))
}

test_fail() {
    local reason="$1"
    echo -e "${RED}❌ FAIL: $reason${NC}"
    ((TESTS_FAILED++))
}

# ============================================================================
# 1. SYNTAX VALIDATION
# ============================================================================

test_case "Python Syntax"
if python3 -m py_compile irrigation_server.py 2>/dev/null; then
    test_pass
else
    test_fail "Python syntax error"
fi

test_case "Bash Syntax"
if bash -n create_test_zones.sh 2>/dev/null; then
    test_pass
else
    test_fail "Bash syntax error"
fi

# ============================================================================
# 2. SERVER HEALTH CHECK
# ============================================================================

test_case "Server Health Check"
RESPONSE=$(curl -s -o /dev/null -w "%{http_code}" $SERVER/health)
if [ "$RESPONSE" = "200" ]; then
    test_pass
else
    test_fail "Server not responding (HTTP $RESPONSE)"
    exit 1
fi

# ============================================================================
# 3. DEVICE REGISTRATION TEST
# ============================================================================

test_case "Device Registration"
REG_RESPONSE=$(curl -s -X POST "$SERVER/api/devices/register" \
  -H "Content-Type: application/json" \
  -d '{
    "type": "device",
    "deviceId": "'$DEVICE_ID'",
    "capacity": {"zones": 4, "sensors": 12},
    "timestamp": "2025-11-25T00:00:00",
    "latitude": 48.8566,
    "longitude": 2.3522
  }')

if echo "$REG_RESPONSE" | grep -q "registered"; then
    test_pass
else
    test_fail "Device registration failed"
    echo "Response: $REG_RESPONSE"
fi

# ============================================================================
# 4. ZONE CREATION TEST
# ============================================================================

echo ""
echo "─ Creating 4 zones..."

# Zone 1
test_case "Zone 1: Potager Nord"
Z1=$(curl -s -X POST "$SERVER/api/devices/$DEVICE_ID/zones" \
  -H "Content-Type: application/json" \
  -d '{
    "zoneId": "zone_potager_nord",
    "physicalZoneNumber": 1,
    "waterPerDay": 2000,
    "irrigationTime": "08:00",
    "humidityThreshold": 25,
    "sensors": [{"sensorId": "s_01"}, {"sensorId": "s_02"}, {"sensorId": "s_03"}],
    "irrigationSchedule": [
      {"time": "08:00", "duration": 15, "daysOfWeek": [1, 3, 5], "isActive": true},
      {"time": "18:00", "duration": 20, "daysOfWeek": [0, 2, 4, 6], "isActive": true}
    ]
  }')

if echo "$Z1" | grep -q "zone_potager_nord"; then
    test_pass
else
    test_fail "Zone 1 creation failed"
    echo "Response: $Z1"
fi

# Zone 2
test_case "Zone 2: Jardin Sud"
Z2=$(curl -s -X POST "$SERVER/api/devices/$DEVICE_ID/zones" \
  -H "Content-Type: application/json" \
  -d '{
    "zoneId": "zone_jardin_sud",
    "physicalZoneNumber": 2,
    "waterPerDay": 3000,
    "irrigationTime": "18:00",
    "humidityThreshold": 30,
    "sensors": [{"sensorId": "s_04"}, {"sensorId": "s_05"}, {"sensorId": "s_06"}],
    "irrigationSchedule": [
      {"time": "06:00", "duration": 20, "daysOfWeek": [1, 2, 3, 4, 5], "isActive": true},
      {"time": "20:00", "duration": 15, "daysOfWeek": [0, 6], "isActive": true}
    ]
  }')

if echo "$Z2" | grep -q "zone_jardin_sud"; then
    test_pass
else
    test_fail "Zone 2 creation failed"
fi

# Zone 3
test_case "Zone 3: Serre"
Z3=$(curl -s -X POST "$SERVER/api/devices/$DEVICE_ID/zones" \
  -H "Content-Type: application/json" \
  -d '{
    "zoneId": "zone_serre",
    "physicalZoneNumber": 3,
    "waterPerDay": 1500,
    "irrigationTime": "12:00",
    "humidityThreshold": 35,
    "sensors": [{"sensorId": "s_07"}, {"sensorId": "s_08"}, {"sensorId": "s_09"}],
    "irrigationSchedule": [
      {"time": "10:00", "duration": 10, "daysOfWeek": [1, 3, 5], "isActive": true}
    ]
  }')

if echo "$Z3" | grep -q "zone_serre"; then
    test_pass
else
    test_fail "Zone 3 creation failed"
fi

# Zone 4
test_case "Zone 4: Verger"
Z4=$(curl -s -X POST "$SERVER/api/devices/$DEVICE_ID/zones" \
  -H "Content-Type: application/json" \
  -d '{
    "zoneId": "zone_verger",
    "physicalZoneNumber": 4,
    "waterPerDay": 1000,
    "irrigationTime": "06:00",
    "humidityThreshold": 40,
    "sensors": [{"sensorId": "s_10"}, {"sensorId": "s_11"}, {"sensorId": "s_12"}],
    "irrigationSchedule": [
      {"time": "07:00", "duration": 25, "daysOfWeek": [0, 1, 2, 3, 4, 5, 6], "isActive": true}
    ]
  }')

if echo "$Z4" | grep -q "zone_verger"; then
    test_pass
else
    test_fail "Zone 4 creation failed"
fi

# ============================================================================
# 5. ZONE RETRIEVAL TEST
# ============================================================================

test_case "Retrieve All Zones"
ZONES=$(curl -s "$SERVER/api/devices/$DEVICE_ID/zones")
ZONE_COUNT=$(echo "$ZONES" | grep -o '"zoneId"' | wc -l)

if [ "$ZONE_COUNT" -eq 4 ]; then
    test_pass
    echo "   Found 4 zones"
else
    test_fail "Expected 4 zones, found $ZONE_COUNT"
fi

# ============================================================================
# 6. CONFIG RETRIEVAL TEST
# ============================================================================

test_case "Retrieve Device Config"
CONFIG=$(curl -s "$SERVER/api/devices/$DEVICE_ID/config")

if echo "$CONFIG" | grep -q '"type":"config"'; then
    test_pass
    echo "   Config structure valid"
else
    test_fail "Config structure invalid"
fi

# ============================================================================
# 7. PHYSICAL ZONE NUMBER VALIDATION
# ============================================================================

test_case "PhysicalZoneNumber Values"
if echo "$CONFIG" | grep -q '"physicalZoneNumber":1' && \
   echo "$CONFIG" | grep -q '"physicalZoneNumber":2' && \
   echo "$CONFIG" | grep -q '"physicalZoneNumber":3' && \
   echo "$CONFIG" | grep -q '"physicalZoneNumber":4'; then
    test_pass
    echo "   All physical zone numbers present (1-4)"
else
    test_fail "Missing physical zone numbers"
fi

# ============================================================================
# 8. IRRIGATION SCHEDULE VALIDATION
# ============================================================================

test_case "IrrigationSchedule Structure"
if echo "$CONFIG" | grep -q '"irrigationSchedule"' && \
   echo "$CONFIG" | grep -q '"daysOfWeek"' && \
   echo "$CONFIG" | grep -q '"duration"'; then
    test_pass
    echo "   Irrigation schedule structure valid"
else
    test_fail "Irrigation schedule structure missing"
fi

# ============================================================================
# 9. SENSOR COUNT TEST
# ============================================================================

test_case "Sensor Count (12 total)"
SENSOR_COUNT=$(echo "$CONFIG" | grep -o '"sensorId"' | wc -l)

if [ "$SENSOR_COUNT" -eq 12 ]; then
    test_pass
    echo "   All 12 sensors present"
else
    test_fail "Expected 12 sensors, found $SENSOR_COUNT"
fi

# ============================================================================
# 10. DEVICE INFO ENDPOINT
# ============================================================================

test_case "Device Info Endpoint"
DEVICE_INFO=$(curl -s "$SERVER/api/devices/$DEVICE_ID")

if echo "$DEVICE_INFO" | grep -q '"device"' && \
   echo "$DEVICE_INFO" | grep -q '"zones"'; then
    test_pass
else
    test_fail "Device info endpoint failed"
fi

# ============================================================================
# SUMMARY
# ============================================================================

echo ""
echo "╔════════════════════════════════════════════════════════════════════════════╗"
echo "║                         📊 TEST RESULTS                                    ║"
echo "╚════════════════════════════════════════════════════════════════════════════╝"
echo ""
echo -e "Tests Passed:  ${GREEN}$TESTS_PASSED${NC}"
echo -e "Tests Failed:  ${RED}$TESTS_FAILED${NC}"
echo ""

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "${GREEN}✅ ALL TESTS PASSED - READY FOR DEPLOYMENT${NC}"
    exit 0
else
    echo -e "${RED}❌ SOME TESTS FAILED - REVIEW ERRORS ABOVE${NC}"
    exit 1
fi
