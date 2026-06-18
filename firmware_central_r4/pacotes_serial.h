/**
 * pacotes_serial.h — Definicao dos pacotes SerialTransfer entre R4 e UNO H2
 *
 * IMPORTANTE: esta estrutura deve ser identica no firmware_central_r4
 * e no firmware_hidrogenio_uno. Se alterar aqui, alterar la tambem.
 *
 * Protocolo:
 *   ID 1 (ID_R4_TO_UNO): R4 envia { ucc, uel } para o UNO
 *   ID 0 (ID_UNO_TO_R4): UNO envia { Pot_cc, Level_H2 } para o R4
 */

#ifndef PACOTES_SERIAL_H
#define PACOTES_SERIAL_H

// IDs unicos de cada tipo de pacote
#define ID_R4_TO_UNO  1
#define ID_UNO_TO_R4  0

// Pacote que o R4 envia PARA o UNO (comandos)
struct R4ToUno_Data {
    float ucc;  // Comando da celula combustivel (0 = fechada, 1 = aberta)
    float uel;  // Comando do eletrolisador    (0 = desligado, 1 = ligado)
};

// Pacote que o UNO envia PARA o R4 (telemetria)
struct UnoToR4_Data {
    float Pot_cc;    // Potencia da celula combustivel [W]
    float Level_H2;  // Nivel de hidrogenio no reservatorio [mm]
};


// ---------------------------------------------------------------------------
// Mapeamento automatico de ID por tipo de struct (template traits)
// Usado em enviarArduino_el() para descobrir o ID sem passar manualmente.
// ---------------------------------------------------------------------------

template<typename T>
struct PacketTraits;

template<>
struct PacketTraits<R4ToUno_Data> {
    static const uint8_t id = ID_R4_TO_UNO;
};

template<>
struct PacketTraits<UnoToR4_Data> {
    static const uint8_t id = ID_UNO_TO_R4;
};

#endif // PACOTES_SERIAL_H
