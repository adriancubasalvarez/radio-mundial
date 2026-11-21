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

/* Fecha local: año, mes (1..12), día (1..31) y día de la semana (0 domingo).
 * false si aún no se sabe. */
bool hal_fecha(int *anio, int *mes, int *dia, int *dia_semana);

/* Diferencia con la hora universal en segundos (p. ej. 3600 en Madrid en
 * invierno): la radio la saca de la previsión del tiempo de su ubicación.
 * En el simulador no hace nada (usa la hora del PC). */
void hal_zona_horaria(int segundos);

/* Hora en otra zona (regla POSIX, como las de zonas.h). false si aún no se
 * sabe la hora o en el simulador. */
bool hal_hora_en(const char *regla, int *hora, int *minuto);

/* Actualización de las emisoras por Internet, en segundo plano */
typedef enum {
    ACT_NADA,
    ACT_COMPROBANDO,
    ACT_AL_DIA,
    ACT_HAY_NUEVA,
    ACT_DESCARGANDO,
    ACT_INSTALANDO,
    ACT_LISTO,
    ACT_ERROR,
} EstadoActualizar;

/* Mira si hay una lista distinta de la instalada */
void hal_actualizar_comprobar(void);

/* Descarga e instala la lista que encontró hal_actualizar_comprobar */
void hal_actualizar_instalar(void);

/* Estado, y progreso de 0 a 100 mientras descarga o instala */
EstadoActualizar hal_actualizar_estado(uint8_t *progreso);

/* Fecha de la lista encontrada */
const char *hal_actualizar_fecha(void);

/* Reinicia la radio */
void hal_reiniciar(void);

/* Número aleatorio de 32 bits */
uint32_t hal_aleatorio(void);

/* Apaga la radio: pantalla y audio fuera y a dormir hasta que se toque la
 * pantalla o pasen «segundos» (0 = sin límite; es para las alarmas). En la
 * placa no vuelve: al despertar arranca de nuevo y suena la última emisora.
 * En el simulador cierra la ventana. */
void hal_apagar(uint32_t segundos);

/* true si esta vez ha despertado el temporizador de una alarma, no un toque */
bool hal_despertado_por_alarma(void);

/* Reinicios inesperados (fallo del programa, perro guardián o bajada de
 * tensión), guardados en la memoria permanente para saber si la radio se
 * reinicia sola y por qué. minuto_del_dia: hora aproximada del último
 * (hora * 60 + minuto), o -1 si no se sabía la hora. En el simulador, 0. */
typedef struct {
    uint16_t veces;
    char motivo[12];
    int minuto_del_dia;
} Reinicios;

void hal_reinicios(Reinicios *r);

/* Pone a cero el registro de reinicios inesperados */
void hal_reinicios_borrar(void);

/* LED RGB de la placa, 0..255 por color (en placas sin LED no hace nada) */
void hal_led(uint8_t r, uint8_t g, uint8_t b);

/* ---------- Red WiFi ----------
 * Cada usuario conecta la radio a su WiFi de dos formas:
 *   - al instalarla desde la web (ESP Web Tools manda la red por USB, Improv);
 *   - con el portal: la radio crea su propia red, el móvil se conecta a ella
 *     y en la página que se abre se elige la WiFi de casa. */

typedef enum {
    RED_CONECTANDO,    /* hay una WiFi guardada y se está conectando */
    RED_CONECTADA,
    RED_PORTAL,        /* portal de configuración abierto */
} EstadoRed;

EstadoRed hal_red_estado(void);

/* Intensidad de la señal WiFi en dBm (p. ej. -60: buena; -80: mala), o 0 si
 * no hay conexión. En el simulador, -50 o lo que diga la variable de entorno
 * RADIO_WIFI (0: sin conexión). */
int hal_red_senal(void);

/* Veces que se ha perdido la WiFi desde que se encendió la radio */
uint16_t hal_red_cortes(void);

/* Pone a cero la cuenta de cortes */
void hal_red_cortes_borrar(void);

/* Nombre de la red que crea la radio cuando el portal está abierto */
const char *hal_red_portal(void);

/* Abre el portal para elegir otra WiFi */
void hal_red_configurar(void);

/* Descarga una URL http:// (sin cifrado) y devuelve el cuerpo terminado en
 * cero, o NULL si falla. Bloquea unos segundos: llamarla solo desde una
 * tarea en segundo plano. Liberar con free(). */
char *hal_http_get(const char *url);

/* Ejecuta tarea() una vez en segundo plano, sin parar la pantalla */
void hal_segundo_plano(void (*tarea)(void));

/* Cierra el portal sin cambiar nada (vuelve a la WiFi guardada) */
void hal_red_cancelar(void);

/* ---------- Ajustes persistentes ---------- */

/* Lee los ajustes guardados (texto JSON). NULL si no hay. Liberar con free(). */
char *hal_ajustes_leer(void);

/* Guarda los ajustes (texto JSON) */
bool hal_ajustes_guardar(const char *json);

/* Emisoras propias («Mis emisoras», texto JSON). Van aparte de los ajustes
 * (en la placa, en su propia partición «usuario»): las escribe la página de
 * instalación por el USB y no se borran al reinstalar. NULL si no hay;
 * liberar con free(). */
char *hal_mis_emisoras_leer(void);
bool hal_mis_emisoras_guardar(const char *json);

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
/* Lee hasta n bytes a continuación de lo ya leído; devuelve cuántos */
size_t hal_archivo_leer(HalArchivo *archivo, void *destino, size_t n);
/* Lee n bytes desde la posición pos (acceso directo, para los mapas) */
bool hal_archivo_leer_en(HalArchivo *archivo, uint32_t pos, void *destino, size_t n);
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

/* Repite sin parar el tono de emergencia de las alarmas (alarma.mp3 de la
 * memoria interna: suena sin Internet) */
void audio_preparar(void);  /* crea la salida de audio sin sonar */
void audio_tono(void);

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

/* Elige por dónde sale el sonido: false = altavoz integrado, true = DAC
 * externo por I2S. Si estaba sonando una emisora, vuelve a sonar por la
 * nueva salida. En el simulador no hace nada. */
void audio_salida(bool externa);

/* Nivel de la música desde la última llamada (pico), de 0 a 255 */
uint8_t audio_nivel(void);

/* Título de lo que suena (metadatos ICY), en UTF-8; "" si la emisora no
 * lo envía o aún no ha llegado */
const char *audio_titulo(void);

/* Enum to track the current state for hysteresis */
enum BatteryLevel {
    BAT_LEVEL_EMPTY,
    BAT_LEVEL_1,
    BAT_LEVEL_2,
    BAT_LEVEL_3,
    BAT_LEVEL_FULL
};

/* Get battery level with hysteresis */
BatteryLevel getBatteryLevel();

/* true si hay una batería conectada (interruptor encendido). Se sabe por el
 * rizado de la tensión en la entrada de batería: sin batería, el cargador deja
 * un rizado grande (~17 mV); con batería, casi nada (~2 mV). En una radio
 * que nunca ha tenido batería devuelve siempre false. Tarda ~40 ms.
 * En el simulador, true salvo con RADIO_BATERIA=x (interruptor apagado). */
bool hal_bateria_conectada(void);
