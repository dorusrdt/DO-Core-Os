#!/usr/bin/env python3
"""
FastAPI Server for ESP32 Master-Slave Communication
Receives data from ESP32 master via HTTP POST requests
"""

from fastapi import FastAPI, Request, HTTPException
from datetime import datetime
import json
import uvicorn
from typing import Dict, Any

app = FastAPI(title="ESP32 Data Collector", version="1.0.0")

# Store received data
received_data = []

@app.post("/data")
async def receive_data(request: Request):
    """Receive data from ESP32 master"""
    try:
        data = await request.json()
        timestamp = datetime.now().isoformat()

        # Add server timestamp
        data_entry = {
            "server_timestamp": timestamp,
            "esp32_data": data
        }

        received_data.append(data_entry)

        # Keep only last 100 entries
        if len(received_data) > 100:
            received_data.pop(0)

        print(f"[{timestamp}] Received data from ESP32:")
        print(json.dumps(data_entry, indent=2))

        return {
            "status": "received",
            "timestamp": timestamp,
            "data_count": len(received_data)
        }

    except Exception as e:
        print(f"Error processing data: {e}")
        raise HTTPException(status_code=400, detail=f"Invalid data format: {str(e)}")

@app.get("/data")
async def get_data():
    """Get all received data"""
    return {
        "data": received_data,
        "count": len(received_data)
    }

@app.get("/health")
async def health_check():
    """Health check endpoint"""
    return {
        "status": "healthy",
        "service": "esp32_data_collector",
        "data_count": len(received_data),
        "timestamp": datetime.now().isoformat()
    }

@app.delete("/data")
async def clear_data():
    """Clear all stored data"""
    global received_data
    count = len(received_data)
    received_data = []
    return {
        "status": "cleared",
        "cleared_count": count
    }

if __name__ == "__main__":
    print("🚀 Starting ESP32 Data Collector Server...")
    print("📡 POST /data - Receive ESP32 data")
    print("📖 GET /data - Get stored data")
    print("💚 GET /health - Health check")
    print("🗑️  DELETE /data - Clear data")
    print("🌐 Server: http://0.0.0.0:8000")
    print("📝 Docs: http://0.0.0.0:8000/docs")

    uvicorn.run(
        "fastapi_server:app",
        host="0.0.0.0",
        port=8000,
        reload=True,
        log_level="info"
    )