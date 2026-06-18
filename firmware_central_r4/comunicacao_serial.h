/**
 * comunicacao_serial.h — Funcoes de comunicacao SerialTransfer entre R4 e UNO H2
 *
 * Encapsula o uso da biblioteca SerialTransfer para que o .ino nao precise
 * lidar com buffers e IDs diretamente.
 */

#ifndef COMUNICACAO_SERIAL_H
#define COMUNICACAO_SERIAL_H

#include <SerialTransfer.h>
#include <HardwareSerial.h>
#include "pacotes_serial.h"

/**
 * Envia um pacote de dados para o Arduino UNO do H2.
 * O ID do pacote e o tamanho sao determinados automaticamente pelo tipo T_Pacote.
 */
template<typename T_Pacote>
inline void enviarArduino_el(SerialTransfer &transferencia, const T_Pacote &pacote) {
    const uint8_t  pacoteID      = PacketTraits<T_Pacote>::id;
    const uint16_t tamanhoPacote = sizeof(pacote);
    memcpy(transferencia.packet.txBuff, (void*)&pacote, tamanhoPacote);
    transferencia.sendData(tamanhoPacote, pacoteID);
}

/**
 * Copia os bytes recebidos do UNO para a struct de destino.
 * Chamar apenas apos confirmar ID correto com currentPacketID().
 */
inline void receberArduino_el(SerialTransfer &rx, void* pacotePtr) {
    memcpy(pacotePtr, rx.packet.rxBuff, rx.bytesRead);
}

#endif // COMUNICACAO_SERIAL_H
