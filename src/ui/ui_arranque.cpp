/**
 * Pantalla de arranque: logo del creador, nombre, firma y versión.
 * Dura TIEMPO_ARRANQUE ms o hasta que se toca la pantalla.
 */
#include "ui.h"
#include "nav.h"
#include "comun.h"
#include "imagen.h"
#include "marca.h"
#include "ajustes.h"
#include "idioma.h"

#include <lvgl.h>
#include <string.h>

#define TIEMPO_ARRANQUE 3000  /* ms */
#define TIEMPO_FUNDIDO  600   /* ms de la aparición del logo */

static Imagen logo;
static lv_timer_t *temporizador;

/* Como una radio de verdad: al encender vuelve a sonar la última emisora */
static void reanudar(void)
{
    if (!ajustes.reanudar || !ajustes.hay_ultima) {
        return;
    }
    nav.emisora = ajustes.ultima.emisora;
    memset(&nav.pais, 0, sizeof(nav.pais));
    strncpy(nav.pais.codigo, ajustes.ultima.pais, sizeof(nav.pais.codigo) - 1);
    strncpy(nav.pais.nombre, ajustes.ultima.nombre_pais, sizeof(nav.pais.nombre) - 1);
    nav_reproducir();
}

/* Después del logo: el mapamundi y, si había una emisora, que vuelva a sonar */
static void empezar(void)
{
    nav_ir(PANT_MUNDO);
    reanudar();
}

/* ---------- Primer arranque: elegir idioma ---------- */

static void al_elegir_idioma(lv_event_t *e)
{
    bool espanol = (bool)(uintptr_t)lv_event_get_user_data(e);
    ajustes_poner_idioma(espanol);
    datos_cargar_indice(&nav.indice);  /* nombres de regiones en el idioma elegido */
    empezar();
}

static lv_obj_t *boton_idioma(lv_obj_t *pantalla, const char *texto, bool espanol, int y)
{
    lv_obj_t *boton = lv_btn_create(pantalla);
    lv_obj_set_size(boton, 220, 56);
    lv_obj_align(boton, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_set_style_radius(boton, 12, 0);
    lv_obj_set_style_bg_color(boton, lv_color_hex(COLOR_CABECERA), 0);
    lv_obj_set_style_border_color(boton, lv_color_hex(COLOR_ACENTO), 0);
    lv_obj_set_style_border_width(boton, 2, 0);
    lv_obj_add_event_cb(boton, al_elegir_idioma, LV_EVENT_CLICKED, (void *)(uintptr_t)espanol);
    lv_obj_t *etiqueta = lv_label_create(boton);
    lv_label_set_text(etiqueta, texto);
    lv_obj_set_style_text_font(etiqueta, &radio_28, 0);
    lv_obj_center(etiqueta);
    return boton;
}

/* Pantalla con dos botones grandes; el inglés, primero */
static void elegir_idioma(void)
{
    lv_obj_t *pantalla = comun_pantalla();
    lv_obj_t *titulo = lv_label_create(pantalla);
    lv_label_set_text(titulo, "Choose your language\nElige tu idioma");
    lv_obj_set_style_text_font(titulo, &radio_20, 0);
    lv_obj_set_style_text_color(titulo, lv_color_hex(COLOR_ACENTO), 0);
    lv_obj_set_style_text_align(titulo, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(titulo, LV_ALIGN_TOP_MID, 0, 18);
    boton_idioma(pantalla, "English", false, 86);
    boton_idioma(pantalla, "Español", true, 156);
    lv_scr_load_anim(pantalla, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, true);
}

static void seguir(void)
{
    if (temporizador != NULL) {
        lv_timer_del(temporizador);
        temporizador = NULL;
        if (ajustes.idioma_elegido) {
            empezar();
        } else {
            elegir_idioma();
        }
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
    lv_label_set_text(firma, TR("by " MARCA_AUTOR, "por " MARCA_AUTOR));
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
