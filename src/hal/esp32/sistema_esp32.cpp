/**
 * Utilidades del sistema en la placa: hora por NTP (Network Time Protocol,
 * se configura al iniciar la red) y números aleatorios.
 */
#include "hal.h"

#include <Arduino.h>
#include <esp_random.h>
#include <time.h>

bool hal_hora(int *hora, int *minuto)
{
    struct tm local;
    if (!getLocalTime(&local, 0)) {
        return false;  // aún no ha llegado la hora por NTP
    }
    *hora = local.tm_hour;
    *minuto = local.tm_min;
    return true;
}

bool hal_fecha(int *anio, int *mes, int *dia, int *dia_semana)
{
    struct tm local;
    if (!getLocalTime(&local, 0)) {
        return false;
    }
    *anio = local.tm_year + 1900;
    *mes = local.tm_mon + 1;
    *dia = local.tm_mday;
    *dia_semana = local.tm_wday;
    return true;
}

/* La biblioteca de C solo calcula la hora de una zona, la de la variable TZ:
 * se cambia un momento y se deja como estaba */
bool hal_hora_en(const char *regla, int *hora, int *minuto)
{
    struct tm local;
    if (!getLocalTime(&local, 0)) {
        return false;
    }
    char antes[32] = "";
    const char *zona = getenv("TZ");
    if (zona != NULL) {
        strncpy(antes, zona, sizeof(antes) - 1);
    }
    time_t ahora = time(NULL);
    setenv("TZ", regla, 1);
    tzset();
    localtime_r(&ahora, &local);
    setenv("TZ", antes, 1);
    tzset();
    *hora = local.tm_hour;
    *minuto = local.tm_min;
    return true;
}

/* La zona horaria en formato POSIX lleva el signo al revés: UTC+1 es
 * «UTC-1». Con los minutos para zonas como la India (UTC+5:30). */
void hal_zona_horaria(int segundos)
{
    static int actual = 1 << 30;
    if (segundos == actual) {
        return;
    }
    actual = segundos;
    int minutos = abs(segundos) / 60;
    char zona[24];
    snprintf(zona, sizeof(zona), "UTC%c%d:%02d", segundos > 0 ? '-' : '+', minutos / 60, minutos % 60);
    setenv("TZ", zona, 1);
    tzset();
    Serial.printf("Zona horaria: %s\n", zona);
}

/* Protección contra un fallo del núcleo de Arduino: stop_ssl_socket() borra
 * con memset su estructura y deja el socket en 0 (no en -1). La siguiente
 * vez, WiFiClientSecure::stop() hace close(0)... y el descriptor 0 no es un
 * socket, sino el primer archivo abierto de LittleFS. La librería de audio
 * llama a stop() en cada cambio de emisora: la tarea de audio cerraba el
 * archivo que estuviera leyendo la pantalla (o el mapa abierto) y la radio
 * se reiniciaba. Con -Wl,--wrap=close (platformio.ini), los close() pasan
 * por aquí; fclose() no usa close(), así que los archivos se cierran bien. */
extern "C" int __real_close(int fd);

extern "C" int __wrap_close(int fd)
{
    if (fd == 0) {
        return -1;
    }
    return __real_close(fd);
}

static void ejecutar_tarea(void *parametro)
{
    ((void (*)(void))parametro)();
    vTaskDelete(NULL);
}

/* En el núcleo 0, con poca prioridad: no estorba a la pantalla (núcleo 1)
 * ni al audio (núcleo 0, prioridad 2) */
void hal_segundo_plano(void (*tarea)(void))
{
    xTaskCreatePinnedToCore(ejecutar_tarea, "fondo", 8192, (void *)tarea, 1, NULL, 0);
}

uint32_t hal_aleatorio(void)
{
    return esp_random();
}
