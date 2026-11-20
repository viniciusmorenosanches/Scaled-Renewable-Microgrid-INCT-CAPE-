/**
 * debug_rele_h2.ino — Código simples para testar o relé do eletrolisador (agora no pino 5).
 *
 * Coloque este arquivo na pasta Testes/debug_rele_h2/.
 * Carregue no Arduino UNO do H2 (com o relé do eletrolisador conectado no pino 5).
 *
 * Funcionamento:
 *   1. Permite ligar/desligar o relé via Monitor Serial (envie '1' ou 'on' para ligar, '0' ou 'off' para desligar).
 *   2. Permite alternar o estado digitando 't'.
 *   3. Lembre-se: O relé é ativo em nível BAIXO (LOW = Ligado, HIGH = Desligado).
 */

const int PINO_RELE = 5;  // Novo pino do relé
bool estadoRele = false;  // false = Desligado (HIGH), true = Ligado (LOW)

void setup() {
  Serial.begin(9600);
  
  // Configura o pino como saída
  pinMode(PINO_RELE, OUTPUT);
  
  // Inicia desligado (HIGH) por segurança
  digitalWrite(PINO_RELE, HIGH);

  Serial.println("\n==================================================");
  Serial.println("    DEBUG DO RELÉ DO ELETROLISADOR — SISTEMA H2    ");
  Serial.println("==================================================");
  Serial.println("Envie comandos pelo Monitor Serial (com terminação Nova Linha/NL):");
  Serial.println("  - '1' ou 'on'  -> Liga o eletrolisador (Escreve LOW no pino)");
  Serial.println("  - '0' or 'off' -> Desliga o eletrolisador (Escreve HIGH no pino)");
  Serial.println("  - 't'          -> Alterna (toggle) o estado atual");
  Serial.println("  - 'status'     -> Mostra o estado atual do pino");
  Serial.println("--------------------------------------------------");
  Serial.print("Relé configurado no pino: ");
  Serial.println(PINO_RELE);
  Serial.println("Estado inicial: DESLIGADO (Pino em HIGH)");
}

void atualizarRelogioRele() {
  if (estadoRele) {
    digitalWrite(PINO_RELE, LOW); // Ativo em nível baixo
    Serial.println("Relé LIGADO (Pino físico = LOW)");
  } else {
    digitalWrite(PINO_RELE, HIGH);
    Serial.println("Relé DESLIGADO (Pino físico = HIGH)");
  }
}

void loop() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    if (input.equalsIgnoreCase("1") || input.equalsIgnoreCase("on")) {
      estadoRele = true;
      atualizarRelogioRele();
    }
    else if (input.equalsIgnoreCase("0") || input.equalsIgnoreCase("off")) {
      estadoRele = false;
      atualizarRelogioRele();
    }
    else if (input.equalsIgnoreCase("t")) {
      estadoRele = !estadoRele;
      Serial.print("Toggling... ");
      atualizarRelogioRele();
    }
    else if (input.equalsIgnoreCase("status")) {
      Serial.print("Relé está atualmente: ");
      Serial.println(estadoRele ? "LIGADO (LOW)" : "DESLIGADO (HIGH)");
      Serial.print("Leitura digital do pino: ");
      Serial.println(digitalRead(PINO_RELE) == LOW ? "LOW" : "HIGH");
    }
    else {
      Serial.print("Comando inválido: '");
      Serial.print(input);
      Serial.println("' (Envie 1, 0, t ou status).");
    }
  }
  delay(50);
}
