#!/bin/bash
# Script to send different zone configurations to the irrigation server
# Supports multiple configuration scenarios for testing

# Server configuration
SERVER_URL="http://192.168.1.72:8000"
DEVICE_ID="ESP32_IRRIGATION_11100454456464674"

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# ============================================================================
# Configuration Templates
# ============================================================================

create_config_1_zone() {
    cat <<EOF
{
  "type": "config",
  "zones": [
    {
      "zoneId": "zone_001",
      "physicalZoneNumber": 1,
      "waterPerDay": 5000,
      "irrigationTime": "08:00",
      "humidityThreshold": 12,
      "sensors": [
        {"sensorId": "s01"}
      ]
    }
  ]
}
EOF
}

create_config_2_zones() {
    cat <<EOF
{
  "type": "config",
  "zones": [
    {
      "zoneId": "zone_001",
      "physicalZoneNumber": 1,
      "waterPerDay": 5000,
      "irrigationTime": "08:00",
      "humidityThreshold": 40,
      "sensors": [
        {"sensorId": "s02"},
        ]
    },
    {
      "zoneId": "zone_002",
      "physicalZoneNumber": 2,
      "waterPerDay": 3000,
      "irrigationTime": "09:00",
      "humidityThreshold": 45,
      "sensors": [
        {"sensorId": "s03"}
      ]
    }
  ]
}
EOF
}

create_config_4_zones_full() {
    cat <<EOF
{
  "type": "config",
  "zones": [
    {
      "zoneId": "zone_001",
      "physicalZoneNumber": 1,
      "waterPerDay": 5000,
      "irrigationTime": "08:00",
      "humidityThreshold": 40,
      "sensors": [
       {"sensorId": "s04"}
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
EOF
}

create_config_emergency_low_threshold() {
    cat <<EOF
{
  "type": "config",
  "zones": [
    {
      "zoneId": "zone_001",
      "physicalZoneNumber": 1,
      "waterPerDay": 5000,
      "irrigationTime": "08:00",
      "humidityThreshold": 20,
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
      "humidityThreshold": 25,
      "sensors": [
        {"sensorId": "s04"},
        {"sensorId": "s05"},
        {"sensorId": "s06"}
      ]
    }
  ]
}
EOF
}

create_config_multiple_sensors_per_zone() {
    cat <<EOF
{
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
EOF
}

create_config_update_existing() {
    cat <<EOF
{
  "type": "config",
  "zones": [
    {
      "zoneId": "zone_001",
      "physicalZoneNumber": 1,
      "waterPerDay": 8000,
      "irrigationTime": "07:00",
      "humidityThreshold": 35,
      "sensors": [
        {"sensorId": "s01"},
        {"sensorId": "s02"},
        {"sensorId": "s03"},
        {"sensorId": "s04"}
      ]
    }
  ]
}
EOF
}

create_config_empty() {
    cat <<EOF
{
  "type": "config",
  "zones": []
}
EOF
}

# ============================================================================
# Helper Functions
# ============================================================================

send_config() {
    local config_json="$1"
    local url="${SERVER_URL}/api/devices/${DEVICE_ID}/config"

    echo "📤 Sending configuration..."
    echo ""

    response=$(curl -s -w "\n%{http_code}" -X PUT \
        -H "Content-Type: application/json" \
        -d "$config_json" \
        "$url" 2>&1)

    http_code=$(echo "$response" | tail -n1)
    body=$(echo "$response" | sed '$d')

    if [ "$http_code" = "200" ]; then
        echo -e "${GREEN}✅ Configuration sent successfully${NC}"
        if command -v jq &> /dev/null; then
            echo "$body" | jq -r '.configHash' | sed 's/^/   Config hash: /'
        else
            echo "$body"
        fi
        return 0
    else
        echo -e "${RED}❌ Error: HTTP $http_code${NC}"
        echo "$body"
        return 1
    fi
}

delete_zone() {
    local zone_id="$1"
    local url="${SERVER_URL}/api/devices/${DEVICE_ID}/zones/${zone_id}"

    echo "🗑️  Deleting zone: $zone_id"
    echo ""

    response=$(curl -s -w "\n%{http_code}" -X DELETE "$url" 2>&1)

    http_code=$(echo "$response" | tail -n1)
    body=$(echo "$response" | sed '$d')

    if [ "$http_code" = "200" ]; then
        echo -e "${GREEN}✅ Zone $zone_id deleted successfully${NC}"
        return 0
    else
        echo -e "${RED}❌ Error: HTTP $http_code${NC}"
        echo "$body"
        return 1
    fi
}

get_device_status() {
    local url="${SERVER_URL}/api/devices/${DEVICE_ID}/status"

    echo "📊 Device Status:"
    echo ""

    response=$(curl -s "$url" 2>&1)

    if [ $? -eq 0 ]; then
        if command -v jq &> /dev/null; then
            echo "$response" | jq '.'
        else
            echo "$response"
        fi
    else
        echo -e "${RED}❌ Error fetching status${NC}"
        return 1
    fi
}

show_help() {
    echo "Usage: $0 <config_type> [options]"
    echo ""
    echo "Available configurations:"
    echo "  1-zone              - Single zone with 3 sensors"
    echo "  2-zones             - Two zones with 6 sensors"
    echo "  4-zones             - Full 4 zones with 12 sensors"
    echo "  emergency           - Low thresholds (triggers emergency)"
    echo "  multiple-sensors    - Multiple sensors per zone"
    echo "  update              - Update existing zone config"
    echo "  empty               - Empty configuration (no zones)"
    echo ""
    echo "Zone management:"
    echo "  delete <zone_id>    - Delete a zone"
    echo "  status              - Show device status"
    echo ""
    echo "Examples:"
    echo "  $0 4-zones"
    echo "  $0 delete zone_001"
    echo "  $0 status"
}

# ============================================================================
# Main
# ============================================================================

main() {
    if [ $# -lt 1 ]; then
        show_help
        exit 1
    fi

    command="$1"

    echo "============================================================================"
    echo -e "${YELLOW}📤 Sending Configuration: $command${NC}"
    echo "🌐 Server: $SERVER_URL"
    echo "📱 Device: $DEVICE_ID"
    echo "============================================================================"
    echo ""

    case "$command" in
        "delete")
            if [ $# -lt 2 ]; then
                echo -e "${RED}❌ Error: Zone ID required${NC}"
                echo "Usage: $0 delete <zone_id>"
                exit 1
            fi
            zone_id="$2"
            delete_zone "$zone_id"
            exit $?
            ;;

        "status")
            get_device_status
            exit $?
            ;;

        "1-zone")
            config=$(create_config_1_zone)
            echo "📋 Configuration:"
            if command -v jq &> /dev/null; then
                echo "$config" | jq '.'
            else
                echo "$config"
            fi
            echo ""
            send_config "$config"
            exit $?
            ;;

        "2-zones")
            config=$(create_config_2_zones)
            echo "📋 Configuration:"
            if command -v jq &> /dev/null; then
                echo "$config" | jq '.'
            else
                echo "$config"
            fi
            echo ""
            send_config "$config"
            exit $?
            ;;

        "4-zones")
            config=$(create_config_4_zones_full)
            echo "📋 Configuration:"
            if command -v jq &> /dev/null; then
                echo "$config" | jq '.'
            else
                echo "$config"
            fi
            echo ""
            send_config "$config"
            exit $?
            ;;

        "emergency")
            config=$(create_config_emergency_low_threshold)
            echo "📋 Configuration:"
            if command -v jq &> /dev/null; then
                echo "$config" | jq '.'
            else
                echo "$config"
            fi
            echo ""
            send_config "$config"
            exit $?
            ;;

        "multiple-sensors")
            config=$(create_config_multiple_sensors_per_zone)
            echo "📋 Configuration:"
            if command -v jq &> /dev/null; then
                echo "$config" | jq '.'
            else
                echo "$config"
            fi
            echo ""
            send_config "$config"
            exit $?
            ;;

        "update")
            config=$(create_config_update_existing)
            echo "📋 Configuration:"
            if command -v jq &> /dev/null; then
                echo "$config" | jq '.'
            else
                echo "$config"
            fi
            echo ""
            send_config "$config"
            exit $?
            ;;

        "empty")
            config=$(create_config_empty)
            echo "📋 Configuration:"
            if command -v jq &> /dev/null; then
                echo "$config" | jq '.'
            else
                echo "$config"
            fi
            echo ""
            send_config "$config"
            exit $?
            ;;

        *)
            echo -e "${RED}❌ Unknown configuration type: $command${NC}"
            echo ""
            show_help
            exit 1
            ;;
    esac
}

# Run main function
main "$@"

