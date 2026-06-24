#include "Sensores.h"
#include "Config.h"
#include <DHT.h>
#include <BH1750.h>
#include <esp_task_wdt.h>
#include <Preferences.h> 

static DHT dht(DHTPIN, DHTTYPE);
static BH1750 lightMeter;

// ==========================================
// FUNÇÃO INTELIGENTE DE ESPERA (Substitui o delay)
// ==========================================
static void esperaNaoBloqueante(uint32_t ms) {
  uint32_t tempoInicio = millis();
  while (millis() - tempoInicio < ms) {
    esp_task_wdt_reset(); // Alimenta o cão de guarda
    yield();              // Libera a CPU para processos em paralelo do ESP32
  }
}

// ==========================================
// FUNÇÕES AUXILIARES DE LEITURA
// ==========================================
static float calcularTensaoBateriaCalibrada() {
  uint32_t somaMv = 0;
  for (int i = 0; i < AMOSTRAS_ADC; i++) {
    somaMv += analogReadMilliVolts(PIN_BATERIA);
    esperaNaoBloqueante(2); // Substituído o delay(2)
  }
  float mediaMv = somaMv / (float)AMOSTRAS_ADC;
  float tensaoRealBateria = (mediaMv * BATERIA_DIVISOR) / 1000.0;
  if (tensaoRealBateria < 0.0) tensaoRealBateria = 0.0;
  if (tensaoRealBateria > 4.25) tensaoRealBateria = 4.25;
  return tensaoRealBateria;
}

static int lerSoloFiltrado() {
  const int NUM_LEITURAS = 5;
  int leituras[NUM_LEITURAS];
  for (int i = 0; i < NUM_LEITURAS; i++) {
    leituras[i] = analogRead(PIN_SOLO);
    esperaNaoBloqueante(5); // Substituído o delay, reduzido para 5ms (ADC é rápido)
  }
  // Ordena (Bubble Sort)
  for (int i = 0; i < NUM_LEITURAS - 1; i++) {
    for (int j = 0; j < NUM_LEITURAS - i - 1; j++) {
      if (leituras[j] > leituras[j + 1]) {
        int temp = leituras[j];
        leituras[j] = leituras[j + 1];
        leituras[j + 1] = temp;
      }
    }
  }
  return leituras[NUM_LEITURAS / 2];
}

// ==========================================
// IMPLEMENTAÇÕES PRINCIPAIS
// ==========================================
void inicializarSensores(int &codigoErro) {
  Serial.println("[SENSORES] Inicializando...");
  dht.begin();
  pinMode(PIN_SOLO, INPUT);
  pinMode(PIN_BATERIA, INPUT);

  if (lightMeter.begin(BH1750::ONE_TIME_HIGH_RES_MODE)) {
    Serial.println("[SENSORES] GY-30 (BH1750) iniciado.");
  } else {
    Serial.println("[SENSORES] FALHA critica BH1750!");
    codigoErro |= ERR_BH1750; 
  }
}

void lerSensores(float &temp, float &umid, int &soloPct, int &lux, float &bateriaV, int &codigoErro) {
  // 1. DISPARA O SENSOR I2C PRIMEIRO (Deixa ele trabalhando fisicamente em paralelo)
  lightMeter.configure(BH1750::ONE_TIME_HIGH_RES_MODE);
  uint32_t tempoLuzAcionada = millis();

  // 2. ENQUANTO ISSO, A CPU FAZ OUTRAS COISAS RÁPIDAS
  Preferences storeSensores;
  storeSensores.begin("calibracao", true); 
  int soloSeco = storeSensores.getInt("soloSeco", SOLO_SECO_DEFAULT);
  int soloUmido = storeSensores.getInt("soloUmido", SOLO_UMIDO_DEFAULT);
  storeSensores.end();

  int soloRaw = lerSoloFiltrado(); 
  soloPct = constrain(map(soloRaw, soloSeco, soloUmido, 0, 100), 0, 100);

  analogSetPinAttenuation(PIN_BATERIA, ADC_11db);
  analogReadResolution(12);
  bateriaV = calcularTensaoBateriaCalibrada();

  // 3. TENTA LER O DHT11
  const int MAX_DHT_RETRIES = 3;
  for (int i = 0; i < MAX_DHT_RETRIES; i++) {
    temp = dht.readTemperature();
    umid = dht.readHumidity();
    if (!isnan(temp) && !isnan(umid)) break;
    
    if (i < MAX_DHT_RETRIES - 1) {
      esperaNaoBloqueante(400); // Tenta de novo sem bloquear o chip
    }
  }

  if (isnan(temp) || isnan(umid)) {
    Serial.println("[SENSORES] Falha DHT11.");
    temp = -999.0; umid = -999.0;
    codigoErro |= ERR_DHT; 
  }

  // 4. FINALMENTE: COLETA A LUZ
  // Como fizemos muitas leituras acima, é provável que os 200ms já tenham passado!
  // Se fomos mais rápidos que o chip, a função abaixo espera apenas a diferença.
  while ((millis() - tempoLuzAcionada) < 200) {
    yield(); 
  }
  lux = (int)lightMeter.readLightLevel();

  Serial.printf("[DADOS LIDOS] Temp: %.1f C | Umid: %.1f %% | Solo: %d %% | Luz: %d lx | Bat: %.2f V\n", temp, umid, soloPct, lux, bateriaV);
}
