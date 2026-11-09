# Mapa de portas: placa ES3C35P (ESP32-S3 N16R8, 3,5" 320x480)

Fontes:
- Esquema do fabricante (`3.5inch_ESP32-S3_Display_Hardware schematic diagram`), folhas ESP32-S3, LCD&TP, TF card, USB, botões e áudio.
- Pinagem publicada no lcdwiki (ES3C35P).
- Driver e sequência de init: pacote "3.5-inch ESP32-S3-ST77922 Driver Display Module Data Package" (bsp_display.c, bsp_touch.c, bsp_i2c.c).

Legenda do status: **OK** = testado na placa; **Pendente** = ainda não testado; **Esquema** = conferido só no esquema.

## Tela ST77922 (QSPI)

| Sinal | GPIO | Status | Observação |
|---|---|---|---|
| CS | IO10 | OK | |
| CLK | IO12 | OK | |
| D0 | IO11 | OK | |
| D1 | IO13 | OK | |
| D2 | IO14 | OK | |
| D3 | IO9 | OK | |
| RST | EN | OK | Reset por software; RST do conector vai ao EN |
| BL (backlight) | IO41 | OK | Alto liga |

Pinos de modo do módulo (conector QD1, módulo HMX035CTFT-001):

| Pino | Ligação na placa | Modo |
|---|---|---|
| IM2 (pino 40) | GND | 0 |
| IM1 (pino 39) | VCC3V3 | 1 |
| IM0 (pino 38) | **sem ligação** | 0 ou 1 (ver nota) |

Modo QSPI do driver ST77922 = IM2 IM1 IM0 = `0 1 1`. Na placa, IM0 não tem ligação, então o modo depende do pull interno do driver. Com a sequência do fabricante, a tela funciona sem jumper, então o IM0 fica como está.

Sequência de init: a do fabricante (79 comandos, em `src/lcd_st77922.c`), clock de 80 MHz. A sequência padrão do componente não mostrou nada.

Formato dos pixels: RGB565 com byte alto primeiro; o ESP32 é little-endian, então os bytes são trocados antes do envio.

## Touch capacitivo

| Sinal | GPIO | Status | Observação |
|---|---|---|---|
| SDA (I2C0) | IO38 | OK | Compartilhado com o codec de áudio ES8311 e a expansão |
| SCL (I2C0) | IO39 | OK | Idem |
| INT | IO47 | Pendente (não usado no teste por polling) | Ativo em nível baixo |
| RST | IO48 | OK | Ativo em nível baixo |

Endereço I2C do touch: **0x55** no código do fabricante (bsp_touch.c), com registradores de 16 bits. Confirmado no scan.

Endereços encontrados no scan: `0x18` (codec ES8311), `0x28` (não identificado), `0x55` (touch).

Aviso no boot: o número máximo de pontos lido em 0x09 veio 0x05 e foi ignorado, usando 1 ponto. A documentação anterior do projeto dizia FT6336G (endereço 0x38). O esquema mostra o conector com `TP_I2C_SCL`, `TP_I2C_SDA`, `TP_INT` e `TP_RESXP`, sem nome de chip. O scan do barramento (`touch_test_scan`) confirma qual endereço responde.

Codec de áudio ES8311 também no mesmo barramento, endereço 0x18 (confirmar no scan).

## Cartão microSD (SDIO 4 bits)

| Sinal | GPIO | Status | Observação |
|---|---|---|---|
| CLK | IO5 | OK | Esquema confirmou (antes estava em dúvida) |
| CMD | IO4 | OK | |
| DATA0 | IO6 | OK | |
| DATA1 | IO7 | OK | |
| DATA2 | IO2 | OK | |
| DATA3 | IO3 | OK | |

Não há pino de detecção de cartão ligado ao ESP32 (CD/DAT3 é o DATA3). O resistor de pull-up do cartão está na placa.

## Áudio I2S (para depois)

| Sinal | GPIO | Observação |
|---|---|---|
| AUDIO_EN (amplificador) | IO1 | Ativo em nível baixo |
| MCLK | IO17 | |
| BCLK | IO18 | |
| LRCK | IO21 | |
| DOUT (para o codec) | IO15 | |
| DIN (do codec) | IO16 | |

## Outros

| Função | GPIO | Observação |
|---|---|---|
| LED RGB | IO40 | Controlado por IC interno |
| Botão BOOT | IO0 | Segurar ao ligar para modo de gravação |
| Botão RESET | EN | |
| Bateria (ADC) | IO8 | Divisor 100k/100k |
| Serial de depuração | IO43 (RX), IO44 (TX) | UART0 |
| USB | IO19/IO20 | Não usar para GPIO comum (USB do chip) |
| Expansão | IO45, IO46 | Livres |

## Pinos a não usar

- IO26 a IO32: memória flash interna.
- IO33 a IO37: PSRAM octal.
- IO19 e IO20: USB nativo (conflita com a porta USB-Serial/JTAG usada para gravar).

## Status geral

| Item | Status |
|---|---|
| Tela (init do fabricante, texto na tela) | OK (texto TESTE validado visualmente) |
| Touch | OK (toque detectado em 0x55; falta validar cantos) |
| SD (SDIO) | OK (cartão de 14,8 GB, 4 bits, 20 MHz, leitura e escrita conferem) |
| Áudio | Não testado |
