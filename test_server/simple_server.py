#!/usr/bin/env python3
from fastapi import FastAPI
from pydantic import BaseModel
from typing import List
import uvicorn
from datetime import datetime

app = FastAPI(title="Simple Sensor Server", version="1.0")

class SensorPacket(BaseModel):
    seq: int
    timestamp: int
    humidity: List[int]

received: List[SensorPacket] = []

@app.get("/")
async def root():
    return {"service": "Simple Sensor Server", "count": len(received), "ts": datetime.now().isoformat()}

@app.post("/api/data")
async def receive(packet: SensorPacket):
    received.append(packet)
    print(f"RX seq={packet.seq} h0={packet.humidity[0]} h11={packet.humidity[-1]} count={len(received)}")
    return {"status": "ok", "count": len(received)}

if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=8000)
