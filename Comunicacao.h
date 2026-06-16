#ifndef COMUNICACAO_H
#define COMUNICACAO_H

#include <Arduino.h>

void inicializarLoRa(int &codigoErro);
void transmitirDadosLoRa(float temp, float umid, int soloPct, int lux, float bateriaV, int &codigoErro);
void dormirLoRa();

#endif // COMUNICACAO_H