/**
 * Pantalla del mapamundi: tocas una región, se resalta y al soltar se entra.
 *
 *   +-----------------------------+  0
 *   | Radio Mundial               |  cabecera (30 px)
 *   +-----------------------------+  30
 *   |        mapamundi            |  imagen (alto según el mapa)
 *   +-----------------------------+
 *   |  región tocada / sonando    |  pie
 *   +-----------------------------+  240
 */
#include "ui.h"
#include "nav.h"
#include "comun.h"
#include "mapa.h"
#include "hal.h"

#include <lvgl.h>
#include <stdio.h>

static Mapa mapa_mundo;
static lv_obj_t *img_mundo;
static lv_obj_t *etiqueta_pie;

/* Texto del pie cuando no se toca nada: lo que suena, o una pista */
static void pie_por_defecto(void)
{
    EstadoAudio estado = audio_estado();
    if (nav.hay_sonando && (estado == AUDIO_SONANDO || estado == AUDIO_CONECTANDO)) {
        lv_label_set_text_fmt(etiqueta_pie, LV_SYMBOL_AUDIO "  %s", nav.sonando.nombre);
        lv_obj_set_style_text_color(etiqueta_pie, lv_color_hex(COLOR_ACENTO), 0);
    } else {
        lv_label_set_text(etiqueta_pie, "Toca una región");
        lv_obj_set_style_text_color(etiqueta_pie, lv_color_hex(COLOR_SECUNDARIO), 0);
    }
}

/* Región que hay bajo el dedo, según la rejilla de zonas */
static const Region *region_bajo_dedo(void)
{
    lv_indev_t *entrada = lv_indev_get_act();
    if (entrada == NULL) {
        return NULL;
    }
    lv_point_t punto;
    lv_indev_get_point(entrada, &punto);

    lv_area_t zona;
    lv_obj_get_coords(img_mundo, &zona);
    uint8_t color = mapa_indice_en(&mapa_mundo, punto.x - zona.x1, punto.y - zona.y1);
    return datos_region_por_indice(&nav.indice, color);
}

static void al_tocar_mapa(lv_event_t *e)
{
    lv_event_code_t codigo = lv_event_get_code(e);

    if (codigo == LV_EVENT_PRESSED || codigo == LV_EVENT_PRESSING) {
        const Region *region = region_bajo_dedo();
        mapa_resaltar(&mapa_mundo, region ? region->indice : 0, img_mundo);
        if (region != NULL) {
            lv_label_set_text(etiqueta_pie, region->nombre);
            lv_obj_set_style_text_color(etiqueta_pie, lv_color_white(), 0);
        } else {
            pie_por_defecto();
        }
    } else if (codigo == LV_EVENT_RELEASED) {
        const Region *region = region_bajo_dedo();
        if (region != NULL) {
            nav.region = *region;
            nav_ir(PANT_REGION);
        } else {
            mapa_resaltar(&mapa_mundo, 0, img_mundo);
            pie_por_defecto();
        }
    } else if (codigo == LV_EVENT_PRESS_LOST) {
        mapa_resaltar(&mapa_mundo, 0, img_mundo);
        pie_por_defecto();
    }
}

/* Tocar el pie mientras suena algo lleva a «Reproduciendo» */
static void al_tocar_pie(lv_event_t *e)
{
    (void)e;
    if (nav.hay_sonando) {
        nav.emisora = nav.sonando;
        nav_ir(PANT_SONANDO);
    }
}

static void al_borrar(lv_event_t *e)
{
    (void)e;
    mapa_liberar(&mapa_mundo);
}

lv_obj_t *pantalla_mundo(void)
{
    lv_obj_t *pantalla = comun_pantalla();
    lv_obj_add_event_cb(pantalla, al_borrar, LV_EVENT_DELETE, NULL);
    comun_cabecera(pantalla, "Radio Mundial", false, NULL);

    if (!mapa_cargar(&mapa_mundo, nav.indice.imagen, nav.indice.zonas)) {
        lv_obj_t *error = lv_label_create(pantalla);
        lv_label_set_text(error, "Faltan los mapas en data/\nEjecuta tools/build_maps.py");
        lv_obj_set_style_text_color(error, lv_color_white(), 0);
        lv_obj_center(error);
        return pantalla;
    }

    img_mundo = lv_img_create(pantalla);
    lv_img_set_src(img_mundo, &mapa_mundo.imagen);
    lv_obj_set_pos(img_mundo, 0, ALTO_CABECERA);
    lv_obj_add_flag(img_mundo, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(img_mundo, al_tocar_mapa, LV_EVENT_ALL, NULL);

    /* Pie: ocupa todo el hueco bajo el mapa para que sea fácil de tocar */
    int y_pie = ALTO_CABECERA + mapa_mundo.imagen.header.h;
    etiqueta_pie = lv_label_create(pantalla);
    lv_label_set_long_mode(etiqueta_pie, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_size(etiqueta_pie, PANTALLA_ANCHO - 16, LV_SIZE_CONTENT);
    lv_obj_set_style_text_font(etiqueta_pie, &radio_20, 0);
    lv_obj_set_style_text_align(etiqueta_pie, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(etiqueta_pie, LV_ALIGN_TOP_MID, 0, y_pie + (PANTALLA_ALTO - y_pie - 24) / 2);
    lv_obj_add_flag(etiqueta_pie, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(etiqueta_pie, 16);
    lv_obj_add_event_cb(etiqueta_pie, al_tocar_pie, LV_EVENT_CLICKED, NULL);
    pie_por_defecto();

    return pantalla;
}

void ui_inicio(void)
{
    /* Tema oscuro de LVGL con nuestros colores y fuente */
    lv_disp_t *disp = lv_disp_get_default();
    lv_theme_t *tema = lv_theme_default_init(disp, lv_color_hex(0x2F6FA8), lv_color_hex(COLOR_ACENTO),
                                             true, &radio_14);
    lv_disp_set_theme(disp, tema);

    nav.volumen = 70;
    if (!datos_cargar_indice(&nav.indice)) {
        lv_obj_t *error = lv_label_create(lv_scr_act());
        lv_label_set_text(error, "No se encuentra data/regiones.json\nEjecuta tools/build_maps.py");
        lv_obj_center(error);
        return;
    }
    printf("Índice cargado: %d regiones\n", nav.indice.num_regiones);
    ui_arranque();
}
