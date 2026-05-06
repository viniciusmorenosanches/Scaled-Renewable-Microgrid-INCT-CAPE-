/////////////////////////////////////////////////////
//Código de controle do sistema turbina-ventilador
//SBAI
//Controle PI LPV-H_infinito - validação experimental
//2025
//Autora: Samira
/////////////////////////////////////////////////////

#include <Servo.h> //biblioteca para o ESC
#include <MatrixMath.h> //operações com matrizes
#include <Wire.h>

#define ENDERECO 10

Servo ESC;
 
//Pinos arduino
int PinEsc = 9;
int PinTurbina = A0;

bool start = false;

volatile float Y_ref = 0; // referencia atual global
volatile float Pot_wt = 0; // global
float u_init[60] = {0,0,0,0,0,0,0,0,0,0,2,4,6,9,12,14,17,20,23,26,30,34,37,41,43,46,49,52,55,57,60,62,64,65,66,67,67,68,68,68,68,68,68,68,68,68,68,68,68,68,68,68,68,68,68,68,68,68};
//FILTROS
float a = 0.8; // parametro filtro de ruido da saída LPF

// Variáveis do Filtro de Kalman
float x_est = 0;  // Estado estimado
float P = 1;  // Incerteza estimada
float Q = 0.1;   // Variância do processo (ruído do modelo)
float R = 0.3;      // Variância da medição (ruído do sensor)
float K;          // Ganho de Kalman

//ação de controle e saída
mtx_type Uc[1][1]; //saída de controle em VOLTS
float U; //saída de controle saturada 0-5V
float Y[2]= {0,0}; // saída medida da turbina em VOLTS
float Y_teorico[2]= {0,0}; //saída teórica baseada no modelo identificado da planta
float Y_filtrado[2]= {0,0}; //a saída y é passada por um filtro passa baixas
float Y_lpf = 0;

//Erro e integral do erro
float E = 0;
float IE_array[2]= {0,0};

float E_teorico = 0.1;
float IE_teorico[2]= {0.1,0.1};

//Matrizes calculados offline matlab

const int N = 2;

mtx_type Y0[N][N] = {
  {3.1488 ,  -0.3766},
   {-0.3766 ,   1.9363}
//  {3.1066,   -0.1584},
//  {-0.1584,    0.1761}
};



mtx_type Y1[N][N] = {
 {0.0240,   -0.0000},
 {-0.0000,    0.0000}
};

mtx_type W0[N] = {5.5171  ,  2.0103};

mtx_type W1[N] = {0.1463 ,  -0.1650};


mtx_type Prho[N][N];  
mtx_type Wrho[1][N];  
mtx_type Krho[1][N];  

mtx_type x[N][1]; //estado estruturado

float rho; //Variável de agendamento
float tau; //contante de tempo MA
float Ke; //ganho MA


//variaveis da emulação

float Ts = 0.1; //Tempo de amostragem em segundos
int k = 0; //iteração
unsigned long previousMillis;
unsigned long currentMillis;
const int tsim = 290;      // tempo total da emulação em segundos
const int numReadings = tsim/Ts; //numero de leituras


//Funcao que atualiza valor da referência
void receberDados() {
  start = true; // libera execução do setup()
  byte buffer[4];
  for (int i=0; i<4 && Wire.available(); i++) {
    buffer[i] = Wire.read();
  }
  memcpy(&Y_ref, buffer, sizeof(float));
  if (Y_ref < 2.0){Y_ref = Y_ref + 2.0;}
}

void enviarDados() {
  byte buffer[4];
  memcpy(buffer, &Pot_wt, sizeof(float));
  Wire.write(buffer, 4);  // envia os 4 bytes de uma vez
}


void setup() {
  Wire.begin(ENDERECO); // escravo com endereço 8
  Wire.onRequest(enviarDados);
  Wire.onReceive(receberDados);

  Serial.begin(115200);
  while(!Serial); //Espera serial iniciar
    //////// inicialização do ESC////////////////
  while  (currentMillis-previousMillis<10000){
    currentMillis = millis();
    ESC.write(0);}

    // Espera o mestre mandar start
  while (!start) {
    // fica travado aqui até receber o comando
    Serial.println("Esperando mestre iniciar...");
  }

  ESC.attach(PinEsc, 1000, 2000); // (pin, min pulse width, max pulse width in microseconds) para o esc
  pinMode(PinTurbina, INPUT); //leitura de tensão gerada pela turbina eólica
  

  Serial.println("U, Y, Yfiltrado, ref, E, Uc"); //legenda Plotter Serial

  previousMillis = millis();
  currentMillis = millis();


    
  previousMillis = millis();
}



void loop() {

  currentMillis = millis();

 
//__________________________________________________________________________________
  
  if (currentMillis - previousMillis >= Ts*1000) //Repete a cada tempo de amostragem
  {
    previousMillis = currentMillis;

    //Calculo do agendamento rho
    
    rho = Y_ref; 
    
    // tau e Ke
    tau = 2.0323 + (-0.1478)*rho;
    Ke = 3.9278 + (-0.1815)*rho;
    

    //Lê tensão de saída da turbina e passa por filtro de ruido
    
    Y[1] = analogRead(PinTurbina) * (5.0 / 1023.0) * 43/10; // Converte a leitura em tensão da saída da turbina levando em consideração o divisor de tensão resistivo

///////// Saída simulada//////////////////////////////////
  //  float noise = random(-15, 10)/100.00;  // Ruído adicionado
  // if (k<100){Y[1]= 0;} //Simula atraso por inércia
  //  else {Y[1] = Y[0] - (Ts/tau) * Y[0] + (Ts/tau) * Ke * U;
  //  Y[1]+=noise;
  //  }
/////////////////////////////////////////////////////////

/////////////filtro passa-baixa 1a ordem/////////////////
    Y_lpf = Y[1];
/////////////////////////////////////////////////////////

//////////////filtro Kalman////////////////////////////////
   //Passo 1: Previsão do estado e covariância
    float x_pred = (1-Ts/tau) * x_est + (Ts/tau) * Ke * U;
    float P_pred = (1-Ts/tau) * P * (1-Ts/tau) + Q;

    //Passo 2: Cálculo do ganho de Kalman
    K = P_pred / (P_pred + R);

    //Passo 3: Atualização da estimativa com a medição
    x_est = x_pred + K * (Y_lpf - x_pred);
    P = (1 - K ) * P_pred;
  


    
     if (x_est<0){x_est=0;}
     Y_filtrado[1] = x_est;

    // if (Y[1] < 1){
    //   Y_filtrado[1] = 0;
    //   x_est=0;
    // }
////////////////////////////////////////////////////////

    Pot_wt = 0.5 * 1.225 * PI *pow(0.15, 2) * pow(Y_filtrado[1], 3)  * 0.35;
    // Serial.println(Pot_wt);
    // Serial.println(Y_filtrado[1]);

    //CALCULO DO ERRO E INTEGRAL DO ERRO

    E = Y_ref - Y_filtrado[1];
    // Serial.println(E);

    if (Uc[0][0]<1.0 || Uc[0][0]>5){IE_array[1] = IE_array[0];} //anti windup clamping (para de acumular IE quando satura)
    else
    {IE_array[1] = IE_array[0] + Ts * E;} 


    //Calculo do estado estruturado x(k) = [e(k) ie(k)]'
    x[0][0] = E;
    x[1][0] = IE_array[1];

    //////////////////////////////////////////////////////////////////////////////////////
    //////  Calculo dos ganhos LPV  ///////
    
    mtx_type temp[N][N];
    mtx_type temp2[N][N];

    // Calculando Prho = inv(Y0 + rho*Y1);
    Matrix.Copy((mtx_type*)Y1, N, N, (mtx_type*)temp); //temp = Y1
    Matrix.Scale((mtx_type*)temp, N, N, rho); //temp = rho*temp
    Matrix.Add((mtx_type*)Y0, (mtx_type*)temp, N, N, (mtx_type*)temp2); // temp2 = Y0 + rho*Y1
    
    Matrix.Copy((mtx_type*)temp2, N, N, (mtx_type*)Prho); // Prho = temp2
    Matrix.Invert((mtx_type*)Prho, N); //Prho = inv(Prho)

    // Calculando Wrho = W0 + rho*W1
    mtx_type tempW[1][N];
  
    Matrix.Copy((mtx_type*)W1, 1, N, (mtx_type*)tempW); //tempW = W1
    Matrix.Scale((mtx_type*)tempW, 1, N, rho); // tempW = rho*tempW
    Matrix.Add((mtx_type*)W0, (mtx_type*)tempW, 1, N, (mtx_type*)Wrho); // Wrho = W0+tempW 

    // Calculando Krho = Wrho * Prho
    Matrix.Multiply((mtx_type*)Wrho, (mtx_type*)Prho, 1, N, N, (mtx_type*)Krho); // Wrho*Prho

    
    Krho[0][0] = Krho[0][0]*0.5;
    Krho[0][1] = Krho[0][1]*0.5;
    
    // Ação de controle PI LPV
    //Uc = Krho*x(k);
    Matrix.Multiply((mtx_type*)Krho, (mtx_type*)x, 1, N, N, (mtx_type*)Uc); // Uc = Krho*x

    

    //////////////////////////////////////////////////////////////////////////////////////
    
    // Saturação da ação de controle 0-5V
 
//    else {
    U = Uc[0][0];
    if (U < 1.0){if (Y_ref != 0){U = 1.0;}else{U=0;}}
      
    if (U > 5){U = 5;}

    if (k<60){
      U = u_init[k]/38;
      IE_array[1] = 0;
      x[1][0] = 0;
    } //Inicialização do motor

    if (Y_ref == 0){U = 0;} //necessário para inicialização do ESC

    
    ESC.write(U*38); // Envia o sinal de controle para o ESC

    // faz print dos dados
    //Serial.print(Uc[0][0]);
    //Serial.print(",");
    Serial.print(U);
    Serial.print(",");
    // Serial.print(Y[1]);
    // Serial.print(",");
    Serial.print(Y_filtrado[1]);
    Serial.print(",");
    //Serial.print(Y[1]);
    //Serial.print(",");
    Serial.println(Y_ref);
    //Serial.print(",");
    // Serial.print(E);
    // Serial.print(",");
    // Serial.println(Uc[0][0]);
    
    

    //Atualização dos arrays
    
    for (int i = 0; i<1; i++){
      IE_array[i] = IE_array[i+1];
      IE_teorico[i] = IE_teorico[i+1];
      Y_teorico[i] = Y_teorico[i+1];
      Y_filtrado[i] = Y_filtrado[i+1];
      Y[i] = Y[i+1];
    }



    k++;// atualiza iteração

  // if (k > numReadings-1) {
  //   ESC.write(0);
  //   while(1);} //se chegou ao fim da emulação, fica no loop onde faz nada
  
  }
}
