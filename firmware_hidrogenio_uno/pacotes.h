/*
 * pacotes.h — Definicao dos pacotes SerialTransfer entre UNO H2 e R4 central
 *
 * IMPORTANTE: esta estrutura deve ser IDENTICA ao pacotes_serial.h do
 * firmware_central_r4. Se alterar aqui, alterar la tambem.
 */

#ifndef PACOTES_H
#define PACOTES_H

// IDs unicos de cada tipo de pacote
#define ID_UNO_TO_R4  0
#define ID_R4_TO_UNO  1

// Pacote que o UNO envia PARA o R4 (telemetria)
struct UnoToR4_Data {
    float Pot_cc;    // Potencia da celula combustivel [W]
    float Level_H2;  // Nivel de hidrogenio no reservatorio [mm]
};

// Pacote que o R4 envia PARA o UNO (comandos)
struct R4ToUno_Data {
    float ucc;  // Comando da celula combustivel (0 = fechada, 1 = aberta)
    float uel;  // Comando do eletrolisador    (0 = desligado, 1 = ligado)
};

#endif // PACOTES_H
