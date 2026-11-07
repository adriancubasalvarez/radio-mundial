#include "nav.h"
#include "ajustes.h"
#include "hal.h"
#include "luz.h"

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
    case PANT_RECIENTES: return pantalla_recientes();
    case PANT_BUSCAR:   return pantalla_buscar();
    case PANT_GENERO:   return pantalla_genero();
    case PANT_MIS_EMISORAS: return pantalla_mis_emisoras();
    case PANT_ALARMAS:  return pantalla_alarmas();
    case PANT_ALARMA:   return pantalla_alarma();
    case PANT_ALARMA_SONIDO: return pantalla_alarma_sonido();
    }
    return pantalla_mundo();
}

/* true mientras dura la animación de un cambio de pantalla. Cambiar otra
 * vez en ese momento (dos toques seguidos en «Atrás», o el aviso de una
 * emisora caída que vuelve atrás solo justo cuando se toca «Atrás») hacía
 * que LVGL usara una pantalla que ya se estaba borrando: la radio se
 * reiniciaba. Mientras tanto, se ignora la orden. */
static bool cambiando(void)
{
    lv_disp_t *d = lv_disp_get_default();
    return d != NULL && d->scr_to_load != NULL;
}

/* Construye la pantalla y sustituye a la actual; la anterior se borra sola
 * al acabar la animación (y con ella sus mapas y listas). */
static void mostrar(Pantalla pantalla, lv_scr_load_anim_t animacion)
{
    lv_obj_t *nueva = construir(pantalla);
    lv_scr_load_anim(nueva, animacion, DURACION_ANIM, 0, true);

    /* Uso de memoria de LVGL con las dos pantallas vivas (el peor momento) */
    lv_mem_monitor_t mem;
    lv_mem_monitor(&mem);
    printf("[memoria] pantalla %d: %u de %u bytes usados (%d%%), fragmentación %d%%\n",
           (int)pantalla, (unsigned)(mem.total_size - mem.free_size), (unsigned)mem.total_size,
           mem.used_pct, mem.frag_pct);
}

void nav_ir(Pantalla pantalla)
{
    if (tope + 1 >= MAX_PILA || cambiando()) {
        return;
    }
    pila[++tope] = pantalla;
    /* La primera pantalla (tras el logo) aparece con un fundido */
    mostrar(pantalla, tope == 0 ? LV_SCR_LOAD_ANIM_FADE_ON : LV_SCR_LOAD_ANIM_MOVE_LEFT);
}

/* En cuanto suena se olvidan sus fallos, en cualquier pantalla: al arrancar,
 * la última emisora empieza sola sobre el mapamundi */
static bool sonido_comprobado;

void nav_atender(void)
{
    if (!sonido_comprobado && nav.hay_sonando && audio_estado() == AUDIO_SONANDO) {
        sonido_comprobado = true;
        ajustes_olvidar_fallo(nav.sonando.url);
    }
}

void nav_reproducir(void)
{
    sonido_comprobado = false;
    audio_reproducir(nav.emisora.url);
    nav.sonando = nav.emisora;
    nav.hay_sonando = true;
    ajustes_recordar_ultima(&nav.emisora, &nav.pais);
    luz_emisora(nav.pais.codigo);
    printf("Reproduciendo: %s (%s)\n  %s\n", nav.emisora.nombre, nav.pais.codigo, nav.emisora.url);
}

void nav_ir_al_pais(void)
{
    const Region *region = datos_region_de_pais(&nav.indice, nav.pais.codigo);
    if (region == NULL || cambiando()) {
        return;
    }
    nav.region = *region;
    pila[0] = PANT_MUNDO;
    pila[1] = PANT_REGION;
    pila[2] = PANT_EMISORAS;
    tope = 2;
    mostrar(PANT_EMISORAS, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
}

void nav_reiniciar(void)
{
    /* La pantalla actual puede ser otro mapamundi, que comparte el mapa en
     * memoria con el nuevo: hay que borrarla ANTES de crear el nuevo. Para
     * eso se pasa por una pantalla vacía, sin animación (se borra al momento). */
    lv_obj_t *vacia = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(vacia, lv_color_hex(0x0B1D33), 0);
    lv_scr_load_anim(vacia, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
    /* Sin pasar por nav_ir(): la carga de la pantalla vacía aún «está en
     * marcha» y nav_ir() la ignoraría */
    tope = 0;
    pila[0] = PANT_MUNDO;
    mostrar(PANT_MUNDO, LV_SCR_LOAD_ANIM_FADE_ON);
}

void nav_atras(void)
{
    if (tope <= 0 || cambiando()) {
        return;
    }
    tope--;
    mostrar(pila[tope], LV_SCR_LOAD_ANIM_MOVE_RIGHT);
}
