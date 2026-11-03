#include "tiempo.h"
#include "hal.h"
#include "idioma.h"

#include <ArduinoJson.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CADA_MS        (30UL * 60 * 1000)  /* una descarga cada 30 minutos */
#define REINTENTO_MS   (2UL * 60 * 1000)   /* si falla, otra vez a los 2 minutos */

/* Iconos de FontAwesome 5 (los lleva la fuente de símbolos de la radio) */
#define ICONO_SOL          "\xEF\x86\x85"  /* f185 */
#define ICONO_LUNA         "\xEF\x86\x86"  /* f186 */
#define ICONO_NUBE         "\xEF\x83\x82"  /* f0c2 */
#define ICONO_SOL_NUBE     "\xEF\x9B\x84"  /* f6c4 */
#define ICONO_LUNA_NUBE    "\xEF\x9B\x83"  /* f6c3 */
#define ICONO_LLUVIA       "\xEF\x9C\xBD"  /* f73d */
#define ICONO_CHUBASCO     "\xEF\x9D\x80"  /* f740 */
#define ICONO_NIEVE        "\xEF\x8B\x9C"  /* f2dc */
#define ICONO_RAYO         "\xEF\x83\xA7"  /* f0e7 */
#define ICONO_NIEBLA       "\xEF\x9D\x9F"  /* f75f */

static Tiempo actual;              /* lo que ve la interfaz */
static Tiempo nuevo;               /* lo que rellena la tarea de fondo */
static volatile bool hay_nuevo;    /* la tarea terminó con datos nuevos */
static volatile bool descargando;
static int zona_segundos;          /* diferencia con la hora universal */
static uint32_t proxima_ms;
static bool primera = true;

/* Ubicación por la IP (una vez por arranque: la radio no se mueve) */
static float latitud, longitud;
static bool hay_ubicacion;

static bool pedir_json(const char *url, JsonDocument &doc)
{
    char *texto = hal_http_get(url);
    if (texto == NULL) {
        return false;
    }
    DeserializationError error = deserializeJson(doc, texto);
    free(texto);
    return !error;
}

static bool ubicar(void)
{
    JsonDocument doc;
    if (!pedir_json("http://ip-api.com/json/?fields=status,city,lat,lon", doc) ||
        strcmp(doc["status"] | "", "success") != 0) {
        return false;
    }
    latitud = doc["lat"] | 0.0f;
    longitud = doc["lon"] | 0.0f;
    strncpy(nuevo.ciudad, doc["city"] | "", sizeof(nuevo.ciudad) - 1);
    hay_ubicacion = true;
    return true;
}

/* "2026-11-03T07:46" -> "07:46" */
static void copiar_hora(char *destino, const char *iso)
{
    const char *t = strchr(iso, 'T');
    snprintf(destino, 6, "%s", t ? t + 1 : "");
}

/* Día de la semana de una fecha "AAAA-MM-DD" (algoritmo de Sakamoto) */
static int dia_semana(const char *fecha)
{
    int a = atoi(fecha), m = atoi(fecha + 5), d = atoi(fecha + 8);
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3) {
        a -= 1;
    }
    return (a + a / 4 - a / 100 + a / 400 + t[(m - 1) % 12] + d) % 7;
}

static bool prever(void)
{
    char url[300];
    snprintf(url, sizeof(url),
             "http://api.open-meteo.com/v1/forecast?latitude=%.3f&longitude=%.3f"
             "&current=temperature_2m,weather_code,is_day"
             "&daily=weather_code,temperature_2m_max,temperature_2m_min,sunrise,sunset"
             "&timezone=auto&forecast_days=%d",
             latitud, longitud, DIAS_PREVISION);
    JsonDocument doc;
    if (!pedir_json(url, doc)) {
        return false;
    }
    zona_segundos = doc["utc_offset_seconds"] | 0;
    nuevo.temperatura = (int)lroundf(doc["current"]["temperature_2m"] | 0.0f);
    nuevo.codigo = doc["current"]["weather_code"] | 0;
    nuevo.de_dia = (doc["current"]["is_day"] | 1) != 0;
    JsonObjectConst diario = doc["daily"];
    for (int i = 0; i < DIAS_PREVISION; i++) {
        nuevo.dia[i].codigo = diario["weather_code"][i] | 0;
        nuevo.dia[i].maxima = (int)lroundf(diario["temperature_2m_max"][i] | 0.0f);
        nuevo.dia[i].minima = (int)lroundf(diario["temperature_2m_min"][i] | 0.0f);
        nuevo.dia[i].dia_semana = dia_semana(diario["time"][i] | "2000-01-01");
    }
    copiar_hora(nuevo.sale_sol, diario["sunrise"][0] | "");
    copiar_hora(nuevo.pone_sol, diario["sunset"][0] | "");
    nuevo.hay = true;
    return true;
}

static void medir_aire(void)
{
    char url[200];
    snprintf(url, sizeof(url),
             "http://air-quality-api.open-meteo.com/v1/air-quality?latitude=%.3f&longitude=%.3f"
             "&current=european_aqi,pm2_5",
             latitud, longitud);
    JsonDocument doc;
    nuevo.hay_aire = pedir_json(url, doc) && !doc["current"]["european_aqi"].isNull();
    if (nuevo.hay_aire) {
        nuevo.indice_aire = doc["current"]["european_aqi"] | 0;
        nuevo.pm25 = doc["current"]["pm2_5"] | 0.0f;
    }
}

/* Tarea de fondo: no toca «actual», solo «nuevo» */
static void descargar(void)
{
    nuevo = actual;
    bool ok = (hay_ubicacion || ubicar()) && prever();
    if (ok) {
        medir_aire();
        printf("[tiempo] %s: %d °C, código %d, aire %d\n", nuevo.ciudad, nuevo.temperatura, nuevo.codigo,
               nuevo.hay_aire ? nuevo.indice_aire : -1);
        hay_nuevo = true;
    } else {
        printf("[tiempo] no se pudo descargar\n");
    }
    descargando = false;
}

void tiempo_atender(void)
{
    if (hay_nuevo) {
        actual = nuevo;
        hay_nuevo = false;
        hal_zona_horaria(zona_segundos);
        proxima_ms = hal_millis() + CADA_MS;
    }
    if (descargando || hal_red_estado() != RED_CONECTADA) {
        return;
    }
    if (primera || (int32_t)(hal_millis() - proxima_ms) >= 0) {
        primera = false;
        proxima_ms = hal_millis() + REINTENTO_MS;  /* si sale bien, se cambia a 30 min */
        descargando = true;
        hal_segundo_plano(descargar);
    }
}

const Tiempo *tiempo_actual(void)
{
    return &actual;
}

/* Códigos WMO de Open-Meteo: 0 despejado, 1-3 nubes, 45-48 niebla,
 * 51-57 llovizna, 61-67 lluvia, 71-77 nieve, 80-82 chubascos,
 * 85-86 chubascos de nieve, 95-99 tormenta */
const char *tiempo_icono(int codigo, bool de_dia)
{
    if (codigo == 0) return de_dia ? ICONO_SOL : ICONO_LUNA;
    if (codigo <= 2) return de_dia ? ICONO_SOL_NUBE : ICONO_LUNA_NUBE;
    if (codigo == 3) return ICONO_NUBE;
    if (codigo <= 48) return ICONO_NIEBLA;
    if (codigo == 65 || codigo == 82) return ICONO_CHUBASCO;
    if (codigo <= 67 || (codigo >= 80 && codigo <= 82)) return ICONO_LLUVIA;
    if (codigo <= 77 || codigo == 85 || codigo == 86) return ICONO_NIEVE;
    return ICONO_RAYO;
}

const char *tiempo_texto(int codigo)
{
    if (codigo == 0) return TR("Clear", "Despejado");
    if (codigo <= 2) return TR("Partly cloudy", "Poco nuboso");
    if (codigo == 3) return TR("Cloudy", "Nublado");
    if (codigo <= 48) return TR("Fog", "Niebla");
    if (codigo <= 57) return TR("Drizzle", "Llovizna");
    if (codigo <= 67) return TR("Rain", "Lluvia");
    if (codigo <= 77) return TR("Snow", "Nieve");
    if (codigo <= 82) return TR("Showers", "Chubascos");
    if (codigo <= 86) return TR("Snow showers", "Chubascos de nieve");
    return TR("Thunderstorm", "Tormenta");
}

/* Índice europeo de calidad del aire (Agencia Europea de Medio Ambiente):
 * niveles y colores oficiales */
const char *aire_texto(int indice)
{
    if (indice <= 20) return TR("Good", "Buena");
    if (indice <= 40) return TR("Fair", "Razonable");
    if (indice <= 60) return TR("Moderate", "Moderada");
    if (indice <= 80) return TR("Poor", "Mala");
    if (indice <= 100) return TR("Very poor", "Muy mala");
    return TR("Extremely poor", "Extremadamente mala");
}

uint32_t aire_color(int indice)
{
    if (indice <= 20) return 0x50F0E6;
    if (indice <= 40) return 0x50CCAA;
    if (indice <= 60) return 0xF0E641;
    if (indice <= 80) return 0xFF5050;
    if (indice <= 100) return 0xC0305A;  /* el oficial (960032) apenas se ve sobre negro */
    return 0xA040B0;
}
