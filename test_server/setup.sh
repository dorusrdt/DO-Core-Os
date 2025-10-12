#!/bin/bash

echo "=========================================="
echo "🚀 Setup Irrigation Test Server"
echo "=========================================="

# Créer l'environnement virtuel
echo "📦 Creating virtual environment..."
python3 -m venv venv

# Activer l'environnement virtuel
echo "✅ Activating virtual environment..."
source venv/bin/activate

# Installer les dépendances
echo "📥 Installing dependencies..."
pip install --upgrade pip
pip install -r requirements.txt

echo ""
echo "=========================================="
echo "✅ Setup Complete!"
echo "=========================================="
echo ""
echo "To start the server:"
echo "  1. Activate venv: source venv/bin/activate"
echo "  2. Run server: python server.py"
echo ""
echo "Or use the start script: ./start.sh"
echo ""
echo "Server will be available at:"
echo "  - API: http://192.168.1.3:3000"
echo "  - Docs: http://192.168.1.3:3000/docs"
echo "  - Health: http://192.168.1.3:3000/api/health"
echo "=========================================="
