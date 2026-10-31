/**
 * Pantalla de región: mapa con sus países. Tocas un país, se resalta y su
 * nombre aparece en la cabecera; al soltar se abren sus emisoras.
 * El botón de lista (arriba a la derecha) muestra los países por orden
 * alfabético, para los que son difíciles de tocar en el mapa.
 */
#include "nav.h"
#include "comun.h"
#include "mapa.h"
#include "hal.h"
#include "idioma.h"

#include <lvgl.h>

static Mapa mapa_region;
static ListaPaises paises;
static lv_obj_t *img_region;
static lv_obj_t *titulo;

static const Pais *pais_bajo_dedo(void)
{
    lv_indev_t *entrada = lv_indev_get_act();
    if (entrada == NULL) {
        return NULL;
    }
    lv_point_t punto;
    lv_indev_get_point(entrada, &punto);

    lv_area_t zona;
    lv_obj_get_coords(img_region, &zona);
    uint8_t color = mapa_indice_en(&mapa_region, punto.x - zona.x1, punto.y - zona.y1);
    return datos_pais_por_indice(&paises, color);
}

static void al_tocar_mapa(lv_event_t *e)
{
    lv_event_code_t codigo = lv_event_get_code(e);

    if (codigo == LV_EVENT_PRESSED || codigo == LV_EVENT_PRESSING) {
        const Pais *pais = pais_bajo_dedo();
        if (pais == NULL) {
            mapa_resaltar(&mapa_region, 0, img_region);
            lv_label_set_text(titulo, nav.region.nombre);
        } else if (pais->emisoras == 0) {
            /* País apagado: se dice por qué no se puede entrar */
            mapa_resaltar(&mapa_region, 0, img_region);
            lv_label_set_text_fmt(titulo, TR("%s: no stations", "%s: sin emisoras"), pais->nombre);
        } else {
            mapa_resaltar(&mapa_region, pais->indice, img_region);
            lv_label_set_text(titulo, pais->nombre);
        }
    } else if (codigo == LV_EVENT_RELEASED) {
        const Pais *pais = pais_bajo_dedo();
        if (pais != NULL && pais->emisoras > 0) {
            nav.pais = *pais;
            nav_ir(PANT_EMISORAS);
        } else {
            mapa_resaltar(&mapa_region, 0, img_region);
            lv_label_set_text(titulo, nav.region.nombre);
        }
    } else if (codigo == LV_EVENT_PRESS_LOST) {
        mapa_resaltar(&mapa_region, 0, img_region);
        lv_label_set_text(titulo, nav.region.nombre);
    }
}

static void al_pulsar_lista(lv_event_t *e)
{
    (void)e;
    nav_ir(PANT_PAISES);
}

static void al_borrar(lv_event_t *e)
{
    (void)e;
    mapa_liberar(&mapa_region);
}

lv_obj_t *pantalla_region(void)
{
    lv_obj_t *pantalla = comun_pantalla();
    lv_obj_add_event_cb(pantalla, al_borrar, LV_EVENT_DELETE, NULL);
    lv_obj_t *cabecera = comun_cabecera(pantalla, nav.region.nombre, true, &titulo);
    comun_boton_cabecera(cabecera, LV_SYMBOL_LIST, al_pulsar_lista);

    if (!datos_cargar_paises(nav.region.id, &paises) ||
        !mapa_cargar(&mapa_region, nav.region.imagen, nav.region.zonas)) {
        lv_obj_t *error = lv_label_create(pantalla);
        lv_label_set_text(error, TR("This region's map is missing", "Falta el mapa de esta región"));
        lv_obj_center(error);
        return pantalla;
    }

    /* Los países sin emisoras se pintan como tierra de fondo */
    for (uint8_t i = 0; i < paises.num; i++) {
        if (paises.paises[i].emisoras == 0) {
            mapa_apagar(&mapa_region, paises.paises[i].indice);
        }
    }

    img_region = lv_img_create(pantalla);
    lv_img_set_src(img_region, &mapa_region.imagen);
    lv_obj_set_pos(img_region, 0, ALTO_CABECERA);
    lv_obj_add_flag(img_region, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(img_region, al_tocar_mapa, LV_EVENT_ALL, NULL);
    return pantalla;
}
