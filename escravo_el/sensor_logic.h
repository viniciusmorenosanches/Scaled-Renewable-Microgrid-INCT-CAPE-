#pragma once // Impede que o arquivo seja incluído múltiplas vezes

#include <Wire.h>
#include <VL6180X.h>

// --- Configurações do Sensor ---
const int NUM_READINGS = 20;      // Número de leituras para a média aparada
const float CUP_HEIGHT = 47.0;    // Altura do recipiente em mm (ajuste conforme necessário)

// Objeto do sensor
VL6180X sensor;

// --- Funções Internas (privadas ao arquivo) ---

// Função interna para obter a média aparada (seu código original)
static float getTrimmedAverage() {
  uint8_t readings[NUM_READINGS];

  // Coleta as leituras
  for (int i = 0; i < NUM_READINGS; i++) {
    readings[i] = sensor.readRangeSingleMillimeters();
    if (sensor.timeoutOccurred()) { 
        // Em caso de erro de leitura, use um valor inválido (ex: 255) ou o anterior
        readings[i] = 255; 
    }
    delay(2);
  }

  // Ordena as leituras (Bubble Sort)
  for (int i = 0; i < NUM_READINGS - 1; i++) {
    for (int j = i + 1; j < NUM_READINGS; j++) {
      if (readings[j] < readings[i]) {
        uint8_t temp = readings[i];
        readings[i] = readings[j];
        readings[j] = temp;
      }
    }
  }

  // Apara os primeiros e últimos 20% dos valores
  int trim = NUM_READINGS / 5;
  float sum = 0;
  for (int i = trim; i < NUM_READINGS - trim; i++) {
    sum += readings[i];
  }

  return sum / (NUM_READINGS - 2 * trim);
}


// --- Funções Públicas (para serem chamadas no seu .ino) ---

void setupSensor() {
  sensor.init();
  sensor.configureDefault();
  sensor.setTimeout(500);

  // Melhora a estabilidade da medição
  sensor.writeReg(VL6180X::SYSRANGE__MAX_CONVERGENCE_TIME, 50);
  sensor.writeReg(VL6180X::READOUT__AVERAGING_SAMPLE_PERIOD, 10);
}

float readWaterLevel() {
  // Obtém a distância média aparada até a superfície
  float avgDistanceToSurface = getTrimmedAverage();

  // Se a leitura falhar, retorna um valor negativo para indicar erro
  if (avgDistanceToSurface >= 255) {
      return -1.0;
  }

  // Calcula o nível da água (invertido)
  float waterLevelDistance = CUP_HEIGHT - avgDistanceToSurface;
  
  // Limita o valor para não ser negativo
  if (waterLevelDistance < 0) {
      waterLevelDistance = 0;
  }

  // Aplica sua fórmula de calibração/arredondamento personalizada
  // Esta fórmula parece ser uma calibração específica, vou mantê-la como está.
  // (21-6)/(21-5) = 15/16 = 0.9375
  waterLevelDistance = round(waterLevelDistance * 0.9375 * 2) / 2.0;

  return waterLevelDistance;
}
