#!/bin/bash

echo "=========================================="
echo "🚀 Starting Irrigation Test Server"
echo "=========================================="

# Vérifier si venv existe
if [ ! -d "venv" ]; then
    echo "❌ Virtual environment not found!"
    echo "Please run: ./setup.sh first"
    exit 1
fi

# Activer l'environnement virtuel
source venv/bin/activate

# Démarrer le serveur
echo "🔌 Starting server on http://192.168.1.3:3000"
echo "📚 API Documentation: http://192.168.1.3:3000/docs"
echo ""
echo "Press Ctrl+C to stop the server"
echo "=========================================="
echo ""

python server.py
