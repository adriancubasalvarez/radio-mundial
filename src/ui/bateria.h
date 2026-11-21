/**
 * Indicadores de la esquina inferior izquierda: batería (si hay y no está
 * llena) y WiFi (si la señal es débil o no hay conexión).
 */
#pragma once

#include <lvgl.h>
#include <stdbool.h>

/* Crea el icono y empieza a vigilar la batería (llamar una vez, al arrancar,
 * antes de construir la primera pantalla) */
void bateria_iniciar(void);

/* true si hay batería conectada (interruptor encendido), según la última
 * lectura */
bool bateria_conectada(void);

/* true si se ve algún icono (batería o WiFi) */
bool bateria_hay(void);

/* Alto en píxeles que las pantallas deben dejar libre abajo, en la esquina
 * izquierda, para que nada quede debajo de los iconos (0 si no se ve ninguno) */
lv_coord_t bateria_reserva(void);

/* Ancho en píxeles que ocupan los iconos desde el borde izquierdo, para lo
 * que va a su derecha en la misma línea (0 si no se ve ninguno) */
lv_coord_t bateria_reserva_ancho(void);
