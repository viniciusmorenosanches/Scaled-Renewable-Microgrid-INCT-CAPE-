#ifndef COMUNICACAO_H
#define COMUNICACAO_H

#include <SerialTransfer.h>

inline void enviarDados(SerialTransfer &tx, uint8_t id, void* pacotePtr, uint16_t tamanho) {
  // 1. Copia a struct (pacote) para o buffer de envio interno
  memcpy(tx.packet.txBuff, pacotePtr, tamanho);
  // 2. Envia pela serial, informando o tamanho real e o ID
  tx.sendData(tamanho, id);
}

inline void receberDados(SerialTransfer &rx, void* pacotePtr) {
  // rx.bytesRead nos diz quantos bytes o payload recebido realmente tinha.
  // Copiamos esse total do buffer de recebimento para nossa struct de destino.
  memcpy(pacotePtr, rx.packet.rxBuff, rx.bytesRead);
}

#endif 