/**
 * «Ubicación»: elegir la ciudad del tiempo y de la hora del reposo.
 *
 * Por defecto la radio se ubica sola por la IP de la conexión, pero algunos
 * proveedores de Internet dan una ciudad muy lejana. Aquí se escribe el
 * nombre de la ciudad con el teclado en pantalla, se busca con el
 * geocodificador de Open-Meteo (gratis, sin clave) y se elige de la lista.
 * La primera fila vuelve a la ubicación automática.
 */
#include "nav.h"
#include "comun.h"
#include "ajustes.h"
#include "tiempo.h"
#include "hal.h"
#include "idioma.h"

#include <ArduinoJson.h>
#include <lvgl.h>
#include <stdio.h>
#include <string.h>

#define ALTO_TECLADO 130
#define MAX_CIUDADES 8
#define ICONO_UBICACION "\xEF\x8F\x85"  /* f3c5, en nuestras fuentes */
#define ANCHO_TEXTO_FILA 230             /* px de texto que caben en una línea de la lista */

typedef struct {
    char nombre[40];   /* lo que se guarda y sale en el reposo: «Lima, Perú» */
    char fila[64];     /* lo que se ve en la lista, con la región para distinguir
                        * homónimos: «Lima, Provincia de Lima» */
    char corta[40];    /* solo la ciudad, si la fila no cabe en una línea */
    char pais[3];      /* código del país, a la derecha de la fila: «PE» */
    float latitud, longitud;
} Ciudad;

enum { BUSQUEDA_NADA, BUSQUEDA_EN_CURSO, BUSQUEDA_LISTA, BUSQUEDA_FALLO };

/* Lo comparten la interfaz y la tarea de fondo: la tarea solo escribe
 * mientras estado es BUSQUEDA_EN_CURSO, y la interfaz solo lee después */
static char url_busqueda[200];
static Ciudad ciudades[MAX_CIUDADES];
static int num_ciudades;
static volatile int estado = BUSQUEDA_NADA;

static lv_obj_t *pantalla_actual;
static lv_obj_t *campo;
static lv_obj_t *teclado;
static lv_obj_t *lista;
static lv_timer_t *vigilante;
static bool teclado_visible;
static bool con_actual;  /* la segunda fila es la ciudad elegida ahora */

/* Copia un texto de la respuesta pasándolo a lo que sabe pintar la fuente */
static void copiar_seguro(char *destino, size_t tam, const char *origen)
{
    comun_texto_seguro(origen ? origen : "", destino, tam);
}

/* Tarea de fondo: pregunta al geocodificador y rellena «ciudades» */
static void buscar_en_fondo(void)
{
    num_ciudades = 0;
    char *texto = hal_http_get(url_busqueda);
    if (texto == NULL) {
        estado = BUSQUEDA_FALLO;
        return;
    }
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, texto);
    free(texto);
    if (error) {
        estado = BUSQUEDA_FALLO;
        return;
    }
    for (JsonObjectConst r : doc["results"].as<JsonArrayConst>()) {
        if (num_ciudades >= MAX_CIUDADES) {
            break;
        }
        Ciudad *c = &ciudades[num_ciudades++];
        char nombre[80];
        const char *pais = r["country"] | "";
        snprintf(nombre, sizeof(nombre), pais[0] ? "%s, %s" : "%s", r["name"] | "?", pais);
        copiar_seguro(c->nombre, sizeof(c->nombre), nombre);
        /* Fila: ciudad y región (o país, si no hay región o se llama igual).
         * El texto largo se queda en la columna ancha; a la derecha solo va el
         * código del país, que no se parte. */
        const char *region = r["admin1"] | "";
        const char *ciudad = r["name"] | "?";
        char fila[96];
        if (region[0] != '\0' && strcmp(region, ciudad) != 0) {
            snprintf(fila, sizeof(fila), "%s, %s", ciudad, region);
        } else {
            snprintf(fila, sizeof(fila), "%s", nombre);
        }
        copiar_seguro(c->fila, sizeof(c->fila), fila);
        copiar_seguro(c->corta, sizeof(c->corta), ciudad);
        strncpy(c->pais, r["country_code"] | "", sizeof(c->pais) - 1);
        c->pais[sizeof(c->pais) - 1] = '\0';
        c->latitud = r["latitude"] | 0.0f;
        c->longitud = r["longitude"] | 0.0f;
    }
    estado = BUSQUEDA_LISTA;
}

static void ajustar_alto_lista(void)
{
    lv_obj_set_height(lista, PANTALLA_ALTO - ALTO_CABECERA - (teclado_visible ? ALTO_TECLADO : 0));
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

/* Guarda la elección, pide el tiempo del nuevo sitio y vuelve atrás */
static void elegir(const Ciudad *c)
{
    if (c == NULL) {
        ajustes.ubicacion_fija = false;
        ajustes.ciudad[0] = '\0';
    } else {
        ajustes.ubicacion_fija = true;
        strncpy(ajustes.ciudad, c->nombre, sizeof(ajustes.ciudad) - 1);
        ajustes.ciudad[sizeof(ajustes.ciudad) - 1] = '\0';
        ajustes.latitud = c->latitud;
        ajustes.longitud = c->longitud;
    }
    ajustes_guardar();
    tiempo_reubicar();
    printf("[ubicacion] %s\n", ajustes.ubicacion_fija ? ajustes.ciudad : "automática");
    nav_atras();
}

static void al_elegir_fila(uint16_t fila);

/* Primeras filas, siempre: «Automática» y, si la hay, la ciudad elegida */
static void nueva_lista(void)
{
    if (lista != NULL) {
        lv_obj_del(lista);
    }
    lista = comun_lista(pantalla_actual, al_elegir_fila);
    lv_obj_move_background(lista);  /* el teclado queda por encima */
    ajustar_alto_lista();

    comun_lista_agregar(lista, LV_SYMBOL_WIFI, TR("Automatic", "Automática"),
                        ajustes.ubicacion_fija ? NULL : LV_SYMBOL_OK, false);
    con_actual = ajustes.ubicacion_fija;
    if (con_actual) {
        comun_lista_agregar(lista, ICONO_UBICACION, ajustes.ciudad, LV_SYMBOL_OK, false);
    }
}

static void mostrar_resultados(void)
{
    nueva_lista();
    if (estado == BUSQUEDA_EN_CURSO) {
        comun_lista_agregar(lista, NULL, TR("Searching...", "Buscando..."), NULL, true);
    } else if (estado == BUSQUEDA_FALLO) {
        comun_lista_agregar(lista, NULL, TR("No connection, try again", "Sin conexión, prueba otra vez"), NULL, true);
    } else if (estado == BUSQUEDA_LISTA && num_ciudades == 0) {
        comun_lista_agregar(lista, NULL, TR("No cities found", "No se encontró la ciudad"), NULL, true);
    } else if (estado == BUSQUEDA_LISTA) {
        for (int i = 0; i < num_ciudades; i++) {
            /* Una línea por fila: si ciudad y región no caben, solo la ciudad */
            const char *texto = ciudades[i].fila;
            if (lv_txt_get_width(texto, strlen(texto), &radio_20, 0, LV_TEXT_FLAG_NONE) > ANCHO_TEXTO_FILA) {
                texto = ciudades[i].corta;
            }
            comun_lista_agregar(lista, ICONO_UBICACION, texto, ciudades[i].pais, false);
        }
    }
}

static void al_elegir_fila(uint16_t fila)
{
    if (fila == 0) {
        elegir(NULL);
        return;
    }
    fila--;
    if (con_actual) {
        if (fila == 0) {
            nav_atras();  /* la que ya estaba: nada que cambiar */
            return;
        }
        fila--;
    }
    if (estado == BUSQUEDA_LISTA && fila < num_ciudades) {
        elegir(&ciudades[fila]);
    }
}

/* Espera a que la tarea de fondo termine y pinta la lista */
static void vigilar(lv_timer_t *t)
{
    (void)t;
    if (estado != BUSQUEDA_EN_CURSO) {
        lv_timer_pause(vigilante);
        mostrar_resultados();
    }
}

/* ✓ en el teclado: buscar lo escrito */
static void buscar(void)
{
    if (estado == BUSQUEDA_EN_CURSO) {
        return;  /* ya hay una búsqueda en marcha */
    }
    /* El teclado solo escribe A-Z, 0-9 y espacios: basta cambiar los
     * espacios por %20 (y quitar los de los extremos) */
    const char *texto = lv_textarea_get_text(campo);
    char nombre[80];
    size_t n = 0;
    for (const char *p = texto; *p && n + 4 < sizeof(nombre); p++) {
        if (*p == ' ') {
            if (n > 0 && p[1] != '\0' && p[1] != ' ') {
                memcpy(nombre + n, "%20", 3);
                n += 3;
            }
        } else {
            nombre[n++] = *p;
        }
    }
    nombre[n] = '\0';
    if (n < 2) {
        return;  /* el geocodificador pide al menos dos letras */
    }
    snprintf(url_busqueda, sizeof(url_busqueda),
             "http://geocoding-api.open-meteo.com/v1/search?name=%s&count=%d&language=%s&format=json", nombre,
             MAX_CIUDADES, idioma_es ? "es" : "en");
    estado = BUSQUEDA_EN_CURSO;
    mostrar_teclado(false);
    mostrar_resultados();
    hal_segundo_plano(buscar_en_fondo);
    lv_timer_resume(vigilante);
}

static void al_cambiar_campo(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        mostrar_teclado(true);
    }
}

static void al_usar_teclado(lv_event_t *e)
{
    lv_event_code_t codigo = lv_event_get_code(e);
    if (codigo == LV_EVENT_READY) {
        buscar();
    } else if (codigo == LV_EVENT_CANCEL) {
        mostrar_teclado(false);
    }
}

static void al_borrar(lv_event_t *e)
{
    (void)e;
    if (vigilante != NULL) {
        lv_timer_del(vigilante);
        vigilante = NULL;
    }
    lista = NULL;  /* se borra con la pantalla */
}

lv_obj_t *pantalla_ubicacion(void)
{
    lv_obj_t *pantalla = comun_pantalla();
    pantalla_actual = pantalla;
    lista = NULL;
    lv_obj_add_event_cb(pantalla, al_borrar, LV_EVENT_DELETE, NULL);

    /* Cabecera con «Atrás» y el campo para escribir la ciudad */
    lv_obj_t *titulo;
    lv_obj_t *cabecera = comun_cabecera(pantalla, "", true, &titulo);
    lv_obj_add_flag(titulo, LV_OBJ_FLAG_HIDDEN);

    campo = lv_textarea_create(cabecera);
    lv_textarea_set_one_line(campo, true);
    lv_textarea_set_max_length(campo, 30);
    lv_textarea_set_placeholder_text(campo, TR(ICONO_UBICACION "  Type your city", ICONO_UBICACION "  Escribe tu ciudad"));
    lv_obj_set_size(campo, PANTALLA_ANCHO - 48 - 8, ALTO_CABECERA - 4);
    lv_obj_align(campo, LV_ALIGN_LEFT_MID, 48, 0);
    lv_obj_set_style_text_font(campo, &radio_14, 0);
    lv_obj_set_style_pad_ver(campo, 4, 0);
    lv_obj_set_style_border_width(campo, 0, 0);
    lv_obj_set_style_bg_color(campo, lv_color_hex(COLOR_LINEA), 0);
    lv_obj_add_event_cb(campo, al_cambiar_campo, LV_EVENT_ALL, NULL);

    teclado = comun_teclado(pantalla, campo, ALTO_TECLADO);
    lv_obj_add_event_cb(teclado, al_usar_teclado, LV_EVENT_ALL, NULL);

    vigilante = lv_timer_create(vigilar, 200, NULL);
    lv_timer_pause(vigilante);

    /* Si quedó una búsqueda en marcha de antes, se espera a que acabe;
     * si no, se empieza de cero con el teclado a la vista */
    if (estado == BUSQUEDA_EN_CURSO) {
        teclado_visible = false;
        lv_obj_add_flag(teclado, LV_OBJ_FLAG_HIDDEN);
        lv_timer_resume(vigilante);
    } else {
        estado = BUSQUEDA_NADA;
        teclado_visible = true;
    }
    mostrar_resultados();
    return pantalla;
}
