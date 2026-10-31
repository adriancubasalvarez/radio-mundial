/**
 * Lista alfabética de los países de la región (alternativa al mapa para los
 * países pequeños o que quedan fuera del encuadre). A la derecha, cuántas
 * emisoras tiene cada uno; los que no tienen ninguna salen apagados.
 */
#include "nav.h"
#include "comun.h"
#include "hal.h"

#include <lvgl.h>
#include <stdio.h>

static ListaPaises paises;

static void al_elegir(uint16_t fila)
{
    nav.pais = paises.paises[fila];
    nav_ir(PANT_EMISORAS);
}

lv_obj_t *pantalla_paises(void)
{
    lv_obj_t *pantalla = comun_pantalla();
    comun_cabecera(pantalla, nav.region.nombre, true, NULL);
    lv_obj_t *lista = comun_lista(pantalla, al_elegir);

    datos_cargar_paises(nav.region.id, &paises);
    for (uint8_t i = 0; i < paises.num; i++) {
        const Pais *pais = &paises.paises[i];
        char cuenta[8];
        snprintf(cuenta, sizeof(cuenta), "%d", pais->emisoras);
        comun_lista_agregar(lista, NULL, pais->nombre, cuenta, pais->emisoras == 0);
    }
    return pantalla;
}
