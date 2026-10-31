/**
 * Red en el simulador: el PC ya está conectado. El portal solo se simula
 * para poder ver y probar su pantalla.
 */
#include "hal.h"

static EstadoRed estado = RED_CONECTADA;

EstadoRed hal_red_estado(void)
{
    return estado;
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
