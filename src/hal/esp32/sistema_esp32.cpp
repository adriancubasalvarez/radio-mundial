/**
 * Utilidades del sistema en la placa: hora por NTP (Network Time Protocol,
 * se configura al iniciar la red) y números aleatorios.
 */
#include "hal.h"

#include <Arduino.h>
#include <esp_random.h>
#include <time.h>

bool hal_hora(int *hora, int *minuto)
{
    struct tm local;
    if (!getLocalTime(&local, 0)) {
        return false;  // aún no ha llegado la hora por NTP
    }
    *hora = local.tm_hour;
    *minuto = local.tm_min;
    return true;
}

uint32_t hal_aleatorio(void)
{
    return esp_random();
}
