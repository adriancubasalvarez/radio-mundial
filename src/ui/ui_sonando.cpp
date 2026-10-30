/**
 * Pantalla «Reproduciendo»: emisora, país, reproducir/parar y volumen.
 *
 * Robustez: un vigilante consulta cada medio segundo el estado del audio.
 * Si la emisora no arranca o se corta, avisa y vuelve a la lista.
 */
#include "nav.h"
#include "comun.h"
#include "hal.h"

#include <lvgl.h>
#include <stdio.h>
#include <string.h>

#define PERIODO_VIGILANTE 500   /* ms entre comprobaciones */
#define TIEMPO_AVISO      2500  /* ms que se ve el aviso de error */

static lv_obj_t *etiqueta_estado;
static lv_obj_t *icono_boton;
static lv_obj_t *etiqueta_volumen;
static lv_timer_t *vigilante;
static lv_timer_t *temporizador_vuelta;
static bool llego_a_sonar;  /* para distinguir «no arrancó» de «se cortó» */

static void mostrar_estado(const char *texto, uint32_t color)
{
    lv_label_set_text(etiqueta_estado, texto);
    lv_obj_set_style_text_color(etiqueta_estado, lv_color_hex(color), 0);
}

static void empezar(void)
{
    audio_reproducir(nav.emisora.url);
    nav.sonando = nav.emisora;
    nav.hay_sonando = true;
    llego_a_sonar = false;
    lv_label_set_text(icono_boton, LV_SYMBOL_STOP);
    mostrar_estado("Conectando...", COLOR_SECUNDARIO);
    printf("Reproduciendo: %s\n  %s\n", nav.emisora.nombre, nav.emisora.url);
    fflush(stdout);
}

static void parar(void)
{
    audio_parar();
    nav.hay_sonando = false;
    lv_label_set_text(icono_boton, LV_SYMBOL_PLAY);
    mostrar_estado("Parado", COLOR_SECUNDARIO);
}

static void volver(lv_timer_t *t)
{
    (void)t;
    temporizador_vuelta = NULL;
    nav_atras();
}

static void vigilar(lv_timer_t *t)
{
    (void)t;
    if (!nav.hay_sonando || temporizador_vuelta != NULL) {
        return;
    }
    switch (audio_estado()) {
    case AUDIO_SONANDO:
        llego_a_sonar = true;
        mostrar_estado("Sonando", COLOR_ACENTO);
        break;
    case AUDIO_CONECTANDO:
        mostrar_estado("Conectando...", COLOR_SECUNDARIO);
        break;
    case AUDIO_ERROR:
    case AUDIO_PARADO:
        /* La emisora no arrancó o se cortó: avisar y volver a la lista */
        audio_parar();
        nav.hay_sonando = false;
        lv_label_set_text(icono_boton, LV_SYMBOL_PLAY);
        mostrar_estado(llego_a_sonar ? "Se ha cortado la emisora" : "No se pudo reproducir esta emisora",
                       COLOR_ERROR);
        printf("Fallo: %s\n", nav.emisora.nombre);
        temporizador_vuelta = lv_timer_create(volver, TIEMPO_AVISO, NULL);
        lv_timer_set_repeat_count(temporizador_vuelta, 1);
        break;
    }
}

static void al_pulsar_boton(lv_event_t *e)
{
    (void)e;
    if (nav.hay_sonando) {
        parar();
    } else {
        empezar();
    }
}

static void al_mover_volumen(lv_event_t *e)
{
    lv_obj_t *barra = lv_event_get_target(e);
    nav.volumen = (uint8_t)lv_slider_get_value(barra);
    lv_label_set_text_fmt(etiqueta_volumen, "%d", nav.volumen);
    /* El volumen se aplica al soltar (en el PC reinicia ffplay) */
    if (lv_event_get_code(e) == LV_EVENT_RELEASED) {
        audio_volumen(nav.volumen);
    }
}

static void al_borrar(lv_event_t *e)
{
    (void)e;
    lv_timer_del(vigilante);
    vigilante = NULL;
    if (temporizador_vuelta != NULL) {
        lv_timer_del(temporizador_vuelta);
        temporizador_vuelta = NULL;
    }
}

/* Etiqueta de una línea que se desplaza si no cabe */
static lv_obj_t *crear_texto(lv_obj_t *padre, const lv_font_t *fuente, uint32_t color, int y)
{
    lv_obj_t *etiqueta = lv_label_create(padre);
    lv_label_set_long_mode(etiqueta, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(etiqueta, PANTALLA_ANCHO - 24);
    lv_obj_set_style_text_font(etiqueta, fuente, 0);
    lv_obj_set_style_text_color(etiqueta, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(etiqueta, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(etiqueta, LV_ALIGN_TOP_MID, 0, y);
    return etiqueta;
}

lv_obj_t *pantalla_sonando(void)
{
    lv_obj_t *pantalla = comun_pantalla();
    lv_obj_add_event_cb(pantalla, al_borrar, LV_EVENT_DELETE, NULL);
    comun_cabecera(pantalla, nav.pais.nombre, true, NULL);

    /* Nombre de la emisora y datos técnicos */
    lv_obj_t *nombre = crear_texto(pantalla, &radio_28, COLOR_TEXTO, ALTO_CABECERA + 14);
    lv_label_set_text(nombre, nav.emisora.nombre);

    lv_obj_t *detalle = crear_texto(pantalla, &radio_14, COLOR_SECUNDARIO, ALTO_CABECERA + 50);
    if (nav.emisora.bitrate > 0) {
        lv_label_set_text_fmt(detalle, "%s · %d kbps", nav.emisora.codec, nav.emisora.bitrate);
    } else {
        lv_label_set_text(detalle, nav.emisora.codec);
    }

    etiqueta_estado = crear_texto(pantalla, &radio_14, COLOR_SECUNDARIO, ALTO_CABECERA + 72);

    /* Botón grande de reproducir / parar */
    lv_obj_t *boton = lv_btn_create(pantalla);
    lv_obj_set_size(boton, 72, 60);
    lv_obj_align(boton, LV_ALIGN_TOP_MID, 0, ALTO_CABECERA + 96);
    lv_obj_set_style_radius(boton, 30, 0);
    lv_obj_set_style_bg_color(boton, lv_color_hex(COLOR_ACENTO), 0);
    lv_obj_add_event_cb(boton, al_pulsar_boton, LV_EVENT_CLICKED, NULL);
    icono_boton = lv_label_create(boton);
    lv_obj_set_style_text_font(icono_boton, &radio_28, 0);
    lv_obj_set_style_text_color(icono_boton, lv_color_hex(COLOR_FONDO), 0);
    lv_obj_center(icono_boton);

    /* Volumen */
    lv_obj_t *icono_vol = lv_label_create(pantalla);
    lv_label_set_text(icono_vol, LV_SYMBOL_VOLUME_MID);
    lv_obj_set_style_text_font(icono_vol, &radio_20, 0);
    lv_obj_set_style_text_color(icono_vol, lv_color_hex(COLOR_SECUNDARIO), 0);
    lv_obj_align(icono_vol, LV_ALIGN_BOTTOM_LEFT, 14, -14);

    lv_obj_t *barra = lv_slider_create(pantalla);
    lv_slider_set_range(barra, 0, 100);
    lv_slider_set_value(barra, nav.volumen, LV_ANIM_OFF);
    lv_obj_set_width(barra, 210);
    lv_obj_align(barra, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_ext_click_area(barra, 12);
    lv_obj_add_event_cb(barra, al_mover_volumen, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(barra, al_mover_volumen, LV_EVENT_RELEASED, NULL);

    etiqueta_volumen = lv_label_create(pantalla);
    lv_label_set_text_fmt(etiqueta_volumen, "%d", nav.volumen);
    lv_obj_set_style_text_color(etiqueta_volumen, lv_color_hex(COLOR_SECUNDARIO), 0);
    lv_obj_align(etiqueta_volumen, LV_ALIGN_BOTTOM_RIGHT, -14, -16);

    /* Si ya sonaba esta misma emisora no se reinicia; si no, se pone */
    EstadoAudio estado = audio_estado();
    bool misma = nav.hay_sonando && strcmp(nav.sonando.url, nav.emisora.url) == 0;
    if (misma && (estado == AUDIO_SONANDO || estado == AUDIO_CONECTANDO)) {
        lv_label_set_text(icono_boton, LV_SYMBOL_STOP);
        llego_a_sonar = (estado == AUDIO_SONANDO);
        mostrar_estado(llego_a_sonar ? "Sonando" : "Conectando...",
                       llego_a_sonar ? COLOR_ACENTO : COLOR_SECUNDARIO);
    } else {
        empezar();
    }

    vigilante = lv_timer_create(vigilar, PERIODO_VIGILANTE, NULL);
    return pantalla;
}
