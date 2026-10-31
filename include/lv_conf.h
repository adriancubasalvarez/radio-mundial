/**
 * Configuración de LVGL 8.4 para Radio Mundial.
 *
 * Solo se definen los valores que cambiamos; el resto toma los valores por
 * defecto de LVGL (lv_conf_internal.h).
 */
#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/* Color de 16 bits (RGB565), el mismo formato que la pantalla ILI9341 */
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0

/* Memoria interna de LVGL para objetos y estilos. La lista de países de
 * África (56 botones) más la pantalla anterior durante la animación no
 * caben en 64 KB. El simulador imprime el uso real al cambiar de pantalla. */
#ifdef RADIO_ESP32
/* En la placa la memoria de LVGL se reserva con malloc al arrancar: sin
 * PSRAM, un bloque estático tan grande no cabe en la RAM de datos. */
#define LV_MEM_SIZE (96U * 1024U)
#define LV_MEM_POOL_INCLUDE <stdlib.h>
#define LV_MEM_POOL_ALLOC malloc
#else
#define LV_MEM_SIZE (128U * 1024U)
#endif

/* Frecuencia de refresco de la pantalla y de lectura del táctil (ms) */
#define LV_DISP_DEF_REFR_PERIOD 16
#define LV_INDEV_DEF_READ_PERIOD 16

/* El reloj de LVGL lo alimenta cada plataforma llamando a lv_tick_inc() */
#define LV_TICK_CUSTOM 0

/* Mensajes de LVGL por consola, útil mientras desarrollamos */
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1

/* Fuentes: las Montserrat integradas de LVGL solo traen ASCII (sin tildes
 * ni ñ), así que se desactivan y usamos las nuestras (src/fuentes/), con
 * Latin-1 y los símbolos de LVGL, generadas por tools/build_fonts.py. */
#define LV_FONT_MONTSERRAT_14 0
#define LV_FONT_CUSTOM_DECLARE LV_FONT_DECLARE(radio_14) LV_FONT_DECLARE(radio_20) LV_FONT_DECLARE(radio_28) \
                               LV_FONT_DECLARE(radio_reloj_48)
#define LV_FONT_DEFAULT &radio_14

#endif /* LV_CONF_H */
