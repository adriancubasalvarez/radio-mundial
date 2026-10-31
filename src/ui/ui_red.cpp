/**
 * Pantalla del portal WiFi: mientras el portal está abierto, cubre todo con
 * las instrucciones y un código QR para que el móvil se una a la red de la
 * radio. Se dibuja en la capa superior de LVGL, por encima de cualquier
 * pantalla, y desaparece sola cuando la radio ya está conectada.
 */
#include "ui.h"
#include "comun.h"
#include "hal.h"

#include <lvgl.h>
#include <stdio.h>
#include <string.h>

#define PERIODO_RED 500   /* ms entre comprobaciones */
#define TAM_QR      110

static lv_obj_t *capa;

static void al_cancelar(lv_event_t *e)
{
    (void)e;
    hal_red_cancelar();
}

static void crear_capa(void)
{
    capa = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(capa);
    lv_obj_set_size(capa, PANTALLA_ANCHO, PANTALLA_ALTO);
    lv_obj_set_style_bg_color(capa, lv_color_hex(COLOR_FONDO), 0);
    lv_obj_set_style_bg_opa(capa, LV_OPA_COVER, 0);
    lv_obj_add_flag(capa, LV_OBJ_FLAG_CLICKABLE);  /* que no pasen toques debajo */

    lv_obj_t *titulo = lv_label_create(capa);
    lv_label_set_text(titulo, LV_SYMBOL_WIFI "  Conecta la radio a tu WiFi");
    lv_obj_set_style_text_font(titulo, &radio_20, 0);
    lv_obj_set_style_text_color(titulo, lv_color_hex(COLOR_ACENTO), 0);
    lv_obj_align(titulo, LV_ALIGN_TOP_MID, 0, 8);

    /* QR con el formato estándar de red WiFi abierta: el móvil se une solo */
    char contenido[64];
    snprintf(contenido, sizeof(contenido), "WIFI:T:nopass;S:%s;;", hal_red_portal());
    lv_obj_t *qr = lv_qrcode_create(capa, TAM_QR, lv_color_black(), lv_color_white());
    lv_qrcode_update(qr, contenido, strlen(contenido));
    lv_obj_set_style_border_color(qr, lv_color_white(), 0);
    lv_obj_set_style_border_width(qr, 4, 0);
    lv_obj_align(qr, LV_ALIGN_LEFT_MID, 14, 8);

    char pasos[200];
    snprintf(pasos, sizeof(pasos),
             "1. Con el móvil, únete\n    a la red:\n    #FFC845 %s#\n"
             "    (o escanea el QR)\n2. Se abrirá una página:\n    elige tu WiFi y\n    escribe la contraseña.",
             hal_red_portal());
    lv_obj_t *texto = lv_label_create(capa);
    lv_label_set_recolor(texto, true);
    lv_label_set_text(texto, pasos);
    lv_obj_set_style_text_font(texto, &radio_14, 0);
    lv_obj_set_style_text_color(texto, lv_color_hex(COLOR_TEXTO), 0);
    lv_obj_align(texto, LV_ALIGN_LEFT_MID, 14 + TAM_QR + 14, 8);

    lv_obj_t *nota = lv_label_create(capa);
    lv_label_set_text(nota, "Si no se abre: 192.168.4.1");
    lv_obj_set_style_text_color(nota, lv_color_hex(COLOR_SECUNDARIO), 0);
    lv_obj_align(nota, LV_ALIGN_BOTTOM_LEFT, 14, -10);

    lv_obj_t *cancelar = lv_btn_create(capa);
    lv_obj_set_size(cancelar, 90, 32);
    lv_obj_align(cancelar, LV_ALIGN_BOTTOM_RIGHT, -10, -4);
    lv_obj_set_style_bg_color(cancelar, lv_color_hex(COLOR_LINEA), 0);
    lv_obj_add_event_cb(cancelar, al_cancelar, LV_EVENT_CLICKED, NULL);
    lv_obj_t *etiqueta = lv_label_create(cancelar);
    lv_label_set_text(etiqueta, "Cancelar");
    lv_obj_center(etiqueta);
}

static void vigilar(lv_timer_t *t)
{
    (void)t;
    bool portal = hal_red_estado() == RED_PORTAL;
    if (portal && capa == NULL) {
        crear_capa();
    } else if (!portal && capa != NULL) {
        lv_obj_del(capa);
        capa = NULL;
    }
}

void red_ui_iniciar(void)
{
    lv_timer_create(vigilar, PERIODO_RED, NULL);
}
