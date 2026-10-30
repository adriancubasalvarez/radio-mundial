/**
 * Pantalla de reposo: tras ajustes.reposo_s segundos sin tocar la pantalla,
 * baja el brillo y muestra un reloj grande con la emisora y la canción.
 * Al tocar, desaparece y se vuelve a donde se estaba.
 *
 * Se dibuja en la capa superior de LVGL (lv_layer_top), por encima de
 * cualquier pantalla, así que no altera la navegación.
 */
#include "ui.h"
#include "nav.h"
#include "comun.h"
#include "ajustes.h"
#include "hal.h"

#include <lvgl.h>
#include <stdio.h>
#include <string.h>

#define PERIODO_REPOSO 1000  /* ms entre comprobaciones */
#define BRILLO_REPOSO  25    /* % de brillo en reposo */
#define BRILLO_NORMAL  100

static lv_obj_t *capa;
static lv_obj_t *etiqueta_reloj;
static lv_obj_t *etiqueta_emisora;
static lv_obj_t *etiqueta_titulo;
static char ultimo_titulo[140];

static void despertar(lv_event_t *e)
{
    (void)e;
    if (capa != NULL) {
        lv_obj_del_async(capa);
        capa = NULL;
        hal_brillo(BRILLO_NORMAL);
        lv_disp_trig_activity(NULL);
    }
}

static void actualizar(void)
{
    int hora, minuto;
    if (hal_hora(&hora, &minuto)) {
        lv_label_set_text_fmt(etiqueta_reloj, "%02d:%02d", hora, minuto);
    } else {
        lv_label_set_text(etiqueta_reloj, "");
    }

    EstadoAudio estado = audio_estado();
    bool suena = nav.hay_sonando && (estado == AUDIO_SONANDO || estado == AUDIO_CONECTANDO);
    lv_label_set_text(etiqueta_emisora, suena ? nav.sonando.nombre : "");

    char titulo[140];
    comun_texto_seguro(suena ? audio_titulo() : "", titulo, sizeof(titulo));
    if (strcmp(titulo, ultimo_titulo) != 0) {
        strcpy(ultimo_titulo, titulo);
        lv_label_set_text(etiqueta_titulo, titulo);
    }
}

static lv_obj_t *crear_texto(const lv_font_t *fuente, uint32_t color, int y)
{
    lv_obj_t *etiqueta = lv_label_create(capa);
    lv_label_set_long_mode(etiqueta, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(etiqueta, PANTALLA_ANCHO - 24);
    lv_obj_set_style_text_font(etiqueta, fuente, 0);
    lv_obj_set_style_text_color(etiqueta, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(etiqueta, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(etiqueta, LV_ALIGN_CENTER, 0, y);
    lv_label_set_text(etiqueta, "");
    return etiqueta;
}

static void dormir(void)
{
    capa = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(capa);
    lv_obj_set_size(capa, PANTALLA_ANCHO, PANTALLA_ALTO);
    lv_obj_set_style_bg_color(capa, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(capa, LV_OPA_COVER, 0);
    lv_obj_add_flag(capa, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(capa, despertar, LV_EVENT_PRESSED, NULL);

    etiqueta_reloj = crear_texto(&radio_reloj_48, COLOR_SECUNDARIO, -30);
    etiqueta_emisora = crear_texto(&radio_20, COLOR_ACENTO, 28);
    etiqueta_titulo = crear_texto(&radio_14, COLOR_SECUNDARIO, 56);
    ultimo_titulo[0] = '\0';
    actualizar();
    hal_brillo(BRILLO_REPOSO);
}

static void vigilar(lv_timer_t *t)
{
    (void)t;
    if (capa != NULL) {
        actualizar();
    } else if (ajustes.reposo_s > 0 && lv_disp_get_inactive_time(NULL) > ajustes.reposo_s * 1000UL) {
        dormir();
    }
}

void reposo_iniciar(void)
{
    lv_timer_create(vigilar, PERIODO_REPOSO, NULL);
}
