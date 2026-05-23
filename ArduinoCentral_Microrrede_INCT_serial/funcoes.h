#pragma once  // impede múltiplas inclusões
#include <SerialTransfer.h>   // Biblioteca para comunicação Serial entre arduinos

// ======================================================
// ==== IMPLEMENTAÇÃO DAS FUNÇÕES ====

#define LED_PIN 7
#define NUM_LEDS 24
#define NUM_FILEIRAS 2.000

#define Arduino_wt 10
#define Arduino_pv 9
#define Matlab Serial
#define Arduino_el Serial1



int splitStringToFloats(String linha, char separador, float valores[], int maxValores) {
  int count = 0;
  int start = 0;
  int sepIndex = 0;

  while (count < maxValores) {
    sepIndex = linha.indexOf(separador, start);

    if (sepIndex == -1) {
      // Último valor
      valores[count++] = linha.substring(start).toFloat();
      break;
    } else {
      valores[count++] = linha.substring(start, sepIndex).toFloat();
      start = sepIndex + 1;
    }
  }

  return count; // retorna quantos valores foram encontrados
}


void atualiza_SoC(float &SoC, float omega, float a, float b, float Pc, float Pd, float Ts){
  SoC = omega*SoC + a*Pc - b*Pd;
  if (SoC > 1) SoC = 1;
  if (SoC < 0) SoC = 0;
}

float arredondaPraBaixo(float x) {
  return (int)floor(x/(NUM_FILEIRAS/NUM_LEDS));
}

float lerDoI2C(int endereco) {
  byte buffer[4] = {0, 0, 0, 0}; // FIX: Initialize buffer with zeros
  float valorRecebido = 0.0;

  int bytesRecebidos = Wire.requestFrom(endereco, 4); // pede 4 bytes
  
  if (bytesRecebidos == 4) { // FIX: Only process if slave actually responded
    for (int i = 0; i < 4; i++) {
      buffer[i] = Wire.read();
    }
    memcpy(&valorRecebido, buffer, sizeof(float)); 
  }
  
  return valorRecebido; // If it fails, it will safely return 0.0 instead of 10.61
}


void enviarI2C(int endereco, float valor) {
  byte buffer[4];
  memcpy(buffer, &valor, sizeof(float));

  Wire.beginTransmission(endereco);
  Wire.write(buffer, 4);  // envia os 4 bytes de uma vez
  Wire.endTransmission();
}


inline void enviarMatlab(Stream &port, std::initializer_list<float> items) {
  int i = 0;
  int totalItems = items.size();
  
  for (float item : items) {
    port.print(item, 2); // Imprime o float com 2 casas decimais
    if (i < totalItems - 1) {
      port.print(","); // Adiciona vírgula, exceto após o último item
    }
    i++;
  }
  port.print("\n"); // Termina o "pacote" com uma nova linha
}

bool receberMatlab(Stream &port, char* destBuffer, int bufferSize) {
  static int bufferIdx = 0; // Índice do buffer (mantém seu valor entre chamadas)
  bool pacoteCompleto = false;

  while (port.available() > 0) {
    char c = port.read();

    if (c == '\n' || c == '\r') { // Detectou fim de linha
      if (bufferIdx > 0) { // Se o buffer não estiver vazio (ignora linhas em branco)
        destBuffer[bufferIdx] = '\0'; // Termina a string (string C)
        pacoteCompleto = true;        // Sinaliza que temos um pacote
      }
      bufferIdx = 0; // Reseta o índice para a próxima linha
      
    } else if (bufferIdx < bufferSize - 1) {
      // Adiciona o caractere ao buffer se houver espaço
      destBuffer[bufferIdx++] = c; 
      
    } // Se bufferIdx >= bufferSize, o caractere é descartado (overflow)

    if (pacoteCompleto) {
      // Retorna true imediatamente após encontrar uma linha completa
      return true; 
    }
  }
  
  return false; // Nenhum pacote completo foi encontrado nesta chamada do loop
}
