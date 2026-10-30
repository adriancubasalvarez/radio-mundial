/**
 * Colores y piezas comunes de todas las pantallas.
 */
#pragma once

#include <lvgl.h>
#include <stddef.h>

#define ALTO_CABECERA 30

#define COLOR_FONDO      0x0B1D33
#define COLOR_CABECERA   0x12304F
#define COLOR_ACENTO     0xFFC845
#define COLOR_TEXTO      0xFFFFFF
#define COLOR_SECUNDARIO 0x9FB3C8
#define COLOR_ERROR      0xFF7A6B
#define COLOR_LINEA      0x1E3A5C  /* separadores y fondo al pulsar */

/* Símbolos propios añadidos a nuestras fuentes (tools/build_fonts.py) */
#define SIMBOLO_ESTRELLA  "\xEF\x80\x85" /* U+F005 */
#define SIMBOLO_ALEATORIO "\xEF\x81\xB4" /* U+F074 */

/* Copia un texto UTF-8 cambiando por '?' los caracteres que no tienen
 * nuestras fuentes (fuera de ASCII y Latin-1), p. ej. títulos en otros
 * alfabetos que llegan de las emisoras. */
void comun_texto_seguro(const char *origen, char *destino, size_t tam);

/* Crea una pantalla vacía con el fondo de la aplicación */
lv_obj_t *comun_pantalla(void);

/* Cabecera de 30 px con botón «Atrás» opcional y un título que se desplaza
 * solo si no cabe. Devuelve la cabecera; *titulo recibe la etiqueta. */
lv_obj_t *comun_cabecera(lv_obj_t *pantalla, const char *texto, bool con_atras, lv_obj_t **titulo);

/* Botón pequeño con un símbolo en la esquina derecha de la cabecera */
lv_obj_t *comun_boton_cabecera(lv_obj_t *cabecera, const char *simbolo, lv_event_cb_t accion);

/* Lista desplazable a pantalla completa bajo la cabecera */
lv_obj_t *comun_lista(lv_obj_t *pantalla);

/* Elemento de lista grande (para el dedo) con icono opcional y texto.
 * Usa estilos compartidos: cada elemento gasta mucha menos memoria que con
 * estilos propios, importante en listas largas. */
lv_obj_t *comun_elemento_lista(lv_obj_t *lista, const char *icono, const char *texto);

/* Texto pequeño y gris a la derecha de un elemento de lista */
lv_obj_t *comun_nota_elemento(lv_obj_t *elemento, const char *texto);

/* Deja un elemento de lista apagado (gris y sin responder) */
void comun_apagar_elemento(lv_obj_t *elemento);
