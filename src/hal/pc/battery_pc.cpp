#include "hal.h"

#include <stdlib.h>

/* Batería en el simulador. Por defecto, «llena» (sin icono), como una radio
 * enchufada o sin batería: así las capturas del manual salen siempre igual.
 * Para probar los iconos, la variable de entorno RADIO_BATERIA elige un
 * nivel fijo (0 vacía, 1, 2, 3, 4 llena) o «ciclo», que los recorre todos
 * (uno por lectura, cada 5 segundos). */
BatteryLevel getBatteryLevel() {
    const char *valor = getenv("RADIO_BATERIA");
    if (valor == NULL || valor[0] == '\0') {
        return BAT_LEVEL_FULL;
    }
    if (valor[0] == 'c') {
        static int llamadas = 0;
        llamadas = (llamadas + 1) % 5;
        return (BatteryLevel)llamadas;
    }
    int nivel = atoi(valor);
    if (nivel < BAT_LEVEL_EMPTY || nivel > BAT_LEVEL_FULL) {
        return BAT_LEVEL_FULL;
    }
    return (BatteryLevel)nivel;
}
