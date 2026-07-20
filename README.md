# Mini Microrrede INCT CAPE — Firmware dos Periféricos

Repositório de firmware para os Arduino da microrrede em escala do laboratório INCT CAPE.

## Estrutura do projeto

```
.
├── firmware_central_r4/          Arduino R4 Wifi — controlador central
├── firmware_painel_solar/        Arduino Uno — painel fotovoltaico
├── firmware_turbina_eolica/      Arduino Uno — turbina eólica
├── firmware_hidrogenio_uno/      Arduino Uno — eletrolisador e célula combustível
├── MGCC_microrrede/              Scripts MATLAB do controlador (MPC/MGCC)
├── web_app/                       App web FastAPI (interface alternativa ao MATLAB)
├── legacy/                        Código anterior (preservado para referência)
├── web_app_architecture.md        Arquitetura planejada de longo prazo
├── README.md                      Este arquivo
└── docs/                          Documentação técnica (ver ordem de leitura abaixo)
    ├── 1_guia_geral.md              1º: visão geral + como compilar + debug
    ├── 2_comunicacao.md             2º: protocolo serial, I2C, SerialTransfer
    └── 3_calibracao_e_design.md     3º: constantes físicas, calibração e design
```

## Visão geral do sistema

### Estado atual (MATLAB como controlador)

```
Controlador (MATLAB ou App Web)
     |
     | USB Serial: Pc, Pd, ucc, uel, ref_sol, ref_vento  (CSV, 6 campos)
     v
Arduino R4 Wifi  (firmware_central_r4)
     |-- I2C addr 9  --> Painel Solar    --> Pot_pv
     |-- I2C addr 10 --> Turbina Eólica  --> Pot_wt
     |-- SerialTransfer UART --> UNO H2  --> Pot_cc, Level_H2
     |
     | USB Serial: Level_H2, Pot_cc, Pot_total, SoC, Pot_wt, Pot_pv, Pc, Pd
     v
Controlador (MATLAB ou App Web)
```

> **Arquitetura de longo prazo:** o app web (FastAPI) passará a ser o hub
> central de comunicação, com o MATLAB podendo ser chamado como um dos
> controladores disponíveis. O firmware do Arduino não muda em nenhum dos
> cenários. Ver [`web_app_architecture.md`](web_app_architecture.md) para
> detalhes.

## Como compilar e carregar

1. Abrir a IDE Arduino (versão 2.x recomendada).
2. Abrir a pasta do firmware desejado (ex: `firmware_central_r4/`).
   > A IDE abre automaticamente todos os arquivos `.h` e `.cpp` da pasta.
3. Selecionar a placa correta no menu **Ferramentas → Placa**:
   - `firmware_central_r4` → Arduino UNO R4 WiFi
   - Demais firmwares → Arduino UNO (ATmega328P)
4. Selecionar a porta serial correspondente.
5. Clicar em **Verificar** (✓) para compilar.
6. Clicar em **Carregar** (→) para gravar na placa.

> **Importante:** a IDE Arduino exige que o nome da pasta seja igual ao nome do arquivo `.ino`.

## Dependências (bibliotecas Arduino)

Instalar via **Ferramentas → Gerenciar Bibliotecas**:

| Biblioteca | Usado em |
|---|---|
| `SerialTransfer` (PowerBroker2) | `firmware_central_r4`, `firmware_hidrogenio_uno` |
| `MatrixMath` (Tom Roberts) | `firmware_painel_solar`, `firmware_turbina_eolica` |
| `VL6180X` (Pololu) | `firmware_hidrogenio_uno` |
| `Servo` (built-in) | `firmware_turbina_eolica`, `firmware_hidrogenio_uno` |
| `Wire` (built-in) | todos |

## Nota sobre `__pycache__` e `.gitignore`

Você pode ver uma pasta `__pycache__/` na raiz ou dentro de `web_app/`.
Ela é criada automaticamente pelo Python quando executa scripts `.py`,
e contém bytecode compilado (arquivos `.pyc`). Pode ser ignorada e deletada
sem prejuízo — o Python a recria quando precisar.

Ela já está no `.gitignore` e não é versionada no repositório.

## Sequência de inicialização

Ao ligar o sistema pela primeira vez:

1. Ligar o **Arduino UNO da turbina** — o ESC leva ~10 s para armar.
2. Ligar o **Arduino R4 central** — aguarda o primeiro pacote do controlador.
3. Ligar o **Arduino UNO do H2** — inicializa o sensor VL6180X.
4. Ligar o **Arduino UNO do painel** — inicializa controle PWM.
5. Iniciar o controlador: MATLAB (`MGCC_microrrede/`) ou o app web (`web_app/`).

## Debug

Cada firmware tem uma flag `DEBUG_SERIAL` no topo do arquivo `.ino`.
Para depurar um periférico isolado, setar `const bool DEBUG_SERIAL = true;`,
recompilar, e abrir o Monitor Serial (115200 baud).

**Atenção:** em operação integrada com o controlador, manter `DEBUG_SERIAL = false`
em todos os firmwares para não interferir no tempo real do controle.

## Documentação adicional

Leia os docs na ordem sugerida em [`docs/README.md`](docs/README.md):

1. **[1_guia_geral.md](docs/1_guia_geral.md)** — começar aqui
2. **[2_comunicacao.md](docs/2_comunicacao.md)** — protocolo entre placas
3. **[3_calibracao_e_design.md](docs/3_calibracao_e_design.md)** — calibração e decisões de projeto

Para a arquitetura de longo prazo do sistema de controle, ver:
- **[web_app_architecture.md](web_app_architecture.md)** — integração FastAPI + MATLAB
