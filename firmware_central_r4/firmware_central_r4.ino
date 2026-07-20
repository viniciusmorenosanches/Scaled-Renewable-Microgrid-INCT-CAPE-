/**
 * firmware_central_r4.ino — Arduino R4 Wifi central da microrrede em escala
 *
 * Autor original: Samira (Agosto/2025)
 * Refatorado: Junho/2026
 *
 * Papel desta placa:
 *  - Receber do MATLAB/app web os comandos de controle (Pc, Pd, ucc, uel)
 *    e as referencias de geracao solar e eolica.
 *  - Repassar as referencias via I2C ao painel solar e a turbina eolica.
 *  - Repassar os comandos via SerialTransfer ao Arduino UNO do H2.
 *  - Atualizar o SoC da bateria localmente.
 *  - Enviar telemetria de volta ao MATLAB/app web.
 *
 * Pacote recebido do MATLAB (CSV, 6 campos):
 *   Pc, Pd, ucc, uel, ref_sol, ref_vento
 *
 * Pacote enviado ao MATLAB (CSV, 8 campos):
 *   Level_H2, Pot_cc, Pot_total, SoC, Pot_wt, Pot_pv, Pc, Pd
 *
 * Ver docs/comunicacao.md para detalhes completos.
 */

#include <Wire.h>
#include <initializer_list>
#include <SerialTransfer.h>
#include <avr/pgmspace.h>

#include "config.h"
#include "funcoes.h"
#include "comunicacao_serial.h"
#include "pacotes_serial.h"

// ---------------------------------------------------------------------------
// Objetos de comunicacao SerialTransfer (R4 <-> UNO H2)
// ---------------------------------------------------------------------------

SerialTransfer  R4Transfer;
UnoToR4_Data    DadosRecebidos;   // Telemetria vinda do UNO (Pot_cc, Level_H2)
R4ToUno_Data    DadosEnviar;      // Comandos enviados ao UNO (ucc, uel)

// ---------------------------------------------------------------------------
// Variaveis de entrada — vindas do MATLAB/app
// ---------------------------------------------------------------------------

float   Pc              = 0.0f;   // Potencia de carga da bateria [W]
float   Pd              = 0.0f;   // Potencia de descarga da bateria [W]
uint8_t ucc             = 0;      // Comando da celula combustivel (0=off, 1=on)
uint8_t uel             = 0;      // Comando do eletrolisador      (0=off, 1=on)
float   ref_sol_serial  = 0.0f;   // Referencia de potencia do painel [V]
float   ref_vento_serial = 0.0f;  // Referencia de potencia da turbina [V]

// ---------------------------------------------------------------------------
// Variaveis de telemetria — vindas dos perifericos
// ---------------------------------------------------------------------------

float Pot_wt   = 0.0f;   // Potencia da turbina eolica [W]
float Pot_pv   = 0.0f;   // Potencia do painel solar   [W]
float Pot_cc   = 0.0f;   // Potencia da celula combustivel [W]
float Level_H2 = 0.0f;   // Nivel de hidrogenio no reservatorio [mm]
float Pot_total = 0.0f;  // Balanco de potencia: Pot_pv + Pot_cc + Pot_wt - Pc + Pd [W]

// ---------------------------------------------------------------------------
// Estado da bateria
// ---------------------------------------------------------------------------

float SoC = 1.0f;   // Estado de Carga da bateria [0..1] — condicao inicial = 100%

// ---------------------------------------------------------------------------
// Controle de tempo
// ---------------------------------------------------------------------------

unsigned long tempoUltimoControle       = 0;
unsigned long tempoUltimaComunicacao_ms = 0;  // Para watchdog

// ---------------------------------------------------------------------------
// Buffer de comunicacao serial com MATLAB
// ---------------------------------------------------------------------------

char  matlabBuffer[256];
float dadosMatlab[10];

int k = 0;           // Contador de iteracoes
int numLedsOn = 0;   // Reservado para uso futuro: Numero de LEDs acesos (proporcional ao SoC)

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------

void setup() {
    Wire.begin();                          // R4 como mestre I2C
    Matlab.begin(BAUD_RATE_MATLAB);
    Arduino_el.begin(BAUD_RATE_ARDUINO_EL);
    R4Transfer.begin(Arduino_el);

    tempoUltimoControle       = millis();
    tempoUltimaComunicacao_ms = millis();
}

// ---------------------------------------------------------------------------
// Loop principal
// ---------------------------------------------------------------------------

void loop() {

    // ---- Recepcao imediata de pacotes do MATLAB (nao-bloqueante) ----
    // Lemos assim que chegam, sem esperar o ciclo de 100 ms,
    // para minimizar latencia no controle.
    if (receberMatlab(Matlab, matlabBuffer, 256)) {
        int qtd = splitStringToFloats(matlabBuffer, ',', dadosMatlab, 10);

        if (qtd >= 4) {
            tempoUltimaComunicacao_ms = millis();  // Reset do watchdog

            Pc  = dadosMatlab[0];
            Pd  = dadosMatlab[1];
            
            // Limiar robusto (0.5f) para evitar erros de truncamento de ponto flutuante
            ucc = (dadosMatlab[2] > 0.5f) ? 1 : 0;
            uel = (dadosMatlab[3] > 0.5f) ? 1 : 0;

            if (qtd >= 6) {
                ref_sol_serial   = dadosMatlab[4];
                ref_vento_serial = dadosMatlab[5];
            }

            Pot_total = Pot_pv + Pot_cc + Pot_wt - Pc + Pd;
            enviarMatlab(Matlab, { Level_H2, Pot_cc, Pot_total, SoC, Pot_wt, Pot_pv, Pc, Pd });
        }
    }

    // ---- Bloco de controle a cada Ts = 100 ms ----
    if (millis() - tempoUltimoControle >= PERIODO_CONTROLE_MS) {
        tempoUltimoControle = millis();
        k++;

        // Watchdog: sem comunicacao por mais de 2 s -> zera comandos por seguranca
        if (millis() - tempoUltimaComunicacao_ms > TIMEOUT_WATCHDOG_MS) {
            Pc               = 0.0f;
            Pd               = 0.0f;
            ucc              = 0;
            uel              = 0;
            ref_sol_serial   = 0.0f;
            ref_vento_serial = 0.0f;
        }

        // Envia referencia ao painel solar e le potencia de volta
        enviarI2C(ENDERECO_I2C_PAINEL_SOLAR, ref_sol_serial);
        Pot_pv = lerDoI2C(ENDERECO_I2C_PAINEL_SOLAR);

        // Envia referencia a turbina eolica e le potencia de volta
        enviarI2C(ENDERECO_I2C_TURBINA, ref_vento_serial);
        Pot_wt = lerDoI2C(ENDERECO_I2C_TURBINA);

        Pot_total = Pot_pv + Pot_cc + Pot_wt - Pc + Pd;

        // Envia comandos ao Arduino UNO do H2
        DadosEnviar.ucc = ucc;
        DadosEnviar.uel = uel;
        enviarArduino_el(R4Transfer, DadosEnviar);

        // Atualiza SoC da bateria
        atualiza_SoC(SoC, SOC_PERDA, SOC_COEF_CARGA, SOC_COEF_DESCARGA, Pc, Pd, TS_SEGUNDOS);
        numLedsOn = arredondaPraBaixo(SoC);
    }

    // ---- Recepcao de telemetria do UNO H2 (nao-bloqueante) ----
    if (R4Transfer.available()) {
        if (R4Transfer.currentPacketID() == ID_UNO_TO_R4) {
            receberArduino_el(R4Transfer, &DadosRecebidos);
            Pot_cc   = DadosRecebidos.Pot_cc;
            Level_H2 = DadosRecebidos.Level_H2;
        }
    }
}
