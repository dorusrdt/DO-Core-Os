#!/bin/bash
# Script de démarrage rapide du serveur d'irrigation

cd "$(dirname "$0")"

echo "🌊 Starting Irrigation Server..."
echo ""

# Activer le venv si disponible
if [ -d "venv" ]; then
    echo "📦 Activating virtual environment..."
    source venv/bin/activate
fi

# Vérifier les dépendances
echo "🔍 Checking dependencies..."
if ! python3 -c "import fastapi, uvicorn" 2>/dev/null; then
    echo "❌ Missing dependencies. Installing..."
    pip install -r requirements.txt
fi

echo ""
echo "🚀 Starting server on http://0.0.0.0:3000"
echo "📝 API Docs: http://0.0.0.0:3000/docs"
echo "📖 Press Ctrl+C to stop"
echo ""

# Démarrer le serveur
python3 irrigation_server.py

