/**
 * Indicador de batería: icono en la esquina inferior izquierda, siempre
 * visible cuando hay batería conectada.
 */
#pragma once

#include <lvgl.h>
#include <stdbool.h>

/* Crea el icono y empieza a vigilar la batería (llamar una vez, al arrancar,
 * antes de construir la primera pantalla) */
void bateria_iniciar(void);

/* true si hay batería conectada */
bool bateria_hay(void);

/* Alto en píxeles que las pantallas deben dejar libre abajo, en la esquina
 * izquierda, para que nada quede debajo del icono (0 si no hay batería) */
lv_coord_t bateria_reserva(void);
