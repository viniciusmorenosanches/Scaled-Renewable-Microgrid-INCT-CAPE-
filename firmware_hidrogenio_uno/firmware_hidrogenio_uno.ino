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
#include <Adafruit_INA219.h>

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
// Medicao da celula combustivel (Sensor digital INA219 via I2C)
//
// O INA219 (endereco padrao 0x40) mede a tensao de barramento (V_bus) e a
// queda no shunt de 0.1 ohm (I_shunt) por sensoriamento high-side diferencial.
//
// Calculo de potencia real:
//   P = V_bus * I_shunt [W]
//
// Funciona com qualquer carga dinamica conectada a celula (LED, ventilador, etc).
// ---------------------------------------------------------------------------

Adafruit_INA219 ina219;
bool ina219Disponivel = false;

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
float tensaoCelula_V         = 0.0f;  // Tensao de barramento da celula [V]
float correnteCelula_A       = 0.0f;  // Corrente entregue pela celula [A]
float potenciaCelula_W       = 0.0f;  // Potencia eletrica real entregue [W]

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
 * Le a tensao, corrente e potencia da celula combustivel via sensor digital INA219.
 * P = V_bus * I_shunt [W].
 */
float lerPotenciaCelula_W() {
    if (!ina219Disponivel) {
        return 0.0f;
    }

    float busVoltage_V = ina219.getBusVoltage_V();
    float current_mA   = ina219.getCurrent_mA();

    // Filtra offset negativo de leitura quando a carga esta desligada/circuito aberto
    if (current_mA < 0.0f)   current_mA   = 0.0f;
    if (busVoltage_V < 0.0f) busVoltage_V = 0.0f;

    tensaoCelula_V   = busVoltage_V;
    correnteCelula_A = current_mA / 1000.0f;

    return tensaoCelula_V * correnteCelula_A;
}

/**
 * Atualiza as medicoes de nivel de H2 e potencia da celula.
 * Em caso de falha persistente do sensor, mantem o ultimo valor valido
 * de nivel de H2 (evita enviar -1.0 ao R4 como se fosse um nivel real).
 */
void atualizarMedicoes() {
    float leituraNivel_mm = lerNivelH2_mm();

    if (leituraNivel_mm > -500.0f) {
        nivelH2_mm             = leituraNivel_mm;
        ultimoNivelH2Valido_mm = leituraNivel_mm;
    } else {
        // Falha persistente (retornou -999.0f): usa o ultimo valor valido
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
    Serial.print(", V_cc=");        Serial.print(tensaoCelula_V, 3);
    Serial.print("V, I_cc=");       Serial.print(correnteCelula_A * 1000.0f, 1);
    Serial.print("mA, Pot_cc_W=");  Serial.println(potenciaCelula_W, 4);
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

    // Inicializa o sensor de tensao/corrente INA219 (endereco padrao 0x40)
    if (ina219.begin()) {
        ina219.setCalibration_32V_2A();
        ina219Disponivel = true;
    }

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
