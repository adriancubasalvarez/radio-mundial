/**
 * Utilidades del sistema en la placa: WiFi provisional, hora por NTP
 * (Network Time Protocol) y números aleatorios.
 *
 * WiFi provisional (hasta el portal cautivo de la Fase 5): se leen la red
 * y la contraseña de /wifi.txt en LittleFS, dos líneas:
 *     nombre-de-la-red
 *     contraseña
 * El archivo data/wifi.txt no se sube a GitHub (está en .gitignore).
 */
#include "hal.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <esp_random.h>
#include <time.h>

/* Hora peninsular española con cambio de horario automático */
#define ZONA_HORARIA "CET-1CEST,M3.5.0,M10.5.0/3"

void red_iniciar(void)
{
    File f = LittleFS.open("/wifi.txt", FILE_READ);
    if (!f) {
        Serial.println("Sin /wifi.txt: la radio no se conectará a Internet");
        return;
    }
    String red = f.readStringUntil('\n');
    String clave = f.readStringUntil('\n');
    f.close();
    red.trim();
    clave.trim();

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);  // sin ahorro de energía: el audio no se corta
    WiFi.begin(red.c_str(), clave.c_str());
    Serial.printf("Conectando a la WiFi «%s»...\n", red.c_str());
    configTzTime(ZONA_HORARIA, "pool.ntp.org", "time.google.com");
}

/* Informa por el puerto serie cuando cambia el estado de la WiFi */
void red_atender(void)
{
    static wl_status_t anterior = WL_IDLE_STATUS;
    wl_status_t estado = WiFi.status();
    if (estado != anterior) {
        anterior = estado;
        if (estado == WL_CONNECTED) {
            Serial.printf("WiFi conectada: IP %s, señal %d dBm\n",
                          WiFi.localIP().toString().c_str(), WiFi.RSSI());
        } else if (estado == WL_DISCONNECTED || estado == WL_CONNECTION_LOST) {
            Serial.println("WiFi desconectada (se reintenta sola)");
        }
    }
}

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

uint32_t hal_aleatorio(void)
{
    return esp_random();
}
