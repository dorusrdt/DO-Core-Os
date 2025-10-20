#!/bin/bash

SERVER="http://192.168.1.3:3000"
DEVICE_ID="ESP32_IRRIGATION_11100454456464674"

# Paramètres par défaut
ZONE_ID="${1:-zone_test_auto}"
DURATION="${2:-60}"

echo "=========================================="
echo "💧 Déclenchement Manuel d'Irrigation"
echo "=========================================="
echo "📍 Zone: $ZONE_ID"
echo "⏱️  Durée: ${DURATION}s"
echo ""

# Déclencher l'irrigation
echo "🚀 Envoi de la commande..."
curl -s -X POST "$SERVER/api/devices/$DEVICE_ID/commands/irrigate?zone_id=$ZONE_ID&duration=$DURATION" \
  -H "Content-Type: application/json" | jq '.'

echo ""
echo "=========================================="
echo "✅ Commande envoyée !"
echo "=========================================="
echo "📺 Surveillez les logs de l'ESP32"
echo "   L'irrigation devrait démarrer dans max 10s"
echo "   (délai = intervalle de poll du Master)"
echo ""
echo "Usage: $0 [zone_id] [duration_seconds]"
echo "Exemple: $0 zone_potager_nord 120"
echo "=========================================="
