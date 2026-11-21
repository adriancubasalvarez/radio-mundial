/**
 * Menú de ajustes rápido: se abre manteniendo pulsado el título del
 * mapamundi. Permite cambiar el idioma, la red WiFi, la luz y la ubicación
 * del tiempo, y apagar la radio.
 * Se dibuja en la capa superior de LVGL, encima del mapamundi.
 *
 * Distribución (320x240): título arriba; botones grandes en dos columnas a
 * la izquierda (cuatro filas: idioma y alarmas, WiFi y luz, deslizador de
 * apagado, brillo y Cerrar) y una columna de iconos a la derecha, uno por
 * fila (ubicación, actualizar emisoras, salida de audio).
 */
#include "ui.h"
#include "nav.h"
#include "comun.h"
#include "ajustes.h"
#include "bateria.h"
#include "alarmas.h"
#include "hal.h"
#include "idioma.h"
#include "luz.h"

#include <lvgl.h>
#include <stdio.h>
#include <string.h>

#define X_IZQUIERDA    8
#define ANCHO_BOTON    120
#define SEPARACION     8
#define ANCHO_IZQUIERDA (2 * ANCHO_BOTON + SEPARACION)    /* 248 */
#define X_ICONOS       (PANTALLA_ANCHO - 8 - ANCHO_ICONO)  /* 266 */
#define ANCHO_ICONO    46
#define Y_FILA(n)      (44 + (n) * 48)                     /* 44, 92, 140, 188 */

static lv_obj_t *capa;

/* Texto de diagnóstico (reinicios y cortes) y el plazo para confirmar su borrado */
static lv_obj_t *etiqueta_diag;
static char texto_diag[96];
static lv_timer_t *temporizador_borrar;

static void quitar_temporizador_borrar(void)
{
    if (temporizador_borrar != NULL) {
        lv_timer_del(temporizador_borrar);
        temporizador_borrar = NULL;
    }
}

static void cerrar(void)
{
    quitar_temporizador_borrar();
    etiqueta_diag = NULL;
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

static void al_alarmas(lv_event_t *e)
{
    (void)e;
    cerrar();
    nav_ir(PANT_ALARMAS);
}

static void al_actualizar(lv_event_t *e)
{
    (void)e;
    cerrar();
    nav_ir(PANT_ACTUALIZAR);
}

static void al_ubicacion(lv_event_t *e)
{
    (void)e;
    cerrar();
    nav_ir(PANT_UBICACION);
}

static void al_wifi(lv_event_t *e)
{
    (void)e;
    cerrar();
    hal_red_configurar();
}

/* Texto del botón de la luz según el modo elegido */
static const char *texto_luz(void)
{
    switch (ajustes.luz) {
    case LUZ_APAGADA: return TR("Light: off", "Luz: no");
    case LUZ_ESTADO: return TR("Light: on", "Luz: fija");
    default: return TR("Light: beat", "Luz: ritmo");
    }
}

/* Cada toque pasa al siguiente modo: ritmo -> apagada -> fija -> ritmo */
static void al_luz(lv_event_t *e)
{
    ajustes.luz = (uint8_t)((ajustes.luz + 1) % NUM_MODOS_LUZ);
    ajustes_guardar();
    lv_label_set_text(lv_obj_get_child(lv_event_get_target(e), 0), texto_luz());
}

static const char *texto_brillo(void)
{
    switch (ajustes.luz_brillo) {
    case 0: return TR("Glow: low", "Brillo: bajo");
    case 1: return TR("Glow: medium", "Brillo: medio");
    default: return TR("Glow: high", "Brillo: alto");
    }
}

/* Cada toque pasa a la siguiente intensidad: baja -> media -> alta */
static void al_brillo(lv_event_t *e)
{
    ajustes.luz_brillo = (uint8_t)((ajustes.luz_brillo + 1) % NUM_BRILLOS_LUZ);
    ajustes_guardar();
    lv_label_set_text(lv_obj_get_child(lv_event_get_target(e), 0), texto_brillo());
}

/* Salida de audio: altavoz integrado o DAC externo (I2S) hacia un
 * amplificador. Si suena una emisora, sigue sonando por la nueva salida. */
static const char *texto_salida(void)
{
    return ajustes.salida_externa ? "DAC" : LV_SYMBOL_VOLUME_MAX;
}

static void al_salida(lv_event_t *e)
{
    ajustes.salida_externa = !ajustes.salida_externa;
    ajustes_guardar();
    audio_salida(ajustes.salida_externa);
    /* resaltado (como un botón marcado) mientras se usa el DAC externo */
    lv_obj_t *b = lv_event_get_target(e);
    lv_obj_t *etiqueta = lv_obj_get_child(b, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(ajustes.salida_externa ? COLOR_ACENTO : COLOR_LINEA), 0);
    lv_obj_set_style_text_color(etiqueta, lv_color_hex(ajustes.salida_externa ? COLOR_FONDO : COLOR_TEXTO), 0);
    lv_label_set_text(etiqueta, texto_salida());
    lv_obj_set_style_text_font(etiqueta, ajustes.salida_externa ? &radio_14 : &radio_20, 0);
}

/* El deslizador vale si el dedo empieza en su mitad izquierda y llega cerca
 * del final: así cuesta poco apagar, pero un toque suelto no apaga */
static bool empezo_a_la_izquierda;

static void al_pulsar_deslizador(lv_event_t *e)
{
    lv_obj_t *deslizador = lv_event_get_target(e);
    lv_point_t punto;
    lv_indev_get_point(lv_indev_get_act(), &punto);
    empezo_a_la_izquierda = punto.x < deslizador->coords.x1 + lv_obj_get_width(deslizador) / 2;
}

/* Al soltar: si llegó casi al final se apaga; si no, vuelve atrás */
static void al_deslizar(lv_event_t *e)
{
    lv_obj_t *deslizador = lv_event_get_target(e);
    if (!empezo_a_la_izquierda || lv_slider_get_value(deslizador) < 80) {
        lv_slider_set_value(deslizador, 0, LV_ANIM_ON);
        return;
    }
    audio_parar();
    ajustes_guardar();
    hal_apagar(alarmas_segundos_para_despertar());
}

/* «Desliza para apagar»: hay que arrastrar el botón redondo hacia el final,
 * así no se apaga con un toque sin querer */
static void deslizador_apagar(lv_obj_t *padre)
{
    lv_obj_t *d = lv_slider_create(padre);
    lv_obj_set_size(d, ANCHO_IZQUIERDA - 40, 40);  /* el botón sobresale media altura a cada lado */
    lv_obj_set_pos(d, X_IZQUIERDA + 20, Y_FILA(2));
    lv_slider_set_range(d, 0, 100);
    lv_obj_set_ext_click_area(d, 8);
    lv_obj_set_style_radius(d, 20, LV_PART_MAIN);
    lv_obj_set_style_bg_color(d, lv_color_hex(COLOR_LINEA), LV_PART_MAIN);
    lv_obj_set_style_radius(d, 20, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(d, lv_color_hex(COLOR_ERROR), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(d, lv_color_hex(COLOR_ERROR), LV_PART_KNOB);
    lv_obj_set_style_pad_all(d, 0, LV_PART_KNOB);
    lv_obj_add_event_cb(d, al_pulsar_deslizador, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(d, al_deslizar, LV_EVENT_RELEASED, NULL);

    lv_obj_t *etiqueta = lv_label_create(d);
    lv_label_set_text(etiqueta, TR(LV_SYMBOL_POWER "  Slide to power off", LV_SYMBOL_POWER "  Desliza para apagar"));
    lv_obj_set_style_text_font(etiqueta, &radio_14, 0);
    lv_obj_set_style_text_color(etiqueta, lv_color_hex(COLOR_SECUNDARIO), 0);
    lv_obj_center(etiqueta);
}

/* Arriba a la derecha, en pequeño, lo que ayuda a saber si algo va mal (solo
 * si lo hay):
 *   - reinicios inesperados: cuántos, por qué el último y a qué hora más o
 *     menos (por ejemplo, si la radio se reinicia sola de noche);
 *   - cortes de la WiFi desde que se encendió (si está lejos del router).
 * Para borrarlos, se toca el texto: pide confirmación («¿Borrar? Toca otra
 * vez») y, si se vuelve a tocar antes de TIEMPO_BORRAR, se borran; si no,
 * vuelve a como estaba. Así no se borran por un toque sin querer. */
#define TIEMPO_BORRAR 3000  /* ms para confirmar */

static void mostrar_diag(const char *texto, uint32_t color)
{
    lv_label_set_text(etiqueta_diag, texto);
    lv_obj_set_style_text_color(etiqueta_diag, lv_color_hex(color), 0);
    /* una línea, a la altura del título; dos, una encima y otra debajo */
    lv_obj_align(etiqueta_diag, LV_ALIGN_TOP_RIGHT, -8, strchr(texto, '\n') != NULL ? 6 : 16);
}

static void al_caducar_borrar(lv_timer_t *t)
{
    (void)t;
    temporizador_borrar = NULL;  /* de una sola vez: LVGL lo borra al acabar */
    if (etiqueta_diag != NULL) {
        mostrar_diag(texto_diag, COLOR_SECUNDARIO);
    }
}

static void al_tocar_diag(lv_event_t *e)
{
    (void)e;
    if (temporizador_borrar == NULL) {
        mostrar_diag(TR("Clear? Tap again", "¿Borrar? Toca otra vez"), COLOR_ACENTO);
        temporizador_borrar = lv_timer_create(al_caducar_borrar, TIEMPO_BORRAR, NULL);
        lv_timer_set_repeat_count(temporizador_borrar, 1);
        return;
    }
    quitar_temporizador_borrar();
    hal_reinicios_borrar();
    hal_red_cortes_borrar();
    /* y se olvida la batería: si sigue conectada, se vuelve a ver en 5 s;
     * si se quitó para siempre, ya no sale la batería con la ✕ */
    ajustes.bateria_vista = false;
    ajustes_guardar();
    /* se oculta en vez de borrarlo: estamos dentro de su propio evento */
    lv_obj_add_flag(etiqueta_diag, LV_OBJ_FLAG_HIDDEN);
}

static void diagnostico(lv_obj_t *padre)
{
    char *texto = texto_diag;
    texto[0] = '\0';
    size_t tam = sizeof(texto_diag);
    Reinicios r;
    hal_reinicios(&r);
    if (r.veces > 0) {
        if (r.minuto_del_dia >= 0) {
            snprintf(texto, tam, LV_SYMBOL_REFRESH " %u · %s · %02d:%02d", (unsigned)r.veces, r.motivo,
                     r.minuto_del_dia / 60, r.minuto_del_dia % 60);
        } else {
            snprintf(texto, tam, LV_SYMBOL_REFRESH " %u · %s", (unsigned)r.veces, r.motivo);
        }
    }
    uint16_t cortes = hal_red_cortes();
    if (cortes > 0) {
        size_t usado = strlen(texto);
        snprintf(texto + usado, tam - usado, "%s%s %u", usado > 0 ? "\n" : "",
                 TR("Wi-Fi drops:", "Cortes WiFi:"), (unsigned)cortes);
    }
    /* Interruptor de la batería, si la radio ha tenido batería alguna vez:
     * en la misma línea que los cortes (o en una nueva) */
    if (ajustes.bateria_vista) {
        size_t usado = strlen(texto);
        const char *separador = cortes > 0 ? " · " : (usado > 0 ? "\n" : "");
        snprintf(texto + usado, tam - usado, "%s%s", separador,
                 bateria_conectada() ? LV_SYMBOL_BATTERY_FULL " ON" : LV_SYMBOL_BATTERY_EMPTY " OFF");
    }
    if (texto[0] == '\0') {
        return;
    }
    etiqueta_diag = lv_label_create(padre);
    lv_obj_set_style_text_font(etiqueta_diag, &radio_14, 0);
    lv_obj_set_style_text_align(etiqueta_diag, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_add_flag(etiqueta_diag, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(etiqueta_diag, 10);  /* texto pequeño: algo más de margen para el dedo */
    lv_obj_add_event_cb(etiqueta_diag, al_tocar_diag, LV_EVENT_CLICKED, NULL);
    mostrar_diag(texto, COLOR_SECUNDARIO);
}

static lv_obj_t *boton(lv_obj_t *padre, const char *texto, bool marcado, lv_event_cb_t accion, void *dato,
                       lv_coord_t ancho = 260)
{
    lv_obj_t *b = lv_btn_create(padre);
    lv_obj_set_size(b, ancho, 40);
    lv_obj_set_style_radius(b, 10, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(marcado ? COLOR_ACENTO : COLOR_LINEA), 0);
    lv_obj_add_event_cb(b, accion, LV_EVENT_CLICKED, dato);
    lv_obj_t *etiqueta = lv_label_create(b);
    lv_label_set_text(etiqueta, texto);
    lv_obj_set_style_text_font(etiqueta, &radio_20, 0);
    lv_obj_set_style_text_color(etiqueta, lv_color_hex(marcado ? COLOR_FONDO : COLOR_TEXTO), 0);
    /* Si el texto no cabe (p. ej. «Light: beat» en un botón estrecho), letra pequeña */
    if (lv_txt_get_width(texto, strlen(texto), &radio_20, 0, LV_TEXT_FLAG_NONE) > ancho - 12) {
        lv_obj_set_style_text_font(etiqueta, &radio_14, 0);
    }
    lv_obj_center(etiqueta);
    return b;
}

/* Botón en una posición fija de la pantalla */
static lv_obj_t *boton_en(lv_coord_t x, lv_coord_t y, lv_coord_t ancho, const char *texto, bool marcado,
                          lv_event_cb_t accion, void *dato = NULL)
{
    lv_obj_t *b = boton(capa, texto, marcado, accion, dato, ancho);
    lv_obj_set_pos(b, x, y);
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
    lv_obj_clear_flag(capa, LV_OBJ_FLAG_SCROLLABLE);  /* que nada lo desplace */

    /* Título arriba, solo, sobre los botones grandes */
    lv_obj_t *titulo = lv_label_create(capa);
    lv_label_set_text(titulo, TR(LV_SYMBOL_SETTINGS "  Settings", LV_SYMBOL_SETTINGS "  Ajustes"));
    lv_obj_set_style_text_font(titulo, &radio_20, 0);
    lv_obj_set_style_text_color(titulo, lv_color_hex(COLOR_ACENTO), 0);
    lv_obj_set_pos(titulo, X_IZQUIERDA + 4, 12);
    diagnostico(capa);

    /* Botones grandes, a la izquierda, en dos columnas */
    const lv_coord_t x1 = X_IZQUIERDA, x2 = X_IZQUIERDA + ANCHO_BOTON + SEPARACION;
    boton_en(x1, Y_FILA(0), ANCHO_BOTON, ajustes.espanol ? "English" : "Español", false, al_elegir_idioma,
             (void *)(uintptr_t)!ajustes.espanol);
    boton_en(x2, Y_FILA(0), ANCHO_BOTON, TR(LV_SYMBOL_BELL " Alarms", LV_SYMBOL_BELL " Alarmas"), false,
             al_alarmas);
    /* El botón de la WiFi dice la señal (dBm) si está conectada */
    char texto_wifi[24];
    int senal = hal_red_senal();
    if (senal != 0) {
        snprintf(texto_wifi, sizeof(texto_wifi), LV_SYMBOL_WIFI " %d dBm", senal);
    } else {
        snprintf(texto_wifi, sizeof(texto_wifi), "%s", TR(LV_SYMBOL_WIFI " Wi-Fi", LV_SYMBOL_WIFI " WiFi"));
    }
    boton_en(x1, Y_FILA(1), ANCHO_BOTON, texto_wifi, false, al_wifi);
    boton_en(x2, Y_FILA(1), ANCHO_BOTON, texto_luz(), false, al_luz);
    deslizador_apagar(capa);
    boton_en(x1, Y_FILA(3), ANCHO_BOTON, texto_brillo(), false, al_brillo);
    boton_en(x2, Y_FILA(3), ANCHO_BOTON, TR("Close", "Cerrar"), false, al_cerrar);

    /* Columna de iconos a la derecha, uno por fila: ubicación del tiempo y la
     * hora, actualizar emisoras y salida de audio. Amarillos si están activos
     * (ciudad elegida a mano, DAC externo). */
    boton_en(X_ICONOS, Y_FILA(0), ANCHO_ICONO, "\xEF\x8F\x85", ajustes.ubicacion_fija, al_ubicacion);
    boton_en(X_ICONOS, Y_FILA(1), ANCHO_ICONO, LV_SYMBOL_DOWNLOAD, false, al_actualizar);
    lv_obj_t *salida = boton_en(X_ICONOS, Y_FILA(2), ANCHO_ICONO, texto_salida(), ajustes.salida_externa,
                                al_salida);
    if (ajustes.salida_externa) {
        lv_obj_set_style_text_font(lv_obj_get_child(salida, 0), &radio_14, 0);
    }
}
