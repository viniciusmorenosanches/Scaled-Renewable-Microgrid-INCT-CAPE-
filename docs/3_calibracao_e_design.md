# 3. Calibração e Decisões de Projeto — Mini Microrrede INCT CAPE

Este documento unifica as constantes físicas de calibração do sistema, as instruções experimentais de bancada e as decisões técnicas de projeto que justificam a estrutura atual do firmware.

---

## 1. Mapeamento Geral e Arquitetura de Hardware

### Por que o R4 usa I2C para painel e turbina, mas SerialTransfer para o H2?
O Arduino Uno do sistema de hidrogênio (H2) precisa se comunicar com o sensor de distância VL6180X usando o protocolo I2C. Como a especificação padrão do I2C define apenas um barramento com um mestre e múltiplos escravos, o Uno do H2 atua como mestre I2C para consultar o sensor.
Para se conectar ao Arduino R4 Central (que é o mestre geral da rede), o Uno do H2 não poderia atuar como escravo I2C no mesmo circuito físico. Por este motivo, a comunicação entre o R4 e o Uno H2 foi implementada via porta serial UART (pinos TX1/RX1) utilizando a biblioteca `SerialTransfer`, que fornece enquadramento de dados com checksum e proteção de integridade.
Os Unos do painel solar e da turbina eólica, por não possuírem sensores I2C locais adicionais, comunicam-se normalmente como escravos I2C nos endereços 9 e 10, respectivamente.

### Leitura Assíncrona do Controlador (MATLAB/Web App)
O R4 recebe pacotes a cada 100 ms, mas os relógios do computador (MATLAB ou Python) e do microcontrolador não estão perfeitamente sincronizados. Para evitar latência ou perda de frames, a função `receberMatlab()` lê a porta serial de forma assíncrona a cada iteração do `loop()` principal. O bloco temporizado de `Ts = 100 ms` executa as rotinas internas e I2C de forma determinística, mas os dados de entrada são atualizados em tempo real conforme chegam.

---

## 2. Parâmetros Globais do Arduino R4 Central (`firmware_central_r4`)

### Modelo de Estado de Carga (SoC) da Bateria
A dinâmica da bateria de escala (nominal de 48 V / 100 Ah, com capacidade total de $E_{nom} = 4800$ Wh) é simulada discretamente:
$$SoC[k+1] = \omega \cdot SoC[k] + a \cdot P_c[k] - b \cdot P_d[k]$$

*   **`SOC_PERDA = 0.995` ($\omega$)**: Fator de auto-descarga passiva por ciclo (100 ms). Ajustável por medições em circuito aberto.
*   **`SOC_COEF_CARGA = 3.92e-5` ($a$)**: Coeficiente de ganho de carga, incorporando eficiência do carregador ($\eta_c = 0.95$):
    $$a = \eta_c \cdot \frac{T_s}{3600} \cdot \frac{P_{escala}}{E_{nom}}$$
*   **`SOC_COEF_DESCARGA = 4.08e-5` ($b$)**: Coeficiente de ganho de descarga, com eficiência do descarregador ($\eta_d = 0.95$):
    $$b = \frac{1}{\eta_d} \cdot \frac{T_s}{3600} \cdot \frac{P_{escala}}{E_{nom}}$$

### Watchdog de Segurança
*   **`TIMEOUT_WATCHDOG_MS = 2000`**: Se o computador (MATLAB ou Web App) parar de transmitir pacotes por mais de 2.0 segundos (por falha de software, cabo desconectado, etc.), o R4 Central zera todos os comandos (`Pc=0`, `Pd=0`, `ucc=0`, `uel=0`, referências eólicas/solares a zero) para proteger o hardware físico contra comandos estacionários.

### LED Matrix e Visualização do SoC
*   **`NUM_LEDS = 24` e `NUM_FILEIRAS = 2.0f`**: O R4 calcula o número de LEDs proporcionais ao SoC utilizando `arredondaPraBaixo(SoC)` ($SoC \cdot NUM\_LEDS$). Esta lógica está reservada para futuras implementações visuais na matriz de LEDs integrada da placa R4 WiFi.

---

## 3. Painel Solar (`firmware_painel_solar`)

### Calibração de Potência Solar
*   **`ESCALA_POTENCIA_SOLAR = 150.0`**: Fator experimental de conversão linear entre a tensão filtrada e a potência teórica ($P_{pv} = 150 \cdot V_{fil}$), adaptado para o perfil solar de calibração.
*   **Status / Ajuste Futuro**: Esta é uma escala temporária. Quando a curva real I-V do painel solar físico for identificada e traçada em bancada, esta conversão linear simplificada deverá ser substituída por uma tabela de consulta (look-up table) ou polinômio físico de calibração.

### Filtro de Tensão Solar
*   **`ALPHA_FILTRO_TENSAO = 0.05`**: Coeficiente do filtro de média móvel exponencial (EMA). A tensão do conversor do painel é inerentemente mais estável, logo o filtro exponencial simples é suficiente para eliminar ruídos de leitura de alta frequência sem introduzir atraso de fase excessivo no controle LPV.

### Controlador LPV do Painel (Variantes de Ganho)
Os parâmetros de interpolação LPV (`TAU_0, TAU_1, KE_0, KE_1, Y0, Y1, W0, W1`) são gerados pela síntese matemática executada no MATLAB.
*   **Aviso Importante**: Não altere estes parâmetros individualmente no firmware. Eles dependem diretamente da identificação de modelo do sistema de conversão de potência do painel. Modificações avulsas causam perda de estabilidade ou divergência de malha de controle.
*   **Anti-windup**: Quando a saída de controle satura nos limites físicos do hardware ($0$ V a $5$ V), o acumulador do erro integrativo do PI é congelado no passo atual para evitar sobressinal (overshoot) excessivo na transição de saída da saturação.

---

## 4. Turbina Eólica (`firmware_turbina_eolica`)

### Divisor de Tensão e Entrada ADC
*   **`GANHO_DIVISOR_TENSAO = 4.3`**: A tensão nominal de geração da turbina excede o range de leitura analógica de 5V do Arduino Uno. Um divisor resistivo de tensão ($43\text{ k}\Omega$ em série com $10\text{ k}\Omega$) escala a tensão real. O fator multiplicativo de conversão é:
    $$V_{real} = V_{medida} \cdot \frac{R_1 + R_2}{R_2}$$
    Para ajustar se os resistores físicos variarem, atualize o ganho de calibração medindo-os com multímetro.

### Filtro de Kalman da Turbina (`Y_raw` vs `Yf`)
Diferente do painel solar, as flutuações aerodinâmicas e mecânicas da turbina eólica produzem variações dinâmicas rápidas no ADC (`Y_raw` no pino `A3`). Usamos um filtro/observador de Kalman escalar:
*   **`RUIDO_PROCESSO_Q = 0.1`**: Confiança no modelo dinâmico do motor ($x_{pred} = (1 - T_s/\tau) x_{est} + (T_s/\tau) K_e U$).
*   **`RUIDO_MEDICAO_R = 0.3`**: Variância estimada do ruído de leitura do ADC ($K \approx 0.35$).
*   **Diagnóstico (`Y_raw` no debug serial):** Como o filtro de Kalman retém ~65% da predição do modelo, mesmo se o fio do sensor no pino `A3` desconectar (`Y_raw = 0.00 V`), o observador ainda gera uma saída estimada proporcional ao comando (`Yf ≈ 0.30 · U`, chegando a `~1.49 V` em `U = 5.0 V`). Sempre monitore `Y_raw` no Monitor Serial para confirmar que a leitura física está chegando em `A3`.

### Calibração de Potência Eólica (Dois Regimes)
A equação física aerodinâmica é $P_{fisica} = 0.5 \cdot \rho_{ar} \cdot \pi \cdot r^2 \cdot C_p \cdot Y_f^3 = 0.01515 \cdot Y_f^3$.
*   **Regime A — Observador de Kalman (`Y_raw ≈ 0 V`, $Y_f \in [0,\; 1.50\text{ V}]$):** Com **`ESCALA_TEMPORARIA_POTENCIA = 4900.0`** e **`OFFSET_TEMPORARIO_POTENCIA_W = 0.0`**, o pico $Y_f = 1.50\text{ V}$ ($0.051\text{ W}$ físicos) é mapeado para $0\text{--}250\text{ W}$.
*   **Regime B — Sensor Físico Conectado em `A3` ($Y_{raw} \in [2.0,\; 4.5\text{ V}]$):** Quando o divisor resistivo real ($43\text{k}/10\text{k}$) estiver entregando de $2\text{ V}$ a $5\text{ V}$ no pino `A3`, reduza a escala (ex.: `ESCALA = 50.0, OFFSET = 150.0` ou `ESCALA = 120.0, OFFSET = 0.0`) para não saturar a potência acima da faixa da microrrede.

### Controle do Motor (ESC), Anti-Windup Direcional e Rampa de Partida
*   **`GANHO_ESC_POR_VOLT = 38.0`**: Converte a tensão de controle teórica em comando angular da biblioteca `Servo` para regular o ESC da turbina.
*   **Re-disparo Dinâmico da Rampa de Partida (`RAMPA_PARTIDA_ESC[60]`)**: Sempre que a referência sai de zero (`refAnterior_V == 0 && ref > 0`), o contador `iteracao` é reiniciado para executar a aceleração progressiva de 6 segundos (60 passos de 100 ms), rompendo o atrito estático de forma suave.
*   **Anti-Windup Direcional:** Congela o acumulador integral do PI apenas quando o atuador satura na mesma direção do erro (`U > 5.0 V` com erro positivo, ou `U < 1.0 V` com erro negativo), permitindo sair imediatamente dos extremos sem travar o controle.
*   **Remoção do Offset +2V:** O antigo ajuste `+2.0 V` na recepção I2C foi removido para permitir que a turbina rastreie dinamicamente todo o perfil de vento.

### Fator de Escala LPV ($\times 0.5$)
*   **Ajuste de Ganho**: Durante os testes experimentais originais, a malha LPV calculada pelo modelo ideal causou oscilações devido a efeitos parasitas mecânicos da turbina de bancada. O ganho integrativo e proporcional foi reduzido à metade (`ganhoK *= 0.5`) para estabilizar a resposta experimental.

---

## 5. Sistema de Hidrogênio (`firmware_hidrogenio_uno`)

### Sensor de Potência Digital INA219 (I2C `0x40`)
*   **Substituição do Shunt Fixo:** O antigo cálculo por divisor/resistor fixo (`RESISTENCIA_CARGA_OHM = 11.7` em `A0`) foi substituído pelo sensor digital **INA219** via barramento I2C (endereço padrão `0x40`).
*   **Medição High-Side:** O INA219 mede diretamente a tensão de barramento ($V_{bus}$) e a corrente sobre o shunt interno de $0.1\,\Omega$ ($I_{shunt}$), calculando $P_{cc} = V_{bus} \cdot I_{shunt}$ independentemente de a carga conectada ser um resistor, LED ou pequeno ventilador.

### Procedimento do Eletrolisador PEM (Horizon Educational)
*   **Água Destilada no Lado VERMELHO (Oxigênio / Ânodo):** Nos kits PEM da Horizon, a reação eletrolítica de quebra da água ($2\text{H}_2\text{O} \to \text{O}_2 + 4\text{H}^+ + 4e^-$) ocorre no **polo positivo / vermelho (Oxigênio)**. Use **exclusivamente água destilada ou desionizada** no lado vermelho (ou hidrate ambos os lados na preparação inicial) — água comum destrói a membrana PEM e operar a seco reduz ou interrompe a geração de $H_2$ no lado preto.

### Pinagem Completa do Arduino Uno H2 + Display Redondo GC9A01 (1.28" TFT)
| Periférico / Função | Pino no Componente | Pino no Arduino Uno | Observação |
| :--- | :--- | :--- | :--- |
| **UART R4 Central** | RX / TX | **D0 (RX) / D1 (TX)** | `SerialTransfer` a `115200` baud |
| **Servo Célula (`ucc`)** | Sinal PWM | **D3** | `0°` = aberta, `180°` = fechada |
| **Relé Eletrolisador (`uel`)** | IN | **D5** | Ativo em nível **BAIXO** (`LOW` = ligado) |
| **VL6180X + INA219** | SDA / SCL | **A4 (SDA) / A5 (SCL)** | I2C compartilhado (`0x29` e `0x40`, sem conflito) |
| **Display GC9A01** | **SCL / SCK** | **D13** | Fixo — Hardware SPI Clock |
| **Display GC9A01** | **DC** | **D9** | Flexível — Data / Command |
| **Display GC9A01** | **SDA / MOSI** | **D11** | Fixo — Hardware SPI Data |
| **Display GC9A01** | **RST / RES** | **D8** | Flexível — Hardware Reset |
| **Display GC9A01** | **CS** | **D10** | Flexível — Chip Select |
| **Display GC9A01** | **VCC / BLK** | **5V / 3.3V** | Ligar **VCC no 5V** (evita corte térmico do regulador 3.3V) e **BLK no 3.3V** (não deixar solto!) |

> Sequência memorizada dos pinos de sinal do display GC9A01 (`Testes/debug_display_gc9a01/`): **`13 - 9 - 11 - 8 - 10`** (SCL, DC, SDA, RST, CS).

### Sensor de Nível VL6180X
O nível de água no reservatório de H2 é medido indiretamente por tempo de vôo (ToF) óptico do sensor VL6180X posicionado no topo do copo.
*   **`ALTURA_COPO_MM = 47.0`**: A distância física interna do reservatório. Como a água sobe, a distância reportada pelo sensor diminui, e o nível real é calculado de forma invertida:
    $$\text{Nível} = \text{ALTURA\_COPO\_MM} - \text{distancia\_medida}$$
*   **`FATOR_CALIBRACAO_NIVEL = 0.9375`**: Fator experimental para corrigir imperfeições ópticas e reflexão da água (fator de escala de 15/16).
*   **`ALPHA_FILTRO_NIVEL = 0.05`**: Coeficiente do filtro exponencial (EMA) para suavizar a interface ar-água e evitar leituras erráticas por flutuações rápidas de bolhas ou movimento de fluido.
*   **`MAX_FALHAS_CONSECUTIVAS = 3`**: Tolerância para leituras com falhas do sensor VL6180X antes de retornar sentinela de erro (`-999.0f`), fazendo o firmware manter o último nível válido (`ultimoNivelH2Valido_mm`).
