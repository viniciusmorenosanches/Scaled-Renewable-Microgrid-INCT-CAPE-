///////////////////////////////////////////////////////////////
// Código do Arduino R4 Wifi central da microrrede em escala //
// Autor: Samira                                             //
// Agosto/2025                                               //
///////////////////////////////////////////////////////////////


//Blibliotecas
//#include <FastLED.h>
#include <Wire.h>
//#include "ArduinoGraphics.h"
#include "Arduino_LED_Matrix.h"
//#include "LEDs_R4_Matriz.h"
#include "funcoes.h"
#include "comunicacao_serial.h"
#include "referencias.h"
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
float ucc = 0; //Controle da célula de carga
float uel = 0; //Controle do eletrolisador

////Entradas vindas dos módulos 
float Pot_wt = 0.00; //Pot [W] Wind Turbine
float Pot_pv = 0.00; //Pot [W] PhotoVoltaic panel
float Pot_cc = 0.00; //Pot [W] célula de carga
float Level_H2 = 0.00; //Level Electrolyzer

////Saídas para MPC
//SoC
//Pot_wt
//Pot_pv
//Level_H2
float Pot_total; 

////Saídas para módulos
//ref_vento(k);
//ref_sol(k);
//ucc
//uel

//////////////////////////////////////////

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

//CRGB leds(NUM_LEDS);

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
  Arduino_el.begin(9600);
  R4Transfer.begin(Arduino_el);

  //FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);

  //setupMatrizLeds(matrix);
  tempo_ultimo_controle = millis();
  tempo_ultimo_controle_mpc = millis();
}

void loop() {

  if (millis() - tempo_ultimo_controle >= Ts*1000) {
    tempo_ultimo_controle = millis();
    k = k+1;
    //matrix.loadFrame(heart);

    //Envia ref sol para Arduino_pv
    enviarI2C(Arduino_pv, ref_Sol(k));
    // Lê pot do Arduino_pv
    Pot_pv = lerDoI2C(Arduino_pv);

    // Envia ref vento para Arduino_wt
    enviarI2C(Arduino_wt, ref_Vento(k));
    // Lê pot do Arduino_wt
    Pot_wt = lerDoI2C(Arduino_wt);

    //Calcula Pot total
    Pot_total = Pot_pv + Pot_cc + Pot_wt - Pc + Pd;

    //Manda valores para MPC
    if (millis() - tempo_ultimo_controle_mpc >= Ts*1000) {
      tempo_ultimo_controle_mpc = millis();
      enviarMatlab(Matlab, { Level_H2, Pot_cc, Pot_total, SoC, Pot_wt, Pot_pv, Pc, Pd});
    }

    // Lê do Matlab
    if (receberMatlab(Matlab, matlabBuffer, 256)) {
      int qtd = splitStringToFloats(matlabBuffer, ',', dadosMatlab, 10); 
      Pc = dadosMatlab[0];
      Pd = dadosMatlab[1];
      ucc = dadosMatlab[2];
      uel = dadosMatlab[3];
    }


    DadosEnviar.ucc = ucc;
    DadosEnviar.uel = uel;
    enviarArduino_el(R4Transfer, DadosEnviar);

    // Atualiza SOC da bateria
    atualiza_SoC(SoC, omega, a, b, Pc, Pd, Ts);
    num_leds_on = arredondaPraBaixo(SoC);
    //fill_solid(leds, num_leds_on, CRGB::Green);
    //fill_solid(leds+(NUM_LEDS/NUM_FILEIRAS), num_leds_on, CRGB::Green);

  }


  if (R4Transfer.available()) {
    if (R4Transfer.currentPacketID() == ID_UNO_TO_R4) {
      // SUCESSO. Pacote 0 (Resposta) chegou.
      
      receberArduino_el(R4Transfer, &DadosRecebidos);
      // ************************************************************

      Pot_cc   = DadosRecebidos.Pot_cc;
      Level_H2 = DadosRecebidos.Level_H2;
      
    } 

  }

}
