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

bool hal_fecha(int *anio, int *mes, int *dia, int *dia_semana)
{
    time_t ahora = time(NULL);
    struct tm *local = localtime(&ahora);
    if (local == NULL) {
        return false;
    }
    *anio = local->tm_year + 1900;
    *mes = local->tm_mon + 1;
    *dia = local->tm_mday;
    *dia_semana = local->tm_wday;
    return true;
}

void hal_zona_horaria(int segundos)
{
    (void)segundos;  /* el simulador usa la hora del PC */
}

/* Aproximada: usa la diferencia con la hora universal de la regla («CET-1...»,
 * «JST-9»...) y no tiene en cuenta el horario de verano */
bool hal_hora_en(const char *regla, int *hora, int *minuto)
{
    const char *p = regla;
    if (*p == '<') {
        p = strchr(p, '>');
        p = p != NULL ? p + 1 : regla;
    }
    while (*p && !(*p == '-' || *p == '+' || (*p >= '0' && *p <= '9'))) {
        p++;
    }
    int horas = 0, minutos = 0;
    sscanf(p, "%d:%d", &horas, &minutos);
    time_t ahora = time(NULL) - (horas * 3600 + (horas < 0 ? -minutos : minutos) * 60);
    struct tm *t = gmtime(&ahora);
    *hora = t->tm_hour;
    *minuto = t->tm_min;
    return true;
}

/* En el simulador las emisoras se regeneran con las herramientas del PC */
static EstadoActualizar estado_actualizar = ACT_NADA;

void hal_actualizar_comprobar(void)
{
    estado_actualizar = ACT_HAY_NUEVA;
}

void hal_actualizar_instalar(void)
{
    estado_actualizar = ACT_AL_DIA;
}

EstadoActualizar hal_actualizar_estado(uint8_t *progreso)
{
    *progreso = 0;
    return estado_actualizar;
}

const char *hal_actualizar_fecha(void)
{
    return "2026-11-07";
}

void hal_reiniciar(void)
{
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
