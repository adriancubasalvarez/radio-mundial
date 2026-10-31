#include "mapa.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Tamaño de la paleta y posición de los píxeles en el .bin de LVGL:
 * cabecera (4 bytes) + paleta (256 × 4 bytes) + un byte por píxel */
#define COLORES_PALETA 256
#define INICIO_PIXELES (sizeof(lv_img_header_t) + COLORES_PALETA * sizeof(lv_color32_t))

/* Índice de la paleta con la tierra de otras regiones (ver build_maps.py) */
#define IDX_TIERRA_FUERA 2

/* ---------- Decodificador propio de LVGL ----------
 * Reconoce nuestras imágenes porque su cabecera dice LV_IMG_CF_USER_ENCODED_0
 * y su campo data apunta al Mapa. A LVGL le entrega color verdadero, línea
 * a línea, traduciendo cada índice con la paleta en uso. */

static bool es_mapa(const void *src)
{
    if (lv_img_src_get_type(src) != LV_IMG_SRC_VARIABLE) {
        return false;
    }
    return ((const lv_img_dsc_t *)src)->header.cf == LV_IMG_CF_USER_ENCODED_0;
}

static lv_res_t info_mapa(lv_img_decoder_t *decoder, const void *src, lv_img_header_t *cabecera)
{
    (void)decoder;
    if (!es_mapa(src)) {
        return LV_RES_INV;
    }
    *cabecera = ((const lv_img_dsc_t *)src)->header;
    cabecera->cf = LV_IMG_CF_TRUE_COLOR;
    return LV_RES_OK;
}

static lv_res_t abrir_mapa(lv_img_decoder_t *decoder, lv_img_decoder_dsc_t *dsc)
{
    (void)decoder;
    if (!es_mapa(dsc->src)) {
        return LV_RES_INV;
    }
    dsc->img_data = NULL;  /* sin imagen en RAM: LVGL pedirá líneas */
    return LV_RES_OK;
}

static lv_res_t leer_linea_mapa(lv_img_decoder_t *decoder, lv_img_decoder_dsc_t *dsc,
                                lv_coord_t x, lv_coord_t y, lv_coord_t largo, uint8_t *destino)
{
    (void)decoder;
    const lv_img_dsc_t *img = (const lv_img_dsc_t *)dsc->src;
    Mapa *mapa = (Mapa *)img->data;
    static uint8_t indices[PANTALLA_ANCHO];
    if (largo > PANTALLA_ANCHO) {
        largo = PANTALLA_ANCHO;
    }
    uint32_t pos = INICIO_PIXELES + (uint32_t)y * img->header.w + x;
    if (!hal_archivo_leer_en(mapa->archivo, pos, indices, largo)) {
        memset(indices, 0, largo);
    }
    lv_color_t *salida = (lv_color_t *)destino;
    for (lv_coord_t i = 0; i < largo; i++) {
        salida[i] = mapa->paleta[indices[i]];
    }
    return LV_RES_OK;
}

static void cerrar_mapa(lv_img_decoder_t *decoder, lv_img_decoder_dsc_t *dsc)
{
    (void)decoder;
    (void)dsc;
}

static void registrar_decodificador(void)
{
    static bool hecho = false;
    if (hecho) {
        return;
    }
    hecho = true;
    lv_img_decoder_t *d = lv_img_decoder_create();
    lv_img_decoder_set_info_cb(d, info_mapa);
    lv_img_decoder_set_open_cb(d, abrir_mapa);
    lv_img_decoder_set_read_line_cb(d, leer_linea_mapa);
    lv_img_decoder_set_close_cb(d, cerrar_mapa);
}

/* ---------- Carga ---------- */

bool mapa_cargar(Mapa *mapa, const char *ruta_imagen, const char *ruta_zonas)
{
    memset(mapa, 0, sizeof(Mapa));
    registrar_decodificador();

    mapa->archivo = hal_archivo_abrir(ruta_imagen);
    size_t tam_zonas = 0;
    mapa->datos_zonas = hal_leer_archivo(ruta_zonas, &tam_zonas);
    if (mapa->archivo == NULL || mapa->datos_zonas == NULL) {
        mapa_liberar(mapa);
        return false;
    }

    /* Cabecera de LVGL 8 (4 bytes): formato, ancho y alto */
    lv_img_header_t cabecera;
    if (!hal_archivo_leer_en(mapa->archivo, 0, &cabecera, sizeof(cabecera)) ||
        cabecera.cf != LV_IMG_CF_INDEXED_8BIT) {
        fprintf(stderr, "%s no es una imagen indexada de 8 bits\n", ruta_imagen);
        mapa_liberar(mapa);
        return false;
    }

    /* Paleta: se guarda la original y se prepara la de uso */
    hal_archivo_leer_en(mapa->archivo, sizeof(cabecera), mapa->original, sizeof(mapa->original));
    for (int i = 0; i < COLORES_PALETA; i++) {
        mapa->paleta[i] = lv_color_make(mapa->original[i].ch.red, mapa->original[i].ch.green,
                                        mapa->original[i].ch.blue);
    }

    mapa->imagen.header = cabecera;
    mapa->imagen.header.cf = LV_IMG_CF_USER_ENCODED_0;  /* lo reconoce nuestro decodificador */
    mapa->imagen.data = (const uint8_t *)mapa;
    mapa->imagen.data_size = 0;

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
    if (mapa->imagen.data != NULL) {
        lv_img_cache_invalidate_src(&mapa->imagen);
    }
    hal_archivo_cerrar(mapa->archivo);
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

void mapa_apagar(Mapa *mapa, uint8_t indice)
{
    mapa->paleta[indice] = mapa->paleta[IDX_TIERRA_FUERA];
}

/* Mezcla un color con blanco para que destaque */
static lv_color_t aclarar(lv_color32_t c)
{
    return lv_color_make(c.ch.red + (255 - c.ch.red) * 6 / 10, c.ch.green + (255 - c.ch.green) * 6 / 10,
                         c.ch.blue + (255 - c.ch.blue) * 6 / 10);
}

void mapa_resaltar(Mapa *mapa, uint8_t indice, lv_obj_t *obj)
{
    if (indice == mapa->resaltado) {
        return;
    }
    if (mapa->resaltado != 0) {
        lv_color32_t o = mapa->original[mapa->resaltado];
        mapa->paleta[mapa->resaltado] = lv_color_make(o.ch.red, o.ch.green, o.ch.blue);
    }
    if (indice != 0) {
        mapa->paleta[indice] = aclarar(mapa->original[indice]);
    }
    mapa->resaltado = indice;

    lv_img_cache_invalidate_src(&mapa->imagen);
    lv_obj_invalidate(obj);
}
