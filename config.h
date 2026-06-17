#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// =============================================
// PINAGEM DO SISTEMA
// =============================================
#define SPI_SCK    12
#define SPI_MISO   13
#define SPI_MOSI   11

#define LORA_NSS   9   
#define LORA_RST   20  
#define LORA_DIO0  47  
#define LORA_DIO1  2   

#define I2C_SDA    48  
#define I2C_SCL    45  

#define DHTPIN     18  
#define DHTTYPE    11 
#define PIN_SOLO   17  
#define PIN_BATERIA 3   
#define MOSFET_SOLO 8

// =============================================
// CALIBRAÇÃO PADRÃO (Solo e I2C)
// =============================================
#define SOLO_SECO_DEFAULT   4095
#define SOLO_UMIDO_DEFAULT  1290

#define ENDERECO_TELA_ESQUERDA 0x3C
#define ENDERECO_TELA_DIREITA  0x3D

// =============================================
// PARÂMETROS DA BATERIA (Divisor e Proteção)
// =============================================
// Divisor resistivo: R1=100k, R2=100k (Ajuste se sua placa usar valores diferentes)
#define BATERIA_R1       100000.0f
#define BATERIA_R2       100000.0f
#define BATERIA_DIVISOR  ((BATERIA_R1 + BATERIA_R2) / BATERIA_R2)
#define BATERIA_MINIMA_V 3.2f // Corte para evitar brownout do ESP32 ao acionar o rádio
#define AMOSTRAS_ADC  20      // Quantidade de leituras para o filtro

// =============================================
// TEMPOS DE DEEP SLEEP
// =============================================
#define SLEEP_NORMAL_MIN 60 //60 min
#define SLEEP_RETRY_MIN  5 //5 min

#define TEMPO_SLEEP_NORMAL ((uint64_t)SLEEP_NORMAL_MIN * 60ULL * 1000000ULL)
#define TEMPO_SLEEP_RETRY  ((uint64_t)SLEEP_RETRY_MIN  * 60ULL * 1000000ULL)

// =============================================
// BITMASK DE ERROS
// =============================================
#define ERR_NONE         0x00 
#define ERR_RADIO_HW     0x01 
#define ERR_RADIO_JOIN   0x02 
#define ERR_RADIO_TX     0x04 
#define ERR_DHT          0x08 
#define ERR_BH1750       0x10 
#define ERR_DISPLAY_ESQ  0x20 
#define ERR_DISPLAY_DIR  0x40 
#define ERR_PAYLOAD      0x80 
#define ERR_JOIN         0x100 // Flag para controle de rede offline
#define ERR_BATERIA      0x200 // Flag para bateria muito baixa

#endif // CONFIG_H
