#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/* Sobe o barramento I2C0 (IO38/IO39), faz reset do touch e lê o número de pontos. */
esp_err_t touch_test_init(void);

/* Pulso de RST no touch (recupera o chip se travar). */
void touch_test_reset(void);

/* Há quantos ms o INT está baixo (0 se estiver alto). */
int touch_test_int_baixo_ms(void);

/* Procura dispositivos no barramento I2C e loga os endereços encontrados. */
void touch_test_scan(void);

/* Lê o primeiro ponto de toque. Retorna true se houver toque, com coordenadas brutas do painel. */
bool touch_test_read(uint16_t *x, uint16_t *y);
