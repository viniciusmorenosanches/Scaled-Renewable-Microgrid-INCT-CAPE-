/**
 * firmware_turbina_eolica.ino — Periferico da turbina eolica da microrrede
 *
 * Autor original: Samira (2025, para o SBAI,  Controle PI LPV-H_infinito - validação experimental)
 * Refatorado: Junho/2026
 *
 * 
 * Papel desta placa (Arduino Uno):
 *  - Receber do Arduino R4 central uma referencia de tensao via I2C.
 *  - Controlar o ESC do conjunto motor/ventilador/turbina.
 *  - Medir a tensao gerada pela turbina.
 *  - Filtrar a medicao com filtro de Kalman.
 *  - Estimar a potencia eolica.
 *  - Devolver essa potencia ao R4 via I2C.
 *
 * Contrato I2C:
 *  - Endereco: 10
 *  - Entrada: 1 float com a referencia da turbina [V]
 *  - Saida:   1 float com a potencia estimada [W]
 *
 * Ver docs/comunicacao.md para o protocolo completo.
 * Ver docs/calibracao.md para explicacao das constantes fisicas.
 */

#include <MatrixMath.h>
#include <Servo.h>
#include <Wire.h>

// ---------------------------------------------------------------------------
// Configuracao de hardware
// ---------------------------------------------------------------------------

const byte ENDERECO_I2C       = 10;  // Endereco I2C desta placa
const int  PINO_ESC           = 3;   // Pino de controle do ESC (PWM via biblioteca Servo)
const int  PINO_SENSOR_TURBINA = A3; // Entrada analogica da tensao da turbina

// ---------------------------------------------------------------------------
// Parametros de tempo
// ---------------------------------------------------------------------------

const float         TS_SEGUNDOS         = 0.1f;
const unsigned long PERIODO_CONTROLE_MS     = 100;
const unsigned long TEMPO_ARME_ESC_MS       = 10000;  // Tempo de arme do ESC [ms]
const unsigned long TIMEOUT_I2C_WATCHDOG_MS = 2000;   // Sem pacote I2C do R4 por mais de 2s -> para a turbina
volatile unsigned long ultimoPacoteI2C_ms   = 0;

// ---------------------------------------------------------------------------
// Conversao ADC
// ---------------------------------------------------------------------------

const float TENSAO_ADC_V = 5.0f;
const float ADC_MAX      = 1023.0f;

// ---------------------------------------------------------------------------
// Divisor resistivo na entrada de medicao
//
// A tensao da turbina e dividida por um divisor resistivo de 43k / 10k
// antes de entrar no ADC. Para recuperar a tensao real, multiplicar por:
//   (43 + 10) / 10 = 4.3
//
// Nota: as resistencias foram medidas em bancada e arredondadas.
// Se substituir os resistores, recalcular este fator.
// ---------------------------------------------------------------------------

const float GANHO_DIVISOR_TENSAO = 43.0f / 10.0f;  // 4.3 [adimensional]

// ---------------------------------------------------------------------------
// Ganho do ESC
//
// Converte o controle em Volts para o comando da biblioteca Servo.
// ESC opera de 0 a ~68 (mapeado internamente para 1000-2000 us).
// Valor empirico: 38 unidades de servo por volt de controle.
// ---------------------------------------------------------------------------

const float GANHO_ESC_POR_VOLT = 38.0f;

// ---------------------------------------------------------------------------
// Escala de potencia da turbina eolica — DOIS REGIMES DE CALIBRACAO
//
// Regime A (Observador de Kalman sem sensor / A3 ~ 0V -> Yf in [0 .. 1.50V]):
//   Quando o divisor de tensao no pino A3 esta desconectado ou lendo ~0V,
//   o filtro de Kalman estima Yf pela predicao do modelo (Yf ~ 0.30 * U),
//   atingindo ~1.49V quando U = 5.00V (potencia aerodinamica fisica ~ 0.051 W).
//   Com ESCALA = 4900.0f e OFFSET = 0.0f, mapeia 0..1.50V -> 0..250 W.
//
// Regime B (Sensor fisico reconectado no A3 -> Y_raw = 2.0 .. 4.5V):
//   Se o divisor resistivo (43k/10k) for reconectado ao A3 e entregar a
//   tensao real do gerador (~2.0V a 4.5V), REDUZIR a escala para nao saturar
//   em milhares de Watts (ex: ESCALA = 50.0f com OFFSET = 150.0f como nos
//   ensaios originais, ou ESCALA = 120.0f com OFFSET = 0.0f para 0..250W).
// ---------------------------------------------------------------------------

const float ESCALA_TEMPORARIA_POTENCIA   = 4900.0f; // [adimensional]
const float OFFSET_TEMPORARIO_POTENCIA_W = 0.0f;    // [W]

// ---------------------------------------------------------------------------
// Debug serial (ligar apenas para desenvolvimento; desligar em operacao)
// ---------------------------------------------------------------------------

const bool DEBUG_SERIAL = true;

// ---------------------------------------------------------------------------
// Rampa de partida do ESC
//
// O ESC precisa de uma sequencia de inicializacao antes de aceitar comandos
// normais. Cada elemento representa o comando Servo (em unidades ESC) a
// aplicar em cada passo de Ts = 100 ms. Total: 60 passos = 6 segundos.
// ---------------------------------------------------------------------------

const float RAMPA_PARTIDA_ESC[60] = {
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     2,  4,  6,  9, 12, 14, 17, 20, 23, 26,
    30, 34, 37, 41, 43, 46, 49, 52, 55, 57,
    60, 62, 64, 65, 66, 67, 67, 68, 68, 68,
    68, 68, 68, 68, 68, 68, 68, 68, 68, 68,
    68, 68, 68, 68, 68, 68, 68, 68, 68, 68
};
const int TAMANHO_RAMPA_PARTIDA = 60;

// ---------------------------------------------------------------------------
// Filtro de Kalman
//
// Usado para filtrar a tensao ruidosa medida no pino analogico.
// Q: ruido do processo (quanto o estado real pode variar a cada passo)
// R: ruido da medicao (quanto a leitura ADC varia em relacao ao estado real)
// Valores ajustados empiricamente para balancear suavizacao e tempo de resposta.
// ---------------------------------------------------------------------------

float estadoKalman      = 0.0f;
float incertezaKalman   = 1.0f;
const float RUIDO_PROCESSO_Q = 0.1f;
const float RUIDO_MEDICAO_R  = 0.3f;
float ganhoKalman       = 0.0f;

// ---------------------------------------------------------------------------
// Parametros do modelo LPV (calculados no MATLAB)
// ---------------------------------------------------------------------------

float rho = 0.0f;  // Variavel de agendamento LPV: referencia atual [V]
float tau = 0.0f;  // Constante de tempo do modelo (funcao de rho)
float Ke  = 0.0f;  // Ganho do modelo (funcao de rho)

// ---------------------------------------------------------------------------
// Parametros do controlador PI LPV
// ---------------------------------------------------------------------------

const int ORDEM_CONTROLADOR = 2;

mtx_type Y0[ORDEM_CONTROLADOR][ORDEM_CONTROLADOR] = {
    {  3.1488f, -0.3766f },
    { -0.3766f,  1.9363f }
};
mtx_type Y1[ORDEM_CONTROLADOR][ORDEM_CONTROLADOR] = {
    {  0.0240f, -0.0000f },
    { -0.0000f,  0.0000f }
};
mtx_type W0[1][ORDEM_CONTROLADOR] = { 5.5171f,  2.0103f };
mtx_type W1[1][ORDEM_CONTROLADOR] = { 0.1463f, -0.1650f };

mtx_type matrizP[ORDEM_CONTROLADOR][ORDEM_CONTROLADOR];
mtx_type matrizW[1][ORDEM_CONTROLADOR];
mtx_type ganhoK[1][ORDEM_CONTROLADOR];
mtx_type estadoControlador[ORDEM_CONTROLADOR][1];
mtx_type controleNaoSaturado[1][1];

// ---------------------------------------------------------------------------
// Estado compartilhado com a ISR do I2C
// ---------------------------------------------------------------------------

volatile bool  mestreIniciou      = false;
volatile float referenciaTurbina_V = 0.0f;
volatile float potenciaEolica_W   = 0.0f;

// ---------------------------------------------------------------------------
// Estado interno do controle
// ---------------------------------------------------------------------------

Servo esc;

float controleSaturado_V     = 0.0f;
float tensaoMedida_V         = 0.0f;  // Leitura atual do ADC convertida [V]
float tensaoMedidaAnterior_V = 0.0f;  // Leitura do passo anterior [V]
float tensaoFiltrada_V       = 0.0f;  // Saida do filtro de Kalman atual [V]
float tensaoFiltradaAnterior_V = 0.0f; // Saida do filtro do passo anterior [V]
float erroControle           = 0.0f;
float integralErro[2]        = { 0.0f, 0.0f };

int           iteracao           = 0;
unsigned long ultimoControle_ms  = 0;

// ---------------------------------------------------------------------------
// Comunicacao I2C
// ---------------------------------------------------------------------------

/**
 * ISR chamada quando o R4 envia a referencia via I2C.
 *
 * Ajuste historico: referencias entre 0 V e 2 V nao vencem a faixa util
 * do conjunto motor/turbina (atrito estatico). Adiciona 2 V para levar
 * a referencia a uma faixa operavel. Remover quando a referencia for
 * recalibrada para a faixa real da turbina.
 */
void receberReferenciaI2C(int quantidadeBytes) {
    mestreIniciou          = true;
    ultimoPacoteI2C_ms     = millis();

    if (quantidadeBytes == sizeof(float)) {
        byte  buffer[sizeof(float)];
        for (int i = 0; i < sizeof(float); i++) {
            buffer[i] = Wire.read();
        }
        float novaReferencia = 0.0f;
        memcpy(&novaReferencia, buffer, sizeof(float));

        // Clampa leituras negativas (ruido de dados ou perfil)
        if (novaReferencia < 0.0f) {
            novaReferencia = 0.0f;
        }

        // NOTA: O ajuste historico de +2.0V foi removido para permitir
        // que a turbina rastreie dinamicamente o perfil de vento.
        // if (novaReferencia > 0.0f && novaReferencia < 2.0f) {
        //     novaReferencia += 2.0f;
        // }

        referenciaTurbina_V = novaReferencia;
        return;
    }

    while (Wire.available()) {
        Wire.read();
    }
}

/**
 * ISR chamada quando o R4 solicita a potencia via I2C.
 */
void enviarPotenciaI2C() {
    float potenciaLocal_W = potenciaEolica_W;
    byte  buffer[sizeof(float)];
    memcpy(buffer, &potenciaLocal_W, sizeof(float));
    Wire.write(buffer, sizeof(float));
}

/**
 * Le a referencia com interrupcoes desabilitadas (leitura atomica de float).
 */
float lerReferenciaAtomica() {
    noInterrupts();
    float referenciaLocal_V = referenciaTurbina_V;
    interrupts();
    return referenciaLocal_V;
}

// ---------------------------------------------------------------------------
// Inicializacao do ESC
// ---------------------------------------------------------------------------

/**
 * Arma o ESC: mantém o sinal em 0 por TEMPO_ARME_ESC_MS.
 * Necessario para que o ESC reconheca o controlador e desbloqueie o motor.
 */
void armarESC() {
    esc.attach(PINO_ESC, 1000, 2000);
    unsigned long inicio_ms = millis();
    while (millis() - inicio_ms < TEMPO_ARME_ESC_MS) {
        esc.write(0);
        delay(10);
    }
}

/**
 * Aguarda o primeiro comando do R4 antes de iniciar o controle.
 * Garante que a turbina nao inicie sem referencia.
 */
void aguardarPrimeiroComandoDoMestre() {
    while (!mestreIniciou) {
        if (DEBUG_SERIAL) {
            Serial.println("Aguardando primeiro comando do R4...");
        }
        delay(200);
    }
}

// ---------------------------------------------------------------------------
// Leitura, filtro e potencia
// ---------------------------------------------------------------------------

float lerTensaoTurbina() {
    int leituraAdc = analogRead(PINO_SENSOR_TURBINA);
    return leituraAdc * (TENSAO_ADC_V / ADC_MAX) * GANHO_DIVISOR_TENSAO;
}

void atualizarParametrosLPV(float referencia_V) {
    rho = referencia_V;
    tau = 2.0323f + (-0.1478f) * rho;
    Ke  = 3.9278f + (-0.1815f) * rho;
}

/**
 * Filtro de Kalman linearizado para suavizar a tensao da turbina.
 * Usa o modelo do sistema para prever o estado antes de corrigir com a medicao.
 */
float filtrarTensaoKalman(float tensaoAtual_V) {
    float fatorModelo      = 1.0f - TS_SEGUNDOS / tau;
    float estadoPrevisto   = fatorModelo * estadoKalman
                             + (TS_SEGUNDOS / tau) * Ke * controleSaturado_V;
    float incertezaPrevista = fatorModelo * incertezaKalman * fatorModelo + RUIDO_PROCESSO_Q;

    ganhoKalman    = incertezaPrevista / (incertezaPrevista + RUIDO_MEDICAO_R);
    estadoKalman   = estadoPrevisto + ganhoKalman * (tensaoAtual_V - estadoPrevisto);
    incertezaKalman = (1.0f - ganhoKalman) * incertezaPrevista;

    if (estadoKalman < 0.0f) {
        estadoKalman = 0.0f;
    }

    return estadoKalman;
}

/**
 * Estima a potencia eolica a partir da tensao filtrada.
 *
 * Formula fisica base: P = 0.5 * rho_ar * A * Cp * v^3
 * Como v (velocidade do vento) e estimada a partir da tensao filtrada da turbina,
 * a formula usa tensao ao cubo como proxy.
 *
 * Em bancada com gerador reduzido, aplica-se o fator de escala ESCALA_TEMPORARIA_POTENCIA
 * para elevar a excursao fisica (0 .. 0.051 W) para o perfil da microrrede (0 .. 250 W).
 */
float calcularPotenciaEolica(float tensaoFiltradaLocal_V) {
    if (tensaoFiltradaLocal_V <= 0.05f) {
        return 0.0f;
    }

    const float densidadeAr         = 1.225f;   // [kg/m3] ar a 15 graus C ao nivel do mar
    const float raioRotor_m         = 0.15f;    // [m] raio do rotor da turbina em escala
    const float coeficientePotencia = 0.35f;    // Cp [-] coeficiente de potencia (Betz realista)

    float constanteAero    = 0.5f * densidadeAr * PI * (raioRotor_m * raioRotor_m) * coeficientePotencia;
    float tensaoAoCubo     = tensaoFiltradaLocal_V * tensaoFiltradaLocal_V * tensaoFiltradaLocal_V;
    float potenciaFisica_W = constanteAero * tensaoAoCubo;

    float potencia_W = potenciaFisica_W * ESCALA_TEMPORARIA_POTENCIA + OFFSET_TEMPORARIO_POTENCIA_W;
    return (potencia_W > 0.0f) ? potencia_W : 0.0f;
}

// ---------------------------------------------------------------------------
// Controlador LPV
// ---------------------------------------------------------------------------

void atualizarIntegralErro(float erro) {
    // Anti-windup com clamping direcional:
    // So bloqueia a integral se o controle estiver saturado na MESMA direcao do erro.
    // Se U < 1.0 V mas erro > 0 (precisa de mais forca), a integral DEVE acumular
    // para tirar o atuador do piso de saturacao!
    bool bloqueiaTeto = (controleNaoSaturado[0][0] > 5.0f) && (erro > 0.0f);
    bool bloqueiaPiso = (controleNaoSaturado[0][0] < 1.0f) && (erro < 0.0f);

    if (bloqueiaTeto || bloqueiaPiso) {
        // Anti-windup direcional: congela acumulo apenas na direcao saturada
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

    // Fator de escala empirico aplicado na validacao original do controlador.
    // Reduz o ganho calculado para evitar overshoots na planta real.
    ganhoK[0][0] *= 0.5f;
    ganhoK[0][1] *= 0.5f;
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

/**
 * Satura o controle entre 1 V e 5 V quando ha referencia.
 * Aplica a rampa de partida nos primeiros TAMANHO_RAMPA_PARTIDA passos.
 * Forca zero quando a referencia e zero (turbina parada).
 */
float saturarControle(float controle_V, float referencia_V) {
    if (controle_V < 1.0f) {
        controle_V = (referencia_V != 0.0f) ? 1.0f : 0.0f;
    }
    if (controle_V > 5.0f) {
        controle_V = 5.0f;
    }

    if (iteracao < TAMANHO_RAMPA_PARTIDA) {
        controle_V              = RAMPA_PARTIDA_ESC[iteracao] / GANHO_ESC_POR_VOLT;
        integralErro[1]         = 0.0f;
        estadoControlador[1][0] = 0.0f;
    }

    if (referencia_V == 0.0f) {
        controle_V = 0.0f;
    }

    return controle_V;
}

void aplicarControleESC(float controle_V) {
    esc.write((int)(controle_V * GANHO_ESC_POR_VOLT));
}

void atualizarEstadosParaProximoPasso() {
    integralErro[0]          = integralErro[1];
    tensaoFiltradaAnterior_V = tensaoFiltrada_V;
    tensaoMedidaAnterior_V   = tensaoMedida_V;
}

// ---------------------------------------------------------------------------
// Debug
// ---------------------------------------------------------------------------

void imprimirDebug(float referencia_V) {
    if (!DEBUG_SERIAL) return;

    Serial.print("Pot_wt=");      Serial.print(potenciaEolica_W, 4);
    Serial.print(", U=");         Serial.print(controleSaturado_V);
    Serial.print(", Y_raw=");     Serial.print(tensaoMedida_V);
    Serial.print(", Yf=");        Serial.print(tensaoFiltrada_V);
    Serial.print(", ref=");       Serial.println(referencia_V);
}

// ---------------------------------------------------------------------------
// Arduino
// ---------------------------------------------------------------------------

void setup() {
    Wire.begin(ENDERECO_I2C);
    Wire.onRequest(enviarPotenciaI2C);
    Wire.onReceive(receberReferenciaI2C);

    Serial.begin(115200);

    armarESC();
    aguardarPrimeiroComandoDoMestre();

    pinMode(PINO_SENSOR_TURBINA, INPUT);
    ultimoControle_ms  = millis();
    ultimoPacoteI2C_ms = millis();
}

void loop() {
    unsigned long agora_ms = millis();
    if (agora_ms - ultimoControle_ms < PERIODO_CONTROLE_MS) {
        return;
    }
    ultimoControle_ms = agora_ms;

    float referenciaLocal_V = lerReferenciaAtomica();

    // Re-dispara a rampa de partida suave do ESC sempre que o comando sai de zero
    static float refAnterior_V = 0.0f;
    if (refAnterior_V == 0.0f && referenciaLocal_V > 0.0f) {
        iteracao = 0;
    }
    refAnterior_V = referenciaLocal_V;

    // Watchdog I2C: se o R4 parar de enviar referencias por mais de 2 s, desliga a turbina
    if (mestreIniciou && (agora_ms - ultimoPacoteI2C_ms > TIMEOUT_I2C_WATCHDOG_MS)) {
        referenciaLocal_V = 0.0f;
    }

    atualizarParametrosLPV(referenciaLocal_V);

    tensaoMedida_V   = lerTensaoTurbina();
    tensaoFiltrada_V = filtrarTensaoKalman(tensaoMedida_V);

    // Protege escrita com noInterrupts() pois a ISR pode ler potenciaEolica_W
    // concorrentemente. float de 4 bytes nao e atomico no AVR.
    noInterrupts();
    if (referenciaLocal_V == 0.0f) {
        potenciaEolica_W = 0.0f;
    } else {
        potenciaEolica_W = calcularPotenciaEolica(tensaoFiltrada_V);
    }
    interrupts();

    float controleCalculado_V = calcularControleLPV(referenciaLocal_V, tensaoFiltrada_V);
    controleSaturado_V        = saturarControle(controleCalculado_V, referenciaLocal_V);
    aplicarControleESC(controleSaturado_V);

    imprimirDebug(referenciaLocal_V);
    atualizarEstadosParaProximoPasso();
    iteracao++;
}
