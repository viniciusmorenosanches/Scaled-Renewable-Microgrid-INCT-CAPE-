/**
 * debug_display_gc9a01.ino — Teste Dedicado do Display Redondo GC9A01 (1.28" TFT SPI)
 *
 * Mapeamento dos pinos (Sequência configurada):
 *   - SCL / SCK   -> Pino 13 (Hardware SPI SCK)
 *   - SDA / MOSI  -> Pino 11 (Hardware SPI MOSI)
 *   - CS          -> Pino 10 (Chip Select)
 *   - DC          -> Pino  9 (Data / Command)
 *   - RST         -> Pino  8 (Reset)
 *   - VCC         -> 5V (ou 3.3V)
 *   - GND         -> GND
 *   - BLK / LED   -> 3.3V (ou 5V com resistor de 100 ohms)
 *
 * NOTA SOBRE A TELA APAGAR / FICAR ESCURA:
 *   - Se a tela apaga após alguns segundos, verifique o pino BLK (Backlight):
 *     1. Se BLK estiver desconectado, o backlight desliga sozinho por falta de polarização.
 *     2. Conecte o pino BLK firmemente ao 3.3V (ou 5V).
 *     3. Evite alimentar VCC e BLK ao mesmo tempo do pino 3.3V do Arduino Uno, pois o regulador
 *        de 3.3V do Uno aquece e entra em corte térmico. Use 5V no VCC do display!
 */

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>

// ---------------------------------------------------------------------------
// Definição dos Pinos
// ---------------------------------------------------------------------------
#define TFT_SCL 13  // SCK (Hardware SPI)
#define TFT_SDA 11  // MOSI (Hardware SPI)
#define TFT_CS  10  // Chip Select
#define TFT_DC   9  // Data / Command
#define TFT_RST  8  // Reset

// ---------------------------------------------------------------------------
// Cores RGB565
// ---------------------------------------------------------------------------
#define COLOR_BLACK       0x0000
#define COLOR_DARKGREY    0x39E7
#define COLOR_LIGHTGREY   0xC618
#define COLOR_BLUE        0x001F
#define COLOR_GREEN       0x07E0
#define COLOR_CYAN        0x07FF
#define COLOR_RED         0xF800
#define COLOR_YELLOW      0xFFE0
#define COLOR_WHITE       0xFFFF
#define COLOR_BG          0x0841  // Fundo cinza escuro

// Instância do display
Adafruit_GC9A01A tft(TFT_CS, TFT_DC, TFT_RST);

// ---------------------------------------------------------------------------
// Funções Gráficas Estáticas (Desenhadas uma única vez)
// ---------------------------------------------------------------------------

void desenharPainelFixo() {
    tft.fillScreen(COLOR_BG);

    // Bordas circulares concêntricas
    tft.drawCircle(120, 120, 118, COLOR_CYAN);
    tft.drawCircle(120, 120, 116, COLOR_DARKGREY);
    tft.drawCircle(120, 120, 114, COLOR_WHITE);

    // Título Superior
    tft.setTextColor(COLOR_WHITE);
    tft.setTextSize(2);
    tft.setCursor(55, 40);
    tft.print(F("MICRORREDE"));

    tft.setTextColor(COLOR_YELLOW);
    tft.setTextSize(1);
    tft.setCursor(75, 65);
    tft.print(F("GC9A01 1.28\""));

    // Linha horizontal decorativa
    tft.drawFastHLine(50, 80, 140, COLOR_DARKGREY);

    // Unidade de Medida
    tft.setTextColor(COLOR_LIGHTGREY);
    tft.setTextSize(2);
    tft.setCursor(90, 160);
    tft.print(F("WATTS"));

    // Status Inferior
    tft.setTextColor(COLOR_GREEN);
    tft.setTextSize(1);
    tft.setCursor(85, 190);
    tft.print(F("STATUS: OK"));
}

// ---------------------------------------------------------------------------
// Atualização Dinâmica Não-Bloqueante (Baixo consumo de RAM para ATmega328P)
// ---------------------------------------------------------------------------

void atualizarValor(int valor, uint16_t cor) {
    // 1. Limpa apenas a caixa delimitadora central do valor numérico
    tft.fillRect(55, 95, 130, 45, COLOR_BG);

    // 2. Escreve o novo número
    tft.setTextColor(cor);
    tft.setTextSize(4);
    
    if (valor < 10) {
        tft.setCursor(105, 105);
    } else if (valor < 100) {
        tft.setCursor(85, 105);
    } else {
        tft.setCursor(65, 105);
    }
    tft.print(valor);

    // 3. Desenha os pontos do arco inferior (0 a 100)
    int pontosAtivos = map(constrain(valor, 0, 100), 0, 100, 0, 15);
    for (int i = 0; i <= 15; i++) {
        // Arco de 150 a 390 graus (arco inferior de 240 graus)
        float rad = (150 + i * 16) * 0.0174533;
        int x = 120 + cos(rad) * 98;
        int y = 120 + sin(rad) * 98;
        uint16_t c = (i <= pontosAtivos) ? cor : COLOR_DARKGREY;
        tft.fillCircle(x, y, 3, c);
    }
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000);

    Serial.println(F("\n========================================================"));
    Serial.println(F("   TESTE DEDICADO DO DISPLAY REDONDO GC9A01 (1.28\" TFT) "));
    Serial.println(F("========================================================"));
    Serial.println(F("  Pinos: SCL=13, SDA=11, CS=10, DC=9, RST=8"));

    // Pulso de Reset de Hardware explícito
    pinMode(TFT_RST, OUTPUT);
    digitalWrite(TFT_RST, HIGH);
    delay(20);
    digitalWrite(TFT_RST, LOW);
    delay(20);
    digitalWrite(TFT_RST, HIGH);
    delay(100);

    Serial.println(F("  Inicializando GC9A01A..."));
    tft.begin();
    tft.setRotation(0);

    // Teste inicial rápido de cores (200ms cada)
    Serial.println(F("  >> Testando RGB..."));
    tft.fillScreen(COLOR_RED);
    delay(250);
    tft.fillScreen(COLOR_GREEN);
    delay(250);
    tft.fillScreen(COLOR_BLUE);
    delay(250);

    // Desenha a interface
    desenharPainelFixo();
    Serial.println(F("  [✓] Painel pronto! Iniciando animacao..."));
}

// ---------------------------------------------------------------------------
// Loop Principal
// ---------------------------------------------------------------------------

int  contadorValor = 0;
bool incrementando = true;
unsigned long ultimoPasso_ms = 0;

void loop() {
    // Atualiza a cada 100 ms (10 Hz)
    if (millis() - ultimoPasso_ms >= 100) {
        ultimoPasso_ms = millis();

        if (incrementando) {
            contadorValor += 2;
            if (contadorValor >= 100) {
                incrementando = false;
            }
        } else {
            contadorValor -= 2;
            if (contadorValor <= 0) {
                incrementando = true;
            }
        }

        // Cor dinâmica
        uint16_t cor = COLOR_GREEN;
        if (contadorValor > 70) {
            cor = COLOR_RED;
        } else if (contadorValor > 35) {
            cor = COLOR_YELLOW;
        } else if (contadorValor > 10) {
            cor = COLOR_CYAN;
        }

        atualizarValor(contadorValor, cor);

        // Heartbeat no Serial para comprovar que o Arduino continua rodando
        if (contadorValor % 20 == 0) {
            Serial.print(F("[Heartbeat OK] Valor atual: "));
            Serial.print(contadorValor);
            Serial.println(F(" Watts"));
        }
    }
}
