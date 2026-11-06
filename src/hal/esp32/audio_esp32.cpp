/**
 * Audio en la placa con la librería ESP32-audioI2S (schreibfaul1).
 *
 * FNK0104: sale por I2S hacia el códec ES8311 (24 bits), configurado por
 * I2C, y de ahí al amplificador (AUDIO_EN = GPIO1, a nivel bajo). El volumen
 * lo baja el códec, no la librería: así no se pierde calidad al bajarlo.
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
#include <LittleFS.h>
#include <WiFi.h>

#include "es8311.h"

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
static bool tono_pedido = false;
static bool preparar_pedido = false;
static int volumen_pedido = -1;

/* Estado (lo escribe la tarea de audio y lo lee la interfaz) */
static volatile EstadoAudio estado = AUDIO_PARADO;
static volatile bool sonido_detectado = false;
static volatile bool fin_de_stream = false;
static bool en_tono = false;  /* suena el tono de emergencia (solo en la tarea) */
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

/* ---------- Salida de audio de cada placa ---------- */


static es8311_handle_t codec;
static uint32_t frecuencia_codec;  /* frecuencia de muestreo configurada */

/* El códec en esclavo: recibe del ESP32 el reloj maestro (MCLK, 256 veces la
 * frecuencia de muestreo) y los datos por I2S. El bus I2C ya lo ha abierto
 * el táctil (Wire, puerto 0). */
static void preparar_salida(void)
{
    codec = es8311_create(I2C_NUM_0, ES8311_ADDRESS_0);
    const es8311_clock_config_t reloj = {
        .mclk_inverted = false,
        .sclk_inverted = false,
        .mclk_from_mclk_pin = true,
        .mclk_frequency = 44100 * 256,
        .sample_frequency = 44100,
    };
    if (codec == NULL || es8311_init(codec, &reloj, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16) != ESP_OK) {
        Serial.println("[audio] el códec ES8311 no responde");
        return;
    }
    frecuencia_codec = 44100;
    Serial.println("[audio] códec ES8311 listo");
}

/* Cada emisora puede venir a otra frecuencia (22,05, 44,1, 48 kHz...): los
 * divisores del códec se ajustan a la que está usando la librería */
static void seguir_frecuencia(Audio *a)
{
    uint32_t fs = a->getSampleRate();
    if (codec != NULL && fs >= 8000 && fs != frecuencia_codec) {
        if (es8311_sample_frequency_config(codec, fs * 256, fs) == ESP_OK) {
            Serial.printf("[audio] códec a %u Hz\n", (unsigned)fs);
        }
        frecuencia_codec = fs;
    }
}

static Audio *nueva_libreria(void)
{
    Audio *a = new Audio(false);  // I2S hacia un DAC externo
    a->setPinout(PIN_I2S_BCK, PIN_I2S_WS, PIN_I2S_DOUT, I2S_PIN_NO_CHANGE, PIN_I2S_MCK);
    a->setVolume(21);  // la librería al máximo: el volumen lo pone el códec
    return a;
}

/* Volumen 0..100 de la interfaz al códec: de -44,5 dB a 0 dB en pasos de
 * medio decibelio (el oído percibe el volumen en decibelios); 0 = silencio.
 * En el controlador, 75 son 0 dB (por encima satura). */
static void poner_volumen(Audio *a, uint8_t v)
{
    (void)a;
    if (codec != NULL) {
        es8311_voice_volume_set(codec, v == 0 ? 0 : 40 + v * 35 / 100, NULL);
    }
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

/* Cada muestra pasa por aquí justo antes de salir al DAC: se guarda el pico
 * para la luz «al ritmo». Dos canales de 16 bits en un uint32_t. */
static volatile uint16_t pico;

void audio_process_i2s(uint32_t *muestra, bool *seguir)
{
    *seguir = true;
    int16_t izq = (int16_t)(*muestra & 0xFFFF);
    int16_t der = (int16_t)(*muestra >> 16);
    uint16_t m = (uint16_t)max(abs(izq), abs(der));
    if (m > pico) {
        pico = m;
    }
}

uint8_t audio_nivel(void)
{
    uint16_t p = pico;
    pico = 0;
    return estado == AUDIO_SONANDO ? (uint8_t)(p >> 7) : 0;
}

/* ---------- Tarea de audio (núcleo 0) ---------- */

static uint8_t volumen_actual = 70;

/* La librería se crea al terminar el logo o al pedir la primera emisora, no
 * al arrancar: durante el arranque la pantalla de bienvenida ocupa memoria
 * que luego se libera. Tampoco debe esperar más: si se creaba minutos
 * después, al saltar una alarma, la emisora conectaba pero no se oía.
 * Reserva 32 KB de búferes DMA para el I2S y, al conectar, unos
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
    audio = nueva_libreria();
    /* El códec se configura con el reloj (MCLK) ya en marcha, como en el
     * ejemplo de Freenove. Configurado sin reloj sonaba solo si la música
     * empezaba enseguida: tras esperar a oscuras una alarma, no sonaba. */
    preparar_salida();
    poner_volumen(audio, volumen_actual);
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
        bool conectar = false, parar = false, tono = false;
        int volumen = -1;
        xSemaphoreTake(cerrojo, portMAX_DELAY);
        if (hay_url_pedida) {
            strcpy(url, url_pedida);
            hay_url_pedida = false;
            conectar = true;
        }
        parar = parar_pedido;
        parar_pedido = false;
        tono = tono_pedido;
        tono_pedido = false;
        volumen = volumen_pedido;
        volumen_pedido = -1;
        bool preparar = preparar_pedido;
        preparar_pedido = false;
        xSemaphoreGive(cerrojo);

        if (preparar) {
            crear_libreria();
        }
        if (volumen >= 0) {
            volumen_actual = volumen;
            if (audio != NULL) {
                poner_volumen(audio, volumen);
            }
        }
        if (parar && audio != NULL) {
            audio->stopSong();
            amplificador(false);
            estado = AUDIO_PARADO;
            en_tono = false;
        }
        if (tono && !crear_libreria()) {
            estado = AUDIO_ERROR;
            tono = false;
        }
        if (tono) {
            en_tono = true;
            esperando_wifi = false;
            fin_de_stream = false;
            sonido_detectado = false;
            amplificador(true);
            estado = audio->connecttoFS(LittleFS, "/alarma.mp3") ? AUDIO_SONANDO : AUDIO_ERROR;
        }
        if (conectar && !crear_libreria()) {
            estado = AUDIO_ERROR;  /* no hay memoria para el audio */
            conectar = false;
        }
        if (conectar) {
            en_tono = false;
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

        if (en_tono && estado == AUDIO_SONANDO) {
            /* cuando acaba el archivo, otra vez desde el principio */
            audio->loop();
            seguir_frecuencia(audio);
            if (!audio->isRunning()) {
                audio->connecttoFS(LittleFS, "/alarma.mp3");
            }
            vTaskDelay(1);
        } else if (estado == AUDIO_CONECTANDO || estado == AUDIO_SONANDO) {
            audio->loop();
            seguir_frecuencia(audio);
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

void audio_preparar(void)
{
    preparar_cerrojo();
    xSemaphoreTake(cerrojo, portMAX_DELAY);
    preparar_pedido = true;
    xSemaphoreGive(cerrojo);
}

void audio_tono(void)
{
    preparar_cerrojo();
    xSemaphoreTake(cerrojo, portMAX_DELAY);
    tono_pedido = true;
    hay_url_pedida = false;
    parar_pedido = false;
    xSemaphoreGive(cerrojo);
}

void audio_parar(void)
{
    preparar_cerrojo();
    xSemaphoreTake(cerrojo, portMAX_DELAY);
    parar_pedido = true;
    hay_url_pedida = false;
    tono_pedido = false;
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

