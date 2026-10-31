/**
 * Red WiFi en la placa.
 *
 * 1. Si hay una WiFi guardada (en la NVS, la guarda el propio ESP32), se
 *    conecta a ella.
 * 2. Si no hay ninguna, o no conecta en TIEMPO_CONEXION, abre el portal
 *    cautivo con WiFiManager: la radio crea la red «RadioMundial-XXXX»;
 *    el móvil se conecta y se abre una página para elegir la WiFi de casa.
 * 3. Además escucha Improv por el puerto serie: así la página de
 *    instalación (ESP Web Tools) puede mandar la WiFi por USB justo después
 *    de instalar el firmware.
 */
#include "hal.h"
#include "marca.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <esp_wifi.h>
#include <improv.h>

/* ms con una WiFi guardada sin conseguir conectar antes de abrir el portal.
 * Generoso: algunos routers rechazan unos segundos a una placa que se acaba
 * de reiniciar («Association refused temporarily»). */
#define TIEMPO_CONEXION 45000
#define TIEMPO_IMPROV   15000   /* ms para conectar con la WiFi recibida por USB */

/* Hora peninsular española con cambio de horario automático */
#define ZONA_HORARIA "CET-1CEST,M3.5.0,M10.5.0/3"

static WiFiManager gestor;
static EstadoRed estado = RED_CONECTANDO;
static uint32_t inicio_conexion;
static char nombre_portal[24];

/* ---------- Portal ---------- */

static void abrir_portal(void)
{
    Serial.printf("Abriendo el portal WiFi «%s»\n", nombre_portal);
    gestor.setConfigPortalBlocking(false);
    gestor.setTitle(MARCA_NOMBRE);
    std::vector<const char *> menu = {"wifi"};
    gestor.setMenu(menu);
    gestor.setConnectTimeout(15);
    gestor.startConfigPortal(nombre_portal);
    estado = RED_PORTAL;
}

static bool hay_wifi_guardada(void)
{
    wifi_config_t config;
    return esp_wifi_get_config(WIFI_IF_STA, &config) == ESP_OK && config.sta.ssid[0] != 0;
}

/* ---------- Improv por el puerto serie ---------- */

static uint8_t bufer_improv[256];
static size_t posicion_improv = 0;

static void enviar_improv(uint8_t tipo, const std::vector<uint8_t> &datos)
{
    std::vector<uint8_t> paquete = {'I', 'M', 'P', 'R', 'O', 'V', improv::IMPROV_SERIAL_VERSION, tipo,
                                    (uint8_t)datos.size()};
    paquete.insert(paquete.end(), datos.begin(), datos.end());
    uint8_t suma = 0;
    for (uint8_t b : paquete) {
        suma += b;
    }
    paquete.push_back(suma);
    Serial.write(paquete.data(), paquete.size());
    Serial.write('\n');
}

static void estado_improv(improv::State s)
{
    enviar_improv(improv::TYPE_CURRENT_STATE, {(uint8_t)s});
}

static void error_improv(improv::Error e)
{
    enviar_improv(improv::TYPE_ERROR_STATE, {(uint8_t)e});
}

static void respuesta_improv(improv::Command orden, const std::vector<String> &datos)
{
    enviar_improv(improv::TYPE_RPC_RESPONSE, improv::build_rpc_response(orden, datos, false));
}

static bool orden_improv(improv::ImprovCommand orden)
{
    switch (orden.command) {
    case improv::GET_CURRENT_STATE:
        estado_improv(WiFi.status() == WL_CONNECTED ? improv::STATE_PROVISIONED : improv::STATE_AUTHORIZED);
        if (WiFi.status() == WL_CONNECTED) {
            respuesta_improv(improv::GET_CURRENT_STATE, {});
        }
        return true;

    case improv::GET_DEVICE_INFO:
        respuesta_improv(improv::GET_DEVICE_INFO, {MARCA_NOMBRE, MARCA_VERSION, "ESP32", MARCA_NOMBRE});
        return true;

    case improv::GET_WIFI_NETWORKS: {
        int n = WiFi.scanNetworks();
        for (int i = 0; i < n; i++) {
            respuesta_improv(improv::GET_WIFI_NETWORKS,
                             {WiFi.SSID(i), String(WiFi.RSSI(i)),
                              WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "NO" : "YES"});
        }
        respuesta_improv(improv::GET_WIFI_NETWORKS, {});  // fin de la lista
        WiFi.scanDelete();
        return true;
    }

    case improv::WIFI_SETTINGS: {
        estado_improv(improv::STATE_PROVISIONING);
        if (estado == RED_PORTAL) {
            gestor.stopConfigPortal();
        }
        WiFi.mode(WIFI_STA);
        WiFi.begin(orden.ssid.c_str(), orden.password.c_str());  // queda guardada
        uint32_t inicio = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - inicio < TIEMPO_IMPROV) {
            delay(200);
        }
        if (WiFi.status() == WL_CONNECTED) {
            estado = RED_CONECTADA;
            estado_improv(improv::STATE_PROVISIONED);
            respuesta_improv(improv::WIFI_SETTINGS, {});
        } else {
            estado = RED_CONECTANDO;
            inicio_conexion = millis();
            estado_improv(improv::STATE_AUTHORIZED);
            error_improv(improv::ERROR_UNABLE_TO_CONNECT);
        }
        return true;
    }

    default:
        error_improv(improv::ERROR_UNKNOWN_RPC);
        return false;
    }
}

static void atender_improv(void)
{
    while (Serial.available()) {
        uint8_t b = Serial.read();
        if (posicion_improv >= sizeof(bufer_improv)) {
            posicion_improv = 0;
        }
        if (improv::parse_improv_serial_byte(posicion_improv, b, bufer_improv, orden_improv,
                                             [](improv::Error e) { error_improv(e); })) {
            bufer_improv[posicion_improv++] = b;
        } else {
            posicion_improv = 0;
        }
    }
}

/* ---------- Llamadas desde main_esp32.cpp ---------- */

void red_iniciar(void)
{
    uint8_t mac[6];
    WiFi.macAddress(mac);
    snprintf(nombre_portal, sizeof(nombre_portal), "RadioMundial-%02X%02X", mac[4], mac[5]);

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);  // sin ahorro de energía: el audio no se corta
    WiFi.setHostname("radio-mundial");
    configTzTime(ZONA_HORARIA, "pool.ntp.org", "time.google.com");

    if (hay_wifi_guardada()) {
        Serial.println("Conectando a la WiFi guardada...");
        WiFi.begin();
        estado = RED_CONECTANDO;
        inicio_conexion = millis();
    } else {
        Serial.println("No hay WiFi guardada");
        abrir_portal();
    }
}

void red_atender(void)
{
    atender_improv();

    if (estado == RED_PORTAL) {
        if (gestor.process()) {  // el usuario eligió red y ya está conectada
            Serial.printf("WiFi configurada desde el portal: %s\n", WiFi.SSID().c_str());
            gestor.stopConfigPortal();
            estado = RED_CONECTADA;
        }
        return;
    }

    bool conectada = WiFi.status() == WL_CONNECTED;
    if (conectada && estado != RED_CONECTADA) {
        Serial.printf("WiFi conectada a «%s»: IP %s, señal %d dBm\n", WiFi.SSID().c_str(),
                      WiFi.localIP().toString().c_str(), WiFi.RSSI());
        estado = RED_CONECTADA;
    } else if (!conectada && estado == RED_CONECTADA) {
        Serial.println("WiFi perdida: se reintenta sola");
        estado = RED_CONECTANDO;
        inicio_conexion = millis();
    } else if (!conectada && estado == RED_CONECTANDO && millis() - inicio_conexion > TIEMPO_CONEXION) {
        abrir_portal();  // no hay manera: que el usuario revise la WiFi
    }
}

/* ---------- HAL ---------- */

EstadoRed hal_red_estado(void)
{
    return estado;
}

const char *hal_red_portal(void)
{
    return nombre_portal;
}

void hal_red_configurar(void)
{
    if (estado != RED_PORTAL) {
        abrir_portal();
    }
}

void hal_red_cancelar(void)
{
    if (estado == RED_PORTAL) {
        gestor.stopConfigPortal();
        WiFi.mode(WIFI_STA);
        WiFi.begin();
        estado = RED_CONECTANDO;
        inicio_conexion = millis();
    }
}
