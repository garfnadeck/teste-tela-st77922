#pragma once

#include "esp_lcd_panel_ops.h"

/* Liga o backlight (IO41). */
void lcd_st77922_backlight_on(void);

/* Inicializa o painel com a sequência de init do fabricante e clock de 80 MHz. */
esp_lcd_panel_handle_t lcd_st77922_init(void);

/* Desenha um texto (fonte 5x7 ampliada, letras T, E, S) centralizado, preto sobre branco. */
void lcd_st77922_show_text(esp_lcd_panel_handle_t panel, const char *texto);
