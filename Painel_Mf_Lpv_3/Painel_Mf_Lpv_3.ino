#include <MatrixMath.h>
#include <Wire.h>

#define ENDERECO 9


const int ledPin = 9;
const int solarPin = A2;

// Simulação da planta
float y = 0;  // Saída simulada
float y_ant = 0;
int valorLido = 0;
float tensaoFiltrada = 1;
float tensaoFiltrada_ant = 0;
float tensao = 0;
float U = 0;
float Ts = 0.1;
volatile float ref = 0; // global
volatile float Pot_pv = 0; // global
float tau0 = 1.4627;
float tau1 = 0.02402;
float Ke0  = 0.3624;
float Ke1  = -0.05229;
float tensao_atual = 0;
const float alpha = 0.05;

float rho = 0;
float tau = 0;
float Ke = 0;
float erro = 1;
float I_erro[2] = {0, 0};

const int N = 2;
mtx_type x[N][1];
mtx_type Uc[1][1];

// Matrizes LPV
mtx_type Y0[N][N] = {
  {1.1441 ,  -0.8255},
  {-0.8255  ,  3.0636}
};

mtx_type Y1[N][N] = {
  {0.0310, 0},
  {0, 0}
};

mtx_type W0[1][N] = {3.2131 ,   2.2778};
mtx_type W1[1][N] = {0.1044  ,  0.4965};

mtx_type Prho[N][N];
mtx_type Wrho[1][N];
mtx_type Krho[1][N];

unsigned long previousMillis = 0;
unsigned long currentMillis = 0;
int k = 0;

//Funcao que atualiza valor da referência
void receberDados(int howMany) {
  if (howMany == 4) {
    byte buffer[4];
    for (int i=0; i<4; i++) {
      buffer[i] = Wire.read();
    }
    float tempRef;
    memcpy(&tempRef, buffer, sizeof(float));
    ref = tempRef;
  } else {
    // descarta dados se tamanho for inesperado
    while (Wire.available()) {
      Wire.read();
    }
  }
}

void enviarDados() {
  float tempPot = Pot_pv;
  byte buffer[4];
  memcpy(buffer, &tempPot, sizeof(float));
  Wire.write(buffer, 4);  // envia os 4 bytes de uma vez
}


void setup() {
  Wire.begin(ENDERECO); // escravo com endereço 9
  Wire.onRequest(enviarDados);
  Wire.onReceive(receberDados);

  pinMode(ledPin, OUTPUT);
  previousMillis = millis();
  
  valorLido = analogRead(solarPin);
  tensaoFiltrada_ant = (valorLido * 5.0) / 1023.0;

  Serial.begin(115200);
  // while(!Serial); //Espera serial iniciar
}

void loop() {
  currentMillis = millis();
  if (currentMillis - previousMillis >= Ts * 1000) {
    previousMillis = currentMillis;
    
    // Leitura atômica da referência para evitar race condition com a ISR de I2C
    noInterrupts();
    float local_ref = ref;
    interrupts();

    // Atualiza parâmetros da planta
    k = k+1;
    tau = tau0 + tau1 * rho;
    Ke  = Ke0 + Ke1 * rho;

    rho = 1*local_ref;

    valorLido = analogRead(solarPin);
    tensao = (valorLido * 5.0) / 1023.0;

    tensaoFiltrada = alpha * tensao + (1 - alpha) * tensaoFiltrada_ant;    

    erro = local_ref - (tensaoFiltrada);
    //erro = local_ref - y;

      if (Uc[0][0]<0.0 || Uc[0][0]>5){I_erro[1] = I_erro[0];} //anti windup clamping (para de acumular IE quando satura)
      else
      {I_erro[1] = I_erro[0] + Ts * erro;} 

    // Atualiza estado do controlador
    x[0][0] = erro;
    x[1][0] = I_erro[1];

    // LPV: Krho = (W0 + rho*W1)*(Y0 + rho*Y1)^-1
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


    // Uc = Krho * x
    Matrix.Multiply((mtx_type*)Krho, (mtx_type*)x, 1, N, N, (mtx_type*)Uc);

    
    // Saturação
    U = Uc[0][0];
    if (U > 5) U = 5;
    if (U < 0) U = 0;

    // Simula planta: y(k+1) = y(k) - (Ts/tau)*y(k) + (Ts/tau)*Ke*u(k)
    y = y_ant - (Ts / tau) * y_ant + (Ts / tau) * Ke * U;

    // Atualiza integral
    I_erro[0] = I_erro[1];

    // PWM de saída (opcional)
    analogWrite(ledPin, (int)(U / 5.0 * 255));

    Pot_pv = 150*tensaoFiltrada;

    //Print para análise
    Serial.print(Uc[0][0]);
    Serial.print(", ");
    Serial.print(U);
    Serial.print(", ");
    Serial.print(erro);
    Serial.print(", ");
    Serial.print(local_ref, 5);
    Serial.print(", ");
    Serial.print(tensao);
    Serial.print(", ");
    Serial.println(tensaoFiltrada);

    y_ant = y;
    tensaoFiltrada_ant = tensaoFiltrada;

    }
  }
