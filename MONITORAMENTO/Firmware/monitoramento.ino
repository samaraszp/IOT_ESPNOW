#include <WiFi.h>
#include <esp_now.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>

// Definições
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW 
#define MAX_DEVICES 4                     
#define DATA_PIN   20 // MOSI
#define CLK_PIN   18 // SCK
#define CS_PIN    5 // SS

#define led_verde 2  
#define led_vermelho 7

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
        Serial.println("LED VERDE ON – dados recebidos"); 
    }
    
    digitalWrite(led_verde, HIGH); 
    digitalWrite(led_vermelho, LOW); 
    
    // Log de Recepção Exato do Edital
    Serial.printf("RX: nivel=%.0f%% temp=%.0fC umd=%.0f%% lux=%d prs=%d ts=%s\n", 
                  dadosRecebidos.nivel_tinta, dadosRecebidos.temperatura, 
                  dadosRecebidos.umidade, dadosRecebidos.luminosidade, 
                  dadosRecebidos.presenca, dadosRecebidos.timestamp); 

    // String JSON via Serial para o Backend
    Serial.printf("JSON_DATA:{\"nivel\":%.1f,\"temp\":%.1f,\"umd\":%.1f,\"lux\":%d,\"prs\":%d,\"ts\":\"%s\"}\n",
                  dadosRecebidos.nivel_tinta, dadosRecebidos.temperatura, 
                  dadosRecebidos.umidade, dadosRecebidos.luminosidade, 
                  dadosRecebidos.presenca, dadosRecebidos.timestamp); 
}

void setup() {
    Serial.begin(115200);
    pinMode(led_verde, OUTPUT);
    pinMode(led_vermelho, OUTPUT);
    
    // Estado de Alerta Inicial
    digitalWrite(led_verde, LOW);
    digitalWrite(led_vermelho, HIGH); 

    P.begin();
    P.setIntensity(5); 
    P.displayClear();
    P.displayText("WAIT", PA_CENTER, 0, 0, PA_PRINT, PA_PRINT);

    WiFi.mode(WIFI_STA); 
    Serial.print("MAC_MONITORAMENTO:");
    Serial.println(WiFi.macAddress()); 

    if (esp_now_init() != ESP_OK) {
        digitalWrite(led_vermelho, HIGH);
        return;
    }
    esp_now_register_recv_cb(OnDataRecv); 
}

void loop() {
    unsigned long currentMillis = millis();
    P.displayAnimate();

    // Detecção de Timeout (5s sem dados)
    if (currentMillis - lastRxMillis > TIMEOUT_LIMIT) {
        // CORRIGIDO: Alterado de digitalWrite para digitalRead
        if (dadosDisponiveis || digitalRead(led_vermelho) == LOW) {
            digitalWrite(led_verde, LOW); 
            digitalWrite(led_vermelho, HIGH); 
            Serial.println("LED VERMELHO ON – timeout de comunicação"); 
            P.displayText("SEM DADOS", PA_CENTER, 0, 0, PA_PRINT, PA_PRINT); 
            dadosDisponiveis = false;
        }
    } 
    // Carrossel de Telas de 2s
    else if (currentMillis - lastDisplayMillis >= DISPLAY_INTERVAL && dadosDisponiveis) {
        lastDisplayMillis = currentMillis;
        
        switch (telaAtual) {
            case 0:
                Serial.println("Tela -> NVL"); 
                sprintf(bufferTexto, "NVL %.0f%%", dadosRecebidos.nivel_tinta); 
                P.displayText(bufferTexto, PA_CENTER, 0, 0, PA_PRINT, PA_PRINT);
                break;
            case 1:
                Serial.println("Tela -> TMP"); 
                sprintf(bufferTexto, "TMP %.0fC", dadosRecebidos.temperatura); 
                P.displayText(bufferTexto, PA_CENTER, 0, 0, PA_PRINT, PA_PRINT);
                break;
            case 2:
                Serial.println("Tela -> UMD"); 
                sprintf(bufferTexto, "UMD %.0f%%", dadosRecebidos.umidade); 
                P.displayText(bufferTexto, PA_CENTER, 0, 0, PA_PRINT, PA_PRINT);
                break;
            case 3:
                Serial.println("Tela -> LUX"); 
                sprintf(bufferTexto, "LUX %d", dadosRecebidos.luminosidade); 
                P.displayText(bufferTexto, PA_CENTER, 0, 0, PA_PRINT, PA_PRINT);
                break;
            case 4:
                Serial.println("Tela -> PRS"); 
                sprintf(bufferTexto, "PRS %s", (dadosRecebidos.presenca == 1) ? "ON" : "OFF"); 
                P.displayText(bufferTexto, PA_CENTER, 0, 0, PA_PRINT, PA_PRINT);
                break;
        }
        telaAtual = (telaAtual + 1) % 5;
    }
}
