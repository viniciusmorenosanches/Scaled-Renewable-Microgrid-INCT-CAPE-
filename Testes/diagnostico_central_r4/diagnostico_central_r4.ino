/**
 * diagnostico_central_r4.ino — Diagnóstico completo de comunicação dos subsistemas
 * Versão 3 — Correções após testes em bancada.
 *
 * Carregue este sketch no Arduino R4 Wifi (central) para verificar
 * se cada subsistema está respondendo corretamente, ANTES de rodar o MATLAB.
 *
 * CORREÇÕES v3:
 *   - I2C: i2cEnviar retorna bool; se falhar, i2cLer não é chamado (evita ler dado
 *     stale do painel solar como se fosse da turbina — estava causando Pot_wt=150W
 *     mesmo com a turbina desconectada).
 *   - H2: receberH2() gateado a 100 ms (evita spam de 'STALE PACKET' do SerialTransfer).
 *   - H2: begin() com debug=false explícito (evita impressões internas da biblioteca).
 *   - Nota: 'ref' do painel/turbina é setpoint do controlador LPV, NÃO liga/desliga.
 *     Com ref=0 o painel continua medindo a tensão física (Pot_pv = 150 * Vfiltrado).
 *
 * Comandos:
 *   help         → lista todos os comandos
 *   scan         → escaneia I2C + verifica H2 SerialTransfer
 *   solar        → sweep de referência no painel solar (0→3 V)
 *   vento        → sweep de referência na turbina eólica (0→3 V); envia ref=0 no fim
 *   all          → monitor contínuo de TODOS os subsistemas (solar + vento + h2)
 *   el1 / el0    → liga/desliga eletrolisador (mantém monitor all ativo)
 *   cc1 / cc0    → abre/fecha célula de combustível (mantém monitor all ativo)
 *   full         → diagnóstico completo sequencial de todos os subsistemas
 *   stop         → para tudo, zera atuadores, volta ao idle
 *   refs XX.X    → envia referência manual a solar E vento, lê os dois
 *   sol XX.X     → envia referência manual só ao painel solar
 *   vent XX.X    → envia referência manual só à turbina
 *
 * Protocolo:
 *   Painel Solar   (I2C addr  9): recebe float ref_sol [V],  envia float Pot_pv [W]
 *   Turbina Eólica (I2C addr 10): recebe float ref_vento [V], envia float Pot_wt [W]
 *   UNO H2 (SerialTransfer, Serial1):
 *     R4→UNO: R4ToUno_Data { float ucc, float uel }  — ID 1
 *     UNO→R4: UnoToR4_Data { float Pot_cc, float Level_H2 } — ID 0
 */

#include <Wire.h>
#include <SerialTransfer.h>

// ---------------------------------------------------------------------------
// Configuração de hardware
// ---------------------------------------------------------------------------

#define BAUD_RATE_USB   115200UL
#define BAUD_RATE_UNO   115200UL
#define ADDR_PAINEL     9    // I2C do painel solar
#define ADDR_TURBINA    10   // I2C da turbina eólica

#define ID_R4_TO_UNO    1
#define ID_UNO_TO_R4    0

// Tempo de espera após enviar ref à turbina (resposta física não é instantânea)
#define TURBINA_DELAY_MS  800UL

struct R4ToUno_Data { float ucc; float uel; };
struct UnoToR4_Data { float Pot_cc; float Level_H2; };

SerialTransfer  H2Transfer;
R4ToUno_Data    cmdH2;
UnoToR4_Data    telH2;

// ---------------------------------------------------------------------------
// Estado global
// ---------------------------------------------------------------------------

enum Modo {
    MODO_IDLE,
    MODO_SOLAR_SWEEP,
    MODO_VENTO_SWEEP,
    MODO_ALL_MONITOR,   // Monitor contínuo de TODOS os subsistemas
    MODO_FULL_DIAG
};

Modo  modoAtual = MODO_IDLE;

float uccAtual  = 0.0f;
float uelAtual  = 0.0f;
float refSolar  = 0.0f;   // Última referência enviada ao painel
float refVento  = 0.0f;   // Última referência enviada à turbina

// Telemetria contínua do monitor 'all'
float telPotPv  = 0.0f;
float telPotWt  = 0.0f;

// Flags e contadores
bool  h2RxOK    = false;
bool  solarOK   = false;
bool  ventoOK   = false;
bool  elOK      = false;
bool  ccOK      = false;

int   sweepStep = 0;
unsigned long ultimoEnvioH2_ms    = 0;
unsigned long ultimoSweepStep_ms  = 0;
unsigned long ultimoPoll_ms       = 0;
unsigned long ultimoMonitorAll_ms = 0;
int fullDiagEtapa                 = 0;

// ---------------------------------------------------------------------------
// Impressão
// ---------------------------------------------------------------------------

void separador(char c = '-', int n = 56) {
    for (int i = 0; i < n; i++) Serial.print(c);
    Serial.println();
}

void cabecalho(const char* titulo) {
    separador('=');
    Serial.print("  "); Serial.println(titulo);
    separador('=');
}

// ---------------------------------------------------------------------------
// Comunicação I2C
// ---------------------------------------------------------------------------

/**
 * Envia float de 4 bytes a um periférico I2C.
 * Retorna true se o envio foi confirmado (ACK do slave).
 * IMPORTANTE: Se retornar false, NÃO chame i2cLer() — o buffer I2C pode
 * conter dados stale de um periférico anterior, levando a leituras erradas.
 */
bool i2cEnviar(int addr, float valor) {
    byte buf[4];
    memcpy(buf, &valor, 4);
    Wire.beginTransmission(addr);
    Wire.write(buf, 4);
    byte err = Wire.endTransmission();
    if (err != 0) {
        // Não imprime no monitor all para não inundar — só imprime em testes explícitos
        return false;
    }
    return true;
}

/**
 * Lê float de 4 bytes de um periférico I2C.
 * Só deve ser chamada APÓS i2cEnviar() retornar true.
 * Retorna NaN se o slave não responder à leitura.
 */
float i2cLer(int addr) {
    byte  buf[4] = {0,0,0,0};
    int   n      = Wire.requestFrom(addr, 4);
    if (n < 4) return NAN;
    for (int i = 0; i < 4; i++) buf[i] = Wire.read();
    float val;
    memcpy(&val, buf, 4);
    return val;
}

/**
 * Versão segura: envia e lê apenas se o envio foi confirmado.
 * Retorna NaN se o dispositivo não estiver presente ou não responder.
 */
float i2cEnviarELer(int addr, float ref) {
    if (!i2cEnviar(addr, ref)) return NAN;  // Dispositivo não presente — não tenta ler
    return i2cLer(addr);
}

// ---------------------------------------------------------------------------
// Envio de comandos ao UNO H2
// ---------------------------------------------------------------------------

void enviarCmdH2() {
    const uint16_t tam = sizeof(R4ToUno_Data);
    memcpy(H2Transfer.packet.txBuff, (void*)&cmdH2, tam);
    H2Transfer.sendData(tam, ID_R4_TO_UNO);
}

/**
 * Tenta receber um pacote do H2 UNO.
 * GATEADA a no mínimo H2_RECV_INTERVAL_MS entre chamadas para evitar
 * que o SerialTransfer imprima 'STALE PACKET' quando called faster than Ts.
 * Retorna true se um pacote novo chegou.
 */
#define H2_RECV_INTERVAL_MS 100UL
bool receberH2() {
    static unsigned long ultimaRx_ms = 0;
    if (millis() - ultimaRx_ms < H2_RECV_INTERVAL_MS) return false;
    ultimaRx_ms = millis();

    if (H2Transfer.available()) {
        if (H2Transfer.currentPacketID() == ID_UNO_TO_R4) {
            memcpy(&telH2, H2Transfer.packet.rxBuff, H2Transfer.bytesRead);
            h2RxOK = true;
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Impressão de linha de telemetria completa
// ---------------------------------------------------------------------------

void imprimirTelemetriaAll() {
    // --- Solar ---
    Serial.print("  [SOL] ref="); Serial.print(refSolar, 2); Serial.print("V");
    Serial.print("  Pot_pv=");
    if (isnan(telPotPv)) Serial.print(" OFFLINE");
    else { Serial.print(telPotPv, 2); Serial.print("W"); }

    // Nota: 'ref' é setpoint do controlador LPV, não liga/desliga.
    // Pot_pv = 150 * Vfisico medido no A2. Com ref=0, o painel ainda mede tensão.

    // --- Vento ---
    Serial.print("   [VENT] ref="); Serial.print(refVento, 2); Serial.print("V");
    Serial.print("  Pot_wt=");
    if (isnan(telPotWt)) Serial.print(" OFFLINE");  // Turbina não conectada no I2C
    else { Serial.print(telPotWt, 2); Serial.print("W"); }

    // --- H2 ---
    Serial.print("   [H2] ");
    if (h2RxOK) {
        Serial.print("Pot_cc="); Serial.print(telH2.Pot_cc, 2); Serial.print("W");
        Serial.print("  Lvl=");  Serial.print(telH2.Level_H2, 1); Serial.print("mm");
        Serial.print("  uel=");  Serial.print((int)uelAtual);
        Serial.print(" ucc=");   Serial.print((int)uccAtual);
    } else {
        Serial.print("aguardando UNO...");
    }
    Serial.println();
}

// ---------------------------------------------------------------------------
// SCAN — I2C + H2 SerialTransfer
// ---------------------------------------------------------------------------

// Verifica presença de um addr I2C (ACK probe)
bool i2cPresente(int addr) {
    Wire.beginTransmission(addr);
    return Wire.endTransmission() == 0;
}

void cmdScan() {
    cabecalho("SCAN — VERIFICACAO DE CONEXOES");

    // --- I2C ---
    Serial.println("  [I2C] Escaneando enderecos 1..126...");
    int encontrados = 0;
    for (byte addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        byte err = Wire.endTransmission();
        if (err == 0) {
            Serial.print("    Encontrado: addr="); Serial.print(addr);
            if (addr == ADDR_PAINEL)  Serial.print("  <-- Painel Solar");
            if (addr == ADDR_TURBINA) Serial.print("  <-- Turbina Eolica");
            Serial.println();
            encontrados++;
        }
    }
    if (encontrados == 0) {
        Serial.println("    Nenhum dispositivo I2C encontrado! Verifique SDA/SCL.");
    }
    Serial.print("  [I2C] Total encontrado: "); Serial.println(encontrados);
    Serial.println();

    // Resumo I2C com leitura de valor real
    Serial.print("  Painel Solar   (I2C addr "); Serial.print(ADDR_PAINEL); Serial.print("): ");
    if (i2cPresente(ADDR_PAINEL)) {
        // Envia ref=0 e lê valor para confirmar que o protocolo funciona
        bool ok = i2cEnviar(ADDR_PAINEL, 0.0f);
        delay(80);
        float pv = ok ? i2cLer(ADDR_PAINEL) : NAN;
        Serial.print("CONECTADO");
        if (!isnan(pv)) { Serial.print(" — Pot_pv="); Serial.print(pv,2); Serial.print("W"); }
        Serial.println();
    } else {
        Serial.println("NAO ENCONTRADO — verifique SDA/SCL/alimentacao");
    }

    Serial.print("  Turbina Eolica (I2C addr "); Serial.print(ADDR_TURBINA); Serial.print("): ");
    if (i2cPresente(ADDR_TURBINA)) {
        bool ok = i2cEnviar(ADDR_TURBINA, 0.0f);
        delay(TURBINA_DELAY_MS);  // Turbina precisa de tempo para responder
        float wt = ok ? i2cLer(ADDR_TURBINA) : NAN;
        Serial.print("CONECTADO");
        if (!isnan(wt)) { Serial.print(" — Pot_wt="); Serial.print(wt,2); Serial.print("W"); }
        Serial.println();
    } else {
        Serial.println("NAO ENCONTRADO — verifique SDA/SCL/alimentacao");
    }

    // --- H2 SerialTransfer (Serial1) ---
    Serial.println();
    Serial.println("  [H2 Serial1] Enviando ping (ucc=0, uel=0) e aguardando resposta (1 s)...");
    cmdH2.ucc = 0.0f; cmdH2.uel = 0.0f;
    enviarCmdH2();

    unsigned long inicio = millis();
    bool h2Found = false;
    while (millis() - inicio < 1000) {
        if (receberH2()) { h2Found = true; break; }
    }

    Serial.print("  H2 UNO (Serial1): ");
    if (h2Found) {
        Serial.print("CONECTADO — Pot_cc="); Serial.print(telH2.Pot_cc, 2);
        Serial.print("W  Level_H2="); Serial.print(telH2.Level_H2, 1); Serial.println("mm");
    } else {
        Serial.println("NAO RESPONDEU (>1 s). Verifique cabo Serial1 e firmware do UNO H2.");
    }

    separador();
    Serial.println("  Resumo:");
    {
        Wire.beginTransmission(ADDR_PAINEL);
        Serial.print("    Painel Solar  : "); Serial.println(Wire.endTransmission() == 0 ? "OK" : "FALHA");
    }
    {
        Wire.beginTransmission(ADDR_TURBINA);
        Serial.print("    Turbina Eolica: "); Serial.println(Wire.endTransmission() == 0 ? "OK" : "FALHA");
    }
    Serial.print("    H2 UNO H2     : "); Serial.println(h2Found ? "OK" : "FALHA");
    separador();
}

// ---------------------------------------------------------------------------
// SOLAR SWEEP
// ---------------------------------------------------------------------------

/**
 * Envia uma referência ao painel e lê a potência imediatamente.
 * O painel solar responde rapidamente via I2C (filtro interno).
 */
bool testarSolarRef(float ref) {
    refSolar = ref;
    bool ok = i2cEnviar(ADDR_PAINEL, ref);
    if (!ok) {
        telPotPv = NAN;
        Serial.print("    ref="); Serial.print(ref, 2);
        Serial.println(" V  =>  Pot_pv= FALHA (I2C sem ACK)");
        return false;
    }
    delay(120);
    telPotPv = i2cLer(ADDR_PAINEL);
    Serial.print("    ref="); Serial.print(ref, 2);
    Serial.print(" V  =>  Pot_pv=");
    if (isnan(telPotPv)) { Serial.println(" FALHA (sem resposta)"); return false; }
    Serial.print(telPotPv, 3); Serial.println(" W");
    return true;
}

void iniciarSolarSweep() {
    cabecalho("TESTE PAINEL SOLAR (I2C addr 9)");
    Serial.println("  Sweep: 0.0 -> 3.0 V (7 passos, intervalo 400 ms)");
    sweepStep        = 0;
    ultimoSweepStep_ms = millis();
    modoAtual        = MODO_SOLAR_SWEEP;
    solarOK          = true;
}

void tickSolarSweep() {
    const float refs[] = {0.0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0};
    const int   nRefs  = 7;
    if (millis() - ultimoSweepStep_ms < 400) return;
    ultimoSweepStep_ms = millis();

    if (sweepStep < nRefs) {
        if (!testarSolarRef(refs[sweepStep])) solarOK = false;
        sweepStep++;
    } else {
        Serial.print("  Resultado Painel Solar: ");
        Serial.println(solarOK ? "OK" : "FALHA");
        separador();
        if (modoAtual == MODO_SOLAR_SWEEP) {
            modoAtual = MODO_IDLE;
            Serial.println("  (Digite 'help' para ver os comandos)");
        }
    }
}

// ---------------------------------------------------------------------------
// TURBINA SWEEP
// ---------------------------------------------------------------------------

/**
 * Envia referência à turbina e aguarda TURBINA_DELAY_MS antes de ler.
 * A turbina usa ESC físico com rampa — a potência não é instantânea.
 */
bool testarVentoRef(float ref) {
    refVento = ref;
    bool ok = i2cEnviar(ADDR_TURBINA, ref);
    if (!ok) {
        telPotWt = NAN;
        Serial.print("    ref="); Serial.print(ref, 2);
        Serial.println(" V  =>  Pot_wt= FALHA (I2C sem ACK — turbina nao conectada?)");
        return false;
    }
    // Aguarda a turbina física atingir o regime (rampa do ESC + medição)
    delay(TURBINA_DELAY_MS);
    telPotWt = i2cLer(ADDR_TURBINA);
    Serial.print("    ref="); Serial.print(ref, 2);
    Serial.print(" V  =>  Pot_wt=");
    if (isnan(telPotWt)) { Serial.println(" FALHA (sem resposta)"); return false; }
    Serial.print(telPotWt, 3); Serial.println(" W");
    return true;
}

void iniciarVentoSweep() {
    cabecalho("TESTE TURBINA EOLICA (I2C addr 10)");
    Serial.println("  Sweep: 0.0 -> 3.0 V (7 passos)");
    Serial.print("  NOTA: delay de "); Serial.print(TURBINA_DELAY_MS);
    Serial.println("ms por passo (rampa ESC fisica).");
    sweepStep          = 0;
    ultimoSweepStep_ms = millis() - TURBINA_DELAY_MS;  // Força 1o passo imediato
    modoAtual          = MODO_VENTO_SWEEP;
    ventoOK            = true;
}

void tickVentoSweep() {
    const float refs[] = {0.0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0};
    const int   nRefs  = 7;
    if (millis() - ultimoSweepStep_ms < (TURBINA_DELAY_MS + 200)) return;
    ultimoSweepStep_ms = millis();

    if (sweepStep < nRefs) {
        if (!testarVentoRef(refs[sweepStep])) ventoOK = false;
        sweepStep++;
    } else {
        // IMPORTANTE: desliga a turbina ao fim do sweep
        Serial.println("  Enviando ref=0 para desligar a turbina...");
        i2cEnviar(ADDR_TURBINA, 0.0f);
        refVento = 0.0f;
        Serial.print("  Resultado Turbina: ");
        Serial.println(ventoOK ? "OK" : "FALHA");
        separador();
        if (modoAtual == MODO_VENTO_SWEEP) {
            modoAtual = MODO_IDLE;
            Serial.println("  (Digite 'help' para ver os comandos)");
        }
    }
}

// ---------------------------------------------------------------------------
// MONITOR ALL — todos os subsistemas em paralelo
// ---------------------------------------------------------------------------

void iniciarMonitorAll() {
    cabecalho("MONITOR CONTINUO — TODOS OS SUBSISTEMAS");
    Serial.println("  Imprime solar + vento + H2 a cada 800 ms.");
    Serial.println("  Use 'el1'/'el0'/'cc1'/'cc0' para controlar H2.");
    Serial.println("  Use 'sol XX.X' ou 'vent XX.X' para mudar referencias.");
    Serial.println("  Use 'stop' para parar.");
    separador();
    modoAtual          = MODO_ALL_MONITOR;
    ultimoMonitorAll_ms = millis();
    ultimoEnvioH2_ms   = millis();
    ultimoPoll_ms      = millis();  // Para watchdog de sem resposta H2
    h2RxOK             = false;
}

void tickMonitorAll() {
    // --- Envio H2 a cada 200 ms ---
    if (millis() - ultimoEnvioH2_ms >= 200) {
        ultimoEnvioH2_ms = millis();
        cmdH2.ucc = uccAtual;
        cmdH2.uel = uelAtual;
        enviarCmdH2();
    }

    // --- Leitura H2 (gateada a 100 ms para evitar STALE PACKET) ---
    receberH2();

    // --- Leitura solar + vento + impressão a cada 800 ms ---
    if (millis() - ultimoMonitorAll_ms >= 800) {
        ultimoMonitorAll_ms = millis();

        // Lê solar: só lê se envio foi confirmado (ACK)
        // Painel retorna Pot_pv = 150 * Vfisico independente da ref (ref é setpoint LPV)
        bool solOk = i2cEnviar(ADDR_PAINEL, refSolar);
        delay(60);
        telPotPv = solOk ? i2cLer(ADDR_PAINEL) : NAN;

        // Lê vento: só lê se envio foi confirmado (evita stale data do painel)
        // A turbina usa ESC físico — Pot_wt demora segundos para estabilizar após ref mudar
        bool ventOk = i2cEnviar(ADDR_TURBINA, refVento);
        delay(60);
        telPotWt = ventOk ? i2cLer(ADDR_TURBINA) : NAN;

        imprimirTelemetriaAll();
        ultimoPoll_ms = millis();
    }

    // Aviso se H2 não responde por mais de 3 s
    if (!h2RxOK && millis() - ultimoPoll_ms >= 3000) {
        ultimoPoll_ms = millis();
        Serial.println("  [H2] Sem resposta do UNO (>3 s). Verifique cabo Serial1.");
    }
}

// ---------------------------------------------------------------------------
// DIAGNÓSTICO COMPLETO
// ---------------------------------------------------------------------------

void iniciarFullDiag() {
    cabecalho("DIAGNOSTICO COMPLETO — TODOS OS SUBSISTEMAS");
    fullDiagEtapa      = 0;
    sweepStep          = 0;
    solarOK  = ventoOK = h2RxOK = elOK = ccOK = false;
    modoAtual          = MODO_FULL_DIAG;
    ultimoSweepStep_ms = millis();
    ultimoEnvioH2_ms   = millis();
    ultimoPoll_ms      = millis();
}

void tickFullDiag() {

    // --- Etapa 0: Scan ---
    if (fullDiagEtapa == 0) {
        cmdScan();
        fullDiagEtapa = 1;
        cabecalho("ETAPA 1/5 — PAINEL SOLAR");
        solarOK = true; sweepStep = 0;
        ultimoSweepStep_ms = millis();
        return;
    }

    // --- Etapa 1: Sweep solar (4 pontos) ---
    if (fullDiagEtapa == 1) {
        const float refs[] = {0.0, 1.0, 2.0, 3.0};
        if (millis() - ultimoSweepStep_ms >= 400) {
            ultimoSweepStep_ms = millis();
            if (sweepStep < 4) {
                if (!testarSolarRef(refs[sweepStep])) solarOK = false;
                sweepStep++;
            } else {
                Serial.print("  Painel Solar: "); Serial.println(solarOK ? "OK" : "FALHA");
                separador();
                fullDiagEtapa = 2;
                cabecalho("ETAPA 2/5 — TURBINA EOLICA");
                Serial.print("  NOTA: cada passo demora ~"); Serial.print(TURBINA_DELAY_MS); Serial.println(" ms.");
                ventoOK = true; sweepStep = 0;
                ultimoSweepStep_ms = millis() - TURBINA_DELAY_MS;
            }
        }
        return;
    }

    // --- Etapa 2: Sweep vento (4 pontos) + desliga ao final ---
    if (fullDiagEtapa == 2) {
        const float refs[] = {0.0, 1.0, 2.0, 3.0};
        if (millis() - ultimoSweepStep_ms >= (TURBINA_DELAY_MS + 200)) {
            ultimoSweepStep_ms = millis();
            if (sweepStep < 4) {
                if (!testarVentoRef(refs[sweepStep])) ventoOK = false;
                sweepStep++;
            } else {
                // Desliga a turbina
                Serial.println("  Enviando ref=0 (desligando turbina)...");
                i2cEnviar(ADDR_TURBINA, 0.0f);
                refVento = 0.0f;
                Serial.print("  Turbina Eolica: "); Serial.println(ventoOK ? "OK" : "FALHA");
                separador();
                fullDiagEtapa = 3;
                cabecalho("ETAPA 3/5 — H2: COMUNICACAO BASE (3 s)");
                cmdH2.ucc = 0.0f; cmdH2.uel = 0.0f;
                ultimoPoll_ms    = millis();
                ultimoEnvioH2_ms = millis() - 500;
                h2RxOK = false; sweepStep = 0;
            }
        }
        return;
    }

    // --- Etapa 3: H2 comunicação base ---
    if (fullDiagEtapa == 3) {
        if (millis() - ultimoEnvioH2_ms >= 200) {
            ultimoEnvioH2_ms = millis();
            cmdH2.ucc = 0.0f; cmdH2.uel = 0.0f;
            enviarCmdH2();
        }
        if (receberH2()) {
            sweepStep++;
            Serial.print("    [H2] pkt#"); Serial.print(sweepStep);
            Serial.print("  Pot_cc="); Serial.print(telH2.Pot_cc, 2);
            Serial.print("W  Level_H2="); Serial.print(telH2.Level_H2, 1); Serial.println("mm");
        }
        if (millis() - ultimoPoll_ms >= 3000) {
            Serial.print("  H2 Comunicacao ("); Serial.print(sweepStep);
            Serial.print(" pkt rcv): "); Serial.println(h2RxOK ? "OK" : "FALHA (sem pacote)");
            separador();
            fullDiagEtapa = 4;
            cabecalho("ETAPA 4/5 — H2: ELETROLISADOR (uel=1 por 2 s)");
            cmdH2.ucc = 0.0f; cmdH2.uel = 1.0f;
            ultimoPoll_ms = millis();
            ultimoEnvioH2_ms = millis() - 500;
            elOK = false;
        }
        return;
    }

    // --- Etapa 4: Eletrolisador ligado 2 s ---
    if (fullDiagEtapa == 4) {
        if (millis() - ultimoEnvioH2_ms >= 200) {
            ultimoEnvioH2_ms = millis();
            cmdH2.ucc = 0.0f; cmdH2.uel = 1.0f;
            enviarCmdH2();
        }
        if (receberH2()) {
            elOK = true;
            if (millis() - ultimoPoll_ms >= 500) {
                ultimoPoll_ms = millis();
                Serial.print("    [EL ON] Pot_cc="); Serial.print(telH2.Pot_cc,2);
                Serial.print("W  Level_H2="); Serial.print(telH2.Level_H2,1); Serial.println("mm");
            }
        }
        if (millis() - ultimoPoll_ms >= 2500) {
            cmdH2.ucc = 0.0f; cmdH2.uel = 0.0f; enviarCmdH2();
            Serial.print("  Eletrolisador: "); Serial.println(elOK ? "OK" : "FALHA");
            separador();
            fullDiagEtapa = 5;
            cabecalho("ETAPA 5/5 — H2: CELULA COMBUSTIVEL (ucc=1 por 2 s)");
            cmdH2.ucc = 1.0f; cmdH2.uel = 0.0f;
            ultimoPoll_ms = millis();
            ultimoEnvioH2_ms = millis() - 500;
            ccOK = false;
        }
        return;
    }

    // --- Etapa 5: Célula aberta 2 s ---
    if (fullDiagEtapa == 5) {
        if (millis() - ultimoEnvioH2_ms >= 200) {
            ultimoEnvioH2_ms = millis();
            cmdH2.ucc = 1.0f; cmdH2.uel = 0.0f;
            enviarCmdH2();
        }
        if (receberH2()) {
            if (telH2.Pot_cc > 0.0f) ccOK = true;
            if (millis() - ultimoPoll_ms >= 500) {
                ultimoPoll_ms = millis();
                Serial.print("    [CC ON] Pot_cc="); Serial.print(telH2.Pot_cc,3);
                Serial.print("W  Level_H2="); Serial.print(telH2.Level_H2,1); Serial.println("mm");
            }
        }
        if (millis() - ultimoPoll_ms >= 2500) {
            cmdH2.ucc = 0.0f; cmdH2.uel = 0.0f; enviarCmdH2();
            Serial.print("  Celula Combustivel: ");
            Serial.println(ccOK ? "OK (Pot_cc>0)" : "ATENCAO (Pot_cc=0 — verifique sensor/servo)");
            separador();
            fullDiagEtapa = 99;
        }
        return;
    }

    // --- Resumo ---
    if (fullDiagEtapa == 99) {
        separador('=');
        Serial.println("  RESUMO DO DIAGNOSTICO");
        separador('=');
        Serial.print("  Painel Solar    : "); Serial.println(solarOK ? "OK" : "FALHA");
        Serial.print("  Turbina Eolica  : "); Serial.println(ventoOK ? "OK" : "FALHA");
        Serial.print("  H2 Comunicacao  : "); Serial.println(h2RxOK  ? "OK" : "FALHA");
        Serial.print("  Eletrolisador   : "); Serial.println(elOK    ? "OK" : "FALHA");
        Serial.print("  Celula Comb.    : "); Serial.println(ccOK    ? "OK" : "ATENCAO");
        separador('=');
        bool tudo = solarOK && ventoOK && h2RxOK && elOK;
        Serial.println(tudo ? "  SISTEMA PRONTO para rodar o MATLAB!"
                            : "  ATENCAO: Ha falhas. Corrija antes do MATLAB.");
        separador('=');
        modoAtual = MODO_IDLE;
        // Garante que turbina ficou desligada (já foi feito na etapa 2, mas confirma)
        i2cEnviar(ADDR_TURBINA, 0.0f);
        cmdH2.ucc = 0.0f; cmdH2.uel = 0.0f; enviarCmdH2();
        uccAtual = uelAtual = 0.0f;
        Serial.println("  (Digite 'help' para mais testes)");
    }
}

// ---------------------------------------------------------------------------
// HELP
// ---------------------------------------------------------------------------

void imprimirHelp() {
    cabecalho("DIAGNOSTICO R4 v2 — COMANDOS");
    Serial.println("  Verificacao:");
    Serial.println("    scan          Escaneia I2C + verifica H2 via SerialTransfer");
    Serial.println("    full          Diagnostico completo sequencial");
    Serial.println();
    Serial.println("  Testes individuais:");
    Serial.println("    solar         Sweep 0->3 V no painel solar");
    Serial.println("    vento         Sweep 0->3 V na turbina (desliga no fim)");
    Serial.println("    all           Monitor continuo de todos os subsistemas");
    Serial.println();
    Serial.println("  Referencias manuais:");
    Serial.println("    sol  XX.X     Envia ref XX.X V ao painel solar");
    Serial.println("    vent XX.X     Envia ref XX.X V a turbina eolica");
    Serial.println("    refs XX.X     Envia ref XX.X V a solar E vento");
    Serial.println();
    Serial.println("  Controle H2 (funciona em qualquer modo):");
    Serial.println("    el1 / el0     Liga / desliga eletrolisador");
    Serial.println("    cc1 / cc0     Abre / fecha celula de combustivel");
    Serial.println("    h2            Alias de 'all' (inicia monitor completo)");
    Serial.println();
    Serial.println("  Outros:");
    Serial.println("    stop          Para tudo e volta ao idle (zera uel, ucc, turbina)");
    separador();
}

// ---------------------------------------------------------------------------
// PARSER DE COMANDOS
// ---------------------------------------------------------------------------

void processarComando(String cmd) {
    cmd.trim();
    cmd.toLowerCase();

    if (cmd == "help" || cmd == "h" || cmd == "?") {
        imprimirHelp();

    } else if (cmd == "scan") {
        modoAtual = MODO_IDLE;
        cmdScan();

    } else if (cmd == "solar") {
        modoAtual = MODO_IDLE;
        iniciarSolarSweep();

    } else if (cmd == "vento") {
        modoAtual = MODO_IDLE;
        iniciarVentoSweep();

    } else if (cmd == "all" || cmd == "h2") {
        iniciarMonitorAll();

    } else if (cmd == "el1") {
        uelAtual = 1.0f; uccAtual = 0.0f;
        cmdH2.uel = 1.0f; cmdH2.ucc = 0.0f;
        enviarCmdH2();
        Serial.println("  >> Eletrolisador LIGADO (uel=1, ucc=0)");
        if (modoAtual != MODO_ALL_MONITOR) iniciarMonitorAll();

    } else if (cmd == "el0") {
        uelAtual = 0.0f;
        cmdH2.uel = 0.0f; cmdH2.ucc = uccAtual;
        enviarCmdH2();
        Serial.println("  >> Eletrolisador DESLIGADO (uel=0)");

    } else if (cmd == "cc1") {
        uccAtual = 1.0f; uelAtual = 0.0f;
        cmdH2.ucc = 1.0f; cmdH2.uel = 0.0f;
        enviarCmdH2();
        Serial.println("  >> Celula combustivel ABERTA (ucc=1, uel=0)");
        if (modoAtual != MODO_ALL_MONITOR) iniciarMonitorAll();

    } else if (cmd == "cc0") {
        uccAtual = 0.0f;
        cmdH2.ucc = 0.0f; cmdH2.uel = uelAtual;
        enviarCmdH2();
        Serial.println("  >> Celula combustivel FECHADA (ucc=0)");

    } else if (cmd == "full") {
        modoAtual = MODO_IDLE;
        iniciarFullDiag();

    } else if (cmd == "stop") {
        // Para tudo e garante estado seguro
        uccAtual = 0.0f; uelAtual = 0.0f;
        cmdH2.ucc = 0.0f; cmdH2.uel = 0.0f;
        enviarCmdH2();
        i2cEnviar(ADDR_TURBINA, 0.0f);
        refVento = 0.0f;
        modoAtual = MODO_IDLE;
        Serial.println("  PARADO. uel=0 ucc=0 ref_vento=0 enviados. Modo: IDLE.");

    } else if (cmd.startsWith("refs ")) {
        float ref = cmd.substring(5).toFloat();
        refSolar = ref; refVento = ref;
        bool solOk  = i2cEnviar(ADDR_PAINEL,  ref);
        bool ventOk = i2cEnviar(ADDR_TURBINA, ref);
        delay(120);
        telPotPv = solOk  ? i2cLer(ADDR_PAINEL)  : NAN;
        telPotWt = ventOk ? i2cLer(ADDR_TURBINA) : NAN;
        Serial.print("  ref="); Serial.print(ref, 2); Serial.print(" V  =>  ");
        Serial.print("Pot_pv="); if (isnan(telPotPv)) Serial.print("OFFLINE"); else Serial.print(telPotPv, 2);
        Serial.print(" W  |  Pot_wt="); if (isnan(telPotWt)) Serial.print("OFFLINE"); else Serial.print(telPotWt, 2);
        Serial.println(" W");
        Serial.println("  NOTA: Pot_wt da turbina demora vários s para estabilizar (ESC fisico).");
        if (!solOk)  Serial.println("  ATENCAO: Painel Solar nao respondeu (I2C sem ACK).");
        if (!ventOk) Serial.println("  ATENCAO: Turbina nao respondeu (I2C sem ACK). Turbina conectada?");

    } else if (cmd.startsWith("sol ")) {
        float ref = cmd.substring(4).toFloat();
        refSolar = ref;
        bool ok = i2cEnviar(ADDR_PAINEL, ref);
        delay(120);
        telPotPv = ok ? i2cLer(ADDR_PAINEL) : NAN;
        Serial.print("  [SOLAR] ref="); Serial.print(ref, 2);
        Serial.print(" V  =>  Pot_pv=");
        if (!ok) Serial.println("OFFLINE (I2C sem ACK)");
        else if (isnan(telPotPv)) Serial.println("FALHA (sem resposta)");
        else { Serial.print(telPotPv, 2); Serial.println(" W"); }
        Serial.println("  NOTA: ref controla o setpoint LPV, nao e um interruptor de potencia.");

    } else if (cmd.startsWith("vent ")) {
        float ref = cmd.substring(5).toFloat();
        refVento = ref;
        bool ok = i2cEnviar(ADDR_TURBINA, ref);
        Serial.print("  [VENTO] ref="); Serial.print(ref, 2);
        if (!ok) Serial.println(" V  =>  OFFLINE (I2C sem ACK — turbina conectada?)");
        else     Serial.println(" V enviado. Pot_wt nao e instantanea (ESC fisico). Use 'all'.");

    } else if (cmd.length() > 0) {
        Serial.print("  Desconhecido: '"); Serial.print(cmd);
        Serial.println("' — Digite 'help'.");
    }
}

// ---------------------------------------------------------------------------
// SETUP e LOOP
// ---------------------------------------------------------------------------

void setup() {
    Serial.begin(BAUD_RATE_USB);
    while (!Serial && millis() < 3000);

    Wire.begin();

    Serial1.begin(BAUD_RATE_UNO);
    H2Transfer.begin(Serial1, false);  // false = desabilita debug interno (evita 'STALE PACKET')

    cmdH2.ucc = 0.0f;
    cmdH2.uel = 0.0f;

    cabecalho("DIAGNOSTICO CENTRAL R4 v2 — MICRORREDE INCT CAPE");
    Serial.println("  Digite 'help'  → lista de comandos");
    Serial.println("  Digite 'scan'  → verifica conexoes");
    Serial.println("  Digite 'full'  → diagnostico completo");
    Serial.println("  Digite 'all'   → monitor continuo de todos os subsistemas");
    separador();
}

void loop() {
    // Leitura do Monitor Serial (nao-bloqueante)
    if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        processarComando(input);
    }

    // Maquina de estados
    switch (modoAtual) {
        case MODO_SOLAR_SWEEP:  tickSolarSweep();  break;
        case MODO_VENTO_SWEEP:  tickVentoSweep();  break;
        case MODO_ALL_MONITOR:  tickMonitorAll();   break;
        case MODO_FULL_DIAG:    tickFullDiag();     break;
        default: break;
    }
}
