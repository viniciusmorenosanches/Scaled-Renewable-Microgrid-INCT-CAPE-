/**
 * debug_servo_h2.ino — Código simples para testar e calibrar o servo da válvula da célula de combustível.
 *
 * Coloque este arquivo na pasta Testes/debug_servo_h2/.
 * Carregue no Arduino UNO do H2 (com o servo conectado no pino 5).
 *
 * Funcionamento:
 *   1. Permite testar ângulos específicos via Monitor Serial (envie um número de 0 a 180).
 *   2. Permite rodar uma varredura automática (sweep) digitando 's'.
 *   3. Mostra dicas sobre alimentação e conexões físicas caso o servo não responda.
 */

#include <Servo.h>

const int PINO_SERVO = 3;  // Pino físico do servo (D5)
Servo servoTeste;

int anguloAtual = 180;     // Estado seguro inicial (válvula fechada)
bool modoVarredura = false; // Se true, o servo fica girando de 0 a 180 continuamente

void setup() {
  Serial.begin(9600);
  
  // Inicializa o servo no pino 5
  servoTeste.attach(PINO_SERVO);
  servoTeste.write(anguloAtual);

  Serial.println("\n==================================================");
  Serial.println("   DEBUG DO SERVO MOTOR — SISTEMA DE HIDROGÊNIO   ");
  Serial.println("==================================================");
  Serial.println("Envie comandos pelo Monitor Serial (com terminação Nova Linha/NL):");
  Serial.println("  - Um número entre 0 e 180 (ex: '90', '0', '180') para mover para o ângulo.");
  Serial.println("  - 's' para iniciar/parar a varredura automática (sweep).");
  Serial.println("  - 'info' para ver instruções de ligação física e problemas comuns.");
  Serial.println("--------------------------------------------------");
  Serial.print("Servo anexado no pino: ");
  Serial.println(PINO_SERVO);
  Serial.print("Ângulo inicial definido: ");
  Serial.println(anguloAtual);
}

void printDicasDebug() {
  Serial.println("\n--- DICAS PARA DIAGNÓSTICO DO SERVO ---");
  Serial.println("1. Alimentação Externa (Crítico):");
  Serial.println("   Servos consomem picos de corrente de 500mA a 1A. Se alimentado diretamente");
  Serial.println("   pelo pino 5V do Arduino (via USB), ele pode não se mover ou resetar a placa.");
  Serial.println("   Recomenda-se alimentar o servo com fonte externa de 5V ou 6V (garantindo que");
  Serial.println("   o GND da fonte externa esteja conectado ao GND do Arduino!).");
  Serial.println("2. Conexão Física dos Cabos:");
  Serial.println("   - Fio Escuro (Preto ou Marrom)  -> GND");
  Serial.println("   - Fio Vermelho                  -> 5V (ou VCC da fonte externa)");
  Serial.println("   - Fio Claro (Amarelo ou Laranja) -> Pino de Sinal (D5)");
  Serial.println("3. Ruído mecânico:");
  Serial.println("   Se o servo tremer ou fizer barulho mas não girar, pode ser falta de corrente");
  Serial.println("   na alimentação ou o ângulo enviado está tentando ir além do limite físico do servo.");
  Serial.println("--------------------------------------\n");
}

void loop() {
  // --- 1. Processa entrada serial do usuário ---
  
 

    // Roda de 0 a 180
    for (int pos = 0; pos <= 180; pos += 15) {
      if (Serial.available() > 0) break; // Interrompe se o usuário mandar comando
      servoTeste.write(pos);
      Serial.print("Sweep: "); Serial.print(pos); Serial.println("°");
      delay(300);
    }
    // Roda de 180 a 0
    for (int pos = 180; pos >= 0; pos -= 15) {
      if (Serial.available() > 0) break; // Interrompe se o usuário mandar comando
      servoTeste.write(pos);
      Serial.print("Sweep: "); Serial.print(pos); Serial.println("°");
      delay(300);
    }
  

  delay(50);
}
