/**
 * Mapas generados por tools/build_maps.py:
 *   - imagen LVGL indexada de 8 bits (cada región o país, un color de la paleta);
 *   - rejilla de zonas táctiles (qué índice hay bajo cada celda).
 *
 * La imagen NO se carga entera en RAM (67 KB por mapa no caben en una placa
 * sin PSRAM): un decodificador propio de LVGL la lee línea a línea del
 * archivo cuando hace falta dibujarla. En RAM solo quedan la paleta (para
 * resaltar y apagar países) y las zonas táctiles.
 */
#pragma once

#include "hal.h"

#include <lvgl.h>
#include <stdint.h>

typedef struct {
    lv_img_dsc_t imagen;          /* lo que se pasa a lv_img_set_src() */
    HalArchivo *archivo;          /* el .bin, abierto mientras se usa el mapa */
    lv_color32_t original[256];   /* paleta tal como viene en el archivo */
    lv_color_t paleta[256];       /* paleta en uso (con resaltado y apagados) */
    uint8_t *datos_zonas;         /* archivo _zonas.bin completo en memoria */
    const uint8_t *celdas;        /* índice de cada celda de la rejilla */
    uint16_t columnas;
    uint16_t filas;
    uint8_t celda;                /* tamaño de la celda en píxeles */
    uint8_t resaltado;            /* índice resaltado ahora (0 = ninguno) */
} Mapa;

/* Carga imagen y zonas desde los datos. Devuelve false si falta algún archivo. */
bool mapa_cargar(Mapa *mapa, const char *ruta_imagen, const char *ruta_zonas);

/* Libera la memoria del mapa y cierra su archivo */
void mapa_liberar(Mapa *mapa);

/* Índice (región o país) en el punto x, y relativo a la imagen; 0 si nada */
uint8_t mapa_indice_en(const Mapa *mapa, int x, int y);

/* Pinta un índice con el color de la tierra de fondo (país sin emisoras).
 * Llamar antes de mostrar la imagen. */
void mapa_apagar(Mapa *mapa, uint8_t indice);

/* Resalta un índice (0 quita el resaltado). obj es el lv_img que lo muestra. */
void mapa_resaltar(Mapa *mapa, uint8_t indice, lv_obj_t *obj);
