/**
 * config.h — Configuracoes e constantes do Arduino R4 central
 *
 * Concentra todos os parametros de hardware, comunicacao e controle
 * em um lugar so. Para calibrar ou ajustar o sistema, editar apenas
 * este arquivo.
 */

#pragma once

// ---------------------------------------------------------------------------
// Enderecos I2C dos perifericos
// ---------------------------------------------------------------------------

#define ENDERECO_I2C_PAINEL_SOLAR  9   // firmware_painel_solar
#define ENDERECO_I2C_TURBINA       10  // firmware_turbina_eolica

// ---------------------------------------------------------------------------
// Mapeamento de portas seriais
// ---------------------------------------------------------------------------

// Comunicacao USB com PC (MATLAB / aplicativo web)
#define Matlab     Serial

// Comunicacao UART com Arduino UNO do sistema de H2
#define Arduino_el Serial1

// ---------------------------------------------------------------------------
// Parametros de temporização
// ---------------------------------------------------------------------------

const float         TS_SEGUNDOS          = 0.1f;    // Periodo de controle [s]
const unsigned long PERIODO_CONTROLE_MS  = 100;     // Periodo de controle [ms]
const unsigned long TIMEOUT_WATCHDOG_MS  = 10000;   // Sem pacote do MATLAB -> zera comandos [ms]

// ---------------------------------------------------------------------------
// Baud rates
// ---------------------------------------------------------------------------

const unsigned long BAUD_RATE_MATLAB     = 115200;
const unsigned long BAUD_RATE_ARDUINO_EL = 115200;

// ---------------------------------------------------------------------------
// LED Matrix (Arduino R4 Wifi)
// ---------------------------------------------------------------------------

const int   NUM_LEDS     = 24;
const float NUM_FILEIRAS = 2.0f;

// ---------------------------------------------------------------------------
// Modelo discreto de SoC da bateria
//
// Bateria de referencia: 48 V / 100 Ah -> E_nom = 4800 Wh
//
// Equacao:
//   SoC[k+1] = omega * SoC[k] + a * Pc[k] - b * Pd[k]
//
// Onde:
//   a     = efic_carga     * (Ts / 3600) * (escala / E_nom)
//   b     = (1/efic_desc.) * (Ts / 3600) * (escala / E_nom)
//   omega = fator de perda por ciclo (auto-descarga)
//
// Para recalibrar: ajustar eficiencias e E_nom conforme bateria real usada.
// ---------------------------------------------------------------------------

const float SOC_COEF_CARGA    = 3.92e-5f;  // a [1/W]
const float SOC_COEF_DESCARGA = 4.08e-5f;  // b [1/W]
const float SOC_PERDA         = 0.995f;    // omega (perda por ciclo)
