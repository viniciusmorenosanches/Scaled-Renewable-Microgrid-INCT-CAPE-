/*
 * data_packets.h
 * Esta estrutura deve ser IDÊNTICA em ambos os Arduinos.
 */

// IDs únicos para cada tipo de pacote de dados
#define ID_UNO_TO_R4  0
#define ID_R4_TO_UNO  1

// Pacote 1: Dados que o UNO envia PARA o R4
struct UnoToR4_Data {
  float Pot_cc;
  float Level_H2;
};


// Pacote 2: Dados que o R4 envia PARA o UNO
struct R4ToUno_Data {
  float ucc;
  float uel;
};