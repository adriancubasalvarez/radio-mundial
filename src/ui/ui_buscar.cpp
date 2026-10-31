/**
 * Buscador.
 *
 *   «Buscar»       campo de texto en la cabecera y teclado en pantalla.
 *                  Sin texto: lista de géneros (y «Mis emisoras» si hay SD).
 *                  Con texto: emisoras cuyo nombre contiene las palabras,
 *                  de la más popular a la menos, mientras se escribe.
 *   «Género»       emisoras más populares de un género.
 *   «Mis emisoras» las de mis_emisoras.csv en la tarjeta SD.
 */
#include "nav.h"
#include "comun.h"
#include "hal.h"
#include "idioma.h"

#include <lvgl.h>
#include <stdio.h>
#include <string.h>

#define ALTO_TECLADO 130
#define ESPERA_BUSQUEDA 300  /* ms sin teclear antes de buscar */

/* Abre «Reproduciendo» con la emisora de un resultado de búsqueda */
static void abrir_resultado(const Resultado *r)
{
    if (datos_emisora_de_resultado(r, &nav.emisora, &nav.pais)) {
        nav.aleatoria = false;
        nav_ir(PANT_SONANDO);
    }
}

static void llenar_resultados(lv_obj_t *lista, const Resultado *resultados, int num)
{
    for (int i = 0; i < num; i++) {
        comun_lista_agregar(lista, LV_SYMBOL_AUDIO, resultados[i].nombre, resultados[i].codigo, false);
    }
}

/* ---------- Pantalla «Buscar» ---------- */

static Resultado resultados_buscar[MAX_RESULTADOS];
static int num_resultados;
static char consulta[40];         /* se conserva al volver de «Reproduciendo» */
static bool hay_mis_emisoras;     /* la primera fila de inicio es «Mis emisoras» */
static bool mostrando_inicio;     /* la lista muestra géneros, no resultados */
static bool teclado_visible;

static lv_obj_t *pantalla_actual;
static lv_obj_t *campo;
static lv_obj_t *lista_buscar;
static lv_obj_t *teclado;
static lv_timer_t *temporizador;

void buscar_desde_cero(void)
{
    consulta[0] = '\0';
}

/* Teclado propio: solo mayúsculas, números y espacio (lo que hace falta
 * para buscar), con teclas más grandes que el de serie. «1#» y «ABC» son
 * los textos que LVGL reconoce para cambiar de modo. */
static const char *mapa_letras[] = {
    "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "\n",
    "A", "S", "D", "F", "G", "H", "J", "K", "L", "\n",
    "1#", "Z", "X", "C", "V", "B", "N", "M", LV_SYMBOL_BACKSPACE, "\n",
    " ", LV_SYMBOL_OK, ""};
static const lv_btnmatrix_ctrl_t control_letras[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1,
    LV_KEYBOARD_CTRL_BTN_FLAGS | 2, 2, 2, 2, 2, 2, 2, 2, LV_BTNMATRIX_CTRL_CHECKED | 3,
    6, LV_KEYBOARD_CTRL_BTN_FLAGS | 2};

static const char *mapa_numeros[] = {
    "1", "2", "3", "4", "5", "\n",
    "6", "7", "8", "9", "0", "\n",
    "ABC", " ", LV_SYMBOL_BACKSPACE, LV_SYMBOL_OK, ""};
static const lv_btnmatrix_ctrl_t control_numeros[] = {
    1, 1, 1, 1, 1,
    1, 1, 1, 1, 1,
    LV_KEYBOARD_CTRL_BTN_FLAGS | 1, 2, LV_BTNMATRIX_CTRL_CHECKED | 1, LV_KEYBOARD_CTRL_BTN_FLAGS | 1};

static void al_elegir_fila(uint16_t fila)
{
    if (!mostrando_inicio) {
        if (fila < num_resultados) {
            abrir_resultado(&resultados_buscar[fila]);
        }
        return;
    }
    if (hay_mis_emisoras) {
        if (fila == 0) {
            nav_ir(PANT_MIS_EMISORAS);
            return;
        }
        fila--;
    }
    nav.genero = fila;
    nav_ir(PANT_GENERO);
}

static void ajustar_alto_lista(void)
{
    lv_obj_set_height(lista_buscar, PANTALLA_ALTO - ALTO_CABECERA - (teclado_visible ? ALTO_TECLADO : 0));
}

/* Crea la lista de nuevo (vacía) debajo del teclado */
static void nueva_lista(void)
{
    if (lista_buscar != NULL) {
        lv_obj_del(lista_buscar);
    }
    lista_buscar = comun_lista(pantalla_actual, al_elegir_fila);
    lv_obj_move_background(lista_buscar);  /* el teclado queda por encima */
    ajustar_alto_lista();
}

/* Sin texto: géneros y, si hay tarjeta con el archivo, «Mis emisoras» */
static void mostrar_inicio(void)
{
    nueva_lista();
    mostrando_inicio = true;
    hay_mis_emisoras = datos_hay_mis_emisoras();
    if (hay_mis_emisoras) {
        comun_lista_agregar(lista_buscar, LV_SYMBOL_SD_CARD, TR("My stations (SD)", "Mis emisoras (SD)"), NULL, false);
    }
    for (int i = 0; i < NUM_GENEROS; i++) {
        comun_lista_agregar(lista_buscar, LV_SYMBOL_LIST, TR(GENEROS[i].nombre_en, GENEROS[i].nombre), NULL, false);
    }
}

static void buscar_ahora(lv_timer_t *t)
{
    (void)t;
    temporizador = NULL;
    const char *texto = lv_textarea_get_text(campo);
    strncpy(consulta, texto, sizeof(consulta) - 1);
    consulta[sizeof(consulta) - 1] = '\0';

    /* ¿Hay algo más que espacios? */
    bool vacio = true;
    for (const char *p = consulta; *p; p++) {
        vacio = vacio && *p == ' ';
    }
    if (vacio) {
        mostrar_inicio();
        return;
    }

    uint32_t inicio = hal_millis();
    num_resultados = datos_buscar(consulta, 0, resultados_buscar, MAX_RESULTADOS);
    printf("[buscar] «%s»: %d resultados en %u ms\n", consulta, num_resultados, (unsigned)(hal_millis() - inicio));
    nueva_lista();
    mostrando_inicio = false;
    if (num_resultados == 0) {
        comun_lista_agregar(lista_buscar, NULL, TR("No results", "Sin resultados"), NULL, true);
        return;
    }
    llenar_resultados(lista_buscar, resultados_buscar, num_resultados);
}

static void programar_busqueda(void)
{
    if (temporizador != NULL) {
        lv_timer_reset(temporizador);
    } else {
        temporizador = lv_timer_create(buscar_ahora, ESPERA_BUSQUEDA, NULL);
        lv_timer_set_repeat_count(temporizador, 1);
    }
}

static void mostrar_teclado(bool visible)
{
    teclado_visible = visible;
    if (visible) {
        lv_obj_clear_flag(teclado, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(teclado, LV_OBJ_FLAG_HIDDEN);
    }
    ajustar_alto_lista();
}

static void al_cambiar_campo(lv_event_t *e)
{
    lv_event_code_t codigo = lv_event_get_code(e);
    if (codigo == LV_EVENT_VALUE_CHANGED) {
        programar_busqueda();
    } else if (codigo == LV_EVENT_CLICKED) {
        mostrar_teclado(true);
    }
}

static void al_usar_teclado(lv_event_t *e)
{
    lv_event_code_t codigo = lv_event_get_code(e);
    if (codigo == LV_EVENT_READY || codigo == LV_EVENT_CANCEL) {
        mostrar_teclado(false); /* ✓: esconder el teclado para ver la lista */
    }
}

static void al_borrar_buscar(lv_event_t *e)
{
    (void)e;
    if (temporizador != NULL) {
        lv_timer_del(temporizador);
        temporizador = NULL;
    }
    lista_buscar = NULL;  /* se borra con la pantalla */
}

lv_obj_t *pantalla_buscar(void)
{
    lv_obj_t *pantalla = comun_pantalla();
    pantalla_actual = pantalla;
    lista_buscar = NULL;
    lv_obj_add_event_cb(pantalla, al_borrar_buscar, LV_EVENT_DELETE, NULL);

    /* Cabecera con «Atrás» y el campo de búsqueda en lugar del título */
    lv_obj_t *titulo;
    lv_obj_t *cabecera = comun_cabecera(pantalla, "", true, &titulo);
    lv_obj_add_flag(titulo, LV_OBJ_FLAG_HIDDEN);

    campo = lv_textarea_create(cabecera);
    lv_textarea_set_one_line(campo, true);
    lv_textarea_set_max_length(campo, sizeof(consulta) - 1);
    lv_textarea_set_placeholder_text(campo, TR(SIMBOLO_BUSCAR "  Search stations", SIMBOLO_BUSCAR "  Buscar emisora"));
    lv_obj_set_size(campo, PANTALLA_ANCHO - 48 - 8, ALTO_CABECERA - 4);
    lv_obj_align(campo, LV_ALIGN_LEFT_MID, 48, 0);
    lv_obj_set_style_text_font(campo, &radio_14, 0);
    lv_obj_set_style_pad_ver(campo, 4, 0);
    lv_obj_set_style_border_width(campo, 0, 0);
    lv_obj_set_style_bg_color(campo, lv_color_hex(COLOR_LINEA), 0);
    lv_obj_add_event_cb(campo, al_cambiar_campo, LV_EVENT_ALL, NULL);

    teclado = lv_keyboard_create(pantalla);
    lv_obj_set_size(teclado, PANTALLA_ANCHO, ALTO_TECLADO);
    lv_obj_align(teclado, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_text_font(teclado, &radio_20, 0);
    lv_obj_set_style_pad_all(teclado, 3, 0);
    lv_obj_set_style_pad_gap(teclado, 3, 0);
    lv_keyboard_set_map(teclado, LV_KEYBOARD_MODE_TEXT_UPPER, mapa_letras, control_letras);
    lv_keyboard_set_map(teclado, LV_KEYBOARD_MODE_SPECIAL, mapa_numeros, control_numeros);
    lv_keyboard_set_mode(teclado, LV_KEYBOARD_MODE_TEXT_UPPER);
    lv_keyboard_set_textarea(teclado, campo);
    lv_obj_add_event_cb(teclado, al_usar_teclado, LV_EVENT_ALL, NULL);

    /* Al volver de «Reproduciendo» se recupera la búsqueda anterior y la
     * lista se ve entera; la primera vez se abre con el teclado a la vista */
    temporizador = NULL;
    if (consulta[0] != '\0') {
        teclado_visible = false;
        lv_obj_add_flag(teclado, LV_OBJ_FLAG_HIDDEN);
        lv_textarea_set_text(campo, consulta);  /* programa la búsqueda */
        nueva_lista();
    } else {
        teclado_visible = true;
        mostrar_inicio();
    }
    return pantalla;
}

/* ---------- Pantalla «Género» ---------- */

static Resultado resultados_genero[MAX_RESULTADOS];
static int num_genero;

static void al_elegir_genero(uint16_t fila)
{
    if (fila < num_genero) {
        abrir_resultado(&resultados_genero[fila]);
    }
}

lv_obj_t *pantalla_genero(void)
{
    lv_obj_t *pantalla = comun_pantalla();
    const Genero *g = &GENEROS[nav.genero];
    comun_cabecera(pantalla, TR(g->nombre_en, g->nombre), true, NULL);
    lv_obj_t *lista = comun_lista(pantalla, al_elegir_genero);
    num_genero = datos_buscar("", g->letra, resultados_genero, MAX_RESULTADOS);
    llenar_resultados(lista, resultados_genero, num_genero);
    return pantalla;
}

/* ---------- Pantalla «Mis emisoras» (tarjeta SD) ---------- */

static ListaEmisoras mis_emisoras;

static void al_elegir_mia(uint16_t fila)
{
    nav.emisora = mis_emisoras.emisoras[fila];
    memset(&nav.pais, 0, sizeof(nav.pais));
    strcpy(nav.pais.codigo, "SD");
    strcpy(nav.pais.nombre, TR("My stations", "Mis emisoras"));
    nav.aleatoria = false;
    nav_ir(PANT_SONANDO);
}

static void al_borrar_mias(lv_event_t *e)
{
    (void)e;
    datos_liberar_emisoras(&mis_emisoras);
}

lv_obj_t *pantalla_mis_emisoras(void)
{
    lv_obj_t *pantalla = comun_pantalla();
    lv_obj_add_event_cb(pantalla, al_borrar_mias, LV_EVENT_DELETE, NULL);
    comun_cabecera(pantalla, TR("My stations", "Mis emisoras"), true, NULL);

    if (!datos_cargar_mis_emisoras(&mis_emisoras) || mis_emisoras.num == 0) {
        lv_obj_t *aviso = lv_label_create(pantalla);
        lv_label_set_text(aviso, TR("No stations in\n" ARCHIVO_MIS_EMISORAS "\n\nOne per line:  name;url",
                                     "No hay emisoras en\n" ARCHIVO_MIS_EMISORAS "\n\nUna por línea:  nombre;url"));
        lv_obj_set_style_text_color(aviso, lv_color_hex(COLOR_SECUNDARIO), 0);
        lv_obj_set_style_text_align(aviso, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(aviso, LV_ALIGN_CENTER, 0, ALTO_CABECERA / 2);
        return pantalla;
    }
    lv_obj_t *lista = comun_lista(pantalla, al_elegir_mia);
    for (uint16_t i = 0; i < mis_emisoras.num; i++) {
        comun_lista_agregar(lista, LV_SYMBOL_SD_CARD, mis_emisoras.emisoras[i].nombre, NULL, false);
    }
    return pantalla;
}
