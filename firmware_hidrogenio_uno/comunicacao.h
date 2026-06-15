/**
 * comunicacao.h — Funcoes de comunicacao SerialTransfer do UNO H2
 *
 * Encapsula o uso da biblioteca SerialTransfer para envio e recepcao
 * de pacotes estruturados entre o UNO H2 e o Arduino R4 central.
 */

#ifndef COMUNICACAO_H
#define COMUNICACAO_H

#include <SerialTransfer.h>

/**
 * Envia um pacote de dados para o R4.
 *   tx:        instancia SerialTransfer configurada
 *   id:        ID do pacote (ver pacotes.h)
 *   pacotePtr: ponteiro para a struct de dados a enviar
 *   tamanho:   sizeof da struct
 */
inline void enviarDados(SerialTransfer &tx, uint8_t id, void* pacotePtr, uint16_t tamanho) {
    memcpy(tx.packet.txBuff, pacotePtr, tamanho);
    tx.sendData(tamanho, id);
}

/**
 * Copia os bytes recebidos do R4 para a struct de destino.
 * Chamar apenas apos confirmar o ID correto com currentPacketID().
 */
inline void receberDados(SerialTransfer &rx, void* pacotePtr) {
    memcpy(pacotePtr, rx.packet.rxBuff, rx.bytesRead);
}

#endif // COMUNICACAO_H
