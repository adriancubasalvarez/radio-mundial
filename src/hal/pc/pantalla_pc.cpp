/**
 * Pantalla y táctil del simulador: una ventana SDL2 (Simple DirectMedia Layer)
 * donde el ratón hace de dedo.
 *
 * La ventana se dibuja ampliada (ZOOM) para verla cómoda en el monitor, pero
 * LVGL trabaja siempre a 320x240, igual que en la placa.
 */
#include "hal.h"
#include "guion_pc.h"

#include <SDL2/SDL.h>
#include <lvgl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Ampliación de la ventana: 2 -> 640x480 en el monitor */
static const int ZOOM = 2;

static SDL_Window *ventana;
static SDL_Renderer *renderizador;
static SDL_Texture *textura;

/* Copia completa de la pantalla en RGB565 que se sube a la textura */
static uint16_t marco[PANTALLA_ANCHO * PANTALLA_ALTO];

/* Estado del "dedo" (ratón) */
static int raton_x;
static int raton_y;
static bool raton_pulsado;

static uint32_t ultimo_tick;

/* LVGL nos entrega un trozo de pantalla ya dibujado */
static void volcar_pantalla(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color)
{
    for (int y = area->y1; y <= area->y2; y++) {
        for (int x = area->x1; x <= area->x2; x++) {
            marco[y * PANTALLA_ANCHO + x] = color->full;
            color++;
        }
    }

    /* Cuando llega el último trozo del fotograma, se muestra en la ventana */
    if (lv_disp_flush_is_last(drv)) {
        SDL_UpdateTexture(textura, NULL, marco, PANTALLA_ANCHO * sizeof(uint16_t));
        SDL_RenderClear(renderizador);
        SDL_RenderCopy(renderizador, textura, NULL, NULL);
        SDL_RenderPresent(renderizador);
    }
    lv_disp_flush_ready(drv);
}

/* LVGL pregunta dónde está el dedo */
static void leer_tactil(lv_indev_drv_t *drv, lv_indev_data_t *datos)
{
    (void)drv;
    datos->point.x = raton_x;
    datos->point.y = raton_y;
    datos->state = raton_pulsado ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

void hal_pantalla_init(void)
{
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "No se pudo iniciar SDL: %s\n", SDL_GetError());
        exit(1);
    }

    ventana = SDL_CreateWindow("Radio Mundial (simulador)",
                               SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               PANTALLA_ANCHO * ZOOM, PANTALLA_ALTO * ZOOM, 0);
    renderizador = SDL_CreateRenderer(ventana, -1, SDL_RENDERER_SOFTWARE);
    textura = SDL_CreateTexture(renderizador, SDL_PIXELFORMAT_RGB565,
                                SDL_TEXTUREACCESS_STREAMING,
                                PANTALLA_ANCHO, PANTALLA_ALTO);

    /* Búfer de dibujo de LVGL: 40 líneas, como en los ejemplos de Freenove */
    static lv_color_t bufer[PANTALLA_ANCHO * 40];
    static lv_disp_draw_buf_t bufer_dibujo;
    lv_disp_draw_buf_init(&bufer_dibujo, bufer, NULL, PANTALLA_ANCHO * 40);

    static lv_disp_drv_t drv_pantalla;
    lv_disp_drv_init(&drv_pantalla);
    drv_pantalla.hor_res = PANTALLA_ANCHO;
    drv_pantalla.ver_res = PANTALLA_ALTO;
    drv_pantalla.flush_cb = volcar_pantalla;
    drv_pantalla.draw_buf = &bufer_dibujo;
    lv_disp_drv_register(&drv_pantalla);

    static lv_indev_drv_t drv_tactil;
    lv_indev_drv_init(&drv_tactil);
    drv_tactil.type = LV_INDEV_TYPE_POINTER;
    drv_tactil.read_cb = leer_tactil;
    lv_indev_drv_register(&drv_tactil);

    ultimo_tick = SDL_GetTicks();
    guion_iniciar();
}

/* Guarda la pantalla actual (marco RGB565) como BMP de 24 bits */
static void guardar_bmp(const char *ruta)
{
    FILE *f = fopen(ruta, "wb");
    if (f == NULL) {
        fprintf(stderr, "No se puede escribir %s\n", ruta);
        return;
    }
    const uint32_t fila = PANTALLA_ANCHO * 3; /* 960, ya múltiplo de 4 */
    const uint32_t datos = fila * PANTALLA_ALTO;
    uint8_t cab[54] = {'B', 'M'};
    uint32_t total = 54 + datos;
    memcpy(cab + 2, &total, 4);
    cab[10] = 54;
    cab[14] = 40;
    int32_t ancho = PANTALLA_ANCHO, alto = PANTALLA_ALTO;
    memcpy(cab + 18, &ancho, 4);
    memcpy(cab + 22, &alto, 4);
    cab[26] = 1;
    cab[28] = 24;
    memcpy(cab + 34, &datos, 4);
    fwrite(cab, 1, sizeof(cab), f);

    /* BMP guarda las filas de abajo arriba, en orden azul, verde, rojo */
    uint8_t linea[PANTALLA_ANCHO * 3];
    for (int y = PANTALLA_ALTO - 1; y >= 0; y--) {
        for (int x = 0; x < PANTALLA_ANCHO; x++) {
            uint16_t c = marco[y * PANTALLA_ANCHO + x];
            linea[x * 3 + 0] = (uint8_t)((c & 0x1F) << 3);
            linea[x * 3 + 1] = (uint8_t)(((c >> 5) & 0x3F) << 2);
            linea[x * 3 + 2] = (uint8_t)(((c >> 11) & 0x1F) << 3);
        }
        fwrite(linea, 1, sizeof(linea), f);
    }
    fclose(f);
}

bool hal_atender(void)
{
    SDL_Event evento;
    while (SDL_PollEvent(&evento)) {
        switch (evento.type) {
        case SDL_QUIT:
            return false;
        case SDL_MOUSEBUTTONDOWN:
            if (evento.button.button == SDL_BUTTON_LEFT) {
                raton_pulsado = true;
            }
            break;
        case SDL_MOUSEBUTTONUP:
            if (evento.button.button == SDL_BUTTON_LEFT) {
                raton_pulsado = false;
            }
            break;
        default:
            break;
        }
    }

    uint32_t ahora = SDL_GetTicks();

    /* El guion de pruebas, si lo hay, manda sobre el ratón */
    int x, y;
    bool pulsado, salir = false;
    if (guion_atender(ahora, &x, &y, &pulsado, &salir)) {
        raton_x = x;
        raton_y = y;
        raton_pulsado = pulsado;
    } else {
        /* Posición del ratón pasada a coordenadas de la pantalla real */
        SDL_GetMouseState(&x, &y);
        raton_x = SDL_clamp(x / ZOOM, 0, PANTALLA_ANCHO - 1);
        raton_y = SDL_clamp(y / ZOOM, 0, PANTALLA_ALTO - 1);
    }
    if (salir) {
        return false;
    }

    const char *captura = guion_captura_pendiente();
    if (captura != NULL) {
        guardar_bmp(captura);
        guion_captura_hecha();
    }

    /* Reloj de LVGL */
    lv_tick_inc(ahora - ultimo_tick);
    ultimo_tick = ahora;
    return true;
}

uint32_t hal_millis(void)
{
    return SDL_GetTicks();
}

void hal_esperar(uint32_t ms)
{
    SDL_Delay(ms);
}
