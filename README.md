# IOT_ESPNOW — Sistema de Monitoramento de Pintura de Blocos de Madeira

Sistema IoT que acompanha o processo de pintura de blocos de madeira em um ambiente simulado de chão de fábrica. Dois ESP32 se comunicam via protocolo ESP-NOW: um coleta dados de sensores no chão de fábrica e o outro exibe as leituras em tempo real numa matriz de LEDs e encaminha os dados para um banco de dados InfluxDB, visualizados via Grafana.

---

## Estrutura do Repositório

```
IOT_ESPNOW/
├── chao_de_fabrica/
│   └── chao_de_fabrica.ino      # Firmware do ESP32 sensor/transmissor
├── monitoramento/
│   └── monitoramento.ino        # Firmware do ESP32 receptor/display (ESP32-S3)
├── backend/
│   └── ...                      # API Python + integração InfluxDB
└── README.md
```

---

## Arquitetura do Sistema

```
[Sensores]
   Ultrassônico (nível)
   DHT (temp/umidade)       -->  [ESP32 Chão de Fábrica]  --ESP-NOW-->  [ESP32 Monitoramento]  -->  [API Backend]
   Fotorresistor (luz)                                                     Matriz de LEDs             [InfluxDB]
   PIR (presença)                                                          LED status                 [Grafana]
```

### ESP32 Chão de Fábrica
- Lê os sensores a cada 2 segundos
- Monta um pacote JSON com os dados coletados
- Transmite via ESP-NOW ao ESP32 de monitoramento
- LED verde: sistema operando normalmente (nível de tinta >= 20%)
- LED vermelho: alerta (nível de tinta < 20% ou falha de envio)

### ESP32 de Monitoramento (ESP32-S3)
- Recebe pacotes via ESP-NOW
- Exibe leituras ciclicamente na matriz de LEDs (MD_Parola), alternando a cada 2 s:
  - `NVL XX%` — nível do tanque
  - `TMP XXC` — temperatura
  - `UMD XX%` — umidade
  - `LUX XXXX` — luminosidade
  - `PRS ON/OFF` — presença
- LED verde: dados recebidos nos últimos 5 s
- LED vermelho: timeout de comunicação (sem dados por mais de 5 s), exibe `SEM DADOS` na matriz
- Envia leituras periodicamente para a API backend via serial/rede

---

## Pacote de Dados (ESP-NOW)

Os dados são transmitidos em formato JSON:

```json
{
  "nivel": 75,
  "temperatura": 24.0,
  "umidade": 55.0,
  "luminosidade": 680,
  "presenca": 1,
  "timestamp": "2025-09-02T14:35:00Z"
}
```

---

## Hardware Necessário

| Componente | Quantidade | Observação |
|---|---|---|
| ESP32 (qualquer variante) | 1 | Chão de fábrica |
| ESP32-S3 | 1 | Monitoramento — requer definição explícita dos pinos SPI para o MD_Parola |
| Sensor ultrassônico (HC-SR04) | 1 | Nível do tanque |
| Sensor DHT11 ou DHT22 | 1 | Temperatura e umidade |
| Fotorresistor (LDR) | 1 | Luminosidade |
| Sensor PIR | 1 | Presença |
| Matriz de LEDs (MAX7219) | 1 | Display no monitoramento |
| LED verde | 2 | Um em cada ESP32 |
| LED vermelho | 2 | Um em cada ESP32 |
| Resistores, jumpers, protoboard | — | Conforme esquema do circuito |

> **Atenção ESP32-S3:** diferente de outros ESP32, o S3 não mapeia os pinos SPI automaticamente. Defina os pinos CLK, MOSI e CS explicitamente ao inicializar o MD_Parola.

---

## Dependências (Arduino IDE)

- `ESP32 Board Support` (via Boards Manager)
- `DHT sensor library` — Adafruit
- `MD_Parola` + `MD_MAX72XX` — MajicDesigns
- `ArduinoJson`

---

## Como Montar o Projeto

1. Instale as bibliotecas listadas acima na Arduino IDE.
2. Monte o circuito do **chão de fábrica** conforme o esquema do projeto, conectando os sensores e LEDs ao ESP32.
3. Monte o circuito de **monitoramento** conectando a matriz de LEDs e os LEDs de status ao ESP32-S3.
4. Abra `chao_de_fabrica/chao_de_fabrica.ino`, insira o endereço MAC do ESP32-S3 na variável de destino e faça o upload.
5. Abra `monitoramento/monitoramento.ino` e faça o upload para o ESP32-S3.
6. Abra o Monitor Serial (115200 baud) em ambos os dispositivos para acompanhar as leituras e o status da comunicação.

---

## Backend (API + InfluxDB)

### Pré-requisitos

- Python 3.x
- InfluxDB 2.x rodando localmente ou em nuvem
- Grafana

### Configuração do InfluxDB

1. Crie um bucket (ex.: `iot_fabrica`) e gere um token de acesso.
2. Anote a URL, organização, bucket e token — serão usados no backend.

### Rodando o Backend

```bash
cd backend
pip install -r requirements.txt
```

Configure as variáveis de ambiente (ou edite o arquivo de configuração):

```env
INFLUX_URL=http://localhost:8086
INFLUX_TOKEN=seu_token_aqui
INFLUX_ORG=sua_org
INFLUX_BUCKET=iot_fabrica
```

```bash
python app.py
```

A API ficará disponível para receber os dados enviados pelo ESP32 de monitoramento.

### Campos persistidos no InfluxDB

| Campo | Tipo | Descrição |
|---|---|---|
| `nivel` | float | Nível do tanque (%) |
| `temperatura` | float | Temperatura (°C) |
| `umidade` | float | Umidade relativa (%) |
| `luminosidade` | int | Valor analógico do LDR |
| `presenca` | int | 0 = ausente, 1 = detectada |
| `timestamp` | time | Timestamp da leitura |

---

## Grafana — Dashboards

1. Adicione o InfluxDB como fonte de dados no Grafana (Flux query language).
2. Crie um dashboard com os painéis:
   - Nível do tanque de tinta ao longo do tempo (gráfico de linha)
   - Temperatura e umidade (gráfico combinado)
   - Luminosidade (gráfico de linha)
   - Presença (stat ou gráfico de estado)
3. Configure atualização automática do dashboard (ex.: a cada 5 s).

---

## Como Consultar os Dados Salvos

Via InfluxDB UI (`http://localhost:8086`) usando Flux:

```flux
from(bucket: "iot_fabrica")
  |> range(start: -1h)
  |> filter(fn: (r) => r._measurement == "sensores")
```

Ou diretamente pelos dashboards do Grafana após a configuração.

---

## Branches

| Branch | Descrição |
|---|---|
| `main` | Código estável |
| `feat/robo-lab` | Branch de desenvolvimento da atividade |

---

## Equipe

Projeto desenvolvido como atividade da Camada de Serviço — ESP-NOW, InfluxDB e Grafana.
