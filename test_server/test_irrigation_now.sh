#!/bin/bash

SERVER="http://192.168.1.3:3000"
DEVICE_ID="ESP32_IRRIGATION_11100454456464674"

# Obtenir l'heure actuelle pour déclencher immédiatement
CURRENT_TIME=$(date +%H:%M)

echo "=========================================="
echo "🚀 Test Irrigation IMMÉDIATE"
echo "=========================================="
echo "⏰ Heure actuelle : $CURRENT_TIME"
echo ""

# Supprimer toutes les zones existantes
echo "🗑️  Nettoyage des zones existantes..."
curl -s -X DELETE "$SERVER/api/devices/$DEVICE_ID/zones" | jq '.'
echo ""
sleep 2

# Créer une zone avec l'heure actuelle (déclenchement immédiat)
echo "➕ Création de la zone avec irrigation MAINTENANT..."
curl -s -X POST "$SERVER/api/devices/$DEVICE_ID/zones" \
  -H "Content-Type: application/json" \
  -d "{
    \"zoneId\": \"zone_test_immediate\",
    \"waterPerDay\": 1000,
    \"irrigationTime\": \"$CURRENT_TIME\",
    \"humidityThreshold\": 20,
    \"sensors\": [
      {\"sensorId\": \"s_01\"},
      {\"sensorId\": \"s_02\"}
    ]
  }" | jq '.'

echo ""
echo "=========================================="
echo "✅ Zone créée - Irrigation devrait démarrer MAINTENANT !"
echo "=========================================="
echo "📍 Zone ID: zone_test_immediate"
echo "💧 Eau par jour: 1000ml"
echo "⏰ Heure d'irrigation: $CURRENT_TIME (MAINTENANT)"
echo "📊 Seuil humidité: 20%"
echo "🔬 Capteurs: s_01, s_02"
echo ""
echo "⚡ L'irrigation devrait démarrer dans les 10 prochaines secondes"
echo "   (délai = intervalle de poll du Master)"
echo "📺 Surveillez les logs de l'ESP32 avec 'log_echo on'"
echo "=========================================="
