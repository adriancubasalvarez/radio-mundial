/**
 * Interfaz de usuario (pantallas LVGL). Mismo código en el PC y en la placa.
 */
#pragma once

/* Crea la primera pantalla */
void ui_inicio(void);

/* Muestra la pantalla de arranque (logo); al acabar abre el mapamundi */
void ui_arranque(void);

/* Activa la pantalla de reposo (reloj) tras un rato sin tocar la pantalla */
void reposo_iniciar(void);
