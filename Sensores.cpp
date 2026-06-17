#include "Sensores.h"
#include "Config.h"
#include <DHT.h>
#include <BH1750.h>
#include <esp_task_wdt.h>
#include <Preferences.h> // MUDANÇA: Adicionado para calibração dinâmica

static DHT dht(DHTPIN, DHTTYPE);
static BH1750 lightMeter;

void inicializarSensores(int &codigoErro) {
  Serial.println("[SENSORES] Inicializando...");
  dht.begin();
  pinMode(PIN_SOLO, INPUT);
  pinMode(PIN_BATERIA, INPUT);

  if (lightMeter.begin(BH1750::ONE_TIME_HIGH_RES_MODE)) {
    Serial.println("[SENSORES] GY-30 (BH1750) iniciado com sucesso.");
  } else {
    Serial.println("[SENSORES] FALHA crítica ao inicializar o BH1750!");
    codigoErro |= ERR_BH1750; 
  }
}

void lerSensores(float &temp, float &umid, int &soloPct, int &lux, float &bateriaV, int &codigoErro) {
  // --- DHT11 ---
  const int MAX_DHT_RETRIES = 3;
  for (int i = 0; i < MAX_DHT_RETRIES; i++) {
    temp = dht.readTemperature();
    umid = dht.readHumidity();
    
    if (!isnan(temp) && !isnan(umid)) break;
    
    if (i < MAX_DHT_RETRIES - 1) {
      delay(2000);
      esp_task_wdt_reset(); 
    }
  }

  if (isnan(temp) || isnan(umid)) {
    Serial.println("[SENSORES] Falha definitiva na leitura do DHT11.");
    temp = -999.0; 
    umid = -999.0;
    codigoErro |= ERR_DHT; 
  }

  // --- Solo (Calibração Dinâmica) ---
  Preferences storeSensores;
  storeSensores.begin("calibracao", true); // Abre em modo somente-leitura
  int soloSeco = storeSensores.getInt("soloSeco", SOLO_SECO_DEFAULT);
  int soloUmido = storeSensores.getInt("soloUmido", SOLO_UMIDO_DEFAULT);
  storeSensores.end();

  int soloRaw = analogRead(PIN_SOLO);
  soloPct = constrain(map(soloRaw, soloSeco, soloUmido, 0, 100), 0, 100);

  // --- Luminosidade ---
  lightMeter.configure(BH1750::ONE_TIME_HIGH_RES_MODE);
  delay(200); 
  float luzRaw = lightMeter.readLightLevel();
  lux = (int)luzRaw;

  // --- Bateria (Divisor Resistivo Documentado) ---
  analogSetPinAttenuation(PIN_BATERIA, ADC_11db);
  analogReadResolution(12);

  
  static float calcularTensaoBateriaCalibrada() {
  uint32_t somaMv = 0;

  
  for (int i = 0; i < AMOSTRAS_ADC; i++) {
    // API Nativa do S3 que lê o eFuse gravado de fábrica
    somaMv += analogReadMilliVolts(PIN_BATERIA);
    delay(2); 
  }

  float mediaMv = somaMv / (float)AMOSTRAS_ADC;
  float tensaoRealBateria = (mediaMv * FATOR_DIVISOR) / 1000.0;

  // Trava de segurança lógica
  if (tensaoRealBateria < 0.0) tensaoRealBateria = 0.0;
  if (tensaoRealBateria > 4.25) tensaoRealBateria = 4.25;

  return tensaoRealBateria;
}
bateriaV = calcularTensaoBateriaCalibrada()
  Serial.printf("[DADOS LIDOS] Temp: %.1f C | Umid: %.1f %% | Solo: %d %% | Luz: %d lx | Bat: %.2f V\n", temp, umid, soloPct, lux, bateriaV);
}
