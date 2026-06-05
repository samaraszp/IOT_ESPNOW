import os
import json
import time
import serial
from influxdb_client import InfluxDBClient, Point, WritePrecision
from datetime import datetime
from dotenv import load_dotenv

# Carrega as configurações do .env
load_dotenv()

INFLUX_URL = os.getenv("INFLUX_URL")
TOKEN = os.getenv("INFLUX_TOKEN")
ORG = os.getenv("INFLUX_ORG")
BUCKET = os.getenv("INFLUX_BUCKET")
SERIAL_PORT = os.getenv("SERIAL_PORT")

# Inicializa o cliente do InfluxDB
client = InfluxDBClient(url=INFLUX_URL, token=TOKEN, org=ORG)
write_api = client.write_api()

print("🚀 Coletor de dados do ESP32 iniciado...")

while True:
    try:
        # Abre a porta serial usando o gerenciador de contexto (with)
        with serial.Serial(SERIAL_PORT, 115200, timeout=1) as ser:
            print(f"🔌 Conectado com sucesso na porta {SERIAL_PORT}!")
            
            while True:
                linha = ser.readline().decode('utf-8', errors='ignore').strip()
                
                if not linha:
                    continue  # Aguarda novos dados
                
                if linha.startswith("JSON_DATA:"):
                    try:
                        json_raw = linha.replace("JSON_DATA:", "")
                        dados = json.loads(json_raw)
                        
                        # Monta o ponto de dados
                        point = Point("medicoes_sensor") \
                            .field("nivel_tinta", float(dados["nivel"])) \
                            .field("temperatura", float(dados["temp"])) \
                            .field("umidade", float(dados["umd"])) \
                            .field("luminosidade", int(dados["lux"])) \
                            .field("presenca", int(dados["prs"])) \
                            .time(datetime.utcnow(), WritePrecision.S)
                        
                        # Grava no banco
                        write_api.write(bucket=BUCKET, org=ORG, record=point)
                        print(f"✅ Dados enviados ao InfluxDB: {dados}")
                        
                    except json.JSONDecodeError:
                        print("⚠️ Falha ao decodificar o JSON da serial.")
                    except Exception as e:
                        print(f"⚠️ Erro ao salvar no InfluxDB: {e}")
                        
    except serial.SerialException:
        print(f"❌ Porta {SERIAL_PORT} não encontrada ou ocupada. Tentando novamente em 5 segundos...")
        time.sleep(5)
    except KeyboardInterrupt:
        print("\n👋 Coletor encerrado pelo usuário.")
        break
