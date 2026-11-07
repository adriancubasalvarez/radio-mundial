/**
 * Actualizar las emisoras: comprueba si hay una lista nueva y la instala.
 * Se abre con el botón de la esquina de Ajustes.
 */
#include "nav.h"
#include "comun.h"
#include "hal.h"
#include "idioma.h"

#include <lvgl.h>
#include <string.h>

#define PERIODO_MS 200
#define ESPERA_REINICIO_MS 1500

static lv_obj_t *texto;
static lv_obj_t *barra;
static lv_obj_t *boton_accion;
static lv_obj_t *texto_accion;
static lv_obj_t *boton_volver;
static lv_timer_t *vigilante;
static EstadoActualizar visto;
static uint32_t listo_desde_ms;

static void mostrar_botones(bool accion, bool volver)
{
    if (accion) {
        lv_obj_clear_flag(boton_accion, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(boton_accion, LV_OBJ_FLAG_HIDDEN);
    }
    if (volver) {
        lv_obj_clear_flag(boton_volver, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(boton_volver, LV_OBJ_FLAG_HIDDEN);
    }
}

static void mostrar_barra(bool si)
{
    if (si) {
        lv_obj_clear_flag(barra, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(barra, LV_OBJ_FLAG_HIDDEN);
    }
}

/* Cambia los textos y botones al pasar de un estado a otro */
static void pintar_estado(EstadoActualizar estado)
{
    switch (estado) {
    case ACT_COMPROBANDO:
        lv_label_set_text(texto, TR("Checking for new stations...", "Buscando emisoras nuevas..."));
        mostrar_botones(false, false);
        break;
    case ACT_AL_DIA:
        lv_label_set_text(texto, TR("Your stations are up to date.", "Tus emisoras están al día."));
        mostrar_botones(false, true);
        break;
    case ACT_HAY_NUEVA:
        lv_label_set_text_fmt(texto, TR("New station list available:\n%s\n\nMusic will stop while it installs.",
                                        "Hay una lista nueva:\n%s\n\nLa música se parará mientras se instala."),
                              hal_actualizar_fecha());
        lv_label_set_text(texto_accion, TR("Update", "Actualizar"));
        mostrar_botones(true, true);
        break;
    case ACT_DESCARGANDO:
        lv_label_set_text(texto, TR("Downloading...", "Descargando..."));
        mostrar_botones(false, false);
        mostrar_barra(true);
        break;
    case ACT_INSTALANDO:
        lv_label_set_text(texto, TR("Installing...\nDo not unplug the radio.", "Instalando...\nNo desenchufes la radio."));
        break;
    case ACT_LISTO:
        lv_label_set_text(texto, TR("Done! Restarting...", "¡Listo! Reiniciando..."));
        mostrar_barra(false);
        listo_desde_ms = hal_millis();
        break;
    case ACT_ERROR:
        lv_label_set_text(texto, TR("Could not update.\nCheck your Wi-Fi and try again.",
                                    "No se pudo actualizar.\nRevisa la WiFi y vuelve a intentarlo."));
        lv_label_set_text(texto_accion, TR("Retry", "Reintentar"));
        mostrar_botones(true, true);
        mostrar_barra(false);
        break;
    default:
        break;
    }
}

static void vigilar(lv_timer_t *t)
{
    (void)t;
    uint8_t progreso = 0;
    EstadoActualizar estado = hal_actualizar_estado(&progreso);
    if (estado != visto) {
        visto = estado;
        pintar_estado(estado);
    }
    if (estado == ACT_DESCARGANDO || estado == ACT_INSTALANDO) {
        lv_bar_set_value(barra, progreso, LV_ANIM_OFF);
    }
    if (estado == ACT_LISTO && hal_millis() - listo_desde_ms > ESPERA_REINICIO_MS) {
        hal_reiniciar();
    }
}

static void al_pulsar_accion(lv_event_t *e)
{
    (void)e;
    if (visto == ACT_HAY_NUEVA) {
        audio_parar();
        nav.hay_sonando = false;
        hal_actualizar_instalar();
    } else {
        hal_actualizar_comprobar();
    }
}

static void al_pulsar_volver(lv_event_t *e)
{
    (void)e;
    nav_atras();
}

static void al_borrar(lv_event_t *e)
{
    (void)e;
    lv_timer_del(vigilante);
    vigilante = NULL;
}

static lv_obj_t *crear_boton(lv_obj_t *pantalla, uint32_t color, lv_align_t lugar, int x, lv_event_cb_t accion, lv_obj_t **etiqueta)
{
    lv_obj_t *b = lv_btn_create(pantalla);
    lv_obj_set_size(b, 140, 48);
    lv_obj_align(b, lugar, x, -14);
    lv_obj_set_style_radius(b, 12, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(color), 0);
    lv_obj_add_event_cb(b, accion, LV_EVENT_CLICKED, NULL);
    *etiqueta = lv_label_create(b);
    lv_obj_set_style_text_font(*etiqueta, &radio_20, 0);
    lv_obj_set_style_text_color(*etiqueta, lv_color_hex(color == COLOR_ACENTO ? COLOR_FONDO : COLOR_TEXTO), 0);
    lv_obj_center(*etiqueta);
    return b;
}

lv_obj_t *pantalla_actualizar(void)
{
    lv_obj_t *pantalla = comun_pantalla();
    lv_obj_add_event_cb(pantalla, al_borrar, LV_EVENT_DELETE, NULL);
    lv_obj_t *titulo;
    comun_cabecera(pantalla, TR("Update stations", "Actualizar emisoras"), false, &titulo);
    lv_obj_set_width(titulo, LV_SIZE_CONTENT);  /* aquí no hay botones a la derecha */

    texto = lv_label_create(pantalla);
    lv_label_set_long_mode(texto, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(texto, PANTALLA_ANCHO - 32);
    lv_obj_set_style_text_font(texto, &radio_20, 0);
    lv_obj_set_style_text_color(texto, lv_color_hex(COLOR_TEXTO), 0);
    lv_obj_set_style_text_align(texto, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(texto, LV_ALIGN_TOP_MID, 0, ALTO_CABECERA + 16);

    barra = lv_bar_create(pantalla);
    lv_obj_set_size(barra, PANTALLA_ANCHO - 48, 14);
    lv_obj_align(barra, LV_ALIGN_BOTTOM_MID, 0, -40);
    lv_bar_set_range(barra, 0, 100);
    lv_obj_set_style_bg_color(barra, lv_color_hex(COLOR_LINEA), 0);
    lv_obj_set_style_bg_color(barra, lv_color_hex(COLOR_ACENTO), LV_PART_INDICATOR);
    mostrar_barra(false);

    lv_obj_t *texto_volver;
    boton_accion = crear_boton(pantalla, COLOR_ACENTO, LV_ALIGN_BOTTOM_RIGHT, -12, al_pulsar_accion, &texto_accion);
    boton_volver = crear_boton(pantalla, COLOR_LINEA, LV_ALIGN_BOTTOM_LEFT, 12, al_pulsar_volver, &texto_volver);
    lv_label_set_text(texto_volver, TR("Back", "Volver"));

    visto = ACT_NADA;
    hal_actualizar_comprobar();
    vigilante = lv_timer_create(vigilar, PERIODO_MS, NULL);
    vigilar(vigilante);
    return pantalla;
}
