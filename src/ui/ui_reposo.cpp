/**
 * Pantalla de reposo: tras ajustes.reposo_s segundos sin tocar la pantalla,
 * baja el brillo y va pasando, con un fundido cada 10 segundos, por:
 *   1. reloj grande, fecha y lo que suena;
 *   2. el tiempo: ahora y los 3 días siguientes;
 *   3. la calidad del aire y la salida y puesta del sol.
 * Las páginas 2 y 3 solo salen si ya han llegado los datos (tiempo.cpp).
 * Cada página aparece unos píxeles más a un lado o a otro (se cambia mientras
 * está fundida a negro, así no se ve moverse), para no marcar la pantalla.
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
#include "idioma.h"
#include "tiempo.h"

#include <lvgl.h>
#include <stdio.h>
#include <string.h>

#define PERIODO_REPOSO 1000   /* ms entre actualizaciones */
#define BRILLO_REPOSO  25     /* % de brillo en reposo */
#define BRILLO_NORMAL  100
#define SEGUNDOS_PAGINA 10    /* cada cuánto cambia de página */
#define DERIVA_X 10           /* hasta cuántos píxeles se mueve cada página */
#define DERIVA_Y 8
#define FUNDIDO_MS 500

#define COLOR_RELOJ 0xFFE9C2  /* blanco cálido, como una radio de válvulas */

#define ICONO_UBICACION "\xEF\x8F\x85"  /* f3c5 */
#define ICONO_HOJA      "\xEF\x81\xAC"  /* f06c */
#define ICONO_ARRIBA    "\xEF\x81\xA2"  /* f062 */
#define ICONO_ABAJO     "\xEF\x81\xA3"  /* f063 */
#define ICONO_SOL       "\xEF\x86\x85"  /* f185 */

enum { PAG_RELOJ, PAG_TIEMPO, PAG_AIRE, NUM_PAGINAS };

static lv_obj_t *capa;
static lv_obj_t *lienzo;                 /* contiene las páginas */
static lv_obj_t *pagina[NUM_PAGINAS];
static int visible;
static uint32_t segundos;                /* desde que se durmió */

/* Página del reloj */
static lv_obj_t *reloj, *fecha, *emisora, *cancion, *resumen;
static char ultimo_titulo[140];

/* Página del tiempo */
static lv_obj_t *ciudad, *icono, *temperatura, *estado_cielo;
static lv_obj_t *dia_nombre[DIAS_PREVISION - 1], *dia_icono[DIAS_PREVISION - 1], *dia_temps[DIAS_PREVISION - 1];

/* Página del aire */
static lv_obj_t *aire_nivel, *aire_valores, *aire_marca, *sol;

static const char *nombre_dia(int d, bool corto)
{
    static const char *const EN[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    static const char *const ES[] = {"domingo", "lunes", "martes", "miércoles", "jueves", "viernes", "sábado"};
    static const char *const EN_C[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static const char *const ES_C[] = {"Dom", "Lun", "Mar", "Mié", "Jue", "Vie", "Sáb"};
    d = (d % 7 + 7) % 7;
    return corto ? TR(EN_C[d], ES_C[d]) : TR(EN[d], ES[d]);
}

static const char *nombre_mes(int m)
{
    static const char *const EN[] = {"January", "February", "March", "April", "May", "June", "July",
                                     "August", "September", "October", "November", "December"};
    static const char *const ES[] = {"enero", "febrero", "marzo", "abril", "mayo", "junio", "julio",
                                     "agosto", "septiembre", "octubre", "noviembre", "diciembre"};
    m = (m - 1 + 12) % 12;
    return TR(EN[m], ES[m]);
}

/* Color del icono según el cielo */
static uint32_t color_cielo(int codigo, bool de_dia)
{
    if (codigo <= 2) return de_dia ? 0xFFC845 : 0xC8D4F0;  /* sol o luna */
    if (codigo == 3 || codigo <= 48) return 0xB8C4D0;      /* nubes, niebla */
    if (codigo <= 67 || (codigo >= 80 && codigo <= 82)) return 0x6FB4FF;  /* lluvia */
    if (codigo <= 86) return 0xFFFFFF;                     /* nieve */
    return 0xFFD54A;                                       /* tormenta */
}

/* ---------- Construcción ---------- */

static lv_obj_t *texto(lv_obj_t *padre, const lv_font_t *fuente, uint32_t color)
{
    lv_obj_t *e = lv_label_create(padre);
    lv_obj_set_style_text_font(e, fuente, 0);
    lv_obj_set_style_text_color(e, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(e, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(e, "");
    return e;
}

/* Texto de una línea que, si no cabe, se desplaza en bucle */
static lv_obj_t *texto_largo(lv_obj_t *padre, const lv_font_t *fuente, uint32_t color, int ancho)
{
    lv_obj_t *e = texto(padre, fuente, color);
    lv_label_set_long_mode(e, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(e, ancho);
    return e;
}

static lv_obj_t *nueva_pagina(void)
{
    lv_obj_t *p = lv_obj_create(lienzo);
    lv_obj_remove_style_all(p);
    lv_obj_set_size(p, PANTALLA_ANCHO, PANTALLA_ALTO);
    lv_obj_clear_flag(p, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_opa(p, LV_OPA_TRANSP, 0);
    return p;
}

static void crear_reloj(lv_obj_t *p)
{
    reloj = texto(p, &radio_reloj_72, COLOR_RELOJ);
    lv_label_set_recolor(reloj, true);  /* los dos puntos parpadean */
    lv_obj_align(reloj, LV_ALIGN_CENTER, 0, -42);
    fecha = texto(p, &radio_20, COLOR_SECUNDARIO);
    lv_obj_align(fecha, LV_ALIGN_CENTER, 0, 10);
    emisora = texto_largo(p, &radio_20, COLOR_ACENTO, PANTALLA_ANCHO - 40);
    lv_obj_align(emisora, LV_ALIGN_CENTER, 0, 52);
    cancion = texto_largo(p, &radio_14, COLOR_SECUNDARIO, PANTALLA_ANCHO - 40);
    lv_obj_align(cancion, LV_ALIGN_CENTER, 0, 78);
    resumen = texto(p, &radio_20, COLOR_SECUNDARIO);  /* el tiempo, en pequeño */
    lv_obj_align(resumen, LV_ALIGN_TOP_RIGHT, -16, 12);
}

static void crear_tiempo(lv_obj_t *p)
{
    ciudad = texto_largo(p, &radio_20, COLOR_SECUNDARIO, PANTALLA_ANCHO - 40);
    lv_obj_align(ciudad, LV_ALIGN_TOP_MID, 0, 12);
    icono = texto(p, &radio_iconos_64, COLOR_ACENTO);
    lv_obj_align(icono, LV_ALIGN_TOP_MID, -70, 44);
    temperatura = texto(p, &radio_reloj_72, COLOR_RELOJ);
    lv_obj_align(temperatura, LV_ALIGN_TOP_MID, 50, 38);
    estado_cielo = texto_largo(p, &radio_20, COLOR_TEXTO, PANTALLA_ANCHO - 40);
    lv_obj_align(estado_cielo, LV_ALIGN_TOP_MID, 0, 122);
    /* Los 3 días siguientes, en columnas */
    for (int i = 0; i < DIAS_PREVISION - 1; i++) {
        int x = (i - 1) * 100;
        dia_nombre[i] = texto(p, &radio_14, COLOR_SECUNDARIO);
        lv_obj_align(dia_nombre[i], LV_ALIGN_TOP_MID, x, 160);
        dia_icono[i] = texto(p, &radio_28, COLOR_TEXTO);
        lv_obj_align(dia_icono[i], LV_ALIGN_TOP_MID, x, 180);
        dia_temps[i] = texto(p, &radio_14, COLOR_TEXTO);
        lv_label_set_recolor(dia_temps[i], true);
        lv_obj_align(dia_temps[i], LV_ALIGN_TOP_MID, x, 214);
    }
}

/* Barra con los seis niveles del índice europeo, de bueno a extremo */
static void crear_aire(lv_obj_t *p)
{
    lv_obj_t *titulo = texto(p, &radio_20, COLOR_SECUNDARIO);
    lv_label_set_text(titulo, TR(ICONO_HOJA "  Air quality", ICONO_HOJA "  Calidad del aire"));
    lv_obj_align(titulo, LV_ALIGN_TOP_MID, 0, 14);
    aire_nivel = texto(p, &radio_28, COLOR_TEXTO);
    lv_obj_align(aire_nivel, LV_ALIGN_TOP_MID, 0, 52);

    static const int VALORES[] = {10, 30, 50, 70, 90, 110};
    const int ancho = 40, x0 = (PANTALLA_ANCHO - 6 * ancho) / 2;
    for (int i = 0; i < 6; i++) {
        lv_obj_t *tramo = lv_obj_create(p);
        lv_obj_remove_style_all(tramo);
        lv_obj_set_size(tramo, ancho - 4, 10);
        lv_obj_set_style_radius(tramo, 5, 0);
        lv_obj_set_style_bg_opa(tramo, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(tramo, lv_color_hex(aire_color(VALORES[i])), 0);
        lv_obj_set_pos(tramo, x0 + i * ancho + 2, 104);
    }
    aire_marca = lv_obj_create(p);  /* marca sobre el nivel actual */
    lv_obj_remove_style_all(aire_marca);
    lv_obj_set_size(aire_marca, 4, 22);
    lv_obj_set_style_radius(aire_marca, 2, 0);
    lv_obj_set_style_bg_opa(aire_marca, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(aire_marca, lv_color_white(), 0);
    lv_obj_set_pos(aire_marca, x0, 98);

    aire_valores = texto(p, &radio_14, COLOR_SECUNDARIO);
    lv_obj_align(aire_valores, LV_ALIGN_TOP_MID, 0, 130);
    sol = texto(p, &radio_20, COLOR_TEXTO);
    lv_label_set_recolor(sol, true);
    lv_obj_align(sol, LV_ALIGN_TOP_MID, 0, 168);
    /* Open-Meteo pide citar la fuente (licencia CC BY 4.0) */
    lv_obj_t *fuente = texto(p, &radio_14, COLOR_LINEA + 0x303030);
    lv_label_set_text(fuente, TR("Weather: Open-Meteo.com", "Tiempo: Open-Meteo.com"));
    lv_obj_align(fuente, LV_ALIGN_BOTTOM_MID, 0, -8);
}

/* ---------- Contenido ---------- */

static void actualizar_reloj(void)
{
    int hora, minuto;
    if (hal_hora(&hora, &minuto)) {
        /* los dos puntos se apagan un segundo de cada dos */
        lv_label_set_text_fmt(reloj, segundos % 2 ? "%02d#3A3A3A :#%02d" : "%02d:%02d", hora, minuto);
    } else {
        lv_label_set_text(reloj, "");
    }
    int anio, mes, dia, semana;
    if (hal_fecha(&anio, &mes, &dia, &semana)) {
        char f[60];
        if (idioma_es) {
            snprintf(f, sizeof(f), "%s, %d de %s", nombre_dia(semana, false), dia, nombre_mes(mes));
            f[0] = (char)(f[0] - 'a' + 'A');  /* «Martes», con mayúscula */
        } else {
            snprintf(f, sizeof(f), "%s, %s %d", nombre_dia(semana, false), nombre_mes(mes), dia);
        }
        lv_label_set_text(fecha, f);
    }

    EstadoAudio estado = audio_estado();
    bool suena = nav.hay_sonando && (estado == AUDIO_SONANDO || estado == AUDIO_CONECTANDO);
    lv_label_set_text(emisora, suena ? nav.sonando.nombre : "");
    char titulo[140];
    comun_texto_seguro(suena ? audio_titulo() : "", titulo, sizeof(titulo));
    if (strcmp(titulo, ultimo_titulo) != 0) {
        strcpy(ultimo_titulo, titulo);
        lv_label_set_text(cancion, titulo);
    }

    const Tiempo *t = tiempo_actual();
    if (t->hay) {
        lv_label_set_text_fmt(resumen, "%s %d°", tiempo_icono(t->codigo, t->de_dia), t->temperatura);
    }
}

static void actualizar_tiempo(void)
{
    const Tiempo *t = tiempo_actual();
    if (!t->hay) {
        return;
    }
    lv_label_set_text_fmt(ciudad, ICONO_UBICACION "  %s", t->ciudad);
    lv_label_set_text(icono, tiempo_icono(t->codigo, t->de_dia));
    lv_obj_set_style_text_color(icono, lv_color_hex(color_cielo(t->codigo, t->de_dia)), 0);
    lv_label_set_text_fmt(temperatura, "%d°", t->temperatura);
    lv_label_set_text_fmt(estado_cielo, "%s  " ICONO_ARRIBA "%d°  " ICONO_ABAJO "%d°", tiempo_texto(t->codigo),
                          t->dia[0].maxima, t->dia[0].minima);
    for (int i = 0; i < DIAS_PREVISION - 1; i++) {
        const DiaPrevision *d = &t->dia[i + 1];
        lv_label_set_text(dia_nombre[i], nombre_dia(d->dia_semana, true));
        lv_label_set_text(dia_icono[i], tiempo_icono(d->codigo, true));
        lv_obj_set_style_text_color(dia_icono[i], lv_color_hex(color_cielo(d->codigo, true)), 0);
        lv_label_set_text_fmt(dia_temps[i], "%d°  #9FB3C8 %d°#", d->maxima, d->minima);
    }
}

static void actualizar_aire(void)
{
    const Tiempo *t = tiempo_actual();
    if (!t->hay_aire) {
        return;
    }
    lv_label_set_text(aire_nivel, aire_texto(t->indice_aire));
    lv_obj_set_style_text_color(aire_nivel, lv_color_hex(aire_color(t->indice_aire)), 0);
    /* la marca recorre la barra: 0..120 del índice sobre sus 240 píxeles */
    int i = t->indice_aire > 120 ? 120 : t->indice_aire;
    lv_obj_set_x(aire_marca, (PANTALLA_ANCHO - 240) / 2 + i * 2 - 2);
    char pm[12];
    snprintf(pm, sizeof(pm), "%.1f", t->pm25);
    if (idioma_es) {
        char *punto = strchr(pm, '.');
        if (punto) *punto = ',';
    }
    lv_label_set_text_fmt(aire_valores, TR("European index %d  ·  PM2.5 %s µg/m³", "Índice europeo %d  ·  PM2,5 %s µg/m³"),
                          t->indice_aire, pm);
    lv_label_set_text_fmt(sol, "#FFC845 " ICONO_SOL ICONO_ARRIBA "# %s      #FF9A5A " ICONO_SOL ICONO_ABAJO "# %s",
                          t->sale_sol, t->pone_sol);
}

/* ---------- Movimiento ---------- */

static bool pagina_disponible(int p)
{
    const Tiempo *t = tiempo_actual();
    return p == PAG_RELOJ || (p == PAG_TIEMPO && t->hay) || (p == PAG_AIRE && t->hay && t->hay_aire);
}

static void pasar_pagina(void)
{
    int siguiente = visible;
    do {
        siguiente = (siguiente + 1) % NUM_PAGINAS;
    } while (!pagina_disponible(siguiente));
    if (siguiente == visible) {
        return;
    }
    /* La siguiente aún es transparente: moverla ahora no se ve */
    lv_obj_set_pos(pagina[siguiente], (int)(hal_aleatorio() % (2 * DERIVA_X + 1)) - DERIVA_X,
                   (int)(hal_aleatorio() % (2 * DERIVA_Y + 1)) - DERIVA_Y);
    lv_obj_fade_out(pagina[visible], FUNDIDO_MS, 0);
    lv_obj_fade_in(pagina[siguiente], FUNDIDO_MS, FUNDIDO_MS);
    visible = siguiente;
}

/* ---------- Dormir y despertar ---------- */

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

static void dormir(void)
{
    capa = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(capa);
    lv_obj_set_size(capa, PANTALLA_ANCHO, PANTALLA_ALTO);
    lv_obj_set_style_bg_color(capa, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(capa, LV_OPA_COVER, 0);
    lv_obj_add_flag(capa, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(capa, despertar, LV_EVENT_PRESSED, NULL);

    lienzo = lv_obj_create(capa);
    lv_obj_remove_style_all(lienzo);
    lv_obj_set_size(lienzo, PANTALLA_ANCHO, PANTALLA_ALTO);
    lv_obj_clear_flag(lienzo, LV_OBJ_FLAG_CLICKABLE);

    for (int i = 0; i < NUM_PAGINAS; i++) {
        pagina[i] = nueva_pagina();
    }
    crear_reloj(pagina[PAG_RELOJ]);
    crear_tiempo(pagina[PAG_TIEMPO]);
    crear_aire(pagina[PAG_AIRE]);
    visible = PAG_RELOJ;
    lv_obj_set_style_opa(pagina[PAG_RELOJ], LV_OPA_COVER, 0);

    segundos = 0;
    ultimo_titulo[0] = '\0';
    actualizar_reloj();
    actualizar_tiempo();
    actualizar_aire();
    hal_brillo(BRILLO_REPOSO);
}

static void vigilar(lv_timer_t *t)
{
    (void)t;
    if (capa != NULL) {
        segundos++;
        actualizar_reloj();
        actualizar_tiempo();
        actualizar_aire();
        if (segundos % SEGUNDOS_PAGINA == 0) {
            pasar_pagina();
        }
    } else if (ajustes.reposo_s > 0 && lv_disp_get_inactive_time(NULL) > ajustes.reposo_s * 1000UL) {
        dormir();
    }
}

void reposo_iniciar(void)
{
    lv_timer_create(vigilar, PERIODO_REPOSO, NULL);
}
