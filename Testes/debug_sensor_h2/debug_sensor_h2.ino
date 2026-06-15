/**
 * debug_sensor_h2.ino — Diagnóstico detalhado do sensor VL6180X de nível de H2
 *
 * Carregue este sketch no Arduino UNO do H2 (firmware_hidrogenio_uno).
 * Abre o Monitor Serial a 9600 baud.
 *
 * O que este sketch faz:
 *   1. Detecta se o VL6180X está presente no I2C.
 *   2. Lê distância bruta (mm) a cada 500 ms e imprime tudo:
 *      - Distância bruta do sensor
 *      - Se ocorreu timeout
 *      - Nível calculado com a fórmula do firmware
 *   3. Permite testar ligar/desligar o relé e o servo enquanto o sensor lê,
 *      para verificar se há interferência entre os atuadores e o I2C.
 *
 * Comandos (Monitor Serial, 9600 baud, NewLine):
 *   el1 / el0   Liga / desliga relé do eletrolisador (pino 5)
 *   cc1 / cc0   Abre / fecha servo da célula (pino 3)
 *   altura XX   Define ALTURA_COPO_MM = XX (para calibrar sem recompilar)
 *   stop        Para os atuadores (estado seguro)
 *
 * DIAGNOSE:
 *   - Se "I2C: NAO ENCONTRADO" → VL6180X não está no barramento (cabo desconectado, sem 3.3V, etc.)
 *   - Se dist=255 ou TIMEOUT toda vez → sensor presente mas não consegue medir (superfície muito
 *     longe, fora do ângulo, luz ambiente excessiva)
 *   - Se nivel=0.0 mas dist é válida → ALTURA_COPO_MM muito pequena (dist > altura → nivel negativo → clamp 0)
 *   - Se nivel varia normalmente → firmware OK, problema é na comunicação com o R4
 */

#include <Wire.h>
#include <VL6180X.h>
#include <Servo.h>

// ---------------------------------------------------------------------------
// Pinos (devem ser IGUAIS ao firmware de produção)
// ---------------------------------------------------------------------------

const int PINO_RELE_ELETROLISADOR = 5;  // Relé (ativo LOW)
const int PINO_SERVO_CELULA       = 3;  // Servo da válvula
const int ANGULO_FECHADO          = 180;
const int ANGULO_ABERTO           = 0;

// ---------------------------------------------------------------------------
// Parâmetros do sensor (iguais ao sensor_nivel_h2.cpp)
// ---------------------------------------------------------------------------

float ALTURA_COPO_MM        = 47.0f;  // Ajustável por comando serial
const float FATOR_CALIBRACAO = 0.9375f;
const float ALPHA_FILTRO     = 0.05f;

// ---------------------------------------------------------------------------
// Objetos
// ---------------------------------------------------------------------------

VL6180X sensor;
Servo   servoCelula;

float distanciaFiltrada_mm = -1.0f;
bool  sensorOK             = false;

// ---------------------------------------------------------------------------
// Scan I2C — detecta o VL6180X
// ---------------------------------------------------------------------------

bool escanearI2C() {
    Serial.println("\n--- SCAN I2C ---");
    int encontrados = 0;
    for (byte addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.print("  Encontrado: 0x"); Serial.print(addr, HEX);
            Serial.print(" ("); Serial.print(addr, DEC); Serial.print(")");
            if (addr == 0x29) Serial.print("  <-- VL6180X");
            Serial.println();
            encontrados++;
        }
    }
    if (encontrados == 0) {
        Serial.println("  NENHUM dispositivo I2C encontrado!");
        Serial.println("  >> Verifique SDA (A4), SCL (A5), alimentacao 3.3V do VL6180X.");
    }
    Serial.print("  Total: "); Serial.println(encontrados);
    Serial.println("----------------");
    return encontrados > 0;
}

// ---------------------------------------------------------------------------
// Inicialização do sensor
// ---------------------------------------------------------------------------

bool inicializarSensor() {
    Serial.println("\n--- INICIALIZANDO VL6180X ---");

    // Verifica presença (endereço padrão 0x29)
    Wire.beginTransmission(0x29);
    byte err = Wire.endTransmission();
    if (err != 0) {
        Serial.print("  ERRO: VL6180X nao encontrado (I2C err=");
        Serial.print(err); Serial.println(")");
        Serial.println("  >> Cheque: cabo SDA->A4, SCL->A5, VCC->3.3V, GND->GND");
        return false;
    }

    sensor.init();
    sensor.configureDefault();
    sensor.setTimeout(200);  // 200 ms para debug (firmware usa 50 ms)
    sensor.writeReg(VL6180X::SYSRANGE__MAX_CONVERGENCE_TIME, 30);
    sensor.writeReg(VL6180X::READOUT__AVERAGING_SAMPLE_PERIOD, 10);

    Serial.println("  VL6180X inicializado com sucesso!");
    return true;
}

// ---------------------------------------------------------------------------
// Leitura e cálculo de nível
// ---------------------------------------------------------------------------

void lerESensar() {
    Serial.print("dist_bruta=");

    uint8_t dist = sensor.readRangeSingleMillimeters();
    bool    timeout = sensor.timeoutOccurred();

    if (timeout) {
        Serial.print("TIMEOUT");
    } else if (dist == 255) {
        Serial.print("255 (INVALIDO/sem reflexo)");
    } else {
        Serial.print(dist); Serial.print("mm");
    }

    // Atualiza filtro
    bool invalido = timeout || dist == 255;
    if (!invalido) {
        if (distanciaFiltrada_mm < 0.0f) {
            distanciaFiltrada_mm = (float)dist;
        } else {
            distanciaFiltrada_mm = ALPHA_FILTRO * (float)dist
                                 + (1.0f - ALPHA_FILTRO) * distanciaFiltrada_mm;
        }
    }

    Serial.print("  |  dist_filtrada=");
    if (distanciaFiltrada_mm < 0.0f) {
        Serial.print("N/A (sem leitura valida ainda)");
    } else {
        Serial.print(distanciaFiltrada_mm, 1); Serial.print("mm");
    }

    // Calcula nível
    float nivel = 0.0f;
    if (distanciaFiltrada_mm >= 0.0f) {
        float n = ALTURA_COPO_MM - distanciaFiltrada_mm;
        if (n < 0.0f) {
            nivel = 0.0f;
            Serial.print("  |  nivel=0.0mm (CLAMP: dist(");
            Serial.print(distanciaFiltrada_mm,1);
            Serial.print(") > altura(");
            Serial.print(ALTURA_COPO_MM,1);
            Serial.print(") -- AJUSTE 'altura XX'!)");
        } else {
            nivel = n * FATOR_CALIBRACAO;
            Serial.print("  |  nivel="); Serial.print(nivel, 2); Serial.print("mm");
        }
    } else {
        Serial.print("  |  nivel=0.0mm (sem leitura)");
    }

    Serial.println();
}

// ---------------------------------------------------------------------------
// Controle dos atuadores
// ---------------------------------------------------------------------------

bool releOn  = false;
bool servoOn = false;

void setRele(bool on) {
    releOn = on;
    digitalWrite(PINO_RELE_ELETROLISADOR, on ? LOW : HIGH);
    Serial.print("  >> Rele (eletrolisador): "); Serial.println(on ? "LIGADO (LOW)" : "DESLIGADO (HIGH)");
}

void setServo(bool aberto) {
    servoOn = aberto;
    servoCelula.write(aberto ? ANGULO_ABERTO : ANGULO_FECHADO);
    Serial.print("  >> Servo (celula): "); Serial.println(aberto ? "ABERTO (0 deg)" : "FECHADO (180 deg)");
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------

void setup() {
    Serial.begin(9600);
    while (!Serial && millis() < 2000);

    Wire.begin();

    // Configura atuadores em estado seguro
    pinMode(PINO_RELE_ELETROLISADOR, OUTPUT);
    digitalWrite(PINO_RELE_ELETROLISADOR, HIGH);  // Desligado
    servoCelula.attach(PINO_SERVO_CELULA);
    servoCelula.write(ANGULO_FECHADO);

    Serial.println("\n========================================");
    Serial.println("  DEBUG SENSOR H2 — VL6180X + ATUADORES");
    Serial.println("========================================");
    Serial.print("  Pino Rele:  D"); Serial.println(PINO_RELE_ELETROLISADOR);
    Serial.print("  Pino Servo: D"); Serial.println(PINO_SERVO_CELULA);
    Serial.print("  ALTURA_COPO_MM: "); Serial.println(ALTURA_COPO_MM);
    Serial.println();
    Serial.println("  Comandos: el1/el0  cc1/cc0  altura XX  stop");
    Serial.println("----------------------------------------");

    // Scan I2C
    escanearI2C();

    // Inicializa sensor
    sensorOK = inicializarSensor();
    if (!sensorOK) {
        Serial.println("\n  SENSOR NAO DISPONIVEL. Continuando sem ele.");
        Serial.println("  Apenas atuadores funcionarao.");
    }

    Serial.println("\n  [Lendo a cada 500 ms...]");
    Serial.println("----------------------------------------");
}

// ---------------------------------------------------------------------------
// Loop
// ---------------------------------------------------------------------------

unsigned long ultimaLeitura_ms = 0;

void loop() {
    // --- Comandos serial ---
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        cmd.toLowerCase();

        if (cmd == "el1") {
            setRele(true);
        } else if (cmd == "el0") {
            setRele(false);
        } else if (cmd == "cc1") {
            setServo(true);
        } else if (cmd == "cc0") {
            setServo(false);
        } else if (cmd == "stop") {
            setRele(false);
            setServo(false);
            Serial.println("  >> Tudo parado (estado seguro).");
        } else if (cmd.startsWith("altura ")) {
            float h = cmd.substring(7).toFloat();
            if (h > 0.0f && h < 200.0f) {
                ALTURA_COPO_MM = h;
                distanciaFiltrada_mm = -1.0f;  // Reseta filtro para recalcular
                Serial.print("  >> ALTURA_COPO_MM atualizado para: "); Serial.println(h);
            } else {
                Serial.println("  >> Valor invalido. Use entre 1 e 200 mm.");
            }
        } else if (cmd == "scan") {
            escanearI2C();
            sensorOK = inicializarSensor();
        } else if (cmd == "reset") {
            distanciaFiltrada_mm = -1.0f;
            Serial.println("  >> Filtro resetado.");
        } else if (cmd.length() > 0) {
            Serial.print("  Desconhecido: '"); Serial.print(cmd); Serial.println("'");
            Serial.println("  Comandos: el1 el0 cc1 cc0 stop  altura XX  scan  reset");
        }
    }

    // --- Leitura periódica ---
    if (millis() - ultimaLeitura_ms >= 500) {
        ultimaLeitura_ms = millis();

        Serial.print("[");
        Serial.print(millis() / 1000.0f, 1);
        Serial.print("s]  el="); Serial.print(releOn ? "ON " : "OFF");
        Serial.print("  cc="); Serial.print(servoOn ? "ON " : "OFF");
        Serial.print("  | ");

        if (!sensorOK) {
            Serial.println("SENSOR OFFLINE - use 'scan' para tentar reconectar");
        } else {
            lerESensar();
        }
    }
}
