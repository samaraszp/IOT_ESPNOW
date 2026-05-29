import os
import json
import serial
import threading
from fastapi import FastAPI
from influxdb_client import InfluxDBClient, Point, WritePrecision
from influxdb_client.client.write_api import SYNCHRONOUS
from datetime import datetime
from dotenv import load_dotenv

# Carregar as variáveis do .env
load_dotenv()

app = FastAPI()

# Puxa os dados direto do arquivo oculto .env
INFLUX_URL = os.getenv("INFLUX_URL")
TOKEN = os.getenv("INFLUX_TOKEN")
ORG = os.getenv("INFLUX_ORG")
BUCKET = os.getenv("INFLUX_BUCKET")
SERIAL_PORT = os.getenv("SERIAL_PORT")

client = InfluxDBClient(url=INFLUX_URL, token=TOKEN, org=ORG)
write_api = client.write_api(write_precision=WritePrecision.S)

def ler_porta_serial():
    try:
        # Usa a porta do .env
        ser = serial.Serial(SERIAL_PORT, 115200, timeout=1) 
        while True:
            linha = ser.readline().decode('utf-8', errors='ignore').strip()
            if linha.startswith("JSON_DATA:"):
                json_raw = linha.replace("JSON_DATA:", "")
                dados = json.loads(json_raw)
                
                point = Point("medicoes_sensor") \
                    .field("nivel_tinta", float(dados["nivel"])) \
                    .field("temperatura", float(dados["temp"])) \
                    .field("umidade", float(dados["umd"])) \
                    .field("luminosidade", int(dados["lux"])) \
                    .field("presenca", int(dados["prs"])) \
                    .time(datetime.utcnow(), WritePrecision.S)
                
                write_api.write(bucket=BUCKET, org=ORG, record=point)
    except Exception as e:
        print(f"Erro serial: {e}")

@app.on_event("startup")
async def startup_event():
    threading.Thread(target=ler_porta_serial, daemon=True).start()

@app.get("/status")
def get_status():
    return {"status": "online"}