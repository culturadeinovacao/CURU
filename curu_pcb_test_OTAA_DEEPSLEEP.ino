#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <esp_task_wdt.h>

// Inclusão dos nossos módulos
#include "Config.h"
#include "Display.h" 
#include "Sensores.h"
#include "Comunicacao.h" 

// =======================================================
// TEMPOS DE HIBERNAÇÃO (Testes de Campo)
// =======================================================

#define TEMPO_SLEEP_NORMAL 30ULL * 60ULL * 1000000ULL //30 min
#define TEMPO_SLEEP_RETRY  60ULL * 1000000ULL  

// A variável sobrevive ao Deep Sleep e acumula o histórico na memória RTC
RTC_DATA_ATTR int codigoErroGlobal = 0; 

void setup() {
  Serial.begin(115200);
  delay(1000); 

  Serial.println("\n=======================================================");
  Serial.println("=== Node S3: MODO DEEP SLEEP (30 MINUTOS) ===");
  Serial.println("=======================================================");

  // Configuração defensiva do Watchdog
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = 60000,
    .idle_core_mask = 0,
    .trigger_panic = true
  };
  if (esp_task_wdt_reconfigure(&wdt_config) != ESP_OK) {
    esp_task_wdt_init(&wdt_config);
  }
  esp_task_wdt_add(NULL); 

  // Inicia Barramento de Tela e exibe a arte inicial
  Wire.begin(I2C_SDA, I2C_SCL);
  inicializarDisplays(codigoErroGlobal);

  // 1. INICIALIZA APENAS O RÁDIO PRIMEIRO (Isso pode demorar de 2 a 10s dependendo da rede)
  inicializarLoRa(codigoErroGlobal); 

  // CHECKPOINT 1: Se o LoRa demorou quase todo o tempo máximo, aborta os sensores!
  if (millis() > TEMPO_MAXIMO_ACORDADO_MS) {
    Serial.println("\n[PANICO] Tempo limite de execução estourado! Abortando sensores.");
    codigoErroGlobal |= ERR_TIMEOUT;
    Serial.println("[SISTEMA] Configurando alarme para 10 MINUTOS (Retry).");
    esp_sleep_enable_timer_wakeup(TEMPO_SLEEP_RETRY);
  }
  // 2. VERIFICA SE TEMOS REDE E SE TEMOS TEMPO DE SOBRA
  else if ((codigoErroGlobal & ERR_JOIN) == 0) {
    
    Serial.println("\n--- [REDE OK] Ligando sensores e realizando leituras ---");
    inicializarSensores(codigoErroGlobal); 
    
    float temperatura, umidade, tensaoBateria; 
    int umidadeSolo, luminosidade;
    
    // Lê e Transmite (Totalmente não-bloqueante agora)
    lerSensores(temperatura, umidade, umidadeSolo, luminosidade, tensaoBateria, codigoErroGlobal);
    transmitirDadosLoRa(temperatura, umidade, umidadeSolo, luminosidade, tensaoBateria, codigoErroGlobal);

    Serial.println("\n[SISTEMA] Missão Cumprida. Configurando alarme para 1 HORA.");
    esp_sleep_enable_timer_wakeup(TEMPO_SLEEP_NORMAL);

  } else {
    // 3. SE NÃO TIVER REDE, ABORTA TUDO
    Serial.println("\n--- [REDE OFFLINE] Abortando leitura para poupar bateria ---");
    Serial.println("[SISTEMA] Configurando alarme para 10 MINUTOS.");
    esp_sleep_enable_timer_wakeup(TEMPO_SLEEP_RETRY);
  }

  // =======================================================
  // ROTINA DE HIBERNAÇÃO UNIVERSAL
  // =======================================================
  Serial.println("\n[SISTEMA] Desligando perifericos...");
  
  // Corta energia física e lógica
  desligarDisplays(); 
  dormirLoRa();
  Wire.end();
  SPI.end();
  
  Serial.println("[SISTEMA] Entrando em Deep Sleep. Zzz...");
  Serial.flush(); // Garante que a porta Serial descarregue o log antes de cortar a CPU

  // Hibernação da placa
  esp_deep_sleep_start();
}

void loop() {
  // Ignorado pelo ESP32 em Deep Sleep
}
