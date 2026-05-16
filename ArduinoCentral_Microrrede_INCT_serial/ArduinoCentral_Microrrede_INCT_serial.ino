///////////////////////////////////////////////////////////////
// Código do Arduino R4 Wifi central da microrrede em escala //
// Autor: Samira                                             //
// Agosto/2025                                               //
///////////////////////////////////////////////////////////////

//Bibliotecas
#include <Wire.h>
#include "Arduino_LED_Matrix.h"
#include "funcoes.h"
#include "comunicacao_serial.h"
//#include "referencias.h"
#include <avr/pgmspace.h> //alocar espaço memória FLASH
#include <initializer_list>
#include <SerialTransfer.h>   // Biblioteca para comunicação Serial entre arduinos

#define Arduino_el Serial1

ArduinoLEDMatrix matrix;
SerialTransfer R4Transfer;
UnoToR4_Data DadosRecebidos;
R4ToUno_Data DadosEnviar;

float valorFiltrado = 0;

//////////// Entradas e saídas //////////// 
//// Entradas vindas do MPC
float Pc = 0; //Pot [W] de carga da bateria que vem do MPC
float Pd = 0; //Pot [W] de descarga da bateria que vem do MPC
uint8_t ucc = 0; //Controle da célula de carga
uint8_t uel = 0; //Controle do eletrolisador

// VARIÁVEIS NOVAS: Clima ditado pelo PC
float ref_sol_serial = 0.0;
float ref_vento_serial = 0.0;

////Entradas vindas dos módulos 
float Pot_wt = 0.00; //Pot [W] Wind Turbine
float Pot_pv = 0.00; //Pot [W] PhotoVoltaic panel
float Pot_cc = 0.00; //Pot [W] célula de carga
float Level_H2 = 0.00; //Level Electrolyzer

float Pot_total; 

const float Ts = 0.1; //segundos
unsigned long tempo_ultimo_controle;
unsigned long tempo_ultimo_controle_mpc;
float dt;
int k = 0; //iteração

float SoC = 1; //State of Charge da bateria [%] -- Condição inicial

//Parâmetros escolhidos de uma bateria 48V, 100Ah
const int E_nom = 4800; //Energia nominal [Wh] 
//const float nc = 0.95; // eficiência de carga
//const float nd = 0.95; // eficiência de descarga
// const float a = nc*(Ts/3600*5000)/E_nom; //[1/W]
// const float b = (1/nd)*(Ts/3600*5000)/(E_nom); //[1/W]
//const float a = nc*(Ts)*0.0005; //[1/W]
const float a = 3.92*pow(10,-5); //[1/W]
//const float b = (1/nd)*(Ts)*0.0005; //[1/W]
const float b = 4.08*pow(10,-5); //[1/W]
const float omega = 0.995; //perda 

int num_leds_on;

char matlabBuffer[256];   
float dadosMatlab[10]; 

//declaração de funções
void atualiza_SoC(float &SoC, float a, float b, float Pc, float Pd, float Ts);
float arredondaPraBaixo(float x);
void recebeSerial(int endereco);
int lerDoI2C(int endereco, int qtdBytes);
void enviarParaMATLAB(float valor);
void enviarI2C(int endereco, float valor);

void setup() {
  Wire.begin(); // Inicia como master
  Matlab.begin(9600);
  Arduino_el.begin(115200);
  R4Transfer.begin(Arduino_el);

  tempo_ultimo_controle = millis();
  tempo_ultimo_controle_mpc = millis();
}


void loop() {

  if (millis() - tempo_ultimo_controle >= Ts*1000) {
    tempo_ultimo_controle = millis();
    k = k+1;

    // NOVO: Envia ref sol que veio do PC para Arduino_pv
    enviarI2C(Arduino_pv, ref_sol_serial);
    Pot_pv = lerDoI2C(Arduino_pv);
    
    // NOVO: Envia ref vento que veio do PC para Arduino_wt
    enviarI2C(Arduino_wt, ref_vento_serial);
    Pot_wt = lerDoI2C(Arduino_wt);
    
    Pot_total = Pot_pv + Pot_cc + Pot_wt - Pc + Pd;

//    //Manda valores para MPC
//    if (millis() - tempo_ultimo_controle_mpc >= Ts*1000) {
//      tempo_ultimo_controle_mpc = millis();
//      enviarMatlab(Matlab, { Level_H2, Pot_cc, Pot_total, SoC, Pot_wt, Pot_pv, Pc, Pd});
//    }

    // Lê do Matlab / Streamlit (AGORA LÊ 6 VALORES)
    if (receberMatlab(Matlab, matlabBuffer, 256)) {
      int qtd = splitStringToFloats(matlabBuffer, ',', dadosMatlab, 10); 
      if (qtd >= 6) {
          Pc = dadosMatlab[0];
          Pd = dadosMatlab[1];
          ucc = dadosMatlab[2];
          uel = dadosMatlab[3];
          ref_sol_serial = dadosMatlab[4];   // Sol do PC
          ref_vento_serial = dadosMatlab[5]; // Vento do PC

          enviarMatlab(Matlab, { Level_H2, Pot_cc, Pot_total, SoC, Pot_wt, Pot_pv, Pc, Pd});

      }
    }

    DadosEnviar.ucc = ucc;
    DadosEnviar.uel = uel;
    enviarArduino_el(R4Transfer, DadosEnviar);

    // DEBUG: O que estou enviando para o Uno?
//    Serial.print("ENVIANDO -> ucc: "); 
//    Serial.print(DadosEnviar.ucc);
//    Serial.print(" | uel: "); 
//    Serial.println(DadosEnviar.uel);

    // Atualiza SOC da bateria
    atualiza_SoC(SoC, omega, a, b, Pc, Pd, Ts);
    num_leds_on = arredondaPraBaixo(SoC);
  }

  if (R4Transfer.available()) {
    if (R4Transfer.currentPacketID() == ID_UNO_TO_R4) {
      receberArduino_el(R4Transfer, &DadosRecebidos);

      Pot_cc   = DadosRecebidos.Pot_cc;
      Level_H2 = DadosRecebidos.Level_H2;

       // DEBUG: O que acabei de receber do Uno?
//      Serial.print("RECEBIDO <- Pot_cc: "); 
//      Serial.print(Pot_cc);
//      Serial.print(" | Level_H2: "); 
//      Serial.println(Level_H2);
//      Serial.println("-----------------------------------");
    } 
  }
}
