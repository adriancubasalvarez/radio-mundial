/**
 * Pantalla del mapamundi: tocas una región, se resalta y al soltar se entra.
 *
 *   +-----------------------------+  0
 *   | logo Radio Mundial    ★  🔀 |  cabecera (30 px): favoritos y aleatoria
 *   +-----------------------------+  30
 *   |        mapamundi            |  imagen (alto según el mapa)
 *   +-----------------------------+
 *   |  región tocada / sonando    |  pie
 *   +-----------------------------+  240
 */
#include "ui.h"
#include "nav.h"
#include "comun.h"
#include "mapa.h"
#include "ajustes.h"
#include "hal.h"

#include <lvgl.h>
#include <stdio.h>
#include <string.h>

#define PERIODO_PIE 1000  /* ms entre actualizaciones del pie */

static Mapa mapa_mundo;
static lv_obj_t *img_mundo;
static lv_obj_t *etiqueta_pie;
static lv_timer_t *temporizador_pie;
static bool tocando;  /* mientras se toca el mapa, el pie muestra la región */
static char ultimo_pie[200];

static void poner_pie(const char *texto, uint32_t color)
{
    /* Solo si cambia, para no reiniciar el desplazamiento del texto */
    if (strcmp(ultimo_pie, texto) != 0) {
        strncpy(ultimo_pie, texto, sizeof(ultimo_pie) - 1);
        lv_label_set_text(etiqueta_pie, texto);
    }
    lv_obj_set_style_text_color(etiqueta_pie, lv_color_hex(color), 0);
}

/* Texto del pie cuando no se toca nada: lo que suena (y su canción), o una pista */
static void pie_por_defecto(void)
{
    EstadoAudio estado = audio_estado();
    if (nav.hay_sonando && (estado == AUDIO_SONANDO || estado == AUDIO_CONECTANDO)) {
        char titulo[120], texto[200];
        comun_texto_seguro(audio_titulo(), titulo, sizeof(titulo));
        if (titulo[0] != '\0') {
            snprintf(texto, sizeof(texto), LV_SYMBOL_AUDIO "  %s · %s", nav.sonando.nombre, titulo);
        } else {
            snprintf(texto, sizeof(texto), LV_SYMBOL_AUDIO "  %s", nav.sonando.nombre);
        }
        poner_pie(texto, COLOR_ACENTO);
    } else {
        if (estado == AUDIO_ERROR) {
            audio_parar(); /* la emisora se cortó mientras navegábamos */
        }
        nav.hay_sonando = false;
        if (hal_red_estado() == RED_CONECTANDO) {
            poner_pie(LV_SYMBOL_WIFI "  Conectando a la WiFi...", COLOR_SECUNDARIO);
        } else {
            poner_pie("Toca una región", COLOR_SECUNDARIO);
        }
    }
}

static void al_refrescar_pie(lv_timer_t *t)
{
    (void)t;
    if (!tocando) {
        pie_por_defecto();
    }
}

/* Región que hay bajo el dedo, según la rejilla de zonas */
static const Region *region_bajo_dedo(void)
{
    lv_indev_t *entrada = lv_indev_get_act();
    if (entrada == NULL) {
        return NULL;
    }
    lv_point_t punto;
    lv_indev_get_point(entrada, &punto);

    lv_area_t zona;
    lv_obj_get_coords(img_mundo, &zona);
    uint8_t color = mapa_indice_en(&mapa_mundo, punto.x - zona.x1, punto.y - zona.y1);
    return datos_region_por_indice(&nav.indice, color);
}

static void al_tocar_mapa(lv_event_t *e)
{
    lv_event_code_t codigo = lv_event_get_code(e);

    if (codigo == LV_EVENT_PRESSED || codigo == LV_EVENT_PRESSING) {
        const Region *region = region_bajo_dedo();
        mapa_resaltar(&mapa_mundo, region ? region->indice : 0, img_mundo);
        tocando = (region != NULL);
        if (region != NULL) {
            poner_pie(region->nombre, COLOR_TEXTO);
        } else {
            pie_por_defecto();
        }
    } else if (codigo == LV_EVENT_RELEASED) {
        const Region *region = region_bajo_dedo();
        tocando = false;
        if (region != NULL) {
            nav.region = *region;
            nav_ir(PANT_REGION);
        } else {
            mapa_resaltar(&mapa_mundo, 0, img_mundo);
            pie_por_defecto();
        }
    } else if (codigo == LV_EVENT_PRESS_LOST) {
        tocando = false;
        mapa_resaltar(&mapa_mundo, 0, img_mundo);
        pie_por_defecto();
    }
}

/* Tocar el pie mientras suena algo lleva a «Reproduciendo». La última
 * emisora guardada en los ajustes es la que suena, con su país. */
static void al_tocar_pie(lv_event_t *e)
{
    (void)e;
    if (nav.hay_sonando && ajustes.hay_ultima) {
        nav.emisora = ajustes.ultima.emisora;
        memset(&nav.pais, 0, sizeof(nav.pais));
        strncpy(nav.pais.codigo, ajustes.ultima.pais, sizeof(nav.pais.codigo) - 1);
        strncpy(nav.pais.nombre, ajustes.ultima.nombre_pais, sizeof(nav.pais.nombre) - 1);
        nav.aleatoria = false;
        nav_ir(PANT_SONANDO);
    }
}

static void al_pulsar_favoritos(lv_event_t *e)
{
    (void)e;
    nav_ir(PANT_FAVORITOS);
}

static void al_mantener_cabecera(lv_event_t *e)
{
    (void)e;
    hal_red_configurar();
}

static void al_pulsar_buscar(lv_event_t *e)
{
    (void)e;
    buscar_desde_cero();
    nav_ir(PANT_BUSCAR);
}

static void al_pulsar_aleatoria(lv_event_t *e)
{
    (void)e;
    if (datos_emisora_aleatoria(&nav.emisora, &nav.pais)) {
        nav.aleatoria = true;
        nav_ir(PANT_SONANDO);
    }
}

static void al_borrar(lv_event_t *e)
{
    (void)e;
    lv_timer_del(temporizador_pie);
    temporizador_pie = NULL;
    mapa_liberar(&mapa_mundo);
}

lv_obj_t *pantalla_mundo(void)
{
    lv_obj_t *pantalla = comun_pantalla();
    lv_obj_add_event_cb(pantalla, al_borrar, LV_EVENT_DELETE, NULL);
    lv_obj_t *cabecera = comun_cabecera(pantalla, "Radio Mundial", false, NULL);
    comun_boton_cabecera(cabecera, SIMBOLO_ALEATORIO, al_pulsar_aleatoria);
    lv_obj_t *favoritos = comun_boton_cabecera(cabecera, SIMBOLO_ESTRELLA, al_pulsar_favoritos);
    lv_obj_align(favoritos, LV_ALIGN_RIGHT_MID, -44, 0);
    lv_obj_t *buscar = comun_boton_cabecera(cabecera, SIMBOLO_BUSCAR, al_pulsar_buscar);
    lv_obj_align(buscar, LV_ALIGN_RIGHT_MID, -88, 0);

    /* Mantener pulsado el logo o el título 2 segundos abre el portal WiFi
     * para cambiar de red (hasta que haya una pantalla de ajustes) */
    lv_obj_add_flag(cabecera, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(cabecera, al_mantener_cabecera, LV_EVENT_LONG_PRESSED, NULL);

    /* El pie se crea antes que el mapa por si falta el mapa */
    etiqueta_pie = lv_label_create(pantalla);
    ultimo_pie[0] = '\0';
    tocando = false;
    temporizador_pie = lv_timer_create(al_refrescar_pie, PERIODO_PIE, NULL);

    if (!mapa_cargar(&mapa_mundo, nav.indice.imagen, nav.indice.zonas)) {
        lv_label_set_text(etiqueta_pie, "Faltan los mapas en data/\nEjecuta tools/build_maps.py");
        lv_obj_center(etiqueta_pie);
        return pantalla;
    }

    img_mundo = lv_img_create(pantalla);
    lv_img_set_src(img_mundo, &mapa_mundo.imagen);
    lv_obj_set_pos(img_mundo, 0, ALTO_CABECERA);
    lv_obj_add_flag(img_mundo, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(img_mundo, al_tocar_mapa, LV_EVENT_ALL, NULL);

    /* Pie: ocupa todo el hueco bajo el mapa para que sea fácil de tocar */
    int y_pie = ALTO_CABECERA + mapa_mundo.imagen.header.h;
    lv_label_set_long_mode(etiqueta_pie, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_size(etiqueta_pie, PANTALLA_ANCHO - 16, LV_SIZE_CONTENT);
    lv_obj_set_style_text_font(etiqueta_pie, &radio_20, 0);
    lv_obj_set_style_text_align(etiqueta_pie, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(etiqueta_pie, LV_ALIGN_TOP_MID, 0, y_pie + (PANTALLA_ALTO - y_pie - 24) / 2);
    lv_obj_add_flag(etiqueta_pie, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(etiqueta_pie, 16);
    lv_obj_add_event_cb(etiqueta_pie, al_tocar_pie, LV_EVENT_CLICKED, NULL);
    pie_por_defecto();

    return pantalla;
}

void ui_inicio(void)
{
    /* Tema oscuro de LVGL con nuestros colores y fuente */
    lv_disp_t *disp = lv_disp_get_default();
    lv_theme_t *tema = lv_theme_default_init(disp, lv_color_hex(0x2F6FA8), lv_color_hex(COLOR_ACENTO),
                                             true, &radio_14);
    lv_disp_set_theme(disp, tema);

    ajustes_cargar();
    audio_volumen(ajustes.volumen);

    if (!datos_cargar_indice(&nav.indice)) {
        lv_obj_t *error = lv_label_create(lv_scr_act());
        lv_label_set_text(error, "No se encuentra data/regiones.json\nEjecuta tools/build_maps.py");
        lv_obj_center(error);
        return;
    }
    printf("Índice cargado: %d regiones\n", nav.indice.num_regiones);
    reposo_iniciar();
    red_ui_iniciar();
    ui_arranque();
}

