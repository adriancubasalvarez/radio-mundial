#include "comun.h"
#include "nav.h"
#include "hal.h"
#include "imagen.h"

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
        /* Hueco a la derecha para dos botones de cabecera */
        lv_obj_set_width(etiqueta, PANTALLA_ANCHO - x_titulo - 2 * 44);
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

/* Estilos compartidos de las listas (se crean una sola vez) */
static lv_style_t estilo_lista;
static lv_style_t estilo_elemento;
static lv_style_t estilo_elemento_pulsado;
static lv_style_t estilo_nota;
static lv_style_t estilo_apagado;

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

    /* Elementos: fondo de la aplicación, una línea fina debajo y el
     * contenido centrado en vertical */
    lv_style_init(&estilo_elemento);
    lv_style_set_text_font(&estilo_elemento, &radio_20);
    lv_style_set_min_height(&estilo_elemento, 44);
    lv_style_set_bg_color(&estilo_elemento, lv_color_hex(COLOR_FONDO));
    lv_style_set_border_side(&estilo_elemento, LV_BORDER_SIDE_BOTTOM);
    lv_style_set_border_width(&estilo_elemento, 1);
    lv_style_set_border_color(&estilo_elemento, lv_color_hex(COLOR_LINEA));
    lv_style_set_pad_hor(&estilo_elemento, 12);
    lv_style_set_flex_cross_place(&estilo_elemento, LV_FLEX_ALIGN_CENTER);
    lv_style_set_flex_track_place(&estilo_elemento, LV_FLEX_ALIGN_CENTER);

    lv_style_init(&estilo_elemento_pulsado);
    lv_style_set_bg_color(&estilo_elemento_pulsado, lv_color_hex(COLOR_LINEA));

    lv_style_init(&estilo_nota);
    lv_style_set_text_font(&estilo_nota, &radio_14);
    lv_style_set_text_color(&estilo_nota, lv_color_hex(COLOR_SECUNDARIO));

    lv_style_init(&estilo_apagado);
    lv_style_set_text_color(&estilo_apagado, lv_color_hex(COLOR_SECUNDARIO));
    lv_style_set_text_opa(&estilo_apagado, LV_OPA_50);
}

lv_obj_t *comun_lista(lv_obj_t *pantalla)
{
    iniciar_estilos();
    lv_obj_t *lista = lv_list_create(pantalla);
    lv_obj_set_size(lista, PANTALLA_ANCHO, PANTALLA_ALTO - ALTO_CABECERA);
    lv_obj_set_pos(lista, 0, ALTO_CABECERA);
    lv_obj_add_style(lista, &estilo_lista, 0);
    return lista;
}

lv_obj_t *comun_elemento_lista(lv_obj_t *lista, const char *icono, const char *texto)
{
    lv_obj_t *elemento = lv_list_add_btn(lista, icono, texto);
    lv_obj_add_style(elemento, &estilo_elemento, 0);
    lv_obj_add_style(elemento, &estilo_elemento_pulsado, LV_STATE_PRESSED);
    return elemento;
}

void comun_texto_seguro(const char *origen, char *destino, size_t tam)
{
    size_t d = 0;
    const unsigned char *s = (const unsigned char *)origen;
    while (*s && d + 3 < tam) {
        /* Decodificar un carácter UTF-8 */
        uint32_t cp;
        int largo;
        if (*s < 0x80) { cp = *s; largo = 1; }
        else if ((*s >> 5) == 6 && (s[1] >> 6) == 2) { cp = ((*s & 0x1F) << 6) | (s[1] & 0x3F); largo = 2; }
        else if ((*s >> 4) == 14 && (s[1] >> 6) == 2 && (s[2] >> 6) == 2) { cp = 0xFFFF; largo = 3; }
        else if ((*s >> 3) == 30 && (s[1] >> 6) == 2 && (s[2] >> 6) == 2 && (s[3] >> 6) == 2) { cp = 0xFFFF; largo = 4; }
        else { cp = 0xFFFF; largo = 1; }

        if ((cp >= 0x20 && cp <= 0x7E) || (cp >= 0xA0 && cp <= 0xFF)) {
            memcpy(destino + d, s, largo);
            d += largo;
        } else if (cp >= 0x20) {
            destino[d++] = '?';
        }
        s += largo;
    }
    destino[d] = '\0';
}

lv_obj_t *comun_nota_elemento(lv_obj_t *elemento, const char *texto)
{
    lv_obj_t *nota = lv_label_create(elemento);
    lv_label_set_text(nota, texto);
    lv_obj_add_style(nota, &estilo_nota, 0);
    return nota;
}

void comun_apagar_elemento(lv_obj_t *elemento)
{
    lv_obj_add_state(elemento, LV_STATE_DISABLED);
    lv_obj_add_style(elemento, &estilo_apagado, LV_STATE_DISABLED);
}
