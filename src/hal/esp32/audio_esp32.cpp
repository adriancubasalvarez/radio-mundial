/**
 * Audio en la placa con la librería ESP32-audioI2S (schreibfaul1).
 *
 * CYD Freenove FNK0114F: sale por el DAC interno del ESP32 en el GPIO26
 * (canal «izquierdo» del DAC; el GPIO25, el otro canal, es del táctil) hacia
 * el amplificador de la placa, que se habilita con AUDIO_EN (GPIO4) a nivel
 * bajo. El DAC es de 8 bits: la calidad es modesta, sirve para probar.
 *
 * La librería necesita que se llame a audio.loop() muy a menudo, así que
 * vive en una tarea propia en el núcleo 0 (el de la WiFi); la interfaz
 * corre en el núcleo 1. Las órdenes de la interfaz se pasan a la tarea con
 * un cerrojo (mutex).
 */
#include "hal.h"
#include "placa.h"

#include <Arduino.h>
#include <Audio.h>
#include <WiFi.h>

/* Si no empieza a sonar en este tiempo, se da por fallida */
#define TIEMPO_MAX_CONEXION 12000

/* Tiempo máximo esperando a que conecte la WiFi antes de dar error */
#define TIEMPO_ESPERA_WIFI 60000

static bool esperando_wifi = false;

/* Memoria interna mínima para crear el audio (búferes y decodificador) */
#define MEMORIA_MINIMA_AUDIO (60 * 1024)

static Audio *audio;
static SemaphoreHandle_t cerrojo;

/* Órdenes pendientes de la interfaz (protegidas por el cerrojo) */
static char url_pedida[256];
static bool hay_url_pedida = false;
static bool parar_pedido = false;
static int volumen_pedido = -1;

/* Estado (lo escribe la tarea de audio y lo lee la interfaz) */
static volatile EstadoAudio estado = AUDIO_PARADO;
static volatile bool sonido_detectado = false;
static volatile bool fin_de_stream = false;
static uint32_t inicio_conexion;
static char titulo[160];

/* El cerrojo se crea en la primera llamada: la interfaz puede pedir cosas
 * (p. ej. el volumen guardado) antes de que arranque la tarea de audio, y
 * esas órdenes quedan pendientes hasta entonces. */
static void preparar_cerrojo(void)
{
    if (cerrojo == NULL) {
        cerrojo = xSemaphoreCreateMutex();
    }
}

/* Volumen 0..100 de la interfaz -> 0..21 de la librería */
static uint8_t a_volumen_libreria(uint8_t v)
{
    return (uint8_t)((v > 100 ? 100 : v) * 21 / 100);
}

static void amplificador(bool encendido)
{
    digitalWrite(PIN_AUDIO_EN, encendido ? LOW : HIGH);  // activo a nivel bajo
}

/* ---------- Avisos de la librería ---------- */

void audio_info(const char *info)
{
    Serial.printf("[audio] %s\n", info);
    /* Al encontrar la primera trama MP3/AAC ya hay sonido */
    if (strstr(info, "syncword found") || strstr(info, "stream ready")) {
        sonido_detectado = true;
    }
}

void audio_showstreamtitle(const char *texto)
{
    preparar_cerrojo();
    xSemaphoreTake(cerrojo, portMAX_DELAY);
    strncpy(titulo, texto, sizeof(titulo) - 1);
    titulo[sizeof(titulo) - 1] = '\0';
    xSemaphoreGive(cerrojo);
}

void audio_eof_stream(const char *info)
{
    (void)info;
    fin_de_stream = true;
}

/* ---------- Tarea de audio (núcleo 0) ---------- */

static uint8_t volumen_actual = 70;

/* La librería se crea al pedir la primera emisora, no al arrancar: durante
 * el arranque la pantalla de bienvenida (logo) ocupa memoria que luego se
 * libera. Reserva 32 KB de búferes DMA para el I2S y, al conectar, unos
 * 30 KB más para el decodificador; si no hay memoria, se avisa en vez de
 * colgarse. */
static bool crear_libreria(void)
{
    if (audio != NULL) {
        return true;
    }
    size_t dma_libre = heap_caps_get_free_size(MALLOC_CAP_DMA);
    if (dma_libre < MEMORIA_MINIMA_AUDIO) {
        Serial.printf("[audio] NO HAY MEMORIA: quedan %u bytes de memoria DMA (hacen falta %u)\n",
                      (unsigned)dma_libre, (unsigned)MEMORIA_MINIMA_AUDIO);
        return false;
    }
    /* true: DAC interno; I2S_DAC_CHANNEL_LEFT_EN: solo el GPIO26 */
    audio = new Audio(true, I2S_DAC_CHANNEL_LEFT_EN);
    audio->setVolume(a_volumen_libreria(volumen_actual));
    audio->setConnectionTimeout(5000, 8000);
    Serial.printf("[audio] librería creada; memoria DMA libre %u\n",
                  (unsigned)heap_caps_get_free_size(MALLOC_CAP_DMA));
    return true;
}

static void tarea_audio(void *parametro)
{
    (void)parametro;
    char url[256];
    for (;;) {
        bool conectar = false, parar = false;
        int volumen = -1;
        xSemaphoreTake(cerrojo, portMAX_DELAY);
        if (hay_url_pedida) {
            strcpy(url, url_pedida);
            hay_url_pedida = false;
            conectar = true;
        }
        parar = parar_pedido;
        parar_pedido = false;
        volumen = volumen_pedido;
        volumen_pedido = -1;
        xSemaphoreGive(cerrojo);

        if (volumen >= 0) {
            volumen_actual = volumen;
            if (audio != NULL) {
                audio->setVolume(a_volumen_libreria(volumen));
            }
        }
        if (parar && audio != NULL) {
            audio->stopSong();
            amplificador(false);
            estado = AUDIO_PARADO;
        }
        if (conectar && !crear_libreria()) {
            estado = AUDIO_ERROR;  /* no hay memoria para el audio */
            conectar = false;
        }
        if (conectar) {
            audio->stopSong();
            sonido_detectado = false;
            fin_de_stream = false;
            titulo[0] = '\0';
            inicio_conexion = millis();
            estado = AUDIO_CONECTANDO;
            if (WiFi.status() != WL_CONNECTED) {
                /* Aún no hay WiFi (p. ej. al reanudar justo tras encender):
                 * se espera a que conecte en lugar de dar error */
                Serial.println("[audio] esperando a la WiFi...");
                esperando_wifi = true;
            } else if (audio->connecttohost(url)) {
                amplificador(true);
            } else {
                estado = AUDIO_ERROR;
            }
        }

        if (esperando_wifi) {
            if (estado != AUDIO_CONECTANDO) {
                esperando_wifi = false;  /* se paró o se pidió otra cosa */
            } else if (WiFi.status() == WL_CONNECTED) {
                esperando_wifi = false;
                inicio_conexion = millis();
                if (audio->connecttohost(url)) {
                    amplificador(true);
                } else {
                    estado = AUDIO_ERROR;
                }
            } else if (millis() - inicio_conexion > TIEMPO_ESPERA_WIFI) {
                esperando_wifi = false;
                estado = AUDIO_ERROR;
            }
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        if (estado == AUDIO_CONECTANDO || estado == AUDIO_SONANDO) {
            audio->loop();
            if (fin_de_stream || (!audio->isRunning() && millis() - inicio_conexion > 2000)) {
                estado = AUDIO_ERROR;  // se cortó o no llegó a arrancar
                amplificador(false);
            } else if (estado == AUDIO_CONECTANDO && sonido_detectado) {
                estado = AUDIO_SONANDO;
            } else if (estado == AUDIO_CONECTANDO && millis() - inicio_conexion > TIEMPO_MAX_CONEXION) {
                audio->stopSong();
                estado = AUDIO_ERROR;
                amplificador(false);
            }
            vTaskDelay(1);
        } else {
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
}

/* Crea la librería y la tarea. Lo llama main_esp32.cpp. */
void audio_iniciar(void)
{
    pinMode(PIN_AUDIO_EN, OUTPUT);
    amplificador(false);
    preparar_cerrojo();
    xTaskCreatePinnedToCore(tarea_audio, "audio", 8192, NULL, 2, NULL, 0);
}

/* ---------- HAL (llamadas desde la interfaz, núcleo 1) ---------- */

void audio_reproducir(const char *url)
{
    preparar_cerrojo();
    xSemaphoreTake(cerrojo, portMAX_DELAY);
    strncpy(url_pedida, url, sizeof(url_pedida) - 1);
    url_pedida[sizeof(url_pedida) - 1] = '\0';
    hay_url_pedida = true;
    parar_pedido = false;
    xSemaphoreGive(cerrojo);
    estado = AUDIO_CONECTANDO;  // la interfaz lo ve al instante
    inicio_conexion = millis();
}

void audio_parar(void)
{
    preparar_cerrojo();
    xSemaphoreTake(cerrojo, portMAX_DELAY);
    parar_pedido = true;
    hay_url_pedida = false;
    xSemaphoreGive(cerrojo);
    estado = AUDIO_PARADO;
}

EstadoAudio audio_estado(void)
{
    return estado;
}

void audio_volumen(uint8_t volumen)
{
    preparar_cerrojo();
    xSemaphoreTake(cerrojo, portMAX_DELAY);
    volumen_pedido = volumen;
    xSemaphoreGive(cerrojo);
}

const char *audio_titulo(void)
{
    static char copia[160];
    preparar_cerrojo();
    xSemaphoreTake(cerrojo, portMAX_DELAY);
    strcpy(copia, estado == AUDIO_SONANDO ? titulo : "");
    xSemaphoreGive(cerrojo);
    return copia;
}
