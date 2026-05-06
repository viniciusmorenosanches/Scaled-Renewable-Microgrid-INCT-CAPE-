#ifndef REFERENCIAS_H
#define REFERENCIAS_H

#include <Arduino.h>

// Função que retorna a referência de irradiação/potência solar para o instante k
float ref_Sol(int k) {
  // Exemplo: Uma curva senoidal simples simulando o dia
  // Ou você pode usar um array com valores pré-definidos
  float valor = 100.0 * sin(0.01 * k); 
  if (valor < 0) valor = 0; // Sol não brilha à noite
  return valor;
}

// Função que retorna a referência de vento para o instante k
float ref_Vento(int k) {
  // Exemplo: Valor constante com um pouco de ruído aleatório
  float base = 10.0;
  float ruido = (random(-10, 10) / 10.0);
  return base + ruido;
}

#endif
