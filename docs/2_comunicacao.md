# 2. Protocolo de Comunicação — Mini Microrrede INCT CAPE

> 📖 **Documento 2 de 3.** Leia depois de [1_guia_geral.md](1_guia_geral.md).
> Consulte este documento ao trabalhar na comunicação entre as placas ou ao integrar um novo controlador.

Descreve todos os contratos de comunicação entre as placas da microrrede e entre o sistema Arduino e o controlador (MATLAB ou app web).

---

## 1. Controlador ↔ Arduino R4 central (USB Serial)

> O protocolo abaixo é o mesmo independentemente do controlador usado (MATLAB, app web / FastAPI, ou outro). O firmware do R4 não muda.

**Baud rate:** 115200  
**Formato:** texto CSV terminado em `\n`

### Pacote Controlador → R4 (comando, 6 campos)

```
Pc,Pd,ucc,uel,ref_sol,ref_vento\n
```

| Campo | Tipo | Unidade | Descrição |
|---|---|---|---|
| `Pc` | float | W | Potência de carga da bateria calculada pelo MPC |
| `Pd` | float | W | Potência de descarga da bateria calculada pelo MPC |
| `ucc` | uint8 (0/1) | — | Comando da célula combustível (1 = abrir válvula) |
| `uel` | uint8 (0/1) | — | Comando do eletrolisador (1 = ligar) |
| `ref_sol` | float | V | Referência de tensão para o painel solar |
| `ref_vento` | float | V | Referência de tensão para a turbina eólica |

**Frequência esperada:** 10 Hz (a cada 100 ms)  
**Watchdog:** se o R4 não receber pacote por mais de 2 s, zera todos os comandos.

### Pacote R4 → Controlador (telemetria, 8 campos)

```
Level_H2,Pot_cc,Pot_total,SoC,Pot_wt,Pot_pv,Pc,Pd\n
```

| Campo | Tipo | Unidade | Descrição |
|---|---|---|---|
| `Level_H2` | float | mm | Nível de H₂ no reservatório |
| `Pot_cc` | float | W | Potência gerada pela célula combustível |
| `Pot_total` | float | W | Balanço: `Pot_pv + Pot_cc + Pot_wt - Pc + Pd` |
| `SoC` | float | [0..1] | Estado de carga da bateria |
| `Pot_wt` | float | W | Potência da turbina eólica |
| `Pot_pv` | float | W | Potência do painel solar |
| `Pc` | float | W | Eco do comando de carga recebido |
| `Pd` | float | W | Eco do comando de descarga recebido |

> Os campos `Pc` e `Pd` são ecos dos comandos recebidos. Permitem ao controlador verificar sincronismo e detectar perda de pacotes. Podem ser removidos no futuro se o pacote ficar grande demais.

---

## 2. R4 ↔ Painel Solar (I2C)

**Endereço I2C do painel:** `9`  
**Master:** Arduino R4 central  
**Slave:** Arduino Uno do painel solar (`firmware_painel_solar`)

| Direção | Conteúdo | Tipo | Unidade |
|---|---|---|---|
| R4 → Painel | Referência de tensão | float (4 bytes) | V |
| Painel → R4 | Potência estimada | float (4 bytes) | W |

**Frequência:** 10 Hz (cada ciclo de 100 ms do R4)

---

## 3. R4 ↔ Turbina Eólica (I2C)

**Endereço I2C da turbina:** `10`  
**Master:** Arduino R4 central  
**Slave:** Arduino Uno da turbina (`firmware_turbina_eolica`)

| Direção | Conteúdo | Tipo | Unidade |
|---|---|---|---|
| R4 → Turbina | Referência de tensão | float (4 bytes) | V |
| Turbina → R4 | Potência estimada | float (4 bytes) | W |

**Frequência:** 10 Hz (cada ciclo de 100 ms do R4)

---

## 4. R4 ↔ Arduino UNO do H₂ (SerialTransfer via UART)

**Hardware:** pino TX1/RX1 do R4 → RX/TX do UNO  
**Baud rate:** 115200  
**Biblioteca:** `SerialTransfer` (PowerBroker2)

### Pacote R4 → UNO (ID = 1, struct `R4ToUno_Data`)

```cpp
struct R4ToUno_Data {
    float ucc;  // Comando da célula combustível (0.0 ou 1.0)
    float uel;  // Comando do eletrolisador (0.0 ou 1.0)
};
```

### Pacote UNO → R4 (ID = 0, struct `UnoToR4_Data`)

```cpp
struct UnoToR4_Data {
    float Pot_cc;    // Potência da célula combustível [W]
    float Level_H2;  // Nível de H₂ no reservatório [mm]
};
```

**Fluxo:** O R4 envia os comandos a cada 100 ms. O UNO responde **imediatamente** com a telemetria mais recente quando recebe o pacote.

---

## 5. Tabela resumo de placas

| Placa | Firmware | Comunicação com R4 | Tipo de placa |
|---|---|---|---|
| R4 central | `firmware_central_r4` | USB Serial (controlador) | Arduino UNO R4 Wifi |
| Painel solar | `firmware_painel_solar` | I2C addr 9 | Arduino Uno |
| Turbina eólica | `firmware_turbina_eolica` | I2C addr 10 | Arduino Uno |
| Sistema H₂ | `firmware_hidrogenio_uno` | SerialTransfer UART | Arduino Uno |

> O "controlador" na coluna de comunicação do R4 pode ser o MATLAB (via porta serial direta) ou o app web FastAPI (que abre a mesma porta serial em Python). O firmware do R4 não precisa mudar em nenhum dos dois casos.
