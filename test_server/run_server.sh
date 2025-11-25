#!/bin/bash
# Quick Start Guide for Irrigation Server v2.0

echo "🌱 Irrigation Server v2.0 - Quick Start"
echo "========================================"
echo ""

# Check if Python3 is available
if ! command -v python3 &> /dev/null; then
    echo "❌ Python3 is not installed"
    exit 1
fi

echo "📍 Current Directory: $(pwd)"
echo ""

# Install dependencies
echo "1️⃣ Installing dependencies..."
pip install fastapi uvicorn pydantic python-multipart -q
echo "   ✅ Dependencies installed"
echo ""

# Run the server
echo "2️⃣ Starting the server..."
echo "   🌐 URL: http://localhost:8000"
echo "   📝 Docs: http://localhost:8000/docs"
echo "   🔍 ReDoc: http://localhost:8000/redoc"
echo ""
echo "3️⃣ To register a device, use:"
echo '   curl -X POST http://localhost:8000/api/devices/register \'
echo '     -H "Content-Type: application/json" \'
echo '     -d '"'"'{
echo '       "type": "register",
echo '       "deviceId": "ESP32_IRRIGATION_12345",
echo '       "capacity": {"zones": 4, "sensors": 12},
echo '       "timestamp": "2025-11-25T12:00:00",
echo '       "latitude": 35.6695,
echo '       "longitude": -5.7857
echo '     }'"'"
echo ""
echo "4️⃣ To create test zones, run:"
echo "   ./create_test_zones.sh"
echo ""
echo "5️⃣ To run all tests, use:"
echo "   ./test_server.sh"
echo ""
echo "📚 For full documentation:"
echo "   - README_V2.md - Complete guide"
echo "   - ANALYSIS_UPDATE_V2.md - Technical analysis"
echo "   - IMPLEMENTATION_SUMMARY.md - Summary of changes"
echo ""
echo "Starting server..."
echo "========================================\n"

python3 irrigation_server.py
