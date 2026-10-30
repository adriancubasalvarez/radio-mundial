/**
 * Imágenes LVGL leídas desde data/ (logo, etc.), en cualquier formato de
 * color de LVGL 8.
 */
#pragma once

#include <lvgl.h>
#include <stdint.h>

typedef struct {
    lv_img_dsc_t dsc;   /* lista para lv_img_set_src() */
    uint8_t *datos;     /* archivo .bin completo en memoria */
} Imagen;

/* Carga una imagen .bin de LVGL. Devuelve false si no existe. */
bool imagen_cargar(Imagen *imagen, const char *ruta);

void imagen_liberar(Imagen *imagen);
