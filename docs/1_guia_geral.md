# 1. Guia para Novos Alunos — Mini Microrrede INCT CAPE

> 📖 **Leia este documento primeiro.** Depois siga para [2_comunicacao.md](2_comunicacao.md) → [3_calibracao_e_design.md](3_calibracao_e_design.md).

Este guia explica o que é o sistema, como está organizado o código e o que você precisa saber para começar a trabalhar no projeto.

---

## O que é este projeto?

A mini microrrede é um sistema em escala de bancada que simula uma microrrede elétrica com fontes renováveis. O sistema controla:

- **Painel solar** (fonte renovável)
- **Turbina eólica** (fonte renovável)
- **Eletrolisador** (converte energia elétrica em H₂ quando há excesso)
- **Célula combustível** (converte H₂ em energia elétrica quando há falta)
- **Bateria** (buffer de energia, modelada no software)

O controlador principal (MGCC — Microgrid Central Controller) é responsável por decidir, a cada 100 ms, quanto cada fonte deve produzir ou consumir para atender a demanda e manter o SoC da bateria dentro de limites seguros.

---

## Arquitetura do sistema

### Estado atual (MATLAB como controlador)

```
MATLAB (MGCC_microrrede/)
     │
     │ USB Serial (CSV, 10 Hz)
     │
     ▼
     
Arduino R4 Wifi  ←── firmware_central_r4/
     │
     ├── I2C (addr 9)  ───► Arduino Uno ←── firmware_painel_solar/
     ├── I2C (addr 10) ───► Arduino Uno ←── firmware_turbina_eolica/
     └── UART          ───► Arduino Uno ←── firmware_hidrogenio_uno/
```

### Arquitetura futura (App Web como hub central)

O objetivo de longo prazo é que o **app web** (FastAPI + WebSockets) seja o hub central de comunicação, substituindo o papel do MATLAB como intermediador entre o controlador e o hardware. O MATLAB passaria a ser apenas um dos "cérebros" disponíveis, chamado pelo Python quando necessário:

```
Controlador (MATLAB MPC, Python, regras heurísticas — intercambiável)
     │
     ▼
App Web / FastAPI  ←── web_app/
     │
     │ USB Serial (CSV, 10 Hz) — mesmo protocolo do firmware_central_r4
     │
     ▼
Arduino R4 Wifi  ←── firmware_central_r4/
     │
     ├── I2C (addr 9)  ───► Arduino Uno ←── firmware_painel_solar/
     ├── I2C (addr 10) ───► Arduino Uno ←── firmware_turbina_eolica/
     └── UART          ───► Arduino Uno ←── firmware_hidrogenio_uno/
```

> O protocolo serial entre o computador e o R4 **não muda** — seja usando MATLAB ou o app web. O firmware do Arduino não precisa ser alterado. Ver `web_app_architecture.md` no diretório raiz para detalhes da arquitetura futura.

Cada Arduino periférico tem **uma responsabilidade só**:
- Receber uma referência
- Controlar um atuador
- Medir uma grandeza física
- Devolver essa medição ao R4

---

## Como abrir e compilar um firmware

1. Baixar e instalar a [IDE Arduino 2.x](https://www.arduino.cc/en/software).
2. Instalar as bibliotecas necessárias via **Ferramentas → Gerenciar Bibliotecas**:
   - `SerialTransfer` (de PowerBroker2)
   - `MatrixMath` (de Tom Roberts)
   - `VL6180X` (de Pololu)
3. Abrir a pasta de um firmware (ex: `firmware_painel_solar/`).
   - A IDE carrega automaticamente todos os arquivos `.h` e `.cpp` da pasta.
4. Selecionar a placa: `Arduino UNO R4 Wifi` para o R4, `Arduino UNO` para os outros.
5. Selecionar a porta serial (verificar no Gerenciador de Dispositivos do Windows).
6. Clicar em **Verificar** e depois **Carregar**.

---

## Estrutura de cada firmware

Cada firmware foi organizado para ser fácil de ler. A estrutura típica de um `.ino` é:

```
1. Configuração de hardware (pinos, endereços)
2. Parâmetros de tempo
3. Parâmetros de calibração  ← ver docs/3_calibracao_e_design.md
4. Variáveis de estado
5. Funções (uma responsabilidade por função)
6. setup() → inicializa hardware
7. loop()  → executa o ciclo de controle de 100 ms
```

---

## O que fazer quando algo não funciona

### "A interface web ou o MATLAB não estão mostrando dados / pararam de atualizar"
1. **Pressione o botão físico RESET em qualquer um dos Arduinos:** Como as placas estão eletricamente interligadas na mesma linha de reset, pressionar o botão RESET de qualquer placa (Central R4, H2 Uno, Painel ou Turbina) reinicia e ressincroniza a comunicação de toda a cadeia de microcontroladores periféricos.
2. **Reinicie a conexão no software:**
   - **No MATLAB:** Pare a execução (Ctrl+C no command window) e rode o script principal novamente.
   - **Na Interface Web:** Clique em **DESCONECTAR** no site, aperte o botão físico de **RESET** em qualquer Arduino e clique em **CONECTAR** novamente no site.

### "A comunicação I2C não funciona"
1. Verificar a cabeação: SDA → SDA, SCL → SCL, GND → GND.
2. Confirmar que os endereços batem (`ENDERECO_I2C` no periférico e `ENDERECO_I2C_PAINEL_SOLAR`/`ENDERECO_I2C_TURBINA` no R4).
3. Ligar o debug no periférico: `const bool DEBUG_SERIAL = true;` e abrir o Monitor Serial.
4. Testar o periférico isolado com um scanner I2C.

### "Os valores de potência estão errados"
1. Verificar as constantes de calibração em [3_calibracao_e_design.md](3_calibracao_e_design.md).
2. No periférico com erro, ligar `DEBUG_SERIAL = true` e verificar os valores crus no Monitor Serial.
3. Comparar com a referência esperada do MATLAB.

### "O ESC da turbina não reage"
1. O ESC precisa de ~10 s de arme. Aguardar.
2. Se ainda não reagir, verificar se `mestreIniciou = true` está sendo setado (o R4 precisa enviar pelo menos uma referência via I2C antes da turbina sair do estado de espera).
3. Ligar `DEBUG_SERIAL = true` na turbina e verificar a mensagem `"Aguardando primeiro comando do R4..."`.

### "O watchdog do R4 está zerando os comandos"
O R4 zera todos os comandos se não receber pacote do controlador (MATLAB ou app web) por mais de 2 s.
1. Verificar se o MATLAB/app ainda está enviando dados.
2. Verificar a porta serial correta.
3. A constante `TIMEOUT_WATCHDOG_MS` em `config.h` pode ser aumentada para testes.

---

## Como adicionar uma nova fonte ou sensor

1. Criar um novo firmware na pasta `firmware_NOME/`.
2. Definir o contrato de comunicação com o R4 (I2C ou SerialTransfer).
3. Documentar o protocolo em [2_comunicacao.md](2_comunicacao.md).
4. Adicionar a nova constante de endereço em `firmware_central_r4/config.h`.
5. Adicionar as chamadas de leitura e envio no `loop()` do R4.
6. Documentar as constantes de calibração em [3_calibracao_e_design.md](3_calibracao_e_design.md).

---

## Pontos de melhoria conhecidos (para futuras versões)

| Item | Status | Descrição |
|---|---|---|
| Calibração do painel solar | ⚠️ Temporária | `ESCALA_POTENCIA_SOLAR` deve ser substituída por curva I-V real |
| Calibração da turbina | ⚠️ Temporária | Fator `×50 + 150` será removido quando o perfil de referência for melhorado |
| Ajuste de referência da turbina | ⚠️ Temporária | Offset de +2V para refs < 2V será removido com novo perfil |
| SoC da bateria | Simplificado | Modelo linear; pode ser substituído por modelo eletroquímico |
| LED Matrix do R4 | Pendente | Código disponível em `legacy/ArduinoCentral.../LEDs_R4_Matriz.h` |
| Displays por subsistema | Futuro | Adicionar displays físicos para visualização local de cada periférico |
| Migrar para app web | Futuro | Substituir MATLAB como hub de comunicação pelo FastAPI (ver `web_app_architecture.md` no raiz) |
