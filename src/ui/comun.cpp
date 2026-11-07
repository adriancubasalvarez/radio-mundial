#include "comun.h"
#include "nav.h"
#include "hal.h"
#include "imagen.h"

#include <ctype.h>
#include <string.h>

lv_obj_t *comun_pantalla(void)
{
    lv_obj_t *pantalla = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(pantalla, lv_color_hex(COLOR_FONDO), 0);
    lv_obj_clear_flag(pantalla, LV_OBJ_FLAG_SCROLLABLE);
    return pantalla;
}

/* Logo pequeño (data/logo_mini.bin): se carga una vez y se queda en memoria
 * porque es diminuto (1,4 KB). NULL si no existe. */
static const lv_img_dsc_t *logo_mini(void)
{
    static Imagen logo;
    static bool intentado = false;
    if (!intentado) {
        intentado = true;
        imagen_cargar(&logo, "logo_mini.bin");
    }
    return logo.datos != NULL ? &logo.dsc : NULL;
}

static void al_pulsar_atras(lv_event_t *e)
{
    (void)e;
    nav_atras();
}

/* Botón de cabecera: 44 px de ancho para que se acierte con el dedo */
static lv_obj_t *boton_cabecera(lv_obj_t *cabecera, const char *simbolo, lv_event_cb_t accion)
{
    lv_obj_t *boton = lv_btn_create(cabecera);
    lv_obj_set_size(boton, 44, ALTO_CABECERA);
    lv_obj_set_style_radius(boton, 0, 0);
    lv_obj_set_style_shadow_width(boton, 0, 0);
    lv_obj_set_style_bg_opa(boton, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(boton, LV_OPA_30, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(boton, lv_color_white(), LV_STATE_PRESSED);
    lv_obj_add_event_cb(boton, accion, LV_EVENT_CLICKED, NULL);
    /* Van pegados al borde de la pantalla, donde el dedo se queda siempre
     * algo hacia dentro: se aceptan toques un poco fuera del botón */
    lv_obj_set_ext_click_area(boton, 10);

    lv_obj_t *icono = lv_label_create(boton);
    lv_label_set_text(icono, simbolo);
    lv_obj_set_style_text_font(icono, &radio_20, 0);
    lv_obj_set_style_text_color(icono, lv_color_hex(COLOR_ACENTO), 0);
    lv_obj_center(icono);
    return boton;
}

lv_obj_t *comun_cabecera(lv_obj_t *pantalla, const char *texto, bool con_atras, lv_obj_t **titulo)
{
    lv_obj_t *cabecera = lv_obj_create(pantalla);
    lv_obj_remove_style_all(cabecera);
    lv_obj_set_size(cabecera, PANTALLA_ANCHO, ALTO_CABECERA);
    lv_obj_set_style_bg_color(cabecera, lv_color_hex(COLOR_CABECERA), 0);
    lv_obj_set_style_bg_opa(cabecera, LV_OPA_COVER, 0);
    lv_obj_clear_flag(cabecera, LV_OBJ_FLAG_SCROLLABLE);

    if (con_atras) {
        lv_obj_t *atras = boton_cabecera(cabecera, LV_SYMBOL_LEFT, al_pulsar_atras);
        lv_obj_align(atras, LV_ALIGN_LEFT_MID, 0, 0);
    }

    lv_obj_t *etiqueta = lv_label_create(cabecera);
    lv_label_set_text(etiqueta, texto);
    lv_label_set_long_mode(etiqueta, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(etiqueta, PANTALLA_ANCHO - 2 * 48);
    lv_obj_set_style_text_font(etiqueta, &radio_20, 0);
    lv_obj_set_style_text_color(etiqueta, lv_color_hex(con_atras ? COLOR_TEXTO : COLOR_ACENTO), 0);
    lv_obj_set_style_text_align(etiqueta, con_atras ? LV_TEXT_ALIGN_CENTER : LV_TEXT_ALIGN_LEFT, 0);
    if (con_atras) {
        lv_obj_align(etiqueta, LV_ALIGN_CENTER, 0, 0);
    } else {
        /* Cabecera principal: logo pequeño del creador delante del título */
        int x_titulo = 8;
        const lv_img_dsc_t *mini = logo_mini();
        if (mini != NULL) {
            lv_obj_t *img = lv_img_create(cabecera);
            lv_img_set_src(img, mini);
            lv_obj_align(img, LV_ALIGN_LEFT_MID, 6, 0);
            x_titulo = 6 + mini->header.w + 6;
        }
        /* Hueco a la derecha para tres botones de cabecera */
        lv_obj_set_width(etiqueta, PANTALLA_ANCHO - x_titulo - 3 * 44);
        lv_obj_align(etiqueta, LV_ALIGN_LEFT_MID, x_titulo, 0);
    }

    if (titulo != NULL) {
        *titulo = etiqueta;
    }
    return cabecera;
}

lv_obj_t *comun_boton_cabecera(lv_obj_t *cabecera, const char *simbolo, lv_event_cb_t accion)
{
    lv_obj_t *boton = boton_cabecera(cabecera, simbolo, accion);
    lv_obj_align(boton, LV_ALIGN_RIGHT_MID, 0, 0);
    return boton;
}

/* ---------- Listas ----------
 * Una lista es una tabla de LVGL (lv_table) de dos columnas: texto (con su
 * icono) y una nota pequeña a la derecha. Toda la lista es UN objeto; con
 * un botón por fila, 56 filas gastaban unos 60 KB de la memoria de LVGL,
 * demasiado para una placa sin PSRAM. */

#define ANCHO_NOTA 54
#define ALTO_FILA_PAD 11   /* relleno arriba y abajo de cada fila (≈ 44 px de alto) */

/* Filas apagadas: se marcan con este control de celda */
#define FILA_APAGADA LV_TABLE_CELL_CTRL_CUSTOM_1
#define FILA_GRIS    LV_TABLE_CELL_CTRL_CUSTOM_2

static lv_style_t estilo_lista;
static lv_style_t estilo_filas;

static void iniciar_estilos(void)
{
    static bool hecho = false;
    if (hecho) {
        return;
    }
    hecho = true;

    lv_style_init(&estilo_lista);
    lv_style_set_radius(&estilo_lista, 0);
    lv_style_set_border_width(&estilo_lista, 0);
    lv_style_set_pad_all(&estilo_lista, 0);
    lv_style_set_bg_color(&estilo_lista, lv_color_hex(COLOR_FONDO));

    /* Filas: fondo de la aplicación, letra grande y una línea fina debajo */
    lv_style_init(&estilo_filas);
    lv_style_set_bg_color(&estilo_filas, lv_color_hex(COLOR_FONDO));
    lv_style_set_bg_opa(&estilo_filas, LV_OPA_COVER);
    lv_style_set_text_font(&estilo_filas, &radio_20);
    lv_style_set_text_color(&estilo_filas, lv_color_hex(COLOR_TEXTO));
    lv_style_set_border_side(&estilo_filas, LV_BORDER_SIDE_BOTTOM);
    lv_style_set_border_width(&estilo_filas, 1);
    lv_style_set_border_color(&estilo_filas, lv_color_hex(COLOR_LINEA));
    lv_style_set_pad_ver(&estilo_filas, ALTO_FILA_PAD);
    lv_style_set_pad_left(&estilo_filas, 12);
    lv_style_set_pad_right(&estilo_filas, 10);
}

/* Al dibujar cada celda: la nota pequeña y a la derecha, las filas apagadas
 * en gris y la fila que se está pulsando, resaltada */
static void al_dibujar_celda(lv_event_t *e)
{
    lv_obj_t *tabla = lv_event_get_target(e);
    lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(e);
    if (dsc->part != LV_PART_ITEMS) {
        return;
    }
    uint32_t fila = dsc->id / 2;
    uint32_t columna = dsc->id % 2;

    if (columna == 1 && dsc->label_dsc != NULL) {
        dsc->label_dsc->font = &radio_14;
        dsc->label_dsc->color = lv_color_hex(COLOR_SECUNDARIO);
        dsc->label_dsc->align = LV_TEXT_ALIGN_RIGHT;
    }
    if (lv_table_has_cell_ctrl(tabla, fila, 0, FILA_GRIS) && dsc->label_dsc != NULL) {
        dsc->label_dsc->color = lv_color_hex(COLOR_SECUNDARIO);
        dsc->label_dsc->opa = LV_OPA_60;
    }
    if (lv_table_has_cell_ctrl(tabla, fila, 0, FILA_APAGADA) && dsc->label_dsc != NULL) {
        dsc->label_dsc->color = lv_color_hex(COLOR_SECUNDARIO);
        dsc->label_dsc->opa = LV_OPA_50;
    }
    uint16_t f, c;
    lv_table_get_selected_cell(tabla, &f, &c);
    if (f == fila && lv_obj_has_state(tabla, LV_STATE_PRESSED) && dsc->rect_dsc != NULL &&
        !lv_table_has_cell_ctrl(tabla, fila, 0, FILA_APAGADA)) {
        dsc->rect_dsc->bg_color = lv_color_hex(COLOR_LINEA);
    }
}

static void al_soltar_celda(lv_event_t *e)
{
    lv_obj_t *tabla = lv_event_get_target(e);
    AlElegirFila accion = (AlElegirFila)lv_event_get_user_data(e);
    uint16_t fila, columna;
    lv_table_get_selected_cell(tabla, &fila, &columna);
    if (fila == LV_TABLE_CELL_NONE || fila >= comun_lista_filas(tabla) ||
        lv_table_has_cell_ctrl(tabla, fila, 0, FILA_APAGADA)) {
        return;
    }
    if (accion != NULL) {
        accion(fila);
    }
}

lv_obj_t *comun_lista(lv_obj_t *pantalla, AlElegirFila accion)
{
    iniciar_estilos();
    lv_obj_t *tabla = lv_table_create(pantalla);
    lv_obj_set_size(tabla, PANTALLA_ANCHO, PANTALLA_ALTO - ALTO_CABECERA);
    lv_obj_set_pos(tabla, 0, ALTO_CABECERA);
    lv_obj_add_style(tabla, &estilo_lista, 0);
    lv_obj_add_style(tabla, &estilo_filas, LV_PART_ITEMS);
    lv_table_set_col_cnt(tabla, 2);
    lv_table_set_col_width(tabla, 0, PANTALLA_ANCHO - ANCHO_NOTA);
    lv_table_set_col_width(tabla, 1, ANCHO_NOTA);
    lv_obj_set_user_data(tabla, (void *)0);  /* número de filas añadidas */
    lv_obj_add_event_cb(tabla, al_dibujar_celda, LV_EVENT_DRAW_PART_BEGIN, NULL);
    lv_obj_add_event_cb(tabla, al_soltar_celda, LV_EVENT_VALUE_CHANGED, (void *)accion);
    return tabla;
}

uint16_t comun_lista_filas(lv_obj_t *lista)
{
    return (uint16_t)(uintptr_t)lv_obj_get_user_data(lista);
}

void comun_lista_agregar(lv_obj_t *lista, const char *icono, const char *texto, const char *nota, bool apagada)
{
    uint16_t fila = comun_lista_filas(lista);
    if (icono != NULL) {
        lv_table_set_cell_value_fmt(lista, fila, 0, "%s  %s", icono, texto);
    } else {
        lv_table_set_cell_value(lista, fila, 0, texto);
    }
    lv_table_set_cell_value(lista, fila, 1, nota != NULL ? nota : "");
    if (apagada) {
        lv_table_add_cell_ctrl(lista, fila, 0, FILA_APAGADA);
    }
    lv_obj_set_user_data(lista, (void *)(uintptr_t)(fila + 1));
}

void comun_lista_gris(lv_obj_t *lista)
{
    lv_table_add_cell_ctrl(lista, comun_lista_filas(lista) - 1, 0, FILA_GRIS);
}

/* Letras cirílicas de А a я, en latino: la fuente solo tiene Latin-1 */
static const char *const CIRILICO[32] = {
    "a", "b", "v", "g", "d", "e", "zh", "z", "i", "y", "k", "l", "m", "n", "o", "p",
    "r", "s", "t", "u", "f", "kh", "ts", "ch", "sh", "shch", "", "y", "", "e", "yu", "ya",
};

/* Griego de α a ω (la ς final está en su sitio) */
static const char *const GRIEGO[25] = {
    "a", "v", "g", "d", "e", "z", "i", "th", "i", "k", "l", "m", "n",
    "x", "o", "p", "r", "s", "s", "t", "y", "f", "ch", "ps", "o",
};

/* Equivalente en letras latinas de un carácter (en minúscula, con *mayuscula
 * a true si iba en mayúscula), o NULL si no lo hay */
static const char *a_latino(uint32_t cp, bool *mayuscula)
{
    *mayuscula = false;
    switch (cp) {
    case 0x2018: case 0x2019: case 0x201A: case 0x2032: return "'";
    case 0x201C: case 0x201D: case 0x201E: return "\"";
    case 0x2013: case 0x2014: case 0x2015: case 0x2212: return "-";
    case 0x2026: return "...";
    case 0x2022: return "\xC2\xB7";  /* · */
    case 0x401: *mayuscula = true; return "yo";
    case 0x451: return "yo";
    case 0x404: *mayuscula = true; return "ye";
    case 0x454: return "ye";
    case 0x406: *mayuscula = true; return "i";
    case 0x456: return "i";
    case 0x407: *mayuscula = true; return "yi";
    case 0x457: return "yi";
    case 0x490: *mayuscula = true; return "g";
    case 0x491: return "g";
    case 0x3AC: return "a";
    case 0x3AD: return "e";
    case 0x3AE: case 0x3AF: case 0x3CC: return cp == 0x3CC ? "o" : "i";
    case 0x3CD: return "y";
    case 0x3CE: return "o";
    default: break;
    }
    if (cp >= 0x410 && cp <= 0x42F) {
        *mayuscula = true;
        return CIRILICO[cp - 0x410];
    }
    if (cp >= 0x430 && cp <= 0x44F) {
        return CIRILICO[cp - 0x430];
    }
    if (cp >= 0x391 && cp <= 0x3A9 && cp != 0x3A2) {
        *mayuscula = true;
        return GRIEGO[cp - 0x391];
    }
    if (cp >= 0x3B1 && cp <= 0x3C9) {
        return GRIEGO[cp - 0x3B1];
    }
    return NULL;
}

/* Lee un carácter UTF-8 y devuelve cuántos bytes ocupa (0 si no es UTF-8) */
static int leer_utf8(const unsigned char *s, uint32_t *cp)
{
    if (*s < 0x80) {
        *cp = *s;
        return 1;
    }
    if ((*s >> 5) == 6 && (s[1] >> 6) == 2) {
        *cp = ((*s & 0x1Fu) << 6) | (s[1] & 0x3F);
        return 2;
    }
    if ((*s >> 4) == 14 && (s[1] >> 6) == 2 && (s[2] >> 6) == 2) {
        *cp = ((*s & 0x0Fu) << 12) | ((s[1] & 0x3Fu) << 6) | (s[2] & 0x3F);
        return 3;
    }
    if ((*s >> 3) == 30 && (s[1] >> 6) == 2 && (s[2] >> 6) == 2 && (s[3] >> 6) == 2) {
        *cp = 0x1FFFFF;  /* emojis y demás: no hay equivalente */
        return 4;
    }
    return 0;
}

/* Algunas emisoras no mandan UTF-8 sino codificaciones antiguas. Si casi todo
 * son bytes altos sueltos, es texto cirílico (Windows-1251); si son pocos,
 * letras acentuadas en Latin-1. */
static bool parece_cp1251(const unsigned char *s)
{
    int altos = 0, letras = 0;
    while (*s) {
        uint32_t cp;
        int largo = leer_utf8(s, &cp);
        if (largo == 0) {
            altos += *s >= 0xC0;
            largo = 1;
        } else if (cp < 0x80 && isalpha((int)cp)) {
            letras++;
        }
        s += largo;
    }
    return altos > letras;
}

void comun_texto_seguro(const char *origen, char *destino, size_t tam)
{
    const unsigned char *s = (const unsigned char *)origen;
    bool cp1251 = parece_cp1251(s);
    size_t d = 0;
    int legibles = 0, perdidos = 0;
    while (*s && d + 6 < tam) {
        uint32_t cp;
        int largo = leer_utf8(s, &cp);
        if (largo == 0) {  /* no es UTF-8: un byte de una codificación antigua */
            largo = 1;
            cp = *s;
            if (cp1251 && cp >= 0xC0) {
                cp += 0x410 - 0xC0;
            }
        }
        s += largo;

        bool mayuscula;
        const char *latino = a_latino(cp, &mayuscula);
        if ((cp >= 0x20 && cp <= 0x7E) || (cp >= 0xA0 && cp <= 0xFF)) {
            if (cp < 0x80) {
                destino[d++] = (char)cp;
                legibles += isalnum((int)cp) != 0;
            } else {
                destino[d++] = (char)(0xC0 | cp >> 6);
                destino[d++] = (char)(0x80 | (cp & 0x3F));
                legibles++;
            }
        } else if (latino != NULL) {
            size_t n = strlen(latino);
            memcpy(destino + d, latino, n);
            if (mayuscula && n > 0) {
                destino[d] = (char)toupper((unsigned char)destino[d]);
            }
            d += n;
            legibles++;
        } else if (cp >= 0x20) {
            perdidos++;  /* árabe, chino, emojis...: la fuente no los tiene */
        }
    }
    destino[d] = '\0';
    /* Con solo unos restos no se entendería nada: mejor sin título */
    if (perdidos > legibles) {
        destino[0] = '\0';
    }
}

