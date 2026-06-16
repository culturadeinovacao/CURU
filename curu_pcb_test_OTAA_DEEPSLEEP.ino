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

  // 1. INICIALIZA O RÁDIO PRIMEIRO (Trata o JOIN e restauração NVS)
  inicializarLoRa(codigoErroGlobal); 

  // 2. VERIFICA SE TEMOS REDE (Se a flag de falha de JOIN NÃO foi ativada)
  if ((codigoErroGlobal & ERR_JOIN) == 0) {
    
    Serial.println("\n--- [REDE OK] Ligando sensores e realizando leituras ---");
    inicializarSensores(codigoErroGlobal); 
    
    float temperatura, umidade, tensaoBateria; 
    int umidadeSolo, luminosidade;
    
    lerSensores(temperatura, umidade, umidadeSolo, luminosidade, tensaoBateria, codigoErroGlobal);
    transmitirDadosLoRa(temperatura, umidade, umidadeSolo, luminosidade, tensaoBateria, codigoErroGlobal);

    Serial.println("\n[SISTEMA] Missão Cumprida. Configurando alarme para 5 MINUTOS.");
    esp_sleep_enable_timer_wakeup(TEMPO_SLEEP_NORMAL);
    
    // Zera os erros apenas se a rodada de comunicação e leitura foi um sucesso,
    // preparando a placa limpa para o próximo ciclo
    codigoErroGlobal = ERR_NONE; 

  } else {
    // 3. SE NÃO TIVER REDE, ABORTA LEITURAS PARA POUPAR BATERIA
    Serial.println("\n--- [REDE OFFLINE] Falha no JOIN. Abortando leitura dos sensores ---");
    Serial.println("[SISTEMA] Configurando alarme rápido de retentativa para 1 MINUTO.");
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