/**
 * Indicadores de la esquina: batería y WiFi.
 *
 * Cada 5 segundos se miran los dos:
 *   - Batería: getBatteryLevel() (el ADC (Analog-to-Digital Converter) de la
 *     placa). Si hay batería y no está llena, se muestra su icono; sin
 *     batería o llena, no.
 *   - Interruptor de la batería: hal_bateria_conectada(). Si la radio ha
 *     tenido alguna vez batería (ajustes.bateria_vista) y ahora no la ve, el
 *     interruptor está apagado: batería gris con una ✕ (va solo con el USB, la
 *     batería ni se carga ni se usa). En una radio sin batería nunca sale.
 *   - WiFi: hal_red_senal(), la señal en dBm. Con buena señal no se muestra
 *     nada; débil, el icono en amarillo; sin conexión, en rojo. Así se ve si
 *     la radio pierde la WiFi por estar lejos del router.
 * Los iconos van en la esquina inferior izquierda: la batería pegada al
 * borde y la WiFi a su derecha (o pegada al borde si no hay batería). Sin
 * nada que avisar, las pantallas usan todo el alto.
 *
 * Los iconos viven en la capa superior de LVGL, por encima de cualquier
 * pantalla: sobreviven a los cambios de pantalla y no se borran nunca.
 * Sin LV_OBJ_FLAG_CLICKABLE los toques pasan al mapa o botón de debajo.
 * Las capas que tapan toda la pantalla (reposo, portal WiFi, menú de
 * ajustes, aviso de alarma) se crean después, así que los tapan mientras
 * están abiertas y al cerrarse vuelven a verse.
 *
 * Las pantallas reservan esa esquina con bateria_reserva() (alto) y
 * bateria_reserva_ancho() (ancho) para no poner nada debajo de los iconos
 * (listas, teclado, pie del mapamundi...).
 */
#include "bateria.h"
#include "comun.h"
#include "hal.h"
#include "ajustes.h"

#include <lvgl.h>
#include <stdio.h>
#include <string.h>

#define PERIODO_MS   5000  /* ms entre lecturas */
#define MARGEN_X     8     /* separación del borde izquierdo */
#define MARGEN_Y     18    /* centro del icono a la misma altura que el
                             centro del texto del pie de la pantalla principal
                             (radio_20, línea de 25 px en y 197-222) */
#define ENTRE_ICONOS 6     /* separación entre la batería y la WiFi */

/* Lo que ocupan el icono de batería y su margen: lo que las pantallas
 * reservan (alto, y ancho si solo está la batería) */
#define ALTO_RESERVA 26

/* Señal WiFi (dBm): por debajo de SENAL_DEBIL se avisa; deja de avisarse al
 * subir de SENAL_BUENA. El hueco entre las dos evita que el icono parpadee
 * cuando la señal ronda el límite */
#define SENAL_DEBIL  -75
#define SENAL_BUENA  -70

typedef enum { WIFI_BIEN, WIFI_DEBIL, WIFI_SIN } NivelWifi;

static lv_obj_t *icono;       /* batería */
static lv_obj_t *icono_wifi;
static BatteryLevel nivel = BAT_LEVEL_FULL;  /* nivel actual, para no actualizar si no cambia */
static bool conectada = true;           /* hay batería (interruptor encendido) */
static bool interruptor_apagado = false; /* se ve la batería con la ✕ */
static NivelWifi nivel_wifi = WIFI_BIEN;
static lv_coord_t ancho_reserva = 0;

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

/* Texto del icono de batería: su nivel o, con el interruptor apagado,
 * la batería vacía con una ✕ */
static const char *texto_bateria(void)
{
    return interruptor_apagado ? LV_SYMBOL_BATTERY_EMPTY LV_SYMBOL_CLOSE : simbolo(nivel);
}

static lv_coord_t ancho_texto(const char *texto)
{
    return lv_txt_get_width(texto, strlen(texto), &radio_20, 0, LV_TEXT_FLAG_NONE);
}

/* Coloca la WiFi a la derecha de la batería (o en el borde) y calcula el
 * ancho que deben reservar las pantallas */
static void colocar(void)
{
    bool hay_bateria = nivel != BAT_LEVEL_FULL || interruptor_apagado;
    bool hay_wifi = nivel_wifi != WIFI_BIEN;
    lv_coord_t x_wifi = MARGEN_X;
    if (hay_bateria) {
        x_wifi = MARGEN_X + ancho_texto(texto_bateria()) + ENTRE_ICONOS;
    }
    lv_obj_align(icono_wifi, LV_ALIGN_BOTTOM_LEFT, x_wifi, -MARGEN_Y);

    if (hay_wifi) {
        ancho_reserva = x_wifi + ancho_texto(LV_SYMBOL_WIFI) + 2;
    } else if (interruptor_apagado) {
        ancho_reserva = MARGEN_X + ancho_texto(texto_bateria()) + 2;
    } else if (hay_bateria) {
        ancho_reserva = ALTO_RESERVA;  /* como antes, con solo la batería */
    } else {
        ancho_reserva = 0;
    }
}

static bool mirar_bateria(void)
{
    conectada = hal_bateria_conectada();
    if (conectada && !ajustes.bateria_vista) {
        ajustes.bateria_vista = true;  /* desde ahora, avisar si se apaga el interruptor */
        ajustes_guardar();
    }
    bool apagado = !conectada && ajustes.bateria_vista;
    BatteryLevel nuevo = getBatteryLevel();
    if (nuevo == nivel && apagado == interruptor_apagado) {
        return false;
    }
    nivel = nuevo;
    interruptor_apagado = apagado;

    if (interruptor_apagado) {
        lv_obj_clear_flag(icono, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(icono, texto_bateria());
        lv_obj_set_style_text_color(icono, lv_color_hex(COLOR_SECUNDARIO), 0);
        return true;
    }

    /* sin batería, o cargada a tope: no se muestra nada */
    if (nivel == BAT_LEVEL_FULL) {
        lv_obj_add_flag(icono, LV_OBJ_FLAG_HIDDEN);
        return true;
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
    return true;
}

static bool mirar_wifi(void)
{
    int senal = hal_red_senal();
    NivelWifi nuevo;
    if (hal_red_estado() != RED_CONECTADA || senal == 0) {
        nuevo = WIFI_SIN;
    } else if (senal <= SENAL_DEBIL) {
        nuevo = WIFI_DEBIL;
    } else if (senal >= SENAL_BUENA) {
        nuevo = WIFI_BIEN;
    } else {
        /* entre los dos límites se queda como estaba (si volvía de no
         * tener conexión, débil) */
        nuevo = nivel_wifi == WIFI_SIN ? WIFI_DEBIL : nivel_wifi;
    }
    if (nuevo == nivel_wifi) {
        return false;
    }
    nivel_wifi = nuevo;
    printf("[wifi] %d dBm: %s\n", senal,
           nuevo == WIFI_BIEN ? "buena" : (nuevo == WIFI_DEBIL ? "débil" : "sin conexión"));

    if (nuevo == WIFI_BIEN) {
        lv_obj_add_flag(icono_wifi, LV_OBJ_FLAG_HIDDEN);
        return true;
    }
    lv_obj_clear_flag(icono_wifi, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_color(icono_wifi, lv_color_hex(nuevo == WIFI_DEBIL ? COLOR_ACENTO : COLOR_ERROR), 0);
    return true;
}

static void mirar(lv_timer_t *t)
{
    (void)t;
    bool cambio = mirar_bateria();
    cambio = mirar_wifi() || cambio;
    if (cambio) {
        colocar();
    }
}

void bateria_iniciar(void)
{
    icono = lv_label_create(lv_layer_top());
    lv_obj_set_style_text_font(icono, &radio_20, 0);
    lv_obj_align(icono, LV_ALIGN_BOTTOM_LEFT, MARGEN_X, -MARGEN_Y);
    lv_obj_add_flag(icono, LV_OBJ_FLAG_HIDDEN);

    icono_wifi = lv_label_create(lv_layer_top());
    lv_obj_set_style_text_font(icono_wifi, &radio_20, 0);
    lv_label_set_text(icono_wifi, LV_SYMBOL_WIFI);
    lv_obj_add_flag(icono_wifi, LV_OBJ_FLAG_HIDDEN);

    lv_timer_create(mirar, PERIODO_MS, NULL);
    /* Primera lectura ya, para que las pantallas sepan si reservar. La WiFi
     * aún no ha tenido tiempo de conectar: no se avisa hasta la siguiente */
    mirar_bateria();
    colocar();
}

bool bateria_hay(void)
{
    /* sin nada que avisar no hay icono que reservar */
    return nivel != BAT_LEVEL_FULL || interruptor_apagado || nivel_wifi != WIFI_BIEN;
}

bool bateria_conectada(void)
{
    return conectada;
}

lv_coord_t bateria_reserva(void)
{
    return bateria_hay() ? ALTO_RESERVA : 0;
}

lv_coord_t bateria_reserva_ancho(void)
{
    return ancho_reserva;
}
