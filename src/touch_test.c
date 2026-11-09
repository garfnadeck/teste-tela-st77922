#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_log.h"

#include "board_pins.h"
#include "touch_test.h"

#define TAG "touch"

#define TOUCH_ADDR      0x55            /* endereço usado pelo código do fabricante */
#define TOUCH_MAX_PONTOS 2              /* limite do exemplo do fabricante */
#define I2C_PORTA       I2C_NUM_0

/* Registradores (código do fabricante, bsp_touch.c) */
#define REG_TOUCH_INFO  0x0010          /* bit 0x08 = há dados de toque */
#define REG_TOUCH_PT0   0x0014          /* 7 bytes por ponto */
#define REG_MAX_TOUCHES 0x0009

static i2c_master_bus_handle_t s_bus = NULL;
static i2c_master_dev_handle_t s_dev = NULL;
static uint8_t s_max_pontos = 1;
static uint8_t s_ultimo_info = 0xFF;   /* diagnóstico: último byte de status lido */

static esp_err_t reg_read(uint16_t reg, uint8_t *buf, size_t len)
{
    uint8_t w[2] = { (uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF) };
    return i2c_master_transmit_receive(s_dev, w, sizeof(w), buf, len, pdMS_TO_TICKS(100));
}

esp_err_t touch_test_init(void)
{
    i2c_master_bus_config_t bus_cfg = {};
    bus_cfg.i2c_port = I2C_PORTA;
    bus_cfg.sda_io_num = PIN_TOUCH_SDA;
    bus_cfg.scl_io_num = PIN_TOUCH_SCL;
    bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_cfg.glitch_ignore_cnt = 7;
    bus_cfg.flags.enable_internal_pullup = 1;
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_cfg, &s_bus), TAG, "falha ao criar barramento I2C");

    /* RST do touch: baixo, espera, alto, espera. INT: entrada com pull-up. */
    gpio_config_t rst_cfg = {
        .pin_bit_mask = 1ULL << PIN_TOUCH_RST,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&rst_cfg), TAG, "gpio RST");
    gpio_set_level(PIN_TOUCH_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(PIN_TOUCH_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    gpio_config_t int_cfg = {
        .pin_bit_mask = 1ULL << PIN_TOUCH_INT,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&int_cfg), TAG, "gpio INT");

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = TOUCH_ADDR,
        .scl_speed_hz = 400000,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(s_bus, &dev_cfg, &s_dev), TAG, "dispositivo I2C");

    uint8_t maxt = 0;
    if (reg_read(REG_MAX_TOUCHES, &maxt, 1) == ESP_OK && maxt > 0 && maxt <= TOUCH_MAX_PONTOS) {
        s_max_pontos = maxt;
    } else {
        s_max_pontos = 1;
        ESP_LOGW(TAG, "nao leu o numero de pontos (0x%02X no endereco 0x%02X)", maxt, TOUCH_ADDR);
    }
    ESP_LOGI(TAG, "touch iniciado, pontos maximos: %d", s_max_pontos);
    return ESP_OK;
}

void touch_test_scan(void)
{
    if (!s_bus) {
        ESP_LOGE(TAG, "scan: barramento nao iniciado");
        return;
    }
    int achados = 0;
    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        if (i2c_master_probe(s_bus, addr, 20) == ESP_OK) {
            ESP_LOGI(TAG, "scan: dispositivo no endereco 0x%02X", addr);
            achados++;
        }
    }
    ESP_LOGI(TAG, "scan: %d dispositivo(s) encontrado(s)", achados);
}

bool touch_test_read(uint16_t *x, uint16_t *y)
{
    if (!s_dev) return false;

    uint8_t info = 0;
    if (reg_read(REG_TOUCH_INFO, &info, 1) != ESP_OK) {
        return false;
    }
    /* Diagnóstico: loga quando o status ou o pino INT mudam. */
    if (info != s_ultimo_info) {
        ESP_LOGI(TAG, "status=0x%02X INT=%d", info, gpio_get_level(PIN_TOUCH_INT));
        s_ultimo_info = info;
    }
    if (!(info & 0x08)) {
        return false;
    }

    uint8_t pontos[7 * TOUCH_MAX_PONTOS] = { 0 };
    esp_err_t err = reg_read(REG_TOUCH_PT0, pontos, 7 * s_max_pontos);

    /* Confirma o evento: escreve 0x00 no status para o chip liberar o próximo toque. */
    uint8_t ack[3] = { (uint8_t)(REG_TOUCH_INFO >> 8), (uint8_t)(REG_TOUCH_INFO & 0xFF), 0x00 };
    esp_err_t err_ack = i2c_master_transmit(s_dev, ack, sizeof(ack), pdMS_TO_TICKS(100));
    if (err_ack != ESP_OK) {
        ESP_LOGW(TAG, "ack falhou: %s", esp_err_to_name(err_ack));
    }

    if (err != ESP_OK) {
        return false;
    }

    ESP_LOGD(TAG, "bytes do ponto 0: %02X %02X %02X %02X %02X %02X %02X",
             pontos[0], pontos[1], pontos[2], pontos[3], pontos[4], pontos[5], pontos[6]);
    for (int i = 0; i < s_max_pontos; i++) {
        int base = i * 7;
        if (!(pontos[base] & 0x80)) continue;   /* bit 7 = ponto válido */
        *x = ((pontos[base] & 0x3F) << 8) | pontos[base + 1];
        *y = ((pontos[base + 2] & 0x3F) << 8) | pontos[base + 3];
        return true;
    }
    return false;
}
