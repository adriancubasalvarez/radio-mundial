/**
 * Mapas generados por tools/build_maps.py:
 *   - imagen LVGL indexada de 8 bits (cada región o país, un color de la paleta);
 *   - rejilla de zonas táctiles (qué índice hay bajo cada celda).
 */
#pragma once

#include <lvgl.h>
#include <stdint.h>

typedef struct {
    lv_img_dsc_t imagen;     /* imagen lista para lv_img_set_src() */
    uint8_t *datos_imagen;   /* archivo .bin completo en memoria */
    uint8_t *datos_zonas;    /* archivo _zonas.bin completo en memoria */
    const uint8_t *celdas;   /* índice de cada celda de la rejilla */
    uint16_t columnas;
    uint16_t filas;
    uint8_t celda;           /* tamaño de la celda en píxeles */
    uint8_t resaltado;       /* índice resaltado ahora (0 = ninguno) */
    lv_color32_t color_original; /* color que tenía el índice resaltado */
} Mapa;

/* Carga imagen y zonas desde data/. Devuelve false si falta algún archivo. */
bool mapa_cargar(Mapa *mapa, const char *ruta_imagen, const char *ruta_zonas);

/* Libera la memoria del mapa */
void mapa_liberar(Mapa *mapa);

/* Índice (región o país) en el punto x, y relativo a la imagen; 0 si nada */
uint8_t mapa_indice_en(const Mapa *mapa, int x, int y);

/* Pinta un índice con el color de la tierra de fondo (país sin emisoras).
 * Llamar antes de mostrar la imagen. */
void mapa_apagar(Mapa *mapa, uint8_t indice);

/* Resalta un índice (0 quita el resaltado). obj es el lv_img que lo muestra. */
void mapa_resaltar(Mapa *mapa, uint8_t indice, lv_obj_t *obj);
