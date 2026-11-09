#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/* Sobe o barramento I2C0 (IO38/IO39), faz reset do touch e lê o número de pontos. */
esp_err_t touch_test_init(void);

/* Procura dispositivos no barramento I2C e loga os endereços encontrados. */
void touch_test_scan(void);

/* Lê o primeiro ponto de toque. Retorna true se houver toque, com coordenadas brutas do painel. */
bool touch_test_read(uint16_t *x, uint16_t *y);
