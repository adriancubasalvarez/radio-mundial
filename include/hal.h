/**
 * HAL (Hardware Abstraction Layer): lo que cada plataforma (PC o placa)
 * tiene que ofrecer para que la aplicación funcione igual en las dos.
 *
 * Implementaciones:
 *   src/hal/pc/     -> ventana SDL2 + ratón + ffplay
 *   src/hal/esp32/  -> pantalla ILI9341 + táctil FT6336U + ES8311 (Fase 1)
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

/* Resolución lógica de la pantalla, en horizontal */
#define PANTALLA_ANCHO 320
#define PANTALLA_ALTO  240

/* ---------- Pantalla y táctil ---------- */

/* Inicializa pantalla y táctil y los registra en LVGL (llamar tras lv_init) */
void hal_pantalla_init(void);

/* Atiende la plataforma (eventos, reloj de LVGL...).
 * Devuelve false si el usuario ha pedido salir (cerrar la ventana en el PC). */
bool hal_atender(void);

/* Milisegundos desde el arranque */
uint32_t hal_millis(void);

/* Pausa en milisegundos */
void hal_esperar(uint32_t ms);

/* ---------- Archivos (carpeta data/) ---------- */

/* Lee un archivo entero de data/ (p. ej. "mapas/mundo.bin") a memoria.
 * Devuelve NULL si no existe. El llamante libera el búfer con free(). */
uint8_t *hal_leer_archivo(const char *ruta, size_t *tam);

/* ---------- Audio ---------- */

/* Empieza a reproducir una emisora; si ya sonaba otra, la para antes */
void audio_reproducir(const char *url);

/* Para la reproducción */
void audio_parar(void);

typedef enum {
    AUDIO_PARADO,      /* no se ha pedido nada, o se ha parado */
    AUDIO_CONECTANDO,  /* se ha pedido una emisora y aún no suena */
    AUDIO_SONANDO,
    AUDIO_ERROR,       /* la emisora no arrancó o se cortó */
} EstadoAudio;

/* Estado de la reproducción */
EstadoAudio audio_estado(void);

/* Volumen de 0 a 100 */
void audio_volumen(uint8_t volumen);
