/**
 * Pantalla «Reproduciendo»: emisora, país, título de la canción,
 * reproducir/parar, favorito y volumen.
 *
 * Robustez: un vigilante consulta cada medio segundo el estado del audio.
 * Si la emisora no arranca o se corta, avisa y vuelve a la lista; en modo
 * aleatorio, salta a otra emisora al azar.
 */
#include "nav.h"
#include "comun.h"
#include "ajustes.h"
#include "hal.h"

#include <lvgl.h>
#include <stdio.h>
#include <string.h>

#define PERIODO_VIGILANTE 500   /* ms entre comprobaciones */
#define TIEMPO_AVISO      2500  /* ms que se ve el aviso de error */
#define TIEMPO_SALTO      1500  /* ms antes de saltar a otra aleatoria */

static lv_obj_t *titulo_cabecera;
static lv_obj_t *etiqueta_nombre;
static lv_obj_t *etiqueta_detalle;
static lv_obj_t *etiqueta_estado;
static lv_obj_t *icono_boton;
static lv_obj_t *icono_favorito;
static lv_obj_t *etiqueta_volumen;
static lv_timer_t *vigilante;
static lv_timer_t *temporizador_aviso;
static bool llego_a_sonar;  /* para distinguir «no arrancó» de «se cortó» */
static char ultimo_estado[160];

/* Cambia el texto de estado solo si es distinto (evita reiniciar el desplazamiento) */
static void mostrar_estado(const char *texto, uint32_t color)
{
    if (strcmp(ultimo_estado, texto) != 0) {
        strncpy(ultimo_estado, texto, sizeof(ultimo_estado) - 1);
        lv_label_set_text(etiqueta_estado, texto);
    }
    lv_obj_set_style_text_color(etiqueta_estado, lv_color_hex(color), 0);
}

static void pintar_favorito(void)
{
    bool es = ajustes_es_favorito(nav.emisora.url);
    lv_obj_set_style_text_color(icono_favorito, lv_color_hex(es ? COLOR_ACENTO : COLOR_SECUNDARIO), 0);
}

/* Rellena los textos con la emisora de nav (al entrar o al saltar a otra) */
static void pintar_emisora(void)
{
    lv_label_set_text(titulo_cabecera, nav.pais.nombre);
    lv_label_set_text(etiqueta_nombre, nav.emisora.nombre);
    if (nav.emisora.bitrate > 0) {
        lv_label_set_text_fmt(etiqueta_detalle, "%s · %d kbps", nav.emisora.codec, nav.emisora.bitrate);
    } else {
        lv_label_set_text(etiqueta_detalle, nav.emisora.codec);
    }
    pintar_favorito();
}

static void empezar(void)
{
    nav_reproducir();
    llego_a_sonar = false;
    lv_label_set_text(icono_boton, LV_SYMBOL_STOP);
    mostrar_estado("Conectando...", COLOR_SECUNDARIO);
}

static void parar(void)
{
    audio_parar();
    nav.hay_sonando = false;
    lv_label_set_text(icono_boton, LV_SYMBOL_PLAY);
    mostrar_estado("Parado", COLOR_SECUNDARIO);
}

static void siguiente_aleatoria(void)
{
    if (datos_emisora_aleatoria(&nav.emisora, &nav.pais)) {
        pintar_emisora();
        empezar();
    }
}

static void al_acabar_aviso(lv_timer_t *t)
{
    (void)t;
    temporizador_aviso = NULL;
    if (nav.aleatoria) {
        siguiente_aleatoria();
    } else {
        nav_atras();
    }
}

static void vigilar(lv_timer_t *t)
{
    (void)t;
    if (!nav.hay_sonando || temporizador_aviso != NULL) {
        return;
    }
    switch (audio_estado()) {
    case AUDIO_SONANDO: {
        llego_a_sonar = true;
        /* Si la emisora envía el título de la canción, se muestra */
        char titulo[140], texto[160];
        comun_texto_seguro(audio_titulo(), titulo, sizeof(titulo));
        if (titulo[0] != '\0') {
            snprintf(texto, sizeof(texto), LV_SYMBOL_AUDIO "  %s", titulo);
            mostrar_estado(texto, COLOR_ACENTO);
        } else {
            mostrar_estado("Sonando", COLOR_ACENTO);
        }
        break;
    }
    case AUDIO_CONECTANDO:
        mostrar_estado("Conectando...", COLOR_SECUNDARIO);
        break;
    case AUDIO_ERROR:
    case AUDIO_PARADO:
        /* La emisora no arrancó o se cortó: avisar y volver (o saltar) */
        audio_parar();
        nav.hay_sonando = false;
        lv_label_set_text(icono_boton, LV_SYMBOL_PLAY);
        if (nav.aleatoria) {
            mostrar_estado("No responde: buscando otra...", COLOR_ERROR);
        } else {
            mostrar_estado(llego_a_sonar ? "Se ha cortado la emisora" : "No se pudo reproducir esta emisora",
                           COLOR_ERROR);
        }
        printf("Fallo: %s\n", nav.emisora.nombre);
        temporizador_aviso = lv_timer_create(al_acabar_aviso, nav.aleatoria ? TIEMPO_SALTO : TIEMPO_AVISO, NULL);
        lv_timer_set_repeat_count(temporizador_aviso, 1);
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

static void al_pulsar_otra(lv_event_t *e)
{
    (void)e;
    if (temporizador_aviso != NULL) {
        lv_timer_del(temporizador_aviso);
        temporizador_aviso = NULL;
    }
    siguiente_aleatoria();
}

static void al_pulsar_favorito(lv_event_t *e)
{
    (void)e;
    bool ahora = ajustes_alternar_favorito(&nav.emisora, &nav.pais);
    pintar_favorito();
    if (!ahora && ajustes.num_favoritos >= MAX_FAVORITOS && !ajustes_es_favorito(nav.emisora.url)) {
        mostrar_estado("Ya tienes 20 favoritos: quita alguno", COLOR_ERROR);
    }
}

static void al_mover_volumen(lv_event_t *e)
{
    lv_obj_t *barra = lv_event_get_target(e);
    ajustes.volumen = (uint8_t)lv_slider_get_value(barra);
    lv_label_set_text_fmt(etiqueta_volumen, "%d", ajustes.volumen);
    /* El volumen se aplica y se guarda al soltar (en el PC reinicia ffplay) */
    if (lv_event_get_code(e) == LV_EVENT_RELEASED) {
        audio_volumen(ajustes.volumen);
        ajustes_guardar();
    }
}

static void al_borrar(lv_event_t *e)
{
    (void)e;
    lv_timer_del(vigilante);
    vigilante = NULL;
    if (temporizador_aviso != NULL) {
        lv_timer_del(temporizador_aviso);
        temporizador_aviso = NULL;
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

/* Botón redondo con un símbolo; se encoge un poco al pulsarlo */
static lv_obj_t *crear_boton_redondo(lv_obj_t *padre, int ancho, int alto, uint32_t color,
                                     const lv_font_t *fuente, lv_event_cb_t accion, lv_obj_t **icono)
{
    lv_obj_t *boton = lv_btn_create(padre);
    lv_obj_set_size(boton, ancho, alto);
    lv_obj_set_style_radius(boton, alto / 2, 0);
    lv_obj_set_style_bg_color(boton, lv_color_hex(color), 0);
    lv_obj_set_style_transform_width(boton, -3, LV_STATE_PRESSED);
    lv_obj_set_style_transform_height(boton, -3, LV_STATE_PRESSED);
    lv_obj_add_event_cb(boton, accion, LV_EVENT_CLICKED, NULL);
    *icono = lv_label_create(boton);
    lv_obj_set_style_text_font(*icono, fuente, 0);
    lv_obj_set_style_text_color(*icono, lv_color_hex(COLOR_FONDO), 0);
    lv_obj_center(*icono);
    return boton;
}

lv_obj_t *pantalla_sonando(void)
{
    lv_obj_t *pantalla = comun_pantalla();
    lv_obj_add_event_cb(pantalla, al_borrar, LV_EVENT_DELETE, NULL);
    lv_obj_t *cabecera = comun_cabecera(pantalla, nav.pais.nombre, true, &titulo_cabecera);
    lv_obj_t *favorito = comun_boton_cabecera(cabecera, SIMBOLO_ESTRELLA, al_pulsar_favorito);
    icono_favorito = lv_obj_get_child(favorito, 0);

    etiqueta_nombre = crear_texto(pantalla, &radio_28, COLOR_TEXTO, ALTO_CABECERA + 14);
    etiqueta_detalle = crear_texto(pantalla, &radio_14, COLOR_SECUNDARIO, ALTO_CABECERA + 50);
    etiqueta_estado = crear_texto(pantalla, &radio_14, COLOR_SECUNDARIO, ALTO_CABECERA + 72);
    ultimo_estado[0] = '\0';

    /* Botón grande de reproducir / parar */
    lv_obj_t *boton = crear_boton_redondo(pantalla, 72, 60, COLOR_ACENTO, &radio_28, al_pulsar_boton, &icono_boton);
    lv_obj_align(boton, LV_ALIGN_TOP_MID, 0, ALTO_CABECERA + 96);

    /* En modo aleatorio, botón para saltar a otra emisora */
    if (nav.aleatoria) {
        lv_obj_t *icono_otra;
        lv_obj_t *otra = crear_boton_redondo(pantalla, 50, 50, COLOR_SECUNDARIO, &radio_20, al_pulsar_otra, &icono_otra);
        lv_label_set_text(icono_otra, SIMBOLO_ALEATORIO);
        lv_obj_align_to(otra, boton, LV_ALIGN_OUT_RIGHT_MID, 28, 0);
    }

    /* Volumen */
    lv_obj_t *icono_vol = lv_label_create(pantalla);
    lv_label_set_text(icono_vol, LV_SYMBOL_VOLUME_MID);
    lv_obj_set_style_text_font(icono_vol, &radio_20, 0);
    lv_obj_set_style_text_color(icono_vol, lv_color_hex(COLOR_SECUNDARIO), 0);
    lv_obj_align(icono_vol, LV_ALIGN_BOTTOM_LEFT, 14, -14);

    lv_obj_t *barra = lv_slider_create(pantalla);
    lv_slider_set_range(barra, 0, 100);
    lv_slider_set_value(barra, ajustes.volumen, LV_ANIM_OFF);
    lv_obj_set_width(barra, 210);
    lv_obj_align(barra, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_ext_click_area(barra, 12);
    lv_obj_add_event_cb(barra, al_mover_volumen, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(barra, al_mover_volumen, LV_EVENT_RELEASED, NULL);

    etiqueta_volumen = lv_label_create(pantalla);
    lv_label_set_text_fmt(etiqueta_volumen, "%d", ajustes.volumen);
    lv_obj_set_style_text_color(etiqueta_volumen, lv_color_hex(COLOR_SECUNDARIO), 0);
    lv_obj_align(etiqueta_volumen, LV_ALIGN_BOTTOM_RIGHT, -14, -16);

    pintar_emisora();

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
