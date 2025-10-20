#!/bin/bash

SERVER="http://192.168.1.3:3000"
DEVICE_ID="ESP32_IRRIGATION_11100454456464674"

# Obtenir l'heure actuelle + 2 minutes pour le test
CURRENT_TIME=$(date +%H:%M)
TEST_TIME=$(date -d "+2 minutes" +%H:%M)

echo "=========================================="
echo "🧪 Test Irrigation Programmée"
echo "=========================================="
echo "📅 Heure actuelle : $CURRENT_TIME"
echo "⏰ Irrigation programmée à : $TEST_TIME"
echo ""

# Supprimer toutes les zones existantes
echo "🗑️  Nettoyage des zones existantes..."
curl -s -X DELETE "$SERVER/api/devices/$DEVICE_ID/zones" | jq '.'
echo ""
sleep 2

# Créer une zone de test avec irrigation dans 2 minutes
echo "➕ Création de la zone de test..."
curl -s -X POST "$SERVER/api/devices/$DEVICE_ID/zones" \
  -H "Content-Type: application/json" \
  -d "{
    \"zoneId\": \"zone_test_auto\",
    \"waterPerDay\": 1500,
    \"irrigationTime\": \"$TEST_TIME\",
    \"humidityThreshold\": 20,
    \"sensors\": [
      {\"sensorId\": \"s_01\"},
      {\"sensorId\": \"s_02\"},
      {\"sensorId\": \"s_03\"}
    ]
  }" | jq '.'

echo ""
echo "=========================================="
echo "✅ Zone créée avec succès !"
echo "=========================================="
echo "📍 Zone ID: zone_test_auto"
echo "💧 Eau par jour: 1500ml"
echo "⏰ Heure d'irrigation: $TEST_TIME"
echo "📊 Seuil humidité: 20%"
echo "🔬 Capteurs: s_01, s_02, s_03"
echo ""
echo "⏳ L'irrigation démarrera automatiquement à $TEST_TIME"
echo "📺 Surveillez les logs de l'ESP32 avec 'log_echo on'"
echo "=========================================="
