/**
 * Pantalla de arranque: logo del creador, nombre, firma y versión.
 * Dura TIEMPO_ARRANQUE ms o hasta que se toca la pantalla.
 */
#include "ui.h"
#include "nav.h"
#include "comun.h"
#include "imagen.h"
#include "marca.h"

#include <lvgl.h>

#define TIEMPO_ARRANQUE 3000  /* ms */
#define TIEMPO_FUNDIDO  600   /* ms de la aparición del logo */

static Imagen logo;
static lv_timer_t *temporizador;

static void seguir(void)
{
    if (temporizador != NULL) {
        lv_timer_del(temporizador);
        temporizador = NULL;
        nav_ir(PANT_MUNDO);
    }
}

static void al_acabar_tiempo(lv_timer_t *t)
{
    (void)t;
    seguir();
}

static void al_tocar(lv_event_t *e)
{
    (void)e;
    seguir();
}

static void al_borrar(lv_event_t *e)
{
    (void)e;
    imagen_liberar(&logo);
}

void ui_arranque(void)
{
    lv_obj_t *pantalla = comun_pantalla();
    lv_obj_add_event_cb(pantalla, al_borrar, LV_EVENT_DELETE, NULL);
    lv_obj_add_event_cb(pantalla, al_tocar, LV_EVENT_CLICKED, NULL);

    int y = 22;
    if (imagen_cargar(&logo, "logo.bin")) {
        lv_obj_t *img = lv_img_create(pantalla);
        lv_img_set_src(img, &logo.dsc);
        lv_obj_align(img, LV_ALIGN_TOP_MID, 0, y);
        lv_obj_fade_in(img, TIEMPO_FUNDIDO, 0);
        y += logo.dsc.header.h + 12;
    } else {
        y += 50;
    }

    lv_obj_t *nombre = lv_label_create(pantalla);
    lv_label_set_text(nombre, MARCA_NOMBRE);
    lv_obj_set_style_text_font(nombre, &radio_28, 0);
    lv_obj_set_style_text_color(nombre, lv_color_hex(COLOR_ACENTO), 0);
    lv_obj_align(nombre, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_fade_in(nombre, TIEMPO_FUNDIDO, TIEMPO_FUNDIDO / 2);

    lv_obj_t *firma = lv_label_create(pantalla);
    lv_label_set_text(firma, MARCA_FIRMA);
    lv_obj_set_style_text_font(firma, &radio_20, 0);
    lv_obj_set_style_text_color(firma, lv_color_hex(COLOR_TEXTO), 0);
    lv_obj_align(firma, LV_ALIGN_TOP_MID, 0, y + 38);
    lv_obj_fade_in(firma, TIEMPO_FUNDIDO, TIEMPO_FUNDIDO);

    lv_obj_t *version = lv_label_create(pantalla);
    lv_label_set_text(version, MARCA_VERSION);
    lv_obj_set_style_text_color(version, lv_color_hex(COLOR_SECUNDARIO), 0);
    lv_obj_align(version, LV_ALIGN_BOTTOM_RIGHT, -8, -6);

    lv_scr_load(pantalla);
    temporizador = lv_timer_create(al_acabar_tiempo, TIEMPO_ARRANQUE, NULL);
}
