#pragma once

#include "esp_err.h"

/* Monta o cartão microSD por SDIO de 4 bits em /sdcard. */
esp_err_t sd_sdio_mount(void);

/* Grava, lê de volta e lista o diretório raiz para validar o cartão. */
void sd_sdio_selftest(void);
