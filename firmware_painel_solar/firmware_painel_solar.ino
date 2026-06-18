/**
 * firmware_painel_solar.ino — Periferico do painel solar da microrrede
 *
 * Papel desta placa (Arduino Uno):
 *  - Receber do Arduino R4 central uma referencia de tensao via I2C.
 *  - Controlar a saida PWM que emula o painel solar.
 *  - Medir a tensao no pino analogico.
 *  - Devolver ao R4 a potencia estimada do painel (potenciaSolar_W).
 *
 * Contrato I2C:
 *  - Endereco: 9
 *  - Entrada: 1 float com a referencia do painel [V]
 *  - Saida:   1 float com a potencia estimada [W]
 *
 * Ver docs/comunicacao.md para o protocolo completo.
 * Ver docs/calibracao.md para explicacao das constantes fisicas.
 */

#include <MatrixMath.h>
#include <Wire.h>

// ---------------------------------------------------------------------------
// Configuracao de hardware
// ---------------------------------------------------------------------------

const byte ENDERECO_I2C    = 9;   // Endereco I2C desta placa
const int  PINO_PWM_PAINEL = 9;   // Saida PWM que controla o conversor/painel
const int  PINO_SENSOR_SOLAR = A2; // Entrada analogica da tensao medida

// ---------------------------------------------------------------------------
// Parametros de tempo
// ---------------------------------------------------------------------------

const float         TS_SEGUNDOS         = 0.1f;
const unsigned long PERIODO_CONTROLE_MS = 100;

// ---------------------------------------------------------------------------
// Conversao ADC
// ---------------------------------------------------------------------------

const float TENSAO_ADC_V = 5.0f;
const float ADC_MAX      = 1023.0f;

// Filtro exponencial da tensao medida. Menor ALPHA = mais suavizacao.
const float ALPHA_FILTRO_TENSAO = 0.05f;

// ---------------------------------------------------------------------------
// Escala de potencia (calibracao temporaria)
//
// A referencia do perfil solar ainda nao esta calibrada para produzir
// diretamente a potencia fisica. Por isso, a potencia enviada ao R4 e:
//
//   potenciaSolar_W = ESCALA_POTENCIA_SOLAR * tensaoFiltrada_V
//
// Quando o perfil de referencia for melhorado, substituir esta relacao
// por uma calibracao fisica mais direta (ex: curva I-V do painel real).
// ---------------------------------------------------------------------------

const float ESCALA_POTENCIA_SOLAR = 150.0f;  // [W/V] — escala experimental

// ---------------------------------------------------------------------------
// Debug serial (ligar apenas para desenvolvimento; desligar em operacao)
// ---------------------------------------------------------------------------

const bool DEBUG_SERIAL = false;

// ---------------------------------------------------------------------------
// Parametros do controlador LPV (calculados no MATLAB com synthesis LPV)
//
// O controlador e do tipo PI com ganho variante no ponto de operacao (rho).
// rho = referencia atual do painel [V]
// tau e Ke sao parametros do modelo do painel que variam com rho.
// ---------------------------------------------------------------------------

const int ORDEM_CONTROLADOR = 2;

const float TAU_0 = 1.4627f;
const float TAU_1 = 0.02402f;
const float KE_0  = 0.3624f;
const float KE_1  = -0.05229f;

mtx_type Y0[ORDEM_CONTROLADOR][ORDEM_CONTROLADOR] = {
    { 1.1441f, -0.8255f },
    { -0.8255f, 3.0636f }
};
mtx_type Y1[ORDEM_CONTROLADOR][ORDEM_CONTROLADOR] = {
    { 0.0310f, 0.0f },
    { 0.0f, 0.0f }
};
mtx_type W0[1][ORDEM_CONTROLADOR] = { 3.2131f, 2.2778f };
mtx_type W1[1][ORDEM_CONTROLADOR] = { 0.1044f, 0.4965f };

mtx_type matrizP[ORDEM_CONTROLADOR][ORDEM_CONTROLADOR];
mtx_type matrizW[1][ORDEM_CONTROLADOR];
mtx_type ganhoK[1][ORDEM_CONTROLADOR];
mtx_type estadoControlador[ORDEM_CONTROLADOR][1];
mtx_type controleNaoSaturado[1][1];

// ---------------------------------------------------------------------------
// Estado compartilhado com a ISR do I2C
// volatile: impede otimizacao do compilador em variaveis acessadas por ISR
// ---------------------------------------------------------------------------

volatile float referenciaPainel_V = 0.0f;
volatile float potenciaSolar_W    = 0.0f;

// ---------------------------------------------------------------------------
// Estado interno do controle
// ---------------------------------------------------------------------------

float tensaoMedida_V         = 0.0f;
float tensaoFiltrada_V       = 1.0f;
float tensaoFiltradaAnterior_V = 0.0f;

float saidaModelo            = 0.0f;
float saidaModeloAnterior    = 0.0f;
float erroControle           = 1.0f;
float integralErro[2]        = { 0.0f, 0.0f };

float rho              = 0.0f;
float tau              = 0.0f;
float Ke               = 0.0f;
float controleSaturado_V = 0.0f;

unsigned long ultimoControle_ms = 0;

// ---------------------------------------------------------------------------
// Comunicacao I2C
// ---------------------------------------------------------------------------

/**
 * ISR chamada quando o R4 envia a referencia via I2C.
 * Le os 4 bytes e converte para float.
 * Descarta pacotes com tamanho inesperado para nao desalinhar a proxima leitura.
 */
void receberReferenciaI2C(int quantidadeBytes) {
    if (quantidadeBytes == sizeof(float)) {
        byte buffer[sizeof(float)];
        for (int i = 0; i < sizeof(float); i++) {
            buffer[i] = Wire.read();
        }
        float novaReferencia = 0.0f;
        memcpy(&novaReferencia, buffer, sizeof(float));
        referenciaPainel_V = novaReferencia;
        return;
    }

    while (Wire.available()) {
        Wire.read();
    }
}

/**
 * ISR chamada quando o R4 solicita a potencia via I2C.
 * Envia os 4 bytes do float potenciaSolar_W.
 */
void enviarPotenciaI2C() {
    float potenciaLocal_W = potenciaSolar_W;
    byte  buffer[sizeof(float)];
    memcpy(buffer, &potenciaLocal_W, sizeof(float));
    Wire.write(buffer, sizeof(float));
}

/**
 * Le a referencia com interrupcoes desabilitadas para garantir leitura atomica.
 * Necessario porque referenciaPainel_V e um float de 4 bytes e pode ser
 * alterado pela ISR no meio de uma leitura no loop().
 */
float lerReferenciaAtomica() {
    noInterrupts();
    float referenciaLocal_V = referenciaPainel_V;
    interrupts();
    return referenciaLocal_V;
}

// ---------------------------------------------------------------------------
// Leitura, filtro e calculo de potencia
// ---------------------------------------------------------------------------

float lerTensaoSolar() {
    int valorAdc = analogRead(PINO_SENSOR_SOLAR);
    return valorAdc * (TENSAO_ADC_V / ADC_MAX);
}

float filtrarTensaoSolar(float tensaoAtual_V) {
    return ALPHA_FILTRO_TENSAO * tensaoAtual_V +
           (1.0f - ALPHA_FILTRO_TENSAO) * tensaoFiltradaAnterior_V;
}

float calcularPotenciaSolar(float tensaoFiltradaLocal_V) {
    return ESCALA_POTENCIA_SOLAR * tensaoFiltradaLocal_V;
}

// ---------------------------------------------------------------------------
// Modelo simulado do painel
// ---------------------------------------------------------------------------

void atualizarParametrosLPV(float referencia_V) {
    rho = referencia_V;
    tau = TAU_0 + TAU_1 * rho;
    Ke  = KE_0  + KE_1  * rho;
}

void atualizarModeloSimulado() {
    saidaModelo = saidaModeloAnterior
                  - (TS_SEGUNDOS / tau) * saidaModeloAnterior
                  + (TS_SEGUNDOS / tau) * Ke * controleSaturado_V;
    saidaModeloAnterior = saidaModelo;
}

// ---------------------------------------------------------------------------
// Controlador LPV
// ---------------------------------------------------------------------------

void atualizarIntegralErro(float erro) {
    bool controleSaturou = controleNaoSaturado[0][0] < 0.0f ||
                           controleNaoSaturado[0][0] > 5.0f;

    if (controleSaturou) {
        // Anti-windup: nao acumula integral quando saturado
        integralErro[1] = integralErro[0];
    } else {
        integralErro[1] = integralErro[0] + TS_SEGUNDOS * erro;
    }
}

void calcularGanhoLPV() {
    mtx_type temp[ORDEM_CONTROLADOR][ORDEM_CONTROLADOR];
    mtx_type temp2[ORDEM_CONTROLADOR][ORDEM_CONTROLADOR];

    Matrix.Copy((mtx_type*)Y1, ORDEM_CONTROLADOR, ORDEM_CONTROLADOR, (mtx_type*)temp);
    Matrix.Scale((mtx_type*)temp, ORDEM_CONTROLADOR, ORDEM_CONTROLADOR, rho);
    Matrix.Add((mtx_type*)Y0, (mtx_type*)temp,
               ORDEM_CONTROLADOR, ORDEM_CONTROLADOR, (mtx_type*)temp2);

    Matrix.Copy((mtx_type*)temp2, ORDEM_CONTROLADOR, ORDEM_CONTROLADOR, (mtx_type*)matrizP);
    Matrix.Invert((mtx_type*)matrizP, ORDEM_CONTROLADOR);

    mtx_type tempW[1][ORDEM_CONTROLADOR];
    Matrix.Copy((mtx_type*)W1, 1, ORDEM_CONTROLADOR, (mtx_type*)tempW);
    Matrix.Scale((mtx_type*)tempW, 1, ORDEM_CONTROLADOR, rho);
    Matrix.Add((mtx_type*)W0, (mtx_type*)tempW, 1, ORDEM_CONTROLADOR, (mtx_type*)matrizW);

    Matrix.Multiply((mtx_type*)matrizW, (mtx_type*)matrizP,
                    1, ORDEM_CONTROLADOR, ORDEM_CONTROLADOR, (mtx_type*)ganhoK);
}

float calcularControleLPV(float referencia_V, float tensaoFiltradaLocal_V) {
    erroControle = referencia_V - tensaoFiltradaLocal_V;
    atualizarIntegralErro(erroControle);

    estadoControlador[0][0] = erroControle;
    estadoControlador[1][0] = integralErro[1];

    calcularGanhoLPV();

    Matrix.Multiply((mtx_type*)ganhoK, (mtx_type*)estadoControlador,
                    1, ORDEM_CONTROLADOR, 1, (mtx_type*)controleNaoSaturado);

    return controleNaoSaturado[0][0];
}

float saturarControle(float controle_V) {
    if (controle_V > 5.0f) return 5.0f;
    if (controle_V < 0.0f) return 0.0f;
    return controle_V;
}

void aplicarPWM(float controle_V) {
    int pwm = (int)(controle_V / 5.0f * 255.0f);
    analogWrite(PINO_PWM_PAINEL, pwm);
}

void atualizarEstadosParaProximoPasso() {
    integralErro[0]          = integralErro[1];
    tensaoFiltradaAnterior_V = tensaoFiltrada_V;
}

// ---------------------------------------------------------------------------
// Debug
// ---------------------------------------------------------------------------

void imprimirDebug(float referencia_V) {
    if (!DEBUG_SERIAL) return;

    Serial.print(controleNaoSaturado[0][0]);  Serial.print(", ");
    Serial.print(controleSaturado_V);          Serial.print(", ");
    Serial.print(erroControle);                Serial.print(", ");
    Serial.print(referencia_V, 5);             Serial.print(", ");
    Serial.print(tensaoMedida_V);              Serial.print(", ");
    Serial.println(tensaoFiltrada_V);
}

// ---------------------------------------------------------------------------
// Arduino
// ---------------------------------------------------------------------------

void setup() {
    Wire.begin(ENDERECO_I2C);
    Wire.onRequest(enviarPotenciaI2C);
    Wire.onReceive(receberReferenciaI2C);

    pinMode(PINO_PWM_PAINEL, OUTPUT);

    // Inicializa o filtro com a leitura real para evitar transitorio inicial
    tensaoMedida_V           = lerTensaoSolar();
    tensaoFiltradaAnterior_V = tensaoMedida_V;

    Serial.begin(115200);
    ultimoControle_ms = millis();
}

void loop() {
    unsigned long agora_ms = millis();
    if (agora_ms - ultimoControle_ms < PERIODO_CONTROLE_MS) {
        return;
    }
    ultimoControle_ms = agora_ms;

    float referenciaLocal_V = lerReferenciaAtomica();

    atualizarParametrosLPV(referenciaLocal_V);

    tensaoMedida_V   = lerTensaoSolar();
    tensaoFiltrada_V = filtrarTensaoSolar(tensaoMedida_V);

    float controleCalculado_V = calcularControleLPV(referenciaLocal_V, tensaoFiltrada_V);
    controleSaturado_V = saturarControle(controleCalculado_V);

    atualizarModeloSimulado();
    aplicarPWM(controleSaturado_V);

    // Protege escrita com noInterrupts() pois a ISR pode ler potenciaSolar_W
    // concorrentemente. float de 4 bytes nao e atomico no AVR.
    noInterrupts();
    potenciaSolar_W = calcularPotenciaSolar(tensaoFiltrada_V);
    interrupts();

    imprimirDebug(referenciaLocal_V);
    atualizarEstadosParaProximoPasso();
}
