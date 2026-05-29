#include <WiFi.h>
#include <esp_now.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>

// Definições
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW 
#define MAX_DEVICES 4                     
#define DATA_PIN  11 // MOSI
#define CLK_PIN   12 // SCK
#define CS_PIN    10 // SS

#define led_verde 4    
#define led_vermelho 5 

MD_Parola P = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

// MAC do Chão de Fábrica Autorizado
uint8_t macAutorizado[] = {0x7C, 0x12, 0xB3, 0x4F, 0xA2, 0x01}; 

// Estrutura de Dados
typedef struct struct_mensagem {
    float nivel_tinta;       
    float temperatura;       
    float umidade;           
    int luminosidade;        
    int presenca;            
    char timestamp[24];      
} struct_mensagem;

struct_mensagem dadosRecebidos;

unsigned long lastRxMillis = 0;
const unsigned long TIMEOUT_LIMIT = 5000;    
unsigned long lastDisplayMillis = 0;
const unsigned long DISPLAY_INTERVAL = 2000; 
int telaAtual = 0;
char bufferTexto[32];
bool dadosDisponiveis = false;

// Callback de Recepção ESPNOW
void OnDataRecv(const esp_now_recv_info_t *recvInfo, const uint8_t *incomingData, int len) {
    // Validação de MAC de Origem
    for (int i = 0; i < 6; i++) {
        if (recvInfo->src_addr[i] != macAutorizado[i]) return; 
    }

    if (len != sizeof(dadosRecebidos)) return;
    
    memcpy(&dadosRecebidos, incomingData, sizeof(dadosRecebidos));
    lastRxMillis = millis(); 
    
    if (!dadosDisponiveis) {
        dadosDisponiveis = true;
        Serial.println("LED VERDE ON – dados recebidos"); [cite: 173]
    }
    
    digitalWrite(led_verde, HIGH); [cite: 143, 191]
    digitalWrite(led_vermelho, LOW); [cite: 191]
    
    // Log de Recepção Exato do Edital
    Serial.printf("RX: nivel=%.0f%% temp=%.0fC umd=%.0f%% lux=%d prs=%d ts=%s\n", 
                  dadosRecebidos.nivel_tinta, dadosRecebidos.temperatura, 
                  dadosRecebidos.umidade, dadosRecebidos.luminosidade, 
                  dadosRecebidos.presenca, dadosRecebidos.timestamp); [cite: 195]

    // String JSON via Serial para o Backend
    Serial.printf("JSON_DATA:{\"nivel\":%.1f,\"temp\":%.1f,\"umd\":%.1f,\"lux\":%d,\"prs\":%d,\"ts\":\"%s\"}\n",
                  dadosRecebidos.nivel_tinta, dadosRecebidos.temperatura, 
                  dadosRecebidos.umidade, dadosRecebidos.luminosidade, 
                  dadosRecebidos.presenca, dadosRecebidos.timestamp); [cite: 217, 221]
}

void setup() {
    Serial.begin(115200);
    pinMode(led_verde, OUTPUT);
    pinMode(led_vermelho, OUTPUT);
    
    // Estado de Alerta Inicial
    digitalWrite(led_verde, LOW);
    digitalWrite(led_vermelho, HIGH); [cite: 144]

    P.begin();
    P.setIntensity(5); 
    P.displayClear();
    P.displayText("WAIT", PA_CENTER, 0, 0, PA_PRINT, PA_PRINT);

    WiFi.mode(WIFI_STA); [cite: 179]
    Serial.print("MAC_MONITORAMENTO:");
    Serial.println(WiFi.macAddress()); 

    if (esp_now_init() != ESP_OK) {
        digitalWrite(led_vermelho, HIGH);
        return;
    }
    esp_now_register_recv_cb(OnDataRecv); [cite: 180]
}

void loop() {
    unsigned long currentMillis = millis();
    P.displayAnimate();

    // Detecção de Timeout (5s sem dados)
    if (currentMillis - lastRxMillis > TIMEOUT_LIMIT) {
        if (dadosDisponiveis || digitalWrite(led_vermelho) == LOW) {
            digitalWrite(led_verde, LOW); [cite: 188]
            digitalWrite(led_vermelho, HIGH); [cite: 188]
            Serial.println("LED VERMELHO ON – timeout de comunicação"); [cite: 173]
            P.displayText("SEM DADOS", PA_CENTER, 0, 0, PA_PRINT, PA_PRINT); [cite: 189]
            dadosDisponiveis = false;
        }
    } 
    // Carrossel de Telas de 2s
    else if (currentMillis - lastDisplayMillis >= DISPLAY_INTERVAL && dadosDisponiveis) {
        lastDisplayMillis = currentMillis;
        
        switch (telaAtual) {
            case 0:
                Serial.println("Tela -> NVL"); [cite: 199]
                sprintf(bufferTexto, "NVL %.0f%%", dadosRecebidos.nivel_tinta); [cite: 163]
                P.displayText(bufferTexto, PA_CENTER, 0, 0, PA_PRINT, PA_PRINT);
                break;
            case 1:
                Serial.println("Tela -> TMP"); [cite: 199]
                sprintf(bufferTexto, "TMP %.0fC", dadosRecebidos.temperatura); [cite: 163]
                P.displayText(bufferTexto, PA_CENTER, 0, 0, PA_PRINT, PA_PRINT);
                break;
            case 2:
                Serial.println("Tela -> UMD"); [cite: 199]
                sprintf(bufferTexto, "UMD %.0f%%", dadosRecebidos.umidade); [cite: 163]
                P.displayText(bufferTexto, PA_CENTER, 0, 0, PA_PRINT, PA_PRINT);
                break;
            case 3:
                Serial.println("Tela -> LUX"); [cite: 199]
                sprintf(bufferTexto, "LUX %d", dadosRecebidos.luminosidade); [cite: 163]
                P.displayText(bufferTexto, PA_CENTER, 0, 0, PA_PRINT, PA_PRINT);
                break;
            case 4:
                Serial.println("Tela -> PRS"); [cite: 199]
                sprintf(bufferTexto, "PRS %s", (dadosRecebidos.presenca == 1) ? "ON" : "OFF"); [cite: 163]
                P.displayText(bufferTexto, PA_CENTER, 0, 0, PA_PRINT, PA_PRINT);
                break;
        }
        telaAtual = (telaAtual + 1) % 5;
    }
}