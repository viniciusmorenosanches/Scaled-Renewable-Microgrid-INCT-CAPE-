#ifndef COMUNICACAO_SERIAL_H
#define COMUNICACAO_SERIAL_H

#include <SerialTransfer.h>
#include "pacotes_serial.h" // Importante: deve ser incluído para sabermos das structs e traits
//#include <initializer_list>
#include <HardwareSerial.h>

/* =========================================================================
 * FUNÇÃO DE ENVIO 
 * =========================================================================
*/
template<typename T_Pacote>
inline void enviarArduino_el(SerialTransfer &transferencia, const T_Pacote &pacote) {
  
  // 1. Descobre automaticamente o ID correto usando o "PacketTraits" 
  //    que definimos em pacotes.h
  const uint8_t pacoteID = PacketTraits<T_Pacote>::id;

  // 2. Descobre automaticamente o tamanho do pacote
  const uint16_t tamanhoPacote = sizeof(pacote);

  // 3. Copia os dados do pacote para o buffer de envio
  //    (Note o (void*)&pacote para converter a referência const em um ponteiro genérico)
  memcpy(transferencia.packet.txBuff, (void*)&pacote, tamanhoPacote);

  // 4. Envia os dados usando o ID e tamanho corretos
  transferencia.sendData(tamanhoPacote, pacoteID);
}


/* =========================================================================
 * FUNÇÃO DE RECEBIMENTO 
 * =========================================================================
*/
inline void receberArduino_el(SerialTransfer &rx, void* pacotePtr) {
  // (Esta é a função que chamamos de 'copiarDadosRecebidos' antes)
  // (Certifique-se que o nome aqui bate com o nome no seu .ino)
  memcpy(pacotePtr, rx.packet.rxBuff, rx.bytesRead);
}


#endif 
