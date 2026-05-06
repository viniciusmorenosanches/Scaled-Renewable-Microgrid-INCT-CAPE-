#include <Wire.h> //para comunicacao I2C entre sensor de nível e ino
#include <Servo.h> //Biblioteca servo para servo motor
#include <SoftwareSerial.h> //para escolher pinos de comunicação serial entre os arduinos
#include "sensor_logic.h" // Inclui nosso novo arquivo com a lógica do sensor!
#include "comunicacao.h" // Inclui funcoes de comunicacao serial entre arduinos
#include "pacotes.h" //Inclui estruturas dos pacotes a enviar e a receber da comunicação serial

// Definições de pinos e ângulos
#define PINO_RELE 4
#define PINO_SERVO 9
#define ANGULO_FECHADO 180
#define ANGULO_ABERTO 0

Servo meuServo;
//SoftwareSerial SerialMestre(10, 11); // SoftwareSerial é usado para 
#define SerialMestre Serial
SerialTransfer Mestre;

UnoToR4_Data DadosEnviar; 
R4ToUno_Data DadosRecebidos;

//variáveis de controle
int  ucc = 0;
int  uel = 0;

float Ts = 0.1;   // período de controle (s)
unsigned long tempo_ultimo_controle;

float Level_H2 = 0;
float Pot_cc = 0;

// ------------------------------------------------------------------

void controle(int &ucc, int &uel) {
  static int lastUcc = -1;
  static int lastUel = -1;

  if (ucc != lastUcc) {
    if (ucc == 1) meuServo.write(ANGULO_ABERTO);
    else meuServo.write(ANGULO_FECHADO);
    lastUcc = ucc;
  }

  if (uel != lastUel) {
    if (uel == 1) digitalWrite(PINO_RELE, LOW);
    else digitalWrite(PINO_RELE, HIGH);
    lastUel = uel;
  }
}

// ------------------------------------------------------------------

void setup() {
  
  Wire.begin();
  //Serial.begin(115200);
  SerialMestre.begin(9600);
  Mestre.begin(SerialMestre);

  // Configuração dos atuadores
  pinMode(PINO_RELE, OUTPUT);
  digitalWrite(PINO_RELE, HIGH);
  meuServo.attach(PINO_SERVO);
  //meuServo.write(ANGULO_FECHADO);

  // Inicializa o sensor chamando a função do nosso arquivo .h
  setupSensor(); 
  tempo_ultimo_controle = millis();
  //Serial.println("Arduino pronto!");

}

void loop() {

  if(Mestre.available()) {

    if(Mestre.currentPacketID() == ID_R4_TO_UNO) {
      // SUCESSO. Pacote 1 (Comando do Mestre) chegou.
      
      receberDados(Mestre, &DadosRecebidos);
      ucc = DadosRecebidos.ucc;
      uel = DadosRecebidos.uel;

      // Serial.println("\nUNO (Tarefa A): === COMANDO RECEBIDO DO MESTRE (ID 1) ===");
      // Serial.print("   > Valor de ucc: "); Serial.println(ucc);
      // Serial.print("   > Valor de uel: "); Serial.println(uel);

      // // Agora, nós NÃO calculamos nada. A Tarefa B já fez isso.
      // // Apenas pegamos a struct dataTX (que está sempre fresca) e enviamos.
      // Serial.print("UNO (Tarefa A): Enviando resposta (Pacote ID 0)...");
      // Serial.print(" Level_H2: "); Serial.println(DadosEnviar.Level_H2);
      DadosEnviar.Pot_cc = Pot_cc;
      DadosEnviar.Level_H2 = Level_H2;
      //Serial.println(DadosEnviar.Level_H2);
      
      enviarDados(Mestre, ID_UNO_TO_R4, &DadosEnviar, sizeof(DadosEnviar));
        
    } 
    // else {
    //   // Pacote válido, ID errado.
    //   Serial.print("UNO (Tarefa A): Recebido pacote com ID inesperado: ");
    //   Serial.println(Mestre.currentPacketID());
    // }
    
  } 
  // else if (Mestre.status < 0) {
  //   // Erro de pacote.
  //   Serial.print("UNO (Tarefa A): Erro de transferencia: ");
  //   Serial.println(Mestre.status);
  // }




  //Loop de atualização das variaveis
  if (millis() - tempo_ultimo_controle >= Ts*1000) {
    tempo_ultimo_controle = millis();

    // 1. Lê o nível da água usando nossa função organizada
    Level_H2 = readWaterLevel();
    Pot_cc = 154.57;
    controle(ucc, uel);

  }
}