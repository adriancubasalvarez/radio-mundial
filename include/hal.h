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

/* Brillo de la pantalla de 0 a 100 */
void hal_brillo(uint8_t porcentaje);

/* Hora local. Devuelve false si aún no se sabe (la placa sin Internet). */
bool hal_hora(int *hora, int *minuto);

/* Número aleatorio de 32 bits */
uint32_t hal_aleatorio(void);

/* ---------- Ajustes persistentes ---------- */

/* Lee los ajustes guardados (texto JSON). NULL si no hay. Liberar con free(). */
char *hal_ajustes_leer(void);

/* Guarda los ajustes (texto JSON) */
bool hal_ajustes_guardar(const char *json);

/* ---------- Archivos de datos ----------
 * Las rutas son relativas a la carpeta de datos (p. ej. "mapas/mundo.bin").
 * Si hay tarjeta SD y el archivo existe en su carpeta radio/, se usa el de
 * la SD (así una base de emisoras más grande en la SD manda); si no, el de
 * la memoria interna (data/ en el PC, LittleFS en la placa). */

/* Lee un archivo entero a memoria. NULL si no existe. Liberar con free(). */
uint8_t *hal_leer_archivo(const char *ruta, size_t *tam);

/* Lectura línea a línea, para archivos grandes como buscar.txt */
typedef struct HalArchivo HalArchivo;
HalArchivo *hal_archivo_abrir(const char *ruta);
/* Lee la siguiente línea sin el salto final; false al llegar al final */
bool hal_archivo_linea(HalArchivo *archivo, char *linea, size_t tam);
void hal_archivo_cerrar(HalArchivo *archivo);

/* ---------- Tarjeta SD (opcional) ----------
 * Rutas relativas a la raíz de la tarjeta. */

/* true si hay tarjeta SD montada */
bool hal_sd_presente(void);

/* Lee un archivo de la SD. NULL si no hay tarjeta o no existe. Liberar con free(). */
uint8_t *hal_sd_leer(const char *ruta, size_t *tam);

/* Escribe (sustituye) un archivo de texto en la SD */
bool hal_sd_escribir(const char *ruta, const char *texto);

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

/* Título de lo que suena (metadatos ICY), en UTF-8; "" si la emisora no
 * lo envía o aún no ha llegado */
const char *audio_titulo(void);
