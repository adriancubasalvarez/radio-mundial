/**
 * Menú de ajustes rápido: se abre manteniendo pulsado el título del
 * mapamundi. Permite cambiar el idioma y la red WiFi. Se dibuja en la capa
 * superior de LVGL, encima del mapamundi.
 */
#include "ui.h"
#include "nav.h"
#include "comun.h"
#include "ajustes.h"
#include "hal.h"
#include "idioma.h"

#include <lvgl.h>

static lv_obj_t *capa;

static void cerrar(void)
{
    if (capa != NULL) {
        lv_obj_del_async(capa);
        capa = NULL;
    }
}

static void al_cerrar(lv_event_t *e)
{
    (void)e;
    cerrar();
}

static void al_elegir_idioma(lv_event_t *e)
{
    bool espanol = (bool)(uintptr_t)lv_event_get_user_data(e);
    cerrar();
    if (espanol != ajustes.espanol) {
        ajustes_poner_idioma(espanol);
        datos_cargar_indice(&nav.indice);  /* nombres de regiones en el nuevo idioma */
        nav_reiniciar();                   /* rehace el mapamundi con los textos nuevos */
    }
}

static void al_wifi(lv_event_t *e)
{
    (void)e;
    cerrar();
    hal_red_configurar();
}

static lv_obj_t *boton(lv_obj_t *padre, const char *texto, bool marcado, lv_event_cb_t accion, void *dato)
{
    lv_obj_t *b = lv_btn_create(padre);
    lv_obj_set_size(b, 260, 42);
    lv_obj_set_style_radius(b, 10, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(marcado ? COLOR_ACENTO : COLOR_LINEA), 0);
    lv_obj_add_event_cb(b, accion, LV_EVENT_CLICKED, dato);
    lv_obj_t *etiqueta = lv_label_create(b);
    lv_label_set_text(etiqueta, texto);
    lv_obj_set_style_text_font(etiqueta, &radio_20, 0);
    lv_obj_set_style_text_color(etiqueta, lv_color_hex(marcado ? COLOR_FONDO : COLOR_TEXTO), 0);
    lv_obj_center(etiqueta);
    return b;
}

void ui_ajustes_abrir(void)
{
    if (capa != NULL) {
        return;
    }
    capa = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(capa);
    lv_obj_set_size(capa, PANTALLA_ANCHO, PANTALLA_ALTO);
    lv_obj_set_style_bg_color(capa, lv_color_hex(COLOR_FONDO), 0);
    lv_obj_set_style_bg_opa(capa, LV_OPA_COVER, 0);
    lv_obj_add_flag(capa, LV_OBJ_FLAG_CLICKABLE);  /* que no pasen toques debajo */
    lv_obj_set_flex_flow(capa, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(capa, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_top(capa, 10, 0);
    lv_obj_set_style_pad_row(capa, 8, 0);

    lv_obj_t *titulo = lv_label_create(capa);
    lv_label_set_text(titulo, TR(LV_SYMBOL_SETTINGS "  Settings", LV_SYMBOL_SETTINGS "  Ajustes"));
    lv_obj_set_style_text_font(titulo, &radio_20, 0);
    lv_obj_set_style_text_color(titulo, lv_color_hex(COLOR_ACENTO), 0);

    /* Idioma: el inglés, primero; el que está en uso, resaltado */
    boton(capa, "English", !ajustes.espanol, al_elegir_idioma, (void *)(uintptr_t)false);
    boton(capa, "Español", ajustes.espanol, al_elegir_idioma, (void *)(uintptr_t)true);
    boton(capa, TR(LV_SYMBOL_WIFI "  Wi-Fi network", LV_SYMBOL_WIFI "  Red WiFi"), false, al_wifi, NULL);
    boton(capa, TR("Close", "Cerrar"), false, al_cerrar, NULL);
}
