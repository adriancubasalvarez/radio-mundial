/**
 * Lista de emisoras favoritas (se añaden y quitan con la estrella de
 * «Reproduciendo»). A la derecha, el código del país de cada una.
 */
#include "nav.h"
#include "comun.h"
#include "ajustes.h"
#include "hal.h"
#include "idioma.h"

#include <lvgl.h>
#include <string.h>

static void al_elegir(uint16_t fila)
{
    const EmisoraGuardada *f = &ajustes.favoritos[fila];
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
    comun_cabecera(pantalla, TR("Favorites", "Favoritos"), true, NULL);

    if (ajustes.num_favoritos == 0) {
        lv_obj_t *aviso = lv_label_create(pantalla);
        lv_label_set_text(aviso, TR("No favorites yet.\n\nTap " SIMBOLO_ESTRELLA " while a station\nis playing to save it.",
                                     "Aún no tienes favoritos.\n\nPulsa " SIMBOLO_ESTRELLA " mientras suena\nuna emisora para guardarla."));
        lv_obj_set_style_text_font(aviso, &radio_20, 0);
        lv_obj_set_style_text_color(aviso, lv_color_hex(COLOR_SECUNDARIO), 0);
        lv_obj_set_style_text_align(aviso, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(aviso, LV_ALIGN_CENTER, 0, ALTO_CABECERA / 2);
        return pantalla;
    }

    lv_obj_t *lista = comun_lista(pantalla, al_elegir);
    for (uint8_t i = 0; i < ajustes.num_favoritos; i++) {
        const EmisoraGuardada *f = &ajustes.favoritos[i];
        bool suena = nav.hay_sonando && strcmp(nav.sonando.url, f->emisora.url) == 0;
        comun_lista_agregar(lista, suena ? LV_SYMBOL_VOLUME_MAX : SIMBOLO_ESTRELLA, f->emisora.nombre, f->pais, false);
    }
    return pantalla;
}
