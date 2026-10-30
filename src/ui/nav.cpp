#include "nav.h"
#include "ajustes.h"
#include "hal.h"

#include <stdio.h>

EstadoNav nav;

#define MAX_PILA 8
#define DURACION_ANIM 200 /* ms de la animación de cambio de pantalla */

static Pantalla pila[MAX_PILA];
static int tope = -1;

static lv_obj_t *construir(Pantalla pantalla)
{
    switch (pantalla) {
    case PANT_MUNDO:    return pantalla_mundo();
    case PANT_REGION:   return pantalla_region();
    case PANT_PAISES:   return pantalla_paises();
    case PANT_EMISORAS: return pantalla_emisoras();
    case PANT_SONANDO:  return pantalla_sonando();
    case PANT_FAVORITOS: return pantalla_favoritos();
    }
    return pantalla_mundo();
}

/* Construye la pantalla y sustituye a la actual; la anterior se borra sola
 * al acabar la animación (y con ella sus mapas y listas). */
static void mostrar(Pantalla pantalla, lv_scr_load_anim_t animacion)
{
    lv_obj_t *nueva = construir(pantalla);
    lv_scr_load_anim(nueva, animacion, DURACION_ANIM, 0, true);

#ifdef RADIO_PC
    /* Uso de memoria de LVGL con las dos pantallas vivas (el peor momento) */
    lv_mem_monitor_t mem;
    lv_mem_monitor(&mem);
    printf("[memoria] pantalla %d: %u de %u bytes usados (%d%%), fragmentación %d%%\n",
           (int)pantalla, (unsigned)(mem.total_size - mem.free_size), (unsigned)mem.total_size,
           mem.used_pct, mem.frag_pct);
#endif
}

void nav_ir(Pantalla pantalla)
{
    if (tope + 1 >= MAX_PILA) {
        return;
    }
    pila[++tope] = pantalla;
    /* La primera pantalla (tras el logo) aparece con un fundido */
    mostrar(pantalla, tope == 0 ? LV_SCR_LOAD_ANIM_FADE_ON : LV_SCR_LOAD_ANIM_MOVE_LEFT);
}

void nav_reproducir(void)
{
    audio_reproducir(nav.emisora.url);
    nav.sonando = nav.emisora;
    nav.hay_sonando = true;
    ajustes_recordar_ultima(&nav.emisora, &nav.pais);
    printf("Reproduciendo: %s (%s)\n  %s\n", nav.emisora.nombre, nav.pais.codigo, nav.emisora.url);
}

void nav_atras(void)
{
    if (tope <= 0) {
        return;
    }
    tope--;
    mostrar(pila[tope], LV_SCR_LOAD_ANIM_MOVE_RIGHT);
}
