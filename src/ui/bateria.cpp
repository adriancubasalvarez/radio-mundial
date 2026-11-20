/**
 * Indicador de batería.
 *
 * Cada 5 segundos se lee el nivel con getBatteryLevel() (el ADC (Analog-to-
 * Digital Converter) de la placa; en el simulador va pasando por todos los
 * niveles). Si hay batería y no está llena, se muestra su icono en la esquina
 * inferior izquierda; si no hay batería o está llena, el icono desaparece y
 * las pantallas usan todo el alto.
 *
 * El icono vive en la capa superior de LVGL, por encima de cualquier
 * pantalla: sobrevive a los cambios de pantalla y se borra solo él, nunca.
 * Sin LV_OBJ_FLAG_CLICKABLE los toques pasan al mapa o botón de debajo.
 * Las capas que tapan toda la pantalla (reposo, portal WiFi, menú de
 * ajustes, aviso de alarma) se crean después, así que lo tapan mientras
 * están abiertas y al cerrarse vuelve a verse.
 *
 * Las pantallas reservan esa esquina con bateria_reserva() para no poner
 * nada debajo del icono (listas, teclado, pie del mapamundi...).
 */
#include "bateria.h"
#include "comun.h"
#include "hal.h"

#include <lvgl.h>

#define PERIODO_MS   5000  /* ms entre lecturas del nivel */
#define MARGEN_X     8     /* separación del borde izquierdo */
#define MARGEN_Y     18    /* centro del icono a la misma altura que el
                             centro del texto del pie de la pantalla principal
                             (radio_20, línea de 25 px en y 197-222) */

/* Lo que ocupan el icono y su margen: lo que las pantallas reservan */
#define ALTO_RESERVA 26

static lv_obj_t *icono;
static BatteryLevel nivel = BAT_LEVEL_FULL;  /* nivel actual, para no actualizar si no cambia */

static const char *simbolo(BatteryLevel n)
{
    switch (n) {
    case BAT_LEVEL_EMPTY: return LV_SYMBOL_BATTERY_EMPTY;
    case BAT_LEVEL_1:     return LV_SYMBOL_BATTERY_1;
    case BAT_LEVEL_2:     return LV_SYMBOL_BATTERY_2;
    case BAT_LEVEL_3:     return LV_SYMBOL_BATTERY_3;
    default:              return LV_SYMBOL_BATTERY_FULL;
    }
}

static void mirar(lv_timer_t *t)
{
    (void)t;
    BatteryLevel nuevo = getBatteryLevel();
    if (nuevo == nivel) {
        return;
    }
    nivel = nuevo;

    /* sin batería, o cargada a tope: no se muestra nada */
    if (nivel == BAT_LEVEL_FULL) {
        lv_obj_add_flag(icono, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_clear_flag(icono, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(icono, simbolo(nivel));
    /* vacía en rojo, un nivel en amarillo y dos o más en verde */
    uint32_t color = COLOR_VERDE;
    if (nivel == BAT_LEVEL_EMPTY) {
        color = COLOR_ERROR;
    } else if (nivel == BAT_LEVEL_1) {
        color = COLOR_ACENTO;
    }
    lv_obj_set_style_text_color(icono, lv_color_hex(color), 0);
}

void bateria_iniciar(void)
{
    icono = lv_label_create(lv_layer_top());
    lv_obj_set_style_text_font(icono, &radio_20, 0);
    lv_obj_align(icono, LV_ALIGN_BOTTOM_LEFT, MARGEN_X, -MARGEN_Y);
    lv_obj_add_flag(icono, LV_OBJ_FLAG_HIDDEN);

    lv_timer_create(mirar, PERIODO_MS, NULL);
    mirar(NULL);  /* primera lectura ya, para que las pantallas sepan si reservar */
}

bool bateria_hay(void)
{
    /* sin batería o con la batería llena no hay icono que reservar */
    return nivel != BAT_LEVEL_FULL;
}

lv_coord_t bateria_reserva(void)
{
    return bateria_hay() ? ALTO_RESERVA : 0;
}
