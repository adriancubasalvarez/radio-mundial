/**
 * Lista desplazable de las emisoras de un país.
 * Las emisoras se cargan al entrar y se liberan al salir.
 */
#include "nav.h"
#include "comun.h"
#include "hal.h"

#include <lvgl.h>
#include <string.h>

static ListaEmisoras emisoras;

static void al_elegir(lv_event_t *e)
{
    uintptr_t i = (uintptr_t)lv_event_get_user_data(e);
    nav.emisora = emisoras.emisoras[i];
    nav_ir(PANT_SONANDO);
}

static void al_borrar(lv_event_t *e)
{
    (void)e;
    datos_liberar_emisoras(&emisoras);
}

lv_obj_t *pantalla_emisoras(void)
{
    lv_obj_t *pantalla = comun_pantalla();
    lv_obj_add_event_cb(pantalla, al_borrar, LV_EVENT_DELETE, NULL);
    comun_cabecera(pantalla, nav.pais.nombre, true, NULL);

    if (!datos_cargar_emisoras(nav.pais.codigo, &emisoras) || emisoras.num == 0) {
        lv_obj_t *aviso = lv_label_create(pantalla);
        lv_label_set_text(aviso, "Todavía no hay emisoras\nde este país");
        lv_obj_set_style_text_font(aviso, &radio_20, 0);
        lv_obj_set_style_text_color(aviso, lv_color_hex(COLOR_SECUNDARIO), 0);
        lv_obj_set_style_text_align(aviso, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(aviso, LV_ALIGN_CENTER, 0, ALTO_CABECERA / 2);
        return pantalla;
    }

    lv_obj_t *lista = comun_lista(pantalla);
    for (uintptr_t i = 0; i < emisoras.num; i++) {
        const Emisora *em = &emisoras.emisoras[i];
        /* La que está sonando se marca con otro icono */
        bool suena = nav.hay_sonando && strcmp(nav.sonando.url, em->url) == 0;
        lv_obj_t *elemento = comun_elemento_lista(lista, suena ? LV_SYMBOL_VOLUME_MAX : LV_SYMBOL_AUDIO, em->nombre);
        lv_obj_add_event_cb(elemento, al_elegir, LV_EVENT_CLICKED, (void *)i);
    }
    return pantalla;
}
