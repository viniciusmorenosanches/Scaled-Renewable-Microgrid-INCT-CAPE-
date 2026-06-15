/**
 * sensor_nivel_h2.h — API publica do sensor de nivel de H2
 *
 * O sensor VL6180X mede a distancia ate a superficie da agua no reservatorio.
 * Este modulo converte essa distancia para nivel de agua usando a altura
 * fisica do copo/reservatorio e uma calibracao experimental.
 *
 * Ver sensor_nivel_h2.cpp para detalhes de calibracao e implementacao.
 * Ver docs/calibracao.md para contexto fisico das constantes.
 */

#pragma once

/**
 * Inicializa o sensor VL6180X.
 * Deve ser chamado uma vez em setup().
 */
void inicializarSensorNivelH2();

/**
 * Le o nivel de H2 no reservatorio em milimetros.
 * Retorna -1.0 se houver falhas persistentes de leitura do sensor
 * (>= MAX_FALHAS_CONSECUTIVAS falhas seguidas).
 */
float lerNivelH2_mm();
