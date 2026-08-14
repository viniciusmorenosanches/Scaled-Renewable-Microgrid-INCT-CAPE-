/**
 * sensor_nivel_h2.cpp — Implementacao do sensor de nivel de H2
 *
 * Usa o sensor de distancia VL6180X (I2C) para medir a distancia ate
 * a superficie da agua no reservatorio de H2. A distancia e convertida
 * para nivel de agua usando a geometria do copo e uma calibracao experimental.
 *
 * Ver docs/calibracao.md para contexto fisico completo das constantes.
 */

#include "sensor_nivel_h2.h"

#include <Arduino.h>
#include <VL6180X.h>
#include <Wire.h>

// ---------------------------------------------------------------------------
// Calibracao do sensor de nivel
//
// ALTURA_COPO_MM:
//   Altura fisica interna do copo/reservatorio usado no experimento [mm].
//   O sensor aponta para baixo e mede a distancia ate a superficie da agua.
//   Nivel = ALTURA_COPO_MM - distancia_medida.
//   Recalibrar se o copo/reservatorio for trocado.
//
// FATOR_CALIBRACAO_NIVEL:
//   Fator experimental para corrigir diferenca entre a leitura do sensor
//   e o nivel real de agua. A geometria interna do copo (nao perfeitamente
//   cilindrica) e a posicao do sensor afetam a medicao bruta.
//   Determinado por comparacao entre leitura e nivel medido com regua.
// ---------------------------------------------------------------------------

static const float ALTURA_COPO_MM        = 47.0f;
static const float FATOR_CALIBRACAO_NIVEL = 0.9375f;

// ---------------------------------------------------------------------------
// Filtro exponencial para suavizar a medicao
//
// Menor ALPHA_FILTRO_NIVEL = mais suavizacao, mais lentidao.
// Maior ALPHA_FILTRO_NIVEL = mais rapido, mais ruido.
// ---------------------------------------------------------------------------

static const float ALPHA_FILTRO_NIVEL = 0.05f;

// ---------------------------------------------------------------------------
// Tolerancia a falhas consecutivas
//
// Se o sensor falhar MAX_FALHAS_CONSECUTIVAS vezes seguidas, lerNivelH2_mm()
// retorna -1.0 para sinalizar erro persistente.
// O caller (firmware_hidrogenio_uno.ino) deve usar o ultimo valor valido
// ao inves de -1.0.
// ---------------------------------------------------------------------------

static const int MAX_FALHAS_CONSECUTIVAS = 3;

// ---------------------------------------------------------------------------
// Estado interno do modulo
// ---------------------------------------------------------------------------

static VL6180X sensor;
static float   distanciaFiltrada_mm  = -1.0f;
static int     falhasConsecutivas    = 0;

// ---------------------------------------------------------------------------
// Implementacao
// ---------------------------------------------------------------------------

void inicializarSensorNivelH2() {
    sensor.init();
    sensor.configureDefault();

    // Timeout de 50 ms: garante margem para os 30 ms de medicao e NUNCA ultrapassa o loop de 100 ms
    sensor.setTimeout(50);

    // Reduz o tempo de medicao mantendo estabilidade aceitavel.
    sensor.writeReg(VL6180X::SYSRANGE__MAX_CONVERGENCE_TIME, 30);
    sensor.writeReg(VL6180X::READOUT__AVERAGING_SAMPLE_PERIOD, 10);
}

float lerNivelH2_mm() {
    uint8_t distanciaBruta_mm = sensor.readRangeSingleMillimeters();
    bool    leituraInvalida   = sensor.timeoutOccurred() || distanciaBruta_mm == 255;

    if (leituraInvalida) {
        falhasConsecutivas++;
        if (falhasConsecutivas >= MAX_FALHAS_CONSECUTIVAS) {
            return -999.0f;  // Sinaliza erro persistente para o caller (sentinela < -500)
        }
        // Falha pontual: nao atualiza o filtro, retorna ultimo valor filtrado valido
    } else {
        falhasConsecutivas = 0;

        if (distanciaFiltrada_mm < 0.0f) {
            // Primeira leitura valida: inicializa o filtro sem transitorio
            distanciaFiltrada_mm = (float)distanciaBruta_mm;
        } else {
            distanciaFiltrada_mm = ALPHA_FILTRO_NIVEL * (float)distanciaBruta_mm
                                   + (1.0f - ALPHA_FILTRO_NIVEL) * distanciaFiltrada_mm;
        }
    }

    if (distanciaFiltrada_mm < 0.0f) {
        return 0.0f;  // Sem leituras validas ainda
    }

    float nivel_mm = ALTURA_COPO_MM - distanciaFiltrada_mm;
    // Removido o clamp para zero para permitir "tara" na interface web

    return nivel_mm * FATOR_CALIBRACAO_NIVEL;
}
