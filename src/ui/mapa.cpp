#include "mapa.h"
#include "hal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Tamaño de la paleta en una imagen indexada de 8 bits */
#define COLORES_PALETA 256

bool mapa_cargar(Mapa *mapa, const char *ruta_imagen, const char *ruta_zonas)
{
    memset(mapa, 0, sizeof(Mapa));

    size_t tam_imagen = 0, tam_zonas = 0;
    mapa->datos_imagen = hal_leer_archivo(ruta_imagen, &tam_imagen);
    mapa->datos_zonas = hal_leer_archivo(ruta_zonas, &tam_zonas);
    if (mapa->datos_imagen == NULL || mapa->datos_zonas == NULL) {
        mapa_liberar(mapa);
        return false;
    }

    /* Cabecera de LVGL 8 (4 bytes): formato, ancho y alto */
    lv_img_header_t cabecera;
    memcpy(&cabecera, mapa->datos_imagen, sizeof(cabecera));
    if (cabecera.cf != LV_IMG_CF_INDEXED_8BIT) {
        fprintf(stderr, "%s no es una imagen indexada de 8 bits\n", ruta_imagen);
        mapa_liberar(mapa);
        return false;
    }
    mapa->imagen.header = cabecera;
    mapa->imagen.data = mapa->datos_imagen + sizeof(cabecera);
    mapa->imagen.data_size = tam_imagen - sizeof(cabecera);

    /* Zonas: "ZN", celda, reservado, columnas y filas (uint16) */
    const uint8_t *z = mapa->datos_zonas;
    if (tam_zonas < 8 || z[0] != 'Z' || z[1] != 'N') {
        fprintf(stderr, "%s no es un archivo de zonas\n", ruta_zonas);
        mapa_liberar(mapa);
        return false;
    }
    mapa->celda = z[2];
    mapa->columnas = (uint16_t)(z[4] | (z[5] << 8));
    mapa->filas = (uint16_t)(z[6] | (z[7] << 8));
    mapa->celdas = z + 8;
    return true;
}

void mapa_liberar(Mapa *mapa)
{
    free(mapa->datos_imagen);
    free(mapa->datos_zonas);
    memset(mapa, 0, sizeof(Mapa));
}

uint8_t mapa_indice_en(const Mapa *mapa, int x, int y)
{
    if (mapa->celdas == NULL || x < 0 || y < 0) {
        return 0;
    }
    int col = x / mapa->celda;
    int fila = y / mapa->celda;
    if (col >= mapa->columnas || fila >= mapa->filas) {
        return 0;
    }
    return mapa->celdas[fila * mapa->columnas + col];
}

/* Índice de la paleta con la tierra de otras regiones (ver build_maps.py) */
#define IDX_TIERRA_FUERA 2

void mapa_apagar(Mapa *mapa, uint8_t indice)
{
    lv_color32_t *paleta = (lv_color32_t *)mapa->imagen.data;
    paleta[indice] = paleta[IDX_TIERRA_FUERA];
}

/* Mezcla un color con blanco para que destaque */
static lv_color32_t aclarar(lv_color32_t c)
{
    c.ch.red = c.ch.red + (255 - c.ch.red) * 6 / 10;
    c.ch.green = c.ch.green + (255 - c.ch.green) * 6 / 10;
    c.ch.blue = c.ch.blue + (255 - c.ch.blue) * 6 / 10;
    return c;
}

void mapa_resaltar(Mapa *mapa, uint8_t indice, lv_obj_t *obj)
{
    if (indice == mapa->resaltado) {
        return;
    }
    /* La paleta va justo al principio de los datos de la imagen */
    lv_color32_t *paleta = (lv_color32_t *)mapa->imagen.data;

    if (mapa->resaltado != 0) {
        paleta[mapa->resaltado] = mapa->color_original;
    }
    if (indice != 0) {
        mapa->color_original = paleta[indice];
        paleta[indice] = aclarar(paleta[indice]);
    }
    mapa->resaltado = indice;

    /* LVGL guarda la imagen decodificada: hay que avisarle del cambio */
    lv_img_cache_invalidate_src(&mapa->imagen);
    lv_obj_invalidate(obj);
}
