/*
 * Teste de bancada da placa ES3C35P: tela ST77922, touch e cartão SD.
 * Pinagem e status: docs/PORTAS.md
 *
 * Monitor serial esperado:
 *   - PSRAM encontrada, backlight ligado, RDDID, tela inicializada
 *   - scan do touch e "touch iniciado"
 *   - cartão montado e leitura OK (se houver cartão)
 *   - "toque x=... y=..." ao encostar na tela
 */

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lcd_st77922.h"
#include "sd_sdio.h"
#include "touch_test.h"

#define TAG "bringup"

static void check_psram(void)
{
    size_t total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    if (total > 0) {
        ESP_LOGI(TAG, "PSRAM encontrada: %u KB", (unsigned)(total / 1024));
    } else {
        ESP_LOGE(TAG, "PSRAM NAO encontrada. Verificar sdkconfig (OCT x QUAD).");
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Iniciando teste da placa ESP32-S3 3.5\"");
    check_psram();

    lcd_st77922_backlight_on();
    esp_lcd_panel_handle_t panel = lcd_st77922_init();
    lcd_st77922_show_text(panel, "TESTE");

    if (touch_test_init() == ESP_OK) {
        touch_test_scan();
    }

    if (sd_sdio_mount() == ESP_OK) {
        sd_sdio_selftest();
    }

    bool estava_tocando = false;
    uint16_t ultimo_x = 0;
    uint16_t ultimo_y = 0;
    while (1) {
        uint16_t x = 0;
        uint16_t y = 0;
        bool tocando = touch_test_read(&x, &y);
        if (tocando && (!estava_tocando || x != ultimo_x || y != ultimo_y)) {
            ESP_LOGI(TAG, "toque x=%u y=%u", (unsigned)x, (unsigned)y);
            ultimo_x = x;
            ultimo_y = y;
        }
        estava_tocando = tocando;
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}
