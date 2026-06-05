#include <esp_now.h>
#include <WiFi.h>
#include <DHT.h>

#define PIN_TRIG      4
#define PIN_ECHO      2
#define PIN_DHT       15
#define PIN_LDR       34   
#define PIN_PIR       35   
#define PIN_LED_VERM  26
#define PIN_LED_VERD  27

#define DHTTYPE DHT22
DHT dht(PIN_DHT, DHTTYPE);

const float DIST_TANQUE_VAZIO = 10.0;  
const float DIST_TANQUE_CHEIO =  2.0;  
const float NIVEL_ALERTA      = 20.0;  

uint8_t macDestino[] = {0xFC, 0x01, 0x2C, 0xD0, 0x8C, 0x34};

// (O ESP32 de monitoramento tem q declarar exatmente IGUAL a struct
typedef struct struct_mensagem {
  float nivel_tinta;
  float temperatura;
  float umidade;
  int   luminosidade;
  int   presenca;
  char  timestamp[24];     
} DadosFabrica;

DadosFabrica meusDados;

bool envioOk = false;

void aoEnviar(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  envioOk = (status == ESP_NOW_SEND_SUCCESS);
  if (envioOk) {
    Serial.println("Pacote enviado com sucesso via ESP-NOW.");
  } else {
    Serial.println("Falha no envio ESP-NOW.");
  }
}

float lerNivelTanque() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  long duracao = pulseIn(PIN_ECHO, HIGH, 30000);
  if (duracao == 0) {
    Serial.println("[ULTRA] Timeout – sem eco. Assumindo tanque vazio.");
    return 0.0;
  }

  float distancia = (duracao * 0.0343f) / 2.0f;

  if (distancia > DIST_TANQUE_VAZIO) distancia = DIST_TANQUE_VAZIO;
  if (distancia < DIST_TANQUE_CHEIO) distancia = DIST_TANQUE_CHEIO;

  float porcentagem = ((DIST_TANQUE_VAZIO - distancia) /
                       (DIST_TANQUE_VAZIO - DIST_TANQUE_CHEIO)) * 100.0f;
  return constrain(porcentagem, 0.0f, 100.0f);
}

void iniciarESPNOW() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != ESP_OK) {
    Serial.println("[ESP-NOW] Falha na inicialização! Reiniciando...");
    delay(2000);
    ESP.restart();
  }

  esp_now_register_send_cb(aoEnviar);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, macDestino, 6);
  peer.channel = 0;      
  peer.encrypt = false;

  if (esp_now_add_peer(&peer) != ESP_OK) {
    Serial.println("[ESP-NOW] Falha ao adicionar peer!");
  } else {
    Serial.println("[ESP-NOW] Peer adicionado com sucesso.");
  }
}

void gerarTimestamp(char *buf, size_t len) {
  unsigned long s = millis() / 1000;
  unsigned long h = s / 3600;
  unsigned long m = (s % 3600) / 60;
  unsigned long seg = s % 60;
  snprintf(buf, len, "1970-01-01T%02lu:%02lu:%02luZ", h, m, seg);
}

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(PIN_TRIG,     OUTPUT);
  pinMode(PIN_ECHO,     INPUT);
  pinMode(PIN_PIR,      INPUT);
  pinMode(PIN_LDR,      INPUT);
  pinMode(PIN_LED_VERM, OUTPUT);
  pinMode(PIN_LED_VERD, OUTPUT);

  digitalWrite(PIN_LED_VERM, LOW);
  digitalWrite(PIN_LED_VERD, LOW);

  dht.begin();

  iniciarESPNOW();

  Serial.println("=== ESP32 Chão de Fábrica iniciado ===");
  Serial.print("MAC deste ESP32: ");
  Serial.println(WiFi.macAddress());
}

void loop() {
  Serial.println("\n=============================");

  meusDados.nivel_tinta  = lerNivelTanque();
  meusDados.temperatura  = dht.readTemperature();
  meusDados.umidade      = dht.readHumidity();
  meusDados.luminosidade = analogRead(PIN_LDR);
  meusDados.presenca     = digitalRead(PIN_PIR);
  gerarTimestamp(meusDados.timestamp, sizeof(meusDados.timestamp));
    
  if (isnan(meusDados.temperatura) || isnan(meusDados.umidade)) {
    Serial.println("[DHT] Erro de leitura! Usando 0.");
    meusDados.temperatura = 0.0f;
    meusDados.umidade     = 0.0f;
  }

  Serial.printf("Nível do tanque: %.1f%%\n",  meusDados.nivel_tinta);
  Serial.printf("Temperatura:     %.1f °C\n", meusDados.temperatura);
  Serial.printf("Umidade:         %.1f%%\n",  meusDados.umidade);
  Serial.printf("Luminosidade:    %d\n",       meusDados.luminosidade);
  Serial.println(meusDados.presenca ? "Presença detectada" : "Sem presença");

bool alertaCritico = (meusDados.nivel_tinta < NIVEL_ALERTA);
  
  if (alertaCritico) {
    Serial.println("Alerta! Nível de tinta baixo.");
    Serial.println("Estado: ALERTA – verificar tanque de tinta");
    digitalWrite(PIN_LED_VERM, HIGH);
    digitalWrite(PIN_LED_VERD, LOW);
  } else {
    Serial.println("Estado: Operação normal");
    digitalWrite(PIN_LED_VERM, LOW);
    digitalWrite(PIN_LED_VERD, HIGH);
  }

  esp_err_t resultado = esp_now_send(macDestino,
                                     (uint8_t *)&meusDados,
                                     sizeof(meusDados));

  if (resultado == ESP_OK) {
    Serial.printf("Pacote enviado: {nivel=%.1f%%, temp=%.1f°C, "
                  "umidade=%.1f%%, luz=%d, presenca=%d}\n",
            meusDados.nivel_tinta, meusDados.temperatura,
            meusDados.umidade, meusDados.luminosidade,
                  meusDados.presenca);
  } else {
    Serial.println("Falha no envio ESP-NOW (esp_now_send retornou erro).");
  }

  delay(20);
  if (envioOk) {
    digitalWrite(PIN_LED_VERD, LOW);
    delay(80);
    digitalWrite(PIN_LED_VERD, !alertaCritico ? HIGH : LOW);
  } else {
    digitalWrite(PIN_LED_VERM, LOW);
    delay(80);
    digitalWrite(PIN_LED_VERM, HIGH);
  }

  delay(1880); 
}
  delay(1880); 
}
