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

### Filtro de Kalman da Turbina
Diferente do painel solar, as flutuações aerodinâmicas e mecânicas da turbina eólica produzem variações dinâmicas rápidas e ruidosas no ADC. Um filtro exponencial simples introduziria atraso demais no loop LPV. Usamos um filtro de Kalman escalar:
*   **`RUIDO_PROCESSO_Q = 0.1`**: Confiança no modelo dinâmico do motor.
*   **`RUIDO_MEDICAO_R = 0.3`**: Variância estimada do ruído de leitura do ADC.
    Para ajustar, altere a proporção $Q/R$ até obter um sinal liso nas transições dinâmicas rápidas.

### Calibração Temporária de Potência Eólica
*   **`ESCALA_TEMPORARIA_POTENCIA = 50.0` e `OFFSET_TEMPORARIO_POTENCIA_W = 150.0`**: Lógica provisória aplicada sobre o sinal de potência estimada ($P_{wt} = P_{fisica} \cdot 50 + 150$) para elevar os valores às proporções esperadas pelo MPC no modelo central. Deve ser eliminada quando os perfis dinâmicos de vento e aerodinâmica da turbina física forem atualizados com modelos correspondentes.

### Controle do Motor (ESC) e Rampa de Partida
*   **`GANHO_ESC_POR_VOLT = 38.0`**: Converte a tensão de controle teórica em comando angular da biblioteca `Servo` para regular o ESC da turbina.
*   **Rampa de Partida (`RAMPA_PARTIDA_ESC[60]`)**: O motor da turbina eólica exige um procedimento cuidadoso de arme e aceleração inicial para evitar surtos de corrente e desalinhamento físico. O vetor inicializa o motor de forma progressiva ao longo de 6 segundos (60 steps de 100 ms).
*   **Offset de Faixa Morta (+2V)**:
    ```cpp
    if (novaReferencia > 0.0 && novaReferencia < 2.0) {
        novaReferencia += 2.0;
    }
    ```
    Referências abaixo de 2V não possuem torque suficiente para romper o atrito estático da turbina. O offset de hardware compensa essa não-linearidade automaticamente no firmware.

### Fator de Escala LPV ($\times 0.5$)
*   **Ajuste de Ganho**: Durante os testes experimentais originais, a malha LPV calculada pelo modelo ideal causou oscilações devido a efeitos parasitas mecânicos da turbina de bancada. O ganho integrativo e proporcional foi reduzido à metade (`ganhoK *= 0.5`) para estabilizar a resposta experimental.

---

## 5. Sistema de Hidrogênio (`firmware_hidrogenio_uno`)

### Resistência de Carga
*   **`RESISTENCIA_CARGA_OHM = 11.7`**: O resistor de shunt conectado na saída da célula a combustível para medição de corrente ($I = V_{A0}/R$). O erro de calibração deste valor é linearmente transmitido para o cálculo de potência reportada ($P_{cc} = V \cdot I$). Meça a resistência física real em bancada e atualize para calibrar.

### Sensor de Nível VL6180X
O nível de água no reservatório de H2 é medido indiretamente por tempo de vôo (ToF) óptico do sensor VL6180X posicionado no topo do copo.
*   **`ALTURA_COPO_MM = 47.0`**: A distância física interna do reservatório. Como a água sobe, a distância reportada pelo sensor diminui, e o nível real é calculado de forma invertida:
    $$\text{Nível} = \text{ALTURA\_COPO\_MM} - \text{distancia\_medida}$$
*   **`FATOR_CALIBRACAO_NIVEL = 0.9375`**: Fator experimental para corrigir imperfeições ópticas e reflexão da água (fator de escala de 15/16).
*   **`ALPHA_FILTRO_NIVEL = 0.05`**: Coeficiente do filtro exponencial (EMA) para suavizar a interface ar-água e evitar leituras erráticas por flutuações rápidas de bolhas ou movimento de fluido.
*   **`MAX_FALHAS_CONSECUTIVAS = 3`**: Tolerância para leituras com falhas do sensor VL6180X antes de emitir o código de falha `-1.0` de proteção ao controle central.
