#pragma once

/*
 * Pinagem da placa ES3C35P (ESP32-S3 N16R8), conferida no esquema do fabricante.
 * Status de cada item e detalhes: docs/PORTAS.md
 */

/* Tela ST77922 (QSPI). RST da tela = EN do ESP32 (reset por software no código). */
#define PIN_LCD_CS      10
#define PIN_LCD_CLK     12
#define PIN_LCD_D0      11
#define PIN_LCD_D1      13
#define PIN_LCD_D2      14
#define PIN_LCD_D3      9
#define PIN_LCD_BL      41      /* alto liga o backlight */

/* Touch capacitivo (I2C0, compartilhado com o codec de áudio ES8311) */
#define PIN_TOUCH_SDA   38
#define PIN_TOUCH_SCL   39
#define PIN_TOUCH_INT   47
#define PIN_TOUCH_RST   48

/* Cartão microSD por SDIO de 4 bits */
#define PIN_SD_CLK      5
#define PIN_SD_CMD      4
#define PIN_SD_D0       6
#define PIN_SD_D1       7
#define PIN_SD_D2       2
#define PIN_SD_D3       3
