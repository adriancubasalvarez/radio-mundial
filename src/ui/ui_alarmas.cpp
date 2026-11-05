/**
 * Alarmas: lista, editor de una alarma y aviso a pantalla completa cuando
 * suena. La lista se abre desde el menú de ajustes.
 */
#include "nav.h"
#include "comun.h"
#include "ui.h"
#include "ajustes.h"
#include "alarmas.h"
#include "hal.h"
#include "idioma.h"

#include <lvgl.h>
#include <stdio.h>
#include <string.h>

#define ALTO_RODILLO 96

/* Días en pantalla, de lunes a domingo: su bit en Alarma.dias */
static const uint8_t ORDEN_DIAS[7] = {1, 2, 3, 4, 5, 6, 0};

static int editando;          /* posición en ajustes.alarmas */
static lv_obj_t *capa_aviso;  /* aviso de alarma sonando */

/* ---------- Lista ---------- */

static void al_elegir(uint16_t fila)
{
    if (fila >= ajustes.num_alarmas) {  /* «Nueva alarma» */
        Alarma *a = &ajustes.alarmas[ajustes.num_alarmas++];
        a->hora = 7;
        a->minuto = 30;
        a->dias = 0x3E;  /* lunes a viernes */
        a->activa = true;
    }
    editando = fila;
    nav_ir(PANT_ALARMA);
}

lv_obj_t *pantalla_alarmas(void)
{
    lv_obj_t *pantalla = comun_pantalla();
    comun_cabecera(pantalla, TR("Alarms", "Alarmas"), true, NULL);
    lv_obj_t *lista = comun_lista(pantalla, al_elegir);
    for (uint8_t i = 0; i < ajustes.num_alarmas; i++) {
        const Alarma *a = &ajustes.alarmas[i];
        char texto[40];
        snprintf(texto, sizeof(texto), "%02d:%02d   %s", a->hora, a->minuto, alarmas_texto_dias(a->dias));
        comun_lista_agregar(lista, a->activa ? LV_SYMBOL_BELL : "  ", texto, NULL, false);
    }
    if (ajustes.num_alarmas < MAX_ALARMAS) {
        comun_lista_agregar(lista, LV_SYMBOL_PLUS, TR("New alarm", "Nueva alarma"), NULL, false);
    }
    return pantalla;
}

/* ---------- Editor ---------- */

static void al_cambiar_hora(lv_event_t *e)
{
    lv_obj_t *rodillo = lv_event_get_target(e);
    uint8_t *campo = (uint8_t *)lv_event_get_user_data(e);
    *campo = (uint8_t)lv_roller_get_selected(rodillo);
}

static void al_cambiar_dia(lv_event_t *e)
{
    lv_obj_t *boton = lv_event_get_target(e);
    uint8_t bit = 1 << (uintptr_t)lv_event_get_user_data(e);
    Alarma *a = &ajustes.alarmas[editando];
    if (lv_obj_has_state(boton, LV_STATE_CHECKED)) {
        a->dias |= bit;
    } else {
        a->dias &= (uint8_t)~bit;
    }
}

static void al_cambiar_activa(lv_event_t *e)
{
    ajustes.alarmas[editando].activa = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
}

static void al_borrar_alarma(lv_event_t *e)
{
    /* un segundo toque durante la animación borraría otra alarma */
    lv_obj_add_state(lv_event_get_target(e), LV_STATE_DISABLED);
    for (int i = editando; i + 1 < ajustes.num_alarmas; i++) {
        ajustes.alarmas[i] = ajustes.alarmas[i + 1];
    }
    ajustes.num_alarmas--;
    nav_atras();
}

/* Se guarda una sola vez, al salir, para no gastar la memoria en cada toque */
static void al_salir(lv_event_t *e)
{
    (void)e;
    ajustes_guardar();
}

static lv_obj_t *rodillo(lv_obj_t *padre, int cuantos, int actual, int x, uint8_t *campo)
{
    static char opciones[60 * 3 + 1];
    char *p = opciones;
    for (int i = 0; i < cuantos; i++) {
        p += sprintf(p, "%02d\n", i);
    }
    p[-1] = '\0';
    lv_obj_t *r = lv_roller_create(padre);
    lv_roller_set_options(r, opciones, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(r, 3);
    lv_obj_set_width(r, 84);
    lv_obj_set_style_text_font(r, &radio_28, LV_PART_SELECTED);
    lv_obj_set_style_text_font(r, &radio_20, 0);
    lv_obj_set_style_bg_color(r, lv_color_hex(COLOR_FONDO), 0);
    lv_obj_set_style_border_width(r, 0, 0);
    lv_obj_set_style_text_color(r, lv_color_hex(COLOR_SECUNDARIO), 0);
    lv_obj_set_style_bg_color(r, lv_color_hex(COLOR_LINEA), LV_PART_SELECTED);
    lv_obj_set_style_text_color(r, lv_color_hex(COLOR_ACENTO), LV_PART_SELECTED);
    lv_roller_set_selected(r, actual, LV_ANIM_OFF);
    lv_obj_align(r, LV_ALIGN_TOP_MID, x, ALTO_CABECERA + 4);
    lv_obj_add_event_cb(r, al_cambiar_hora, LV_EVENT_VALUE_CHANGED, campo);
    return r;
}

lv_obj_t *pantalla_alarma(void)
{
    Alarma *a = &ajustes.alarmas[editando];
    lv_obj_t *pantalla = comun_pantalla();
    comun_cabecera(pantalla, TR("Alarm", "Alarma"), true, NULL);
    lv_obj_add_event_cb(pantalla, al_salir, LV_EVENT_DELETE, NULL);

    rodillo(pantalla, 24, a->hora, -52, &a->hora);
    rodillo(pantalla, 60, a->minuto, 52, &a->minuto);
    lv_obj_t *dos_puntos = lv_label_create(pantalla);
    lv_label_set_text(dos_puntos, ":");
    lv_obj_set_style_text_font(dos_puntos, &radio_28, 0);
    lv_obj_set_style_text_color(dos_puntos, lv_color_hex(COLOR_ACENTO), 0);
    lv_obj_align(dos_puntos, LV_ALIGN_TOP_MID, 0, ALTO_CABECERA + 4 + ALTO_RODILLO / 2 - 16);

    /* Días, de lunes a domingo; ninguno marcado = suena una vez */
    const char *letras = TR("MTWTFSS", "LMXJVSD");
    for (int i = 0; i < 7; i++) {
        lv_obj_t *b = lv_btn_create(pantalla);
        lv_obj_set_size(b, 40, 36);
        lv_obj_set_pos(b, 11 + i * 43, 142);
        lv_obj_add_flag(b, LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_style_radius(b, 8, 0);
        lv_obj_set_style_shadow_width(b, 0, 0);
        lv_obj_set_style_bg_color(b, lv_color_hex(COLOR_LINEA), 0);
        lv_obj_set_style_bg_color(b, lv_color_hex(COLOR_ACENTO), LV_STATE_CHECKED);
        lv_obj_set_style_text_color(b, lv_color_hex(COLOR_TEXTO), 0);
        lv_obj_set_style_text_color(b, lv_color_hex(COLOR_FONDO), LV_STATE_CHECKED);
        if (a->dias & (1 << ORDEN_DIAS[i])) {
            lv_obj_add_state(b, LV_STATE_CHECKED);
        }
        lv_obj_add_event_cb(b, al_cambiar_dia, LV_EVENT_VALUE_CHANGED, (void *)(uintptr_t)ORDEN_DIAS[i]);
        char letra[2] = {letras[i], '\0'};
        lv_obj_t *e = lv_label_create(b);
        lv_label_set_text(e, letra);
        lv_obj_set_style_text_font(e, &radio_20, 0);
        lv_obj_center(e);
    }

    lv_obj_t *interruptor = lv_switch_create(pantalla);
    lv_obj_set_size(interruptor, 56, 30);
    lv_obj_set_pos(interruptor, 14, 196);
    lv_obj_set_style_bg_color(interruptor, lv_color_hex(COLOR_LINEA), 0);
    lv_obj_set_style_bg_color(interruptor, lv_color_hex(COLOR_ACENTO), (unsigned)LV_PART_INDICATOR | LV_STATE_CHECKED);
    if (a->activa) {
        lv_obj_add_state(interruptor, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(interruptor, al_cambiar_activa, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_set_ext_click_area(interruptor, 8);
    lv_obj_t *texto_activa = lv_label_create(pantalla);
    lv_label_set_text(texto_activa, TR("On", "Activa"));
    lv_obj_set_style_text_font(texto_activa, &radio_20, 0);
    lv_obj_set_style_text_color(texto_activa, lv_color_hex(COLOR_TEXTO), 0);
    lv_obj_set_pos(texto_activa, 80, 200);

    lv_obj_t *borrar = lv_btn_create(pantalla);
    lv_obj_set_size(borrar, 110, 40);
    lv_obj_set_pos(borrar, 200, 192);
    lv_obj_set_style_radius(borrar, 10, 0);
    lv_obj_set_style_shadow_width(borrar, 0, 0);
    lv_obj_set_style_bg_color(borrar, lv_color_hex(COLOR_LINEA), 0);
    lv_obj_add_event_cb(borrar, al_borrar_alarma, LV_EVENT_CLICKED, NULL);
    lv_obj_t *texto_borrar = lv_label_create(borrar);
    lv_label_set_text(texto_borrar, TR(LV_SYMBOL_TRASH " Delete", LV_SYMBOL_TRASH " Borrar"));
    lv_obj_set_style_text_font(texto_borrar, &radio_20, 0);
    lv_obj_set_style_text_color(texto_borrar, lv_color_hex(COLOR_ERROR), 0);
    lv_obj_center(texto_borrar);
    return pantalla;
}

/* ---------- Aviso cuando suena ---------- */

static void al_apagar(lv_event_t *e)
{
    (void)e;
    alarmas_apagar();
    if (capa_aviso != NULL) {
        lv_obj_del_async(capa_aviso);
        capa_aviso = NULL;
    }
    lv_disp_trig_activity(NULL);
}

void alarma_mostrar(int hora, int minuto)
{
    if (capa_aviso != NULL) {
        return;
    }
    capa_aviso = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(capa_aviso);
    lv_obj_set_size(capa_aviso, PANTALLA_ANCHO, PANTALLA_ALTO);
    lv_obj_set_style_bg_color(capa_aviso, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(capa_aviso, LV_OPA_COVER, 0);
    lv_obj_add_flag(capa_aviso, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *titulo = lv_label_create(capa_aviso);
    lv_label_set_text(titulo, TR(LV_SYMBOL_BELL "  Alarm", LV_SYMBOL_BELL "  Alarma"));
    lv_obj_set_style_text_font(titulo, &radio_20, 0);
    lv_obj_set_style_text_color(titulo, lv_color_hex(COLOR_ACENTO), 0);
    lv_obj_align(titulo, LV_ALIGN_TOP_MID, 0, 18);

    lv_obj_t *reloj = lv_label_create(capa_aviso);
    lv_label_set_text_fmt(reloj, "%02d:%02d", hora, minuto);
    lv_obj_set_style_text_font(reloj, &radio_reloj_72, 0);
    lv_obj_set_style_text_color(reloj, lv_color_hex(0xFFE9C2), 0);
    lv_obj_align(reloj, LV_ALIGN_CENTER, 0, -24);

    lv_obj_t *apagar = lv_btn_create(capa_aviso);
    lv_obj_set_size(apagar, 220, 60);
    lv_obj_align(apagar, LV_ALIGN_BOTTOM_MID, 0, -16);
    lv_obj_set_style_radius(apagar, 14, 0);
    lv_obj_set_style_shadow_width(apagar, 0, 0);
    lv_obj_set_style_bg_color(apagar, lv_color_hex(COLOR_ACENTO), 0);
    lv_obj_add_event_cb(apagar, al_apagar, LV_EVENT_CLICKED, NULL);
    lv_obj_t *texto = lv_label_create(apagar);
    lv_label_set_text(texto, TR("Turn off", "Apagar"));
    lv_obj_set_style_text_font(texto, &radio_28, 0);
    lv_obj_set_style_text_color(texto, lv_color_hex(COLOR_FONDO), 0);
    lv_obj_center(texto);
}
