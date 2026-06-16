#include "Comunicacao.h"
#include "Config.h"
#include <SPI.h>
#include <RadioLib.h>
#include <Preferences.h>
#include <esp_task_wdt.h>

// =============================================
// CÓDIGOS DE ESTADO DO RADIOLIB
// =============================================
#define RADIOLIB_ERR_NO_RX_WINDOW (-1111)
#define RADIOLIB_ERR_RX1_TIMEOUT  (-1116)
#define RADIOLIB_ERR_RX2_TIMEOUT  (-1118)

// =============================================
// CREDENCIAIS LORAWAN OTAA
// =============================================

//inforamções direto da Everynet/TTN/Chirpstack
static uint64_t devEUI  = 0x0000000000000000;  
static uint64_t joinEUI = 0x0000000000000000; 

static uint8_t appKey[] = { 
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
};

static uint8_t nwkKey[] = {  
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
};

static const LoRaWANBand_t region = AU915;
static const uint8_t subBand = 1; 

static SX1276 radio = new Module(LORA_NSS, LORA_DIO0, LORA_RST, LORA_DIO1, SPI);
static LoRaWANNode node(&radio, &region, subBand);
static Preferences store;

// =============================================
// FUNÇÕES ROBUSTAS DE NVS (Persistência)
// =============================================
static void limparNVS() {
  store.remove("session");
  store.remove("nonces");
  Serial.println("[NVS] ATENCAO: Memoria formatada. Chaves e sessoes limpas.");
}

static void salvarNonces() {
  uint8_t *nonces = node.getBufferNonces();
  if (nonces) {
    size_t escritos = store.putBytes("nonces", nonces, RADIOLIB_LORAWAN_NONCES_BUF_SIZE);
    if (escritos == RADIOLIB_LORAWAN_NONCES_BUF_SIZE) {
      Serial.println("[NVS] Nonces gravados na flash com sucesso.");
    } else {
      Serial.println("[NVS] ERRO CRITICO: Falha de I/O ao gravar Nonces na flash.");
    }
  }
}

static void salvarSessao() {
  uint8_t *session = node.getBufferSession();
  if (session) {
    size_t escritos = store.putBytes("session", session, RADIOLIB_LORAWAN_SESSION_BUF_SIZE);
    if (escritos == RADIOLIB_LORAWAN_SESSION_BUF_SIZE) {
      Serial.println("[NVS] Estado da Sessao (MAC/Counters) gravado com sucesso.");
    } else {
      Serial.println("[NVS] ERRO CRITICO: Falha de I/O ao gravar Sessao na flash.");
    }
  }
}

static void carregarNVS() {
  // 1. Carrega os Nonces (Validação estrita de tamanho)
  if (store.isKey("nonces")) {
    size_t len = store.getBytesLength("nonces");
    if (len == RADIOLIB_LORAWAN_NONCES_BUF_SIZE) {
      uint8_t buf[RADIOLIB_LORAWAN_NONCES_BUF_SIZE];
      store.getBytes("nonces", buf, len);
      node.setBufferNonces(buf);
      Serial.println("[NVS] Buffer de Nonces carregado na memoria.");
    } else {
      Serial.printf("[NVS] ERRO: Tamanho de Nonces invalido (%d bytes). Ignorando.\n", len);
    }
  }

  // 2. Carrega a Sessao (Validação estrita de tamanho)
  if (store.isKey("session")) {
    size_t len = store.getBytesLength("session");
    if (len == RADIOLIB_LORAWAN_SESSION_BUF_SIZE) {
      uint8_t buf[RADIOLIB_LORAWAN_SESSION_BUF_SIZE];
      store.getBytes("session", buf, len);
      
      int ret = node.setBufferSession(buf);
      if (ret == RADIOLIB_ERR_NONE) {
        Serial.println("[NVS] Buffer de Sessao hidratado. Aguardando validacao pelo stack.");
      } else {
        Serial.printf("[NVS] ERRO: O Stack rejeitou a sessao NVS (Codigo: %d). Limpando.\n", ret);
        store.remove("session");
      }
    } else {
      Serial.printf("[NVS] ERRO: Tamanho de Sessao invalido (%d bytes). Ignorando.\n", len);
      store.remove("session");
    }
  }
}

// =============================================
// IMPLEMENTAÇÕES DA INTERFACE
// =============================================
void inicializarLoRa(int &codigoErro) {
  Serial.println("\n[LORA] Inicializando modulo SX1276 (OTAA)...");
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);

  store.begin("lorawan", false); 

  int state = radio.begin();
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("[LORA] ERRO FATAL: Radio nao responde (Codigo: %d)\n", state);
    codigoErro |= ERR_RADIO_HW; 
    return; 
  }
  
  radio.setOutputPower(20);
  delay(2000); //teste de qualidade

  state = node.beginOTAA(joinEUI, devEUI, nwkKey, appKey);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("[LORA] ERRO FATAL: beginOTAA falhou (Codigo: %d)\n", state);
    codigoErro |= ERR_RADIO_JOIN; 
    return;
  }

  carregarNVS();

  Serial.println("[OTAA] Verificando ativacao da rede...");
  const int MAX_TENTATIVAS = 3;
  bool redeOk = false;

  for (int t = 1; t <= MAX_TENTATIVAS; t++) {
    esp_task_wdt_reset(); 
    state = node.activateOTAA();

    if (state == RADIOLIB_ERR_NONE || state == RADIOLIB_LORAWAN_NEW_SESSION) {
      Serial.printf("[OTAA] SUCESSO! Rede ativa operacional. DevAddr: 0x%08X\n", node.getDevAddr());
      
      // Se foi um novo JOIN efetivo, gravamos os Nonces para não usá-los novamente.
      if (state == RADIOLIB_LORAWAN_NEW_SESSION) {
        salvarNonces();
        salvarSessao();
      }
      redeOk = true;
      break; 
    } else {
      Serial.printf("[OTAA] Tentativa %d/%d falhou (Codigo: %d). Aguardando janela...\n", t, MAX_TENTATIVAS, state);
      if (t < MAX_TENTATIVAS) {
        //radio.sleep(); // Economia agressiva durante o backoff de conexao
        delay(4000); 
      }
    }
  }

  if (!redeOk) {
    // Falha temporária. Mantém a sessão atual intacta, apenas aborta a rodada.
    Serial.println("[LORA] AVISO: Gateway fora de alcance. Sessao em disco mantida intacta.");
    codigoErro |= ERR_JOIN; 
  }
}

void transmitirDadosLoRa(float temp, float umid, int soloPct, int lux, float bateriaV, int &codigoErro) {
  char payload[64];
  snprintf(payload, sizeof(payload), "T:%.1f;U:%.1f;S:%d;L:%d;B:%.2f;E:%d", temp, umid, soloPct, lux, bateriaV, codigoErro);
  
  Serial.printf("[LORA] Payload gerado: %s\n", payload);
  Serial.println("[LORA] Transmitindo uplink...");

  int state = node.sendReceive((uint8_t*)payload, strlen(payload), 1, true);
  Serial.printf("[LORA] FCntUp local: %lu\n", node.getFCntUp());

  /* 
  if (state >= 0) {
    Serial.printf("[LORA] OK! Pacote entregue a rede (Retorno: %d)\n", state);
    // Salva a sessão para garantir que o incremento do FCnt vá para a memória não-volátil
    salvarSessao();
    delay(500); 
  } else {
    Serial.printf("[LORA] ERRO: Falha na transmissao RF (Codigo: %d)\n", state);
    codigoErro |= ERR_RADIO_TX; 
    
    // Tratamento exclusivo de corrupção confirmada pelo servidor
    if (state == RADIOLIB_ERR_SESSION_DISCARDED || state == -1101) {
      Serial.println("[LORA] FATAL: O Servidor de Rede rejeitou a sessao atual.");
      limparNVS();
    }
  }*/

  if (state > 0) {

    Serial.println("[LORA] ACK recebido do servidor.");

    Serial.printf(
      "[LORA] FCntUp local: %lu\n",
      node.getFCntUp()
    );

    salvarSessao();
    delay(1000);
  }
  else if (state == 0) {

    Serial.println("[LORA] Uplink enviado sem downlink.");

    Serial.printf(
      "[LORA] FCntUp local: %lu\n",
      node.getFCntUp()
    );

    salvarSessao();
    delay(1000);
  }

}

void dormirLoRa() {
  radio.sleep();
  Serial.println("[LORA] O chip de RF entrou em Sleep.");
}
