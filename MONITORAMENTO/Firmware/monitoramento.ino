#include <WiFi.h>
#include <esp_now.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>

// ======================
// MATRIZ LED
// ======================

#define DATA_PIN 20
#define CLK_PIN 18
#define CS_PIN 5

// ======================
// LEDs DE STATUS
// ======================
#define led_verde 2
#define led_vermelho 7

// ======================
// MATRIZ LED
// ======================
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 1

#define CS_PIN 5

MD_Parola P = MD_Parola(
  HARDWARE_TYPE,
  CS_PIN,
  MAX_DEVICES
);

// ======================
// MAC AUTORIZADO
// ======================

uint8_t macAutorizado[] = {
  0x04, 0x83, 0x08,
  0xab, 0xcc, 0x1c
};

// ======================
// ESTRUTURA DE DADOS
// ======================
typedef struct struct_mensagem {
  float nivel_tinta;
  float temperatura;
  float umidade;
  int luminosidade;
  int presenca;
  char timestamp[24];
} struct_mensagem;

struct_mensagem dadosRecebidos;

// ======================
// CONTROLE
// ======================
unsigned long lastRxMillis = 0;
const unsigned long TIMEOUT_LIMIT = 5000;

int telaAtual = 0;
char bufferTexto[32];

bool dadosDisponiveis = false;
bool timeoutExibido = false;

// ======================
// MOCK DATA
// ======================
bool modoMock = false;

unsigned long lastMockMillis = 0;
const unsigned long MOCK_INTERVAL = 3000;

// ======================
// CALLBACK ESP-NOW
// ======================
void OnDataRecv(
  const esp_now_recv_info_t *info,
  const uint8_t *incomingData,
  int len)
{
  if (memcmp(info->src_addr, macAutorizado, 6) != 0)
  {
    Serial.println("Pacote ignorado - MAC nao autorizado");
    return;
  }

  if (len != sizeof(struct_mensagem))
  {
    Serial.println("Tamanho de pacote invalido");
    return;
  }

  memcpy(
    &dadosRecebidos,
    incomingData,
    sizeof(dadosRecebidos)
  );

  lastRxMillis = millis();
  dadosDisponiveis = true;
  timeoutExibido = false;

  digitalWrite(led_verde, HIGH);
  digitalWrite(led_vermelho, LOW);

  Serial.println("Dados recebidos via ESP-NOW");

  Serial.printf(
    "JSON_DATA:{\"nivel\":%.1f,\"temp\":%.1f,\"umd\":%.1f,\"lux\":%d,\"prs\":%d,\"ts\":\"%s\"}\n",
    dadosRecebidos.nivel_tinta,
    dadosRecebidos.temperatura,
    dadosRecebidos.umidade,
    dadosRecebidos.luminosidade,
    dadosRecebidos.presenca,
    dadosRecebidos.timestamp
  );
}

// ======================
// GERA DADOS FALSOS
// ======================
void gerarMockData()
{
  dadosRecebidos.nivel_tinta = random(20, 101);
  dadosRecebidos.temperatura = random(18, 41);
  dadosRecebidos.umidade = random(30, 91);
  dadosRecebidos.luminosidade = random(100, 1000);
  dadosRecebidos.presenca = random(0, 2);

  strcpy(
    dadosRecebidos.timestamp,
    "2026-06-03 14:30"
  );

  lastRxMillis = millis();
  dadosDisponiveis = true;

  digitalWrite(led_verde, HIGH);
  digitalWrite(led_vermelho, LOW);

  Serial.printf(
    "MOCK LOCAL -> nivel=%.0f%% temp=%.0fC umd=%.0f%% lux=%d prs=%d\n",
    dadosRecebidos.nivel_tinta,
    dadosRecebidos.temperatura,
    dadosRecebidos.umidade,
    dadosRecebidos.luminosidade,
    dadosRecebidos.presenca
  );

  Serial.printf(
    "JSON_DATA:{\"nivel\":%.1f,\"temp\":%.1f,\"umd\":%.1f,\"lux\":%d,\"prs\":%d,\"ts\":\"%s\"}\n",
    dadosRecebidos.nivel_tinta,
    dadosRecebidos.temperatura,
    dadosRecebidos.umidade,
    dadosRecebidos.luminosidade,
    dadosRecebidos.presenca,
    dadosRecebidos.timestamp
  );
}

// ======================
// SETUP
// ======================
void setup()
{
  Serial.begin(115200);

  randomSeed(micros());

  pinMode(led_verde, OUTPUT);
  pinMode(led_vermelho, OUTPUT);

  digitalWrite(led_verde, LOW);
  digitalWrite(led_vermelho, HIGH);

  P.begin();
  P.setIntensity(5);
  P.displayClear();

  P.displayText(
    "AGD",
    PA_LEFT,
    50,
    0,
    PA_SCROLL_LEFT,
    PA_SCROLL_LEFT
  );

  WiFi.mode(WIFI_STA);

  Serial.print("MAC_MONITORAMENTO:");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK)
  {
    Serial.println("Erro ao iniciar ESP-NOW");
    return;
  }

  esp_now_register_recv_cb(OnDataRecv);

  Serial.println("Sistema iniciado.");
  Serial.println("Modo Mock Desativado.");
}

// ======================
// LOOP
// ======================
void loop()
{
  unsigned long currentMillis = millis();

  bool animacaoTerminou = P.displayAnimate();

  // ======================
  // MOCK
  // ======================
  if (
    modoMock &&
    currentMillis - lastMockMillis >= MOCK_INTERVAL
  )
  {
    lastMockMillis = currentMillis;
    gerarMockData();
  }

 if (currentMillis - lastRxMillis > TIMEOUT_LIMIT)
{
  if (!timeoutExibido)
  {
    digitalWrite(led_verde, LOW);
    digitalWrite(led_vermelho, HIGH);

    Serial.println(
      "LED VERMELHO ON - timeout de comunicacao"
    );

    P.displayText(
      "NOD",
      PA_LEFT,
      50,
      0,
      PA_SCROLL_LEFT,
      PA_SCROLL_LEFT
    );

    dadosDisponiveis = false;
    timeoutExibido = true;
  }
}

  else if (dadosDisponiveis)
{
  if (animacaoTerminou)
  {
    switch (telaAtual)
    {
      case 0:
        sprintf(
          bufferTexto,
          "NVL%.0f",
          dadosRecebidos.nivel_tinta
        );
        break;

      case 1:
        sprintf(
          bufferTexto,
          "TMP%.0f",
          dadosRecebidos.temperatura
        );
        break;

      case 2:
        sprintf(
          bufferTexto,
          "UMD%.0f",
          dadosRecebidos.umidade
        );
        break;

      case 3:
        sprintf(
          bufferTexto,
          "LUX%d",
          dadosRecebidos.luminosidade
        );
        break;

      case 4:
        sprintf(
          bufferTexto,
          "PRS%s",
          dadosRecebidos.presenca ?
          "SIM" : "NAO"
        );
        break;
    }

    Serial.print("Tela -> ");
    Serial.println(bufferTexto);

    P.displayText(
      bufferTexto,
      PA_LEFT,
      50,
      0,
      PA_SCROLL_LEFT,
      PA_SCROLL_LEFT
    );

    telaAtual = (telaAtual + 1) % 5;
  }
}
}
