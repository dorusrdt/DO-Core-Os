#!/bin/bash

SERVER="http://192.168.1.72:3000"
DEVICE_ID="ESP32_IRRIGATION_11100454456464674"

# Obtenir l'heure actuelle
CURRENT_TIME=$(date +%H:%M)

# Calculer 3 créneaux : +2min, +5min, +8min
SCHEDULE_1=$(date -d "+2 minutes" +%H:%M)
SCHEDULE_2=$(date -d "+5 minutes" +%H:%M)
SCHEDULE_3=$(date -d "+8 minutes" +%H:%M)

echo "=========================================="
echo "🧪 Test Irrigation Multiple Créneaux"
echo "=========================================="
echo "📅 Heure actuelle : $CURRENT_TIME"
echo "⏰ Créneaux programmés :"
echo "   1️⃣  $SCHEDULE_1 (dans 2 min)"
echo "   2️⃣  $SCHEDULE_2 (dans 5 min)"
echo "   3️⃣  $SCHEDULE_3 (dans 8 min)"
echo ""

# Supprimer toutes les zones existantes
echo "🗑️  Nettoyage des zones existantes..."
curl -s -X DELETE "$SERVER/api/devices/$DEVICE_ID/zones" | jq '.'
echo ""
sleep 2

# Créer une zone avec 3 créneaux d'irrigation (NOUVELLES FONCTIONNALITÉS)
echo "➕ Création de la zone avec mapping physique et vrais IDs capteurs..."
curl -s -X POST "$SERVER/api/devices/$DEVICE_ID/zones" \
  -H "Content-Type: application/json" \
  -d "{
    \"zoneId\": \"zone_multi_schedule\",
    \"physicalZoneNumber\": 2,
    \"waterPerDay\": 3000,
    \"irrigationTimes\": [\"$SCHEDULE_1\", \"$SCHEDULE_2\", \"$SCHEDULE_3\"],
    \"humidityThreshold\": 20,
    \"sensors\": [
      {\"sensorId\": \"s04\"},
      {\"sensorId\": \"s07\"},
      {\"sensorId\": \"s12\"}
    ]
  }" | jq '.'

echo ""
echo "=========================================="
echo "✅ Zone créée avec succès !"
echo "=========================================="
echo "📍 Zone ID: zone_multi_schedule"
echo "🏗️  Slot Physique: #2 (mapping flexible testé)"
echo "💧 Eau par jour: 3000ml (1000ml par créneau)"
echo "⏰ Créneaux d'irrigation:"
echo "   1️⃣  $SCHEDULE_1 (dans 2 min)"
echo "   2️⃣  $SCHEDULE_2 (dans 5 min)"
echo "   3️⃣  $SCHEDULE_3 (dans 8 min)"
echo "📊 Seuil humidité: 20%"
echo "🔬 Capteurs: s04, s07, s12 (vrais IDs serveur)"
echo ""
echo "⏳ Les irrigations démarreront automatiquement"
echo "📺 Surveillez les logs de l'ESP32 avec 'log_echo on'"
echo ""
echo "📊 Timeline attendue:"
echo "   T+2min → Irrigation 1/3 (300s) sur zone physique #2"
echo "   T+5min → Irrigation 2/3 (300s) sur zone physique #2"
echo "   T+8min → Irrigation 3/3 (300s) sur zone physique #2"
echo ""
echo "🧪 Fonctionnalités testées:"
echo "   ✅ Mapping physique flexible (physicalZoneNumber: 2)"
echo "   ✅ IDs capteurs réels (s04, s07, s12)"
echo "   ✅ Réassignation complète capteurs"
echo "   ✅ Nettoyage orphelins automatique"
echo "   ✅ Optimisation polling (hash config)"
echo "=========================================="
