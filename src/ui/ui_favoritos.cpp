/**
 * Lista de emisoras favoritas (se añaden y quitan con la estrella de
 * «Reproduciendo»). A la derecha, el código del país de cada una.
 */
#include "nav.h"
#include "comun.h"
#include "ajustes.h"
#include "hal.h"

#include <lvgl.h>
#include <string.h>

static void al_elegir(lv_event_t *e)
{
    uintptr_t i = (uintptr_t)lv_event_get_user_data(e);
    const EmisoraGuardada *f = &ajustes.favoritos[i];
    nav.emisora = f->emisora;
    memset(&nav.pais, 0, sizeof(nav.pais));
    strncpy(nav.pais.codigo, f->pais, sizeof(nav.pais.codigo) - 1);
    strncpy(nav.pais.nombre, f->nombre_pais, sizeof(nav.pais.nombre) - 1);
    nav.aleatoria = false;
    nav_ir(PANT_SONANDO);
}

lv_obj_t *pantalla_favoritos(void)
{
    lv_obj_t *pantalla = comun_pantalla();
    comun_cabecera(pantalla, "Favoritos", true, NULL);

    if (ajustes.num_favoritos == 0) {
        lv_obj_t *aviso = lv_label_create(pantalla);
        lv_label_set_text(aviso, "Aún no tienes favoritos.\n\nPulsa " SIMBOLO_ESTRELLA " mientras suena\nuna emisora para guardarla.");
        lv_obj_set_style_text_font(aviso, &radio_20, 0);
        lv_obj_set_style_text_color(aviso, lv_color_hex(COLOR_SECUNDARIO), 0);
        lv_obj_set_style_text_align(aviso, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(aviso, LV_ALIGN_CENTER, 0, ALTO_CABECERA / 2);
        return pantalla;
    }

    lv_obj_t *lista = comun_lista(pantalla);
    for (uintptr_t i = 0; i < ajustes.num_favoritos; i++) {
        const EmisoraGuardada *f = &ajustes.favoritos[i];
        bool suena = nav.hay_sonando && strcmp(nav.sonando.url, f->emisora.url) == 0;
        lv_obj_t *elemento = comun_elemento_lista(lista, suena ? LV_SYMBOL_VOLUME_MAX : SIMBOLO_ESTRELLA,
                                                  f->emisora.nombre);
        comun_nota_elemento(elemento, f->pais);
        lv_obj_add_event_cb(elemento, al_elegir, LV_EVENT_CLICKED, (void *)i);
    }
    return pantalla;
}
