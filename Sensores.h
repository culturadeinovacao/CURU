#ifndef SENSORES_H
#define SENSORES_H

#include <Arduino.h>

// Inicializa o DHT11, configura o pino do solo e inicia o BH1750 (GY-30)
void inicializarSensores(int &codigoErro);

// Realiza a leitura de todos os sensores e devolve os valores nas variáveis informadas
void lerSensores(float &temp, float &umid, int &soloPct, int &lux, float &bateriaV, int &codigoErro);

#endif // SENSORES_H