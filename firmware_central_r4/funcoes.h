/**
 * funcoes.h — Funcoes utilitarias do Arduino R4 central
 *
 * Contem:
 *  - Parser do pacote CSV do MATLAB
 *  - Recepcao e envio serial para o MATLAB
 *  - Comunicacao I2C com perifericos (painel e turbina)
 *  - Atualizacao do estado de carga (SoC) da bateria
 *  - Calculo do numero de LEDs para visualizacao do SoC
 *
 * Todas as constantes de configuracao estao em config.h.
 */

#pragma once

#include <Wire.h>
#include <initializer_list>
#include "config.h"

// ---------------------------------------------------------------------------
// Parser CSV
// ---------------------------------------------------------------------------

/**
 * Divide uma string com separador em um array de floats.
 * Retorna o numero de valores encontrados.
 * Exemplo: "10.5,3.2,0,1" com separador ',' -> {10.5, 3.2, 0.0, 1.0}
 */
int splitStringToFloats(String linha, char separador, float valores[], int maxValores) {
    int count    = 0;
    int start    = 0;
    int sepIndex = 0;

    while (count < maxValores) {
        sepIndex = linha.indexOf(separador, start);

        if (sepIndex == -1) {
            valores[count++] = linha.substring(start).toFloat();
            break;
        } else {
            valores[count++] = linha.substring(start, sepIndex).toFloat();
            start = sepIndex + 1;
        }
    }

    return count;
}

// ---------------------------------------------------------------------------
// Comunicacao serial com o MATLAB
// ---------------------------------------------------------------------------

/**
 * Le bytes da porta serial e monta uma linha completa no buffer destino.
 * Retorna true quando encontra '\n' ou '\r' com conteudo no buffer.
 * Nao-bloqueante: pode ser chamada a cada iteracao do loop().
 */
bool receberMatlab(Stream &port, char* destBuffer, int bufferSize) {
    static int bufferIdx = 0;
    bool pacoteCompleto  = false;

    while (port.available() > 0) {
        char c = port.read();

        if (c == '\n' || c == '\r') {
            if (bufferIdx > 0) {
                destBuffer[bufferIdx] = '\0';
                pacoteCompleto        = true;
            }
            bufferIdx = 0;

        } else if (bufferIdx < bufferSize - 1) {
            destBuffer[bufferIdx++] = c;
        }
        // Se bufferIdx >= bufferSize, o caracter e descartado (overflow silencioso)

        if (pacoteCompleto) {
            return true;
        }
    }

    return false;
}

/**
 * Envia uma lista de floats separados por virgula para a porta serial,
 * terminando com '\n'. Formato: "v1,v2,...,vN\n"
 * Exemplo: enviarMatlab(Matlab, {Level_H2, Pot_cc, SoC})
 */
inline void enviarMatlab(Stream &port, std::initializer_list<float> items) {
    int i          = 0;
    int totalItems = items.size();

    for (float item : items) {
        port.print(item, 2);
        if (i < totalItems - 1) {
            port.print(",");
        }
        i++;
    }
    port.print("\n");
}

// ---------------------------------------------------------------------------
// Comunicacao I2C com perifericos
// ---------------------------------------------------------------------------

/**
 * Envia um float de 4 bytes para um periferico via I2C.
 * Usado para enviar a referencia de potencia ao painel e a turbina.
 */
void enviarI2C(int endereco, float valor) {
    byte buffer[4];
    memcpy(buffer, &valor, sizeof(float));
    Wire.beginTransmission(endereco);
    Wire.write(buffer, 4);
    Wire.endTransmission();
}

/**
 * Le um float de 4 bytes de um periferico via I2C.
 * Retorna 0.0 em caso de falha (nenhum byte recebido).
 * Usado para ler Pot_pv do painel e Pot_wt da turbina.
 */
float lerDoI2C(int endereco) {
    byte  buffer[4]        = {0, 0, 0, 0};
    float valorRecebido    = 0.0f;
    int   bytesRecebidos   = Wire.requestFrom(endereco, 4);

    if (bytesRecebidos == 4) {
        for (int i = 0; i < 4; i++) {
            buffer[i] = Wire.read();
        }
        memcpy(&valorRecebido, buffer, sizeof(float));
    }

    return valorRecebido;
}

// ---------------------------------------------------------------------------
// Controle e SoC
// ---------------------------------------------------------------------------

/**
 * Atualiza o Estado de Carga (SoC) da bateria a cada passo de tempo.
 * Equacao: SoC[k+1] = omega * SoC[k] + a * Pc - b * Pd
 *
 *   SoC   : Estado de carga [0..1]
 *   omega : Fator de perda por ciclo (ver config.h)
 *   a     : Coeficiente de carga (ver config.h)
 *   b     : Coeficiente de descarga (ver config.h)
 *   Pc    : Potencia de carga da bateria [W]
 *   Pd    : Potencia de descarga da bateria [W]
 *   Ts    : Periodo de amostragem [s]
 */
void atualiza_SoC(float &SoC, float omega, float a, float b, float Pc, float Pd, float Ts) {
    SoC = omega * SoC + a * Pc - b * Pd;
    if (SoC > 1.0f) SoC = 1.0f;
    if (SoC < 0.0f) SoC = 0.0f;
}

/**
 * Converte o SoC (0.0 a 1.0) em numero inteiro de LEDs a acender
 * na matriz do R4 Wifi (total de NUM_LEDS).
 */
float arredondaPraBaixo(float x) {
    return (int)floor(x * NUM_LEDS);
}
