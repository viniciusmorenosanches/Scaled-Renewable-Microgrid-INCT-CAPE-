/**
 * debug_sensor_h2.ino — Diagnóstico Completo do Subsistema de Hidrogênio (H2)
 *
 * Hardware testado:
 *   1. Sensor ToF de nível de H2: VL6180X (I2C: 0x29, pinos A4/A5)
 *   2. Sensor de corrente/potência da célula: INA219 (I2C: 0x40, pinos A4/A5)
 *   3. Relé do eletrolisador (Pino Digital 5, ativo LOW)
 *   4. Servo da válvula da célula a combustível (Pino Digital 3, 0° aberto / 180° fechado)
 *
 * Carregue no Arduino UNO do H2 e abra o Monitor Serial a 115200 baud (com terminação "Both NL & CR" ou "Newline").
 *
 * Comandos no Monitor Serial:
 *   el1 / el0       -> Liga / Desliga o relé do eletrolisador (D5)
 *   cc1 / cc0       -> Abre (0°) / Fecha (180°) a válvula da célula (D3)
 *   cc <0-180>      -> Ajusta o servo em um ângulo específico (ex: cc 90)
 *   altura <XX>     -> Ajusta ALTURA_COPO_MM para calibrar nível de H2 (ex: altura 47.0)
 *   scan            -> Reexecuta varredura I2C e reinicializa os sensores
 *   stop            -> Desliga relé e fecha servo (estado seguro)
 *   help            -> Mostra lista de comandos
 */

#include <Wire.h>
#include <VL6180X.h>
#include <Adafruit_INA219.h>
#include <Servo.h>

// ---------------------------------------------------------------------------
// Pinos dos atuadores (idênticos ao firmware de produção)
// ---------------------------------------------------------------------------

const int PINO_RELE_ELETROLISADOR = 5;  // Relé (ativo LOW)
const int PINO_SERVO_CELULA       = 3;  // Servo da válvula
const int ANGULO_FECHADO          = 180;
const int ANGULO_ABERTO           = 0;

// ---------------------------------------------------------------------------
// Parâmetros do sensor VL6180X
// ---------------------------------------------------------------------------

float ALTURA_COPO_MM        = 47.0f;  // Altura física do reservatório [mm]
const float FATOR_CALIBRACAO = 0.9375f;
const float ALPHA_FILTRO     = 0.05f;

// ---------------------------------------------------------------------------
// Instâncias dos sensores e atuadores
// ---------------------------------------------------------------------------

VL6180X         sensorNivel;
Adafruit_INA219 sensorPotencia;
Servo           servoCelula;

bool vl6180xOK = false;
bool ina219OK  = false;

float distanciaFiltrada_mm = -1.0f;

bool releOn       = false;
bool servoAberto  = false;
int  anguloAtual  = ANGULO_FECHADO;

// ---------------------------------------------------------------------------
// Scan I2C
// ---------------------------------------------------------------------------

void escanearI2C() {
    Serial.println(F("\n--- VARREDURA I2C (Pinos A4=SDA, A5=SCL) ---"));
    int encontrados = 0;
    for (byte addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.print(F("  [✓] Encontrado dispositivo no endereco: 0x"));
            if (addr < 16) Serial.print('0');
            Serial.print(addr, HEX);
            Serial.print(F(" (")); Serial.print(addr, DEC); Serial.print(F(")"));

            if (addr == 0x29) Serial.print(F("  <-- VL6180X (Nivel H2)"));
            if (addr == 0x40) Serial.print(F("  <-- INA219 (Tensao / Corrente Celula)"));
            Serial.println();
            encontrados++;
        }
    }

    if (encontrados == 0) {
        Serial.println(F("  [!] NENHUM dispositivo I2C encontrado no barramento!"));
        Serial.println(F("      Verifique as conexoes de 5V, GND, SDA (A4) e SCL (A5)."));
    } else {
        Serial.print(F("  Total de dispositivos detectados: "));
        Serial.println(encontrados);
    }
    Serial.println(F("---------------------------------------------"));
}

// ---------------------------------------------------------------------------
// Inicialização dos sensores
// ---------------------------------------------------------------------------

void inicializarSensores() {
    // 1. VL6180X (0x29)
    Wire.beginTransmission(0x29);
    if (Wire.endTransmission() == 0) {
        sensorNivel.init();
        sensorNivel.configureDefault();
        sensorNivel.setTimeout(200);
        sensorNivel.writeReg(VL6180X::SYSRANGE__MAX_CONVERGENCE_TIME, 30);
        sensorNivel.writeReg(VL6180X::READOUT__AVERAGING_SAMPLE_PERIOD, 10);
        vl6180xOK = true;
        Serial.println(F("  [OK] Sensor VL6180X inicializado com sucesso!"));
    } else {
        vl6180xOK = false;
        Serial.println(F("  [FALHA] VL6180X nao respondeu no endereco 0x29."));
    }

    // 2. INA219 (0x40)
    if (sensorPotencia.begin()) {
        sensorPotencia.setCalibration_16V_400mA();
        ina219OK = true;
        Serial.println(F("  [OK] Sensor INA219 inicializado com sucesso!"));
    } else {
        ina219OK = false;
        Serial.println(F("  [FALHA] INA219 nao respondeu no endereco 0x40."));
    }
}

// ---------------------------------------------------------------------------
// Controle de Atuadores
// ---------------------------------------------------------------------------

void setRele(bool on) {
    releOn = on;
    digitalWrite(PINO_RELE_ELETROLISADOR, on ? LOW : HIGH);
    digitalWrite(LED_BUILTIN, on ? HIGH : LOW);
    Serial.print(F("  >> RELE ELETROLISADOR: "));
    Serial.println(on ? F("LIGADO (LOW)") : F("DESLIGADO (HIGH)"));
}

void setServoAngulo(int angulo) {
    if (angulo < 0)   angulo = 0;
    if (angulo > 180) angulo = 180;
    anguloAtual = angulo;
    servoAberto = (angulo < 90);
    servoCelula.write(angulo);
    Serial.print(F("  >> SERVO CELULA: Angulo definido para "));
    Serial.print(angulo);
    Serial.println(F(" graus."));
}

void imprimirMenuAjuda() {
    Serial.println(F("\n========================================================"));
    Serial.println(F("  COMANDOS DISPONIVEIS NO MONITOR SERIAL (115200 baud):"));
    Serial.println(F("========================================================"));
    Serial.println(F("  el1 / el0    -> Liga / Desliga o rele do eletrolisador (D5)"));
    Serial.println(F("  cc1 / cc0    -> Abre (0 deg) / Fecha (180 deg) o servo da celula (D3)"));
    Serial.println(F("  cc <0-180>   -> Define angulo especifico do servo (ex: cc 90)"));
    Serial.println(F("  altura <XX>  -> Ajusta ALTURA_COPO_MM (ex: altura 47.0)"));
    Serial.println(F("  scan         -> Faz nova varredura I2C"));
    Serial.println(F("  stop         -> Para tudo com seguranca (Rele OFF, Servo FECHADO)"));
    Serial.println(F("  help         -> Mostra este menu de ajuda"));
    Serial.println(F("========================================================\n"));
}

// ---------------------------------------------------------------------------
// Leitura e Exibição Periódica
// ---------------------------------------------------------------------------

void lerEExibirSensores() {
    Serial.print(F("["));
    Serial.print(millis() / 1000.0f, 1);
    Serial.print(F("s] "));

    // Estado dos Atuadores
    Serial.print(F("Rele:"));
    Serial.print(releOn ? F("ON ") : F("OFF"));
    Serial.print(F(" | Servo:"));
    Serial.print(anguloAtual);
    Serial.print(F("deg | "));

    // 1. Leituras do VL6180X (Nível de H2)
    Serial.print(F("H2: "));
    if (!vl6180xOK) {
        Serial.print(F("[OFFLINE] "));
    } else {
        uint8_t distBruta = sensorNivel.readRangeSingleMillimeters();
        bool timeout = sensorNivel.timeoutOccurred();

        if (timeout || distBruta == 255) {
            Serial.print(F("Dist=ERRO "));
        } else {
            if (distanciaFiltrada_mm < 0.0f) {
                distanciaFiltrada_mm = (float)distBruta;
            } else {
                distanciaFiltrada_mm = ALPHA_FILTRO * (float)distBruta + (1.0f - ALPHA_FILTRO) * distanciaFiltrada_mm;
            }
            float nivel_mm = (ALTURA_COPO_MM - distanciaFiltrada_mm) * FATOR_CALIBRACAO;
            Serial.print(F("Dist=")); Serial.print(distBruta); Serial.print(F("mm, "));
            Serial.print(F("Nivel=")); Serial.print(nivel_mm, 1); Serial.print(F("mm | "));
        }
    }

    // 2. Leituras do INA219 (Tensão, Corrente, Potência da Célula)
    Serial.print(F("Celula: "));
    if (!ina219OK) {
        Serial.println(F("[OFFLINE]"));
    } else {
        float v_bus_V   = sensorPotencia.getBusVoltage_V();
        float i_shunt_mA = sensorPotencia.getCurrent_mA();
        if (i_shunt_mA < 0.0f) i_shunt_mA = 0.0f;
        if (v_bus_V < 0.0f)    v_bus_V = 0.0f;

        float p_mW = v_bus_V * i_shunt_mA;

        Serial.print(F("V=")); Serial.print(v_bus_V, 3); Serial.print(F("V, "));
        Serial.print(F("I=")); Serial.print(i_shunt_mA, 1); Serial.print(F("mA, "));
        Serial.print(F("P=")); Serial.print(p_mW, 1); Serial.print(F("mW ("));
        Serial.print(p_mW / 1000.0f, 4); Serial.println(F("W)"));
    }
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000);

    Wire.begin();

    // Configura atuadores em estado seguro inicial
    pinMode(PINO_RELE_ELETROLISADOR, OUTPUT);
    digitalWrite(PINO_RELE_ELETROLISADOR, HIGH);  // Desligado (ativo LOW)

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    servoCelula.attach(PINO_SERVO_CELULA);
    servoCelula.write(ANGULO_FECHADO);

    Serial.println(F("\n========================================================"));
    Serial.println(F("   TESTE DE BANCADA — SUBSISTEMA DE H2 / CELULA / RELE  "));
    Serial.println(F("========================================================"));
    Serial.print(F("  Pino Rele:  D")); Serial.println(PINO_RELE_ELETROLISADOR);
    Serial.print(F("  Pino Servo: D")); Serial.println(PINO_SERVO_CELULA);
    Serial.print(F("  ALTURA_COPO_MM: ")); Serial.println(ALTURA_COPO_MM);

    escanearI2C();
    inicializarSensores();
    imprimirMenuAjuda();

    Serial.println(F("--- INICIANDO LEITURAS EM TEMPO REAL (a cada 500 ms) ---"));
}

// ---------------------------------------------------------------------------
// Loop Principal
// ---------------------------------------------------------------------------

unsigned long ultimaLeitura_ms = 0;

void loop() {
    // Processamento de comandos enviados pelo Monitor Serial
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        cmd.toLowerCase();

        if (cmd == "el1") {
            setRele(true);
        } else if (cmd == "el0") {
            setRele(false);
        } else if (cmd == "cc1") {
            setServoAngulo(ANGULO_ABERTO);
        } else if (cmd == "cc0") {
            setServoAngulo(ANGULO_FECHADO);
        } else if (cmd.startsWith("cc ")) {
            int ang = cmd.substring(3).toInt();
            setServoAngulo(ang);
        } else if (cmd == "stop") {
            setRele(false);
            setServoAngulo(ANGULO_FECHADO);
            Serial.println(F("  >> [PARADA DE EMERGENCIA] Relé desligado e válvula fechada."));
        } else if (cmd.startsWith("altura ")) {
            float h = cmd.substring(7).toFloat();
            if (h > 0.0f && h < 200.0f) {
                ALTURA_COPO_MM = h;
                distanciaFiltrada_mm = -1.0f;
                Serial.print(F("  >> ALTURA_COPO_MM atualizada para: ")); Serial.println(h);
            } else {
                Serial.println(F("  >> Altura invalida. Use entre 1 e 200 mm."));
            }
        } else if (cmd == "scan") {
            escanearI2C();
            inicializarSensores();
        } else if (cmd == "help") {
            imprimirMenuAjuda();
        } else if (cmd.length() > 0) {
            Serial.print(F("  [!] Comando nao reconhecido: '")); Serial.print(cmd); Serial.println(F("'"));
            Serial.println(F("      Digite 'help' para listar os comandos."));
        }
    }

    // Leitura periódica dos sensores a cada 500 ms
    if (millis() - ultimaLeitura_ms >= 500) {
        ultimaLeitura_ms = millis();
        lerEExibirSensores();
    }
}

