#include <dirent.h>
#include <stdio.h>
#include <string.h>

#include "driver/sdmmc_host.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

#include "board_pins.h"
#include "sd_sdio.h"

#define TAG "sd"
#define MOUNT_POINT "/sdcard"

static sdmmc_card_t *s_card = NULL;

esp_err_t sd_sdio_mount(void)
{
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();

    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = 4;
    slot.clk = PIN_SD_CLK;
    slot.cmd = PIN_SD_CMD;
    slot.d0 = PIN_SD_D0;
    slot.d1 = PIN_SD_D1;
    slot.d2 = PIN_SD_D2;
    slot.d3 = PIN_SD_D3;

    esp_vfs_fat_sdmmc_mount_config_t mount_cfg = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
    };

    esp_err_t err = esp_vfs_fat_sdmmc_mount(MOUNT_POINT, &host, &slot, &mount_cfg, &s_card);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "falha ao montar o cartao: %s (cartao inserido?)", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "cartao montado em %s", MOUNT_POINT);
    sdmmc_card_print_info(stdout, s_card);
    return ESP_OK;
}

void sd_sdio_selftest(void)
{
    const char *arquivo = MOUNT_POINT "/teste.txt";
    const char *conteudo = "teste SDIO ok\n";
    char lido[64] = { 0 };

    FILE *f = fopen(arquivo, "w");
    if (!f) {
        ESP_LOGE(TAG, "nao consegui criar %s", arquivo);
        return;
    }
    fputs(conteudo, f);
    fclose(f);

    f = fopen(arquivo, "r");
    if (!f) {
        ESP_LOGE(TAG, "nao consegui abrir %s para leitura", arquivo);
        return;
    }
    fgets(lido, sizeof(lido), f);
    fclose(f);
    ESP_LOGI(TAG, "leitura: %s", strcmp(lido, conteudo) == 0 ? "OK, conteudo confere" : "CONTEUDO DIFERENTE");

    DIR *dir = opendir(MOUNT_POINT);
    if (dir) {
        struct dirent *e;
        while ((e = readdir(dir)) != NULL) {
            ESP_LOGI(TAG, "arquivo: %s", e->d_name);
        }
        closedir(dir);
    }
}
