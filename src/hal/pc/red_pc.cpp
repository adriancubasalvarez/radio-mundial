/**
 * Red en el simulador: el PC ya está conectado. El portal solo se simula
 * para poder ver y probar su pantalla.
 */
#include "hal.h"

#include <stdlib.h>

static EstadoRed estado = RED_CONECTADA;

EstadoRed hal_red_estado(void)
{
    return estado;
}

/* Para probar el icono de WiFi: RADIO_WIFI=-80 (débil), RADIO_WIFI=0 (sin conexión) */
int hal_red_senal(void)
{
    const char *valor = getenv("RADIO_WIFI");
    if (valor == NULL || valor[0] == '\0') {
        return -50;
    }
    return atoi(valor);
}

static bool cortes_borrados = false;

uint16_t hal_red_cortes(void)
{
    const char *valor = getenv("RADIO_WIFI");
    return (!cortes_borrados && valor != NULL && atoi(valor) == 0 && valor[0] == '0') ? 1 : 0;
}

void hal_red_cortes_borrar(void)
{
    cortes_borrados = true;
}

const char *hal_red_portal(void)
{
    return "RadioMundial-PC00";
}

void hal_red_configurar(void)
{
    estado = RED_PORTAL;
}

void hal_red_cancelar(void)
{
    estado = RED_CONECTADA;
}
