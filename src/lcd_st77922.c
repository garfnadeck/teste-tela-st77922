#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_cache.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_st77922.h"

#include "board_pins.h"
#include "lcd_st77922.h"

#define TAG "lcd"

#define LCD_HOST        SPI2_HOST
#define LCD_H_RES       320
#define LCD_V_RES       480
#define LCD_PCLK_HZ     (80 * 1000 * 1000)  /* 80 MHz, igual ao exemplo do fabricante */
#define BAND_ROWS       40                  /* linhas por transferência: 320 * 40 * 2 = 25 KB */

#define COR_FUNDO       0xFFFF              /* branco */
#define COR_TEXTO       0x0000              /* preto */
#define TEXTO_ESCALA    10                  /* cada pixel da fonte vira 10x10 */

/* Sequência de inicialização do fabricante (bsp_display.c, pacote ST77922). */
static const st77922_lcd_init_cmd_t lcd_init_cmds[] = {
    {0xF1, (uint8_t []){0x00}, 1, 0},
    {0x60, (uint8_t []){0x00, 0x00, 0x00}, 3, 0},
    {0x65, (uint8_t []){0x80}, 1, 0},
    {0x79, (uint8_t []){0x06}, 1, 0},
    {0x7B, (uint8_t []){0x00, 0x08, 0x08}, 3, 0},
    {0x80, (uint8_t []){0x55, 0x62, 0x2F, 0x17, 0xF0, 0x52, 0x70, 0xD2, 0x52, 0x62, 0xEA}, 11, 0},
    {0x81, (uint8_t []){0x26, 0x52, 0x72, 0x27}, 4, 0},
    {0x84, (uint8_t []){0x92, 0x25}, 2, 0},
    {0x87, (uint8_t []){0x10, 0x10, 0x58, 0x00, 0x02, 0x3A}, 6, 0},
    {0x88, (uint8_t []){0x00, 0x00, 0x2C, 0x10, 0x04, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x06}, 15, 0},
    {0x89, (uint8_t []){0x00, 0x00, 0x00}, 3, 0},
    {0x8A, (uint8_t []){0x13, 0x00, 0x2C, 0x00, 0x00, 0x2C, 0x10, 0x10, 0x00, 0x3E, 0x19}, 11, 0},
    {0x8B, (uint8_t []){0x15, 0xB1, 0xB1, 0x44, 0x96, 0x2C, 0x10, 0x97, 0x8E}, 9, 0},
    {0x8C, (uint8_t []){0x1D, 0xB1, 0xB1, 0x44, 0x96, 0x2C, 0x10, 0x50, 0x0F, 0x01, 0xC5, 0x12, 0x09}, 13, 0},
    {0x8D, (uint8_t []){0x0C}, 1, 0},
    {0x8E, (uint8_t []){0x33, 0x01, 0x0C, 0x13, 0x01, 0x01}, 6, 0},
    {0xB3, (uint8_t []){0x00, 0x30}, 2, 0},
    {0xF1, (uint8_t []){0x00}, 1, 0},
    {0x71, (uint8_t []){0xD0}, 1, 0},
    {0x66, (uint8_t []){0x02, 0x3F}, 2, 0},
    {0xBE, (uint8_t []){0x26, 0x00, 0x9D}, 3, 0},
    {0x70, (uint8_t []){0x01, 0xA0, 0x11, 0x40, 0xE0, 0x00, 0x11, 0x69, 0x11, 0x00, 0x00, 0x1A}, 12, 0},
    {0x90, (uint8_t []){0x04, 0x04, 0x55, 0x74, 0x00, 0x40, 0x43, 0x27, 0x27}, 9, 0},
    {0x91, (uint8_t []){0x04, 0x04, 0x55, 0x75, 0x00, 0x40, 0x42, 0x27, 0x27}, 9, 0},
    {0x92, (uint8_t []){0x04, 0x44, 0x55, 0xC0, 0x06, 0x00, 0x07, 0x05, 0x90, 0x27}, 10, 0},
    {0x93, (uint8_t []){0x04, 0x43, 0x11, 0x00, 0x00, 0x00, 0x00, 0x05, 0x90, 0x27}, 10, 0},
    {0x94, (uint8_t []){0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 6, 0},
    {0x95, (uint8_t []){0x96, 0x16, 0x00, 0x00, 0xFF}, 5, 0},
    {0x96, (uint8_t []){0x44, 0x53, 0x03, 0x12, 0x23, 0x24, 0x06, 0x05, 0x94, 0x27, 0x00, 0x44}, 12, 0},
    {0x97, (uint8_t []){0x44, 0x53, 0x47, 0x56, 0x20, 0x20, 0x02, 0x01, 0x94, 0x27, 0x00, 0x44}, 12, 0},
    {0xBA, (uint8_t []){0x55, 0x94, 0x2D, 0x94, 0x27}, 5, 0},
    {0x9A, (uint8_t []){0x40, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00}, 7, 0},
    {0x9B, (uint8_t []){0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00}, 7, 0},
    {0x9C, (uint8_t []){0x5C, 0x12, 0x00, 0x00, 0x10, 0x12, 0x00, 0x00, 0x10, 0x02, 0x00, 0x00, 0x00}, 13, 0},
    {0x9D, (uint8_t []){0x8A, 0x51, 0x00, 0x00, 0x00, 0x80, 0x1E, 0x01}, 8, 0},
    {0x9E, (uint8_t []){0x51, 0x00, 0x00, 0x00, 0x80, 0x1E, 0x01}, 7, 0},
    {0xB4, (uint8_t []){0x1D, 0x1C, 0x1E, 0x0B, 0x14, 0x02, 0x13, 0x09, 0x1E, 0x00, 0x1E, 0x10}, 12, 0},
    {0xB5, (uint8_t []){0x1D, 0x1C, 0x1E, 0x0A, 0x15, 0x03, 0x11, 0x08, 0x1E, 0x01, 0x1E, 0x12}, 12, 0},
    {0xB6, (uint8_t []){0x77, 0x77, 0x00, 0x0A, 0xFF, 0x0A, 0xFF}, 7, 0},
    {0x86, (uint8_t []){0xCD, 0x04, 0xB1, 0x02, 0x58, 0x12, 0x58, 0x0C, 0x13, 0x01, 0xA5, 0x00, 0xA5, 0xA5}, 14, 0},
    {0xB7, (uint8_t []){0x07, 0x0A, 0x0E, 0x06, 0x05, 0x03, 0x2B, 0x03, 0x03, 0x42, 0x07, 0x10, 0x10, 0x2E, 0x3F, 0x0D}, 16, 0},
    {0xB8, (uint8_t []){0x07, 0x0A, 0x0D, 0x05, 0x05, 0x02, 0x2B, 0x02, 0x03, 0x42, 0x06, 0x10, 0x0F, 0x2E, 0x3F, 0x0D}, 16, 0},
    {0xB9, (uint8_t []){0x23, 0x23}, 2, 0},
    {0xBF, (uint8_t []){0x10, 0x14, 0x14, 0x0B, 0x0B, 0x0B}, 6, 0},
    {0xF2, (uint8_t []){0x00}, 1, 0},
    {0x73, (uint8_t []){0x04, 0xDA, 0x12, 0x54, 0x47}, 5, 0},
    {0x77, (uint8_t []){0x6B, 0x5B, 0xFD, 0xC3, 0xC5}, 5, 0},
    {0x7A, (uint8_t []){0x15, 0x27}, 2, 0},
    {0x7B, (uint8_t []){0x04, 0x57}, 2, 0},
    {0x7E, (uint8_t []){0x01, 0x0E}, 2, 0},
    {0xBF, (uint8_t []){0x36}, 1, 0},
    {0xE3, (uint8_t []){0x40, 0x40}, 2, 0},
    {0xF0, (uint8_t []){0x00}, 1, 0},
    {0xD0, (uint8_t []){0x00}, 1, 0},
    {0x2A, (uint8_t []){0x00, 0x00, 0x01, 0x3F}, 4, 0},
    {0x2B, (uint8_t []){0x00, 0x00, 0x01, 0xDF}, 4, 0},
    {0x21, (uint8_t []){0x00}, 0, 0},
    {0x11, (uint8_t []){0x00}, 0, 120},
    {0x29, (uint8_t []){0x00}, 0, 0},
    {0x2C, (uint8_t []){0x00}, 0, 0},
    {0x3A, (uint8_t []){0x01}, 1, 0},
    {0x36, (uint8_t []){0x00}, 1, 0},
    {0x35, (uint8_t []){0x01}, 1, 20},
};

/* Quadro inteiro na PSRAM. Alocado uma vez e nunca liberado: o envio por SPI é assíncrono. */
static uint16_t *s_quadro = NULL;

void lcd_st77922_backlight_on(void)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << PIN_LCD_BL,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&io));
    gpio_set_level(PIN_LCD_BL, 1);
    ESP_LOGI(TAG, "Backlight ligado (IO%d)", PIN_LCD_BL);
}

esp_lcd_panel_handle_t lcd_st77922_init(void)
{
    spi_bus_config_t bus = {
        .sclk_io_num = PIN_LCD_CLK,
        .data0_io_num = PIN_LCD_D0,
        .data1_io_num = PIN_LCD_D1,
        .data2_io_num = PIN_LCD_D2,
        .data3_io_num = PIN_LCD_D3,
        .max_transfer_sz = LCD_H_RES * BAND_ROWS * 2,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &bus, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num = PIN_LCD_CS,
        .dc_gpio_num = -1,          /* QSPI não usa DC */
        .spi_mode = 0,
        .pclk_hz = LCD_PCLK_HZ,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 32,
        .lcd_param_bits = 8,
        .flags = { .quad_mode = 1 },
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_cfg, &io));

    esp_lcd_panel_handle_t panel = NULL;
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = -1,       /* RST está no EN */
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    st77922_vendor_config_t vendor_cfg = {
        .init_cmds = lcd_init_cmds,
        .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]),
        .flags = { .use_qspi_interface = 1 },
    };
    panel_cfg.vendor_config = &vendor_cfg;
    ESP_ERROR_CHECK(esp_lcd_new_panel_st77922(io, &panel_cfg, &panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    vTaskDelay(pdMS_TO_TICKS(200));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));

    /* Diagnóstico: ID do painel (RDDID). Zeros indicam que o painel não responde ao QSPI. */
    uint8_t rddid[3] = { 0 };
    esp_err_t err = esp_lcd_panel_io_rx_param(io, 0x04, rddid, sizeof(rddid));
    ESP_LOGI(TAG, "RDDID: err=%s  %02X %02X %02X", esp_err_to_name(err),
             rddid[0], rddid[1], rddid[2]);

    ESP_LOGI(TAG, "Tela inicializada (%dx%d)", LCD_H_RES, LCD_V_RES);
    return panel;
}

/* Glifo 5x7: uma linha por byte, bit 4 = pixel da esquerda. */
static uint8_t glifo_linha(char c, int linha)
{
    static const uint8_t T[7] = { 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 };
    static const uint8_t E[7] = { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F };
    static const uint8_t S[7] = { 0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E };
    switch (c) {
        case 'T': return T[linha];
        case 'E': return E[linha];
        case 'S': return S[linha];
        default:  return 0;
    }
}

/* Retorna 1 se o pixel (x, y) faz parte do texto centralizado. */
static int pixel_do_texto(const char *texto, int x, int y)
{
    const int n = (int)strlen(texto);
    const int avanco = 6 * TEXTO_ESCALA;
    const int largura = n * avanco - TEXTO_ESCALA;
    const int altura = 7 * TEXTO_ESCALA;
    const int x0 = (LCD_H_RES - largura) / 2;
    const int y0 = (LCD_V_RES - altura) / 2;

    if (n == 0 || x < x0 || y < y0 || x >= x0 + largura || y >= y0 + altura) return 0;

    int dx = x - x0;
    int dy = y - y0;
    int cx = dx % avanco;
    if (cx >= 5 * TEXTO_ESCALA) return 0;

    int letra = dx / avanco;
    int coluna = cx / TEXTO_ESCALA;
    int linha = dy / TEXTO_ESCALA;
    return (glifo_linha(texto[letra], linha) >> (4 - coluna)) & 1;
}

void lcd_st77922_show_text(esp_lcd_panel_handle_t panel, const char *texto)
{
    const size_t total = LCD_H_RES * LCD_V_RES * sizeof(uint16_t);

    if (!s_quadro) {
        s_quadro = heap_caps_aligned_alloc(64, total, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!s_quadro) {
            ESP_LOGE(TAG, "Sem memoria (PSRAM) para o quadro");
            return;
        }
    }

    for (int i = 0; i < LCD_H_RES * LCD_V_RES; i++) {
        int x = i % LCD_H_RES;
        int y = i / LCD_H_RES;
        uint16_t c = pixel_do_texto(texto, x, y) ? COR_TEXTO : COR_FUNDO;
        /* ESP32 é little-endian; o painel espera o byte alto primeiro. */
        s_quadro[i] = (uint16_t)((c >> 8) | (c << 8));
    }
    /* O DMA lê direto da PSRAM: descarrega o cache antes de enviar. */
    ESP_ERROR_CHECK(esp_cache_msync(s_quadro, total, ESP_CACHE_MSYNC_FLAG_DIR_C2M));

    for (int y = 0; y < LCD_V_RES; y += BAND_ROWS) {
        esp_lcd_panel_draw_bitmap(panel, 0, y, LCD_H_RES, y + BAND_ROWS,
                                  s_quadro + y * LCD_H_RES);
    }
    ESP_LOGI(TAG, "Texto \"%s\" enviado", texto);
}
