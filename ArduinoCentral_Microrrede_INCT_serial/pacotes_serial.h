
#ifndef PACOTES_SERIAL_H
#define PACOTES_SERIAL_H


// Pacote 1: Dados que o R4 envia PARA o UNO
struct R4ToUno_Data {
  float ucc;
  float uel;
};

// Pacote 0: Dados que o UNO envia PARA o R4
struct UnoToR4_Data {
  float Pot_cc;
  float Level_H2;
};


// --- NOVO BLOCO: MAPEAMENTO DE ID DE PACOTE ---
// Aqui nós associamos um ID a cada tipo de struct.

// 1. Definição do template padrão (não usado diretamente)
template<typename T>
struct PacketTraits;

// 2. Especialização para R4ToUno_Data: associamos o ID 1
template<>
struct PacketTraits<R4ToUno_Data> {
  static const uint8_t id = 1; // <--- ID associado
};

// 3. Especialização para UnoToR4_Data: associamos o ID 0
template<>
struct PacketTraits<UnoToR4_Data> {
  static const uint8_t id = 0; // <--- ID associado
};

#define ID_R4_TO_UNO 1
#define ID_UNO_TO_R4 0


#endif // PACOTES_H