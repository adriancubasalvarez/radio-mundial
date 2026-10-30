/**
 * Utilidades del sistema en el simulador: hora, azar y ajustes.
 *
 * Los ajustes se guardan en ajustes.json en la carpeta desde la que se
 * ejecuta el simulador (se puede cambiar con la variable RADIO_AJUSTES).
 * En la placa irán a la NVS (Non-Volatile Storage).
 */
#include "hal.h"

#include <random>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static const char *ruta_ajustes(void)
{
    const char *ruta = getenv("RADIO_AJUSTES");
    return ruta ? ruta : "ajustes.json";
}

bool hal_hora(int *hora, int *minuto)
{
    time_t ahora = time(NULL);
    struct tm *local = localtime(&ahora);
    if (local == NULL) {
        return false;
    }
    *hora = local->tm_hour;
    *minuto = local->tm_min;
    return true;
}

uint32_t hal_aleatorio(void)
{
    static std::mt19937 generador{std::random_device{}()};
    return generador();
}

char *hal_ajustes_leer(void)
{
    FILE *f = fopen(ruta_ajustes(), "rb");
    if (f == NULL) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long largo = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *texto = (char *)malloc(largo + 1);
    if (texto != NULL) {
        size_t leidos = fread(texto, 1, largo, f);
        texto[leidos] = '\0';
    }
    fclose(f);
    return texto;
}

bool hal_ajustes_guardar(const char *json)
{
    FILE *f = fopen(ruta_ajustes(), "wb");
    if (f == NULL) {
        return false;
    }
    fputs(json, f);
    fclose(f);
    return true;
}
