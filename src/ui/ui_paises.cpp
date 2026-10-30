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

static void al_elegir(lv_event_t *e)
{
    uintptr_t i = (uintptr_t)lv_event_get_user_data(e);
    nav.pais = paises.paises[i];
    nav_ir(PANT_EMISORAS);
}

lv_obj_t *pantalla_paises(void)
{
    lv_obj_t *pantalla = comun_pantalla();
    comun_cabecera(pantalla, nav.region.nombre, true, NULL);
    lv_obj_t *lista = comun_lista(pantalla);

    datos_cargar_paises(nav.region.id, &paises);
    for (uintptr_t i = 0; i < paises.num; i++) {
        const Pais *pais = &paises.paises[i];
        lv_obj_t *elemento = comun_elemento_lista(lista, NULL, pais->nombre);

        char cuenta[8];
        snprintf(cuenta, sizeof(cuenta), "%d", pais->emisoras);
        comun_nota_elemento(elemento, cuenta);

        if (pais->emisoras == 0) {
            comun_apagar_elemento(elemento);
        } else {
            lv_obj_add_event_cb(elemento, al_elegir, LV_EVENT_CLICKED, (void *)i);
        }
    }
    return pantalla;
}
