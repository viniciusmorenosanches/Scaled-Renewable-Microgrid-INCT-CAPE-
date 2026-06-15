/**
 * firmware_hidrogenio_uno.ino — Periferico de hidrogenio/eletrolisador
 *
 * Papel desta placa (Arduino Uno):
 *  - Receber do Arduino R4 central os comandos ucc (celula combustivel)
 *    e uel (eletrolisador) via SerialTransfer.
 *  - Acionar a valvula/servo da celula combustivel.
 *  - Acionar o rele do eletrolisador.
 *  - Medir o nivel de H2 no reservatorio e a potencia da celula combustivel.
 *  - Responder ao R4 com Pot_cc e Level_H2.
 *
 * Protocolo SerialTransfer:
 *  - R4  -> UNO: pacote ID_R4_TO_UNO com R4ToUno_Data  { ucc, uel }
 *  - UNO -> R4:  pacote ID_UNO_TO_R4 com UnoToR4_Data  { Pot_cc, Level_H2 }
 *
 * Ver docs/comunicacao.md para o protocolo completo.
 * Ver docs/calibracao.md para explicacao das constantes fisicas.
 */

#include <Servo.h>
#include <Wire.h>

#include "comunicacao.h"
#include "pacotes.h"
#include "sensor_nivel_h2.h"

// ---------------------------------------------------------------------------
// Configuracao geral
// ---------------------------------------------------------------------------

const unsigned long BAUD_RATE           = 115200;
const float         TS_SEGUNDOS         = 0.1f;
const unsigned long PERIODO_CONTROLE_MS = 100;

// Debug serial (ligar apenas para desenvolvimento; desligar em operacao normal)
const bool DEBUG_SERIAL = false;

// ---------------------------------------------------------------------------
// Pinos dos atuadores
// ---------------------------------------------------------------------------

const int PINO_RELE_ELETROLISADOR = 5;  // Rele do eletrolisador (ativo em nivel BAIXO)
const int PINO_SERVO_CELULA       = 3;  // Servo da valvula da celula combustivel
const int ANGULO_CELULA_FECHADA   = 180; // Posicao do servo com valvula fechada [graus]
const int ANGULO_CELULA_ABERTA    = 0;   // Posicao do servo com valvula aberta [graus]

// ---------------------------------------------------------------------------
// Medicao da celula combustivel
//
// A tensao e medida sobre um resistor fisico ligado na saida da celula.
// O resistor foi medido em bancada: RESISTENCIA_CARGA_OHM = 11.7 ohms.
//
// Calculo de potencia:
//   I = V / R     (lei de Ohm)
//   P = V * I = V^2 / R
//
// Para recalibrar: medir o resistor com multimetro e atualizar a constante.
// ---------------------------------------------------------------------------

const int   PINO_TENSAO_CELULA     = A0;     // Entrada analogica da tensao da celula
const float RESISTENCIA_CARGA_OHM  = 11.7f;  // Resistor medido em bancada [ohm]
const float TENSAO_ADC_V           = 5.0f;
const float ADC_MAX                = 1023.0f;

// ---------------------------------------------------------------------------
// Comunicacao SerialTransfer
// ---------------------------------------------------------------------------

#define SerialMestre Serial  // Porta serial conectada ao R4 (via cabo USB/UART)

SerialTransfer Mestre;
Servo          servoCelula;

UnoToR4_Data dadosParaR4;  // Telemetria a enviar ao R4 (Pot_cc, Level_H2)
R4ToUno_Data dadosDoR4;    // Comandos recebidos do R4 (ucc, uel)

// ---------------------------------------------------------------------------
// Estado do controle
// ---------------------------------------------------------------------------

uint8_t comandoCelula        = 0;  // ucc: 1 = abre a celula combustivel
uint8_t comandoEletrolisador = 0;  // uel: 1 = liga o eletrolisador

float nivelH2_mm             = 0.0f;
float ultimoNivelH2Valido_mm = 0.0f;  // Cache do ultimo valor valido (evita enviar -1.0 ao R4)
float potenciaCelula_W       = 0.0f;

unsigned long ultimoControle_ms = 0;

// ---------------------------------------------------------------------------
// Atuadores
// ---------------------------------------------------------------------------

/**
 * Configura pinos e estado inicial dos atuadores.
 * Eletrolisador desligado e valvula fechada por seguranca.
 */
void configurarAtuadores() {
    pinMode(PINO_RELE_ELETROLISADOR, OUTPUT);

    // Rele ativo em nivel baixo: HIGH = desligado (estado seguro inicial)
    digitalWrite(PINO_RELE_ELETROLISADOR, HIGH);

    // Configura o LED embutido para feedback visual do controle
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    servoCelula.attach(PINO_SERVO_CELULA);
    servoCelula.write(ANGULO_CELULA_FECHADA);
}

/**
 * Atualiza os atuadores somente quando o comando muda.
 * Evita comandos redundantes ao servo e ao rele a cada 100 ms.
 */
void atualizarAtuadores(uint8_t novoComandoCelula, uint8_t novoComandoEletrolisador) {
    static int ultimoComandoCelula        = -1;
    static int ultimoComandoEletrolisador = -1;

    if (novoComandoCelula != ultimoComandoCelula) {
        if (novoComandoCelula == 1) {
            servoCelula.write(ANGULO_CELULA_ABERTA);
        } else {
            servoCelula.write(ANGULO_CELULA_FECHADA);
        }
        ultimoComandoCelula = novoComandoCelula;
    }

    if (novoComandoEletrolisador != ultimoComandoEletrolisador) {
        // Rele ativo em nivel baixo: LOW = ligado, HIGH = desligado
        digitalWrite(PINO_RELE_ELETROLISADOR,
                     novoComandoEletrolisador == 1 ? LOW : HIGH);
        
        // LED embutido acende junto com o eletrolisador para confirmacao visual
        digitalWrite(LED_BUILTIN,
                     novoComandoEletrolisador == 1 ? HIGH : LOW);
                     
        ultimoComandoEletrolisador = novoComandoEletrolisador;
    }
}

// ---------------------------------------------------------------------------
// Medicoes
// ---------------------------------------------------------------------------

/**
 * Le a potencia da celula combustivel estimada pelo resistor shunt.
 * P = V^2 / R, onde V e a tensao medida no pino ADC.
 */
float lerPotenciaCelula_W() {
    int   leituraAdc = analogRead(PINO_TENSAO_CELULA);
    float tensao_V   = leituraAdc * (TENSAO_ADC_V / ADC_MAX);
    float corrente_A = tensao_V / RESISTENCIA_CARGA_OHM;
    return tensao_V * corrente_A;
}

/**
 * Atualiza as medicoes de nivel de H2 e potencia da celula.
 * Em caso de falha persistente do sensor, mantem o ultimo valor valido
 * de nivel de H2 (evita enviar -1.0 ao R4 como se fosse um nivel real).
 */
void atualizarMedicoes() {
    float leituraNivel_mm = lerNivelH2_mm();

    if (leituraNivel_mm >= 0.0f) {
        nivelH2_mm             = leituraNivel_mm;
        ultimoNivelH2Valido_mm = leituraNivel_mm;
    } else {
        // Falha persistente: usa o ultimo valor valido
        nivelH2_mm = ultimoNivelH2Valido_mm;
    }

    potenciaCelula_W = lerPotenciaCelula_W();
}

// ---------------------------------------------------------------------------
// Comunicacao com o R4
// ---------------------------------------------------------------------------

/**
 * Verifica se ha um pacote disponivel do R4.
 * Se sim, le os comandos e responde com a telemetria atual.
 * Esta funcao deve ser chamada a cada iteracao do loop() (nao-bloqueante).
 */
void receberComandoDoR4() {
    if (!Mestre.available()) {
        return;
    }

    if (Mestre.currentPacketID() != ID_R4_TO_UNO) {
        return;
    }

    receberDados(Mestre, &dadosDoR4);

    // Limiar robusto (0.5f) para evitar erros de truncamento de ponto flutuante
    comandoCelula        = (dadosDoR4.ucc > 0.5f) ? 1 : 0;
    comandoEletrolisador = (dadosDoR4.uel > 0.5f) ? 1 : 0;

    // Responde imediatamente com a telemetria mais recente
    dadosParaR4.Pot_cc   = potenciaCelula_W;
    dadosParaR4.Level_H2 = nivelH2_mm;
    enviarDados(Mestre, ID_UNO_TO_R4, &dadosParaR4, sizeof(dadosParaR4));
}

// ---------------------------------------------------------------------------
// Debug
// ---------------------------------------------------------------------------

void imprimirDebug() {
    if (!DEBUG_SERIAL) return;

    Serial.print("ucc=");           Serial.print(comandoCelula);
    Serial.print(", uel=");         Serial.print(comandoEletrolisador);
    Serial.print(", Level_H2_mm="); Serial.print(nivelH2_mm);
    Serial.print(", Pot_cc_W=");    Serial.println(potenciaCelula_W);
}

// ---------------------------------------------------------------------------
// Arduino
// ---------------------------------------------------------------------------

void setup() {
    Wire.begin();

    SerialMestre.begin(BAUD_RATE);
    Mestre.begin(SerialMestre);

    configurarAtuadores();
    inicializarSensorNivelH2();

    ultimoControle_ms = millis();
}

void loop() {
    // Recepcao de comandos do R4 (nao-bloqueante, responde imediatamente)
    receberComandoDoR4();

    // Bloco de controle periodico a cada Ts = 100 ms
    unsigned long agora_ms = millis();
    if (agora_ms - ultimoControle_ms < PERIODO_CONTROLE_MS) {
        return;
    }
    ultimoControle_ms = agora_ms;

    atualizarMedicoes();
    atualizarAtuadores(comandoCelula, comandoEletrolisador);
    imprimirDebug();
}
