#!/bin/bash

# Script de démarrage du serveur FastAPI pour ESP32
echo "🚀 Starting ESP32 FastAPI Data Collector Server..."
echo "📡 Server will listen on http://0.0.0.0:8000"
echo "📝 API Docs: http://0.0.0.0:8000/docs"
echo ""

# Vérifier si Python et pip sont installés
if ! command -v python3 &> /dev/null; then
    echo "❌ Python3 not found. Please install Python3."
    exit 1
fi

if ! command -v pip3 &> /dev/null; then
    echo "❌ pip3 not found. Please install pip3."
    exit 1
fi

# Installer les dépendances
echo "📦 Installing dependencies..."
pip3 install fastapi uvicorn[standard]

# Démarrer le serveur
echo "🌐 Starting server..."
python3 fastapi_server.py