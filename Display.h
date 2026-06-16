#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>

void inicializarDisplays(int &codigoErro);
void desenharOlhos();
void desligarDisplays();
void atualizarTextoDisplays(String textoEsq, String textoDir);

#endif // DISPLAY_H