/**
 * Actualización de las emisoras por Internet, sin flashear.
 *
 * El paquete (tools/build_paquete.py) se descarga entero y se comprueba antes
 * de tocar nada. Se desempaqueta en /upd y lo último que se escribe es la
 * marca /upd/listo: hasta que existe, la radio sigue usando lo que tenía, así
 * que un corte de luz no puede dejarla con una mezcla de archivos.
 *
 * No comprueba el certificado del servidor: no hay que mantener certificados
 * en la placa, y el paquete solo trae listas de emisoras. Por eso se aceptan
 * únicamente las rutas de esas listas.
 */
#include "hal.h"
#include "marca.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#define CARPETA      "/upd"
#define MARCA_LISTO  "/upd/listo"
#define TEMPORAL     "/paquete.tmp"
#define TAM_BLOQUE   2048
#define TIEMPO_RED   20000   /* ms sin recibir nada antes de rendirse */
#define PILA_TAREA   10240

static volatile EstadoActualizar estado = ACT_NADA;
static volatile uint8_t porcentaje;
static volatile bool ocupada;
static char version_nueva[16];
static char fecha_nueva[16];
static uint32_t suma_nueva;
static size_t tam_nuevo;

/* CRC-32, la misma suma que zlib.crc32() en el PC */
static uint32_t sumar(uint32_t suma, const uint8_t *datos, size_t n)
{
    suma = ~suma;
    while (n--) {
        suma ^= *datos++;
        for (int i = 0; i < 8; i++) {
            suma = (suma >> 1) ^ (0xEDB88320u & (0u - (suma & 1)));
        }
    }
    return ~suma;
}

static bool empezar(HTTPClient &http, WiFiClientSecure &cliente, const char *archivo)
{
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }
    String url = String(MARCA_URL_EMISORAS) + archivo;
    cliente.setInsecure();
    http.setTimeout(TIEMPO_RED);
    http.setUserAgent("RadioMundial/1.0");
    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    if (!http.begin(cliente, url)) {
        return false;
    }
    int codigo = http.GET();
    if (codigo != 200) {
        Serial.printf("[actualizar] %s -> %d\n", url.c_str(), codigo);
        http.end();
        return false;
    }
    return true;
}

/* La versión instalada es la que guarda la marca: si un corte de luz la dejó a
 * medias no hay marca, la radio usa la lista de fábrica y se puede reinstalar */
static String version_instalada(void)
{
    if (!LittleFS.exists(MARCA_LISTO)) {
        return "";
    }
    File marca = LittleFS.open(MARCA_LISTO, FILE_READ);
    return marca ? marca.readString() : String("");
}

/* version.txt: «suma tamaño fecha» */
static void comprobar(void)
{
    WiFiClientSecure cliente;
    HTTPClient http;
    if (!empezar(http, cliente, "version.txt")) {
        estado = ACT_ERROR;
        return;
    }
    String texto = http.getString();
    http.end();
    unsigned long tam = 0;
    int campos = sscanf(texto.c_str(), "%15s %lu %15s", version_nueva, &tam, fecha_nueva);
    if (campos != 3 || tam == 0) {
        estado = ACT_ERROR;
        return;
    }
    suma_nueva = strtoul(version_nueva, NULL, 16);
    tam_nuevo = tam;
    estado = version_instalada() == version_nueva ? ACT_AL_DIA : ACT_HAY_NUEVA;
    Serial.printf("[actualizar] versión %s, %u bytes\n", version_nueva, (unsigned)tam_nuevo);
}

/* Baja el paquete a la memoria y comprueba tamaño y suma */
static bool descargar(void)
{
    size_t libre = LittleFS.totalBytes() - LittleFS.usedBytes();
    if (libre < tam_nuevo * 2 + 64 * 1024) {
        Serial.printf("[actualizar] sin espacio: %u bytes libres\n", (unsigned)libre);
        return false;
    }
    WiFiClientSecure cliente;
    HTTPClient http;
    if (!empezar(http, cliente, "emisoras.pack")) {
        return false;
    }
    File f = LittleFS.open(TEMPORAL, FILE_WRITE);
    uint8_t *bloque = (uint8_t *)malloc(TAM_BLOQUE);
    if (!f || bloque == NULL) {
        free(bloque);
        http.end();
        return false;
    }
    WiFiClient *flujo = http.getStreamPtr();
    size_t total = 0;
    uint32_t suma = 0, ultimo = millis();
    while (total < tam_nuevo && millis() - ultimo < TIEMPO_RED) {
        size_t hay = flujo->available();
        if (hay == 0) {
            if (!http.connected()) {
                break;
            }
            delay(5);
            continue;
        }
        size_t pido = min(min(hay, (size_t)TAM_BLOQUE), tam_nuevo - total);
        int n = flujo->read(bloque, pido);
        if (n <= 0) {
            continue;
        }
        if (f.write(bloque, n) != (size_t)n) {
            break;
        }
        suma = sumar(suma, bloque, n);
        total += n;
        porcentaje = (uint8_t)(total * 100 / tam_nuevo);
        ultimo = millis();
    }
    free(bloque);
    f.close();
    http.end();
    bool bien = total == tam_nuevo && suma == suma_nueva;
    if (!bien) {
        Serial.printf("[actualizar] paquete mal: %u bytes, suma %08x\n", (unsigned)total, (unsigned)suma);
        LittleFS.remove(TEMPORAL);
    }
    return bien;
}

/* Quita lo que hubiera de otra actualización */
static void vaciar_carpeta(void)
{
    LittleFS.remove(MARCA_LISTO);
    File carpeta = LittleFS.open(CARPETA "/emisoras");
    if (carpeta && carpeta.isDirectory()) {
        for (File f = carpeta.openNextFile(); f; f = carpeta.openNextFile()) {
            String ruta = String(CARPETA "/emisoras/") + f.name();
            f.close();
            LittleFS.remove(ruta);
        }
    }
    carpeta.close();
    LittleFS.remove(CARPETA "/buscar.txt");
}

/* Solo listas de emisoras: nada de salirse de la carpeta */
static bool ruta_valida(const String &ruta)
{
    return ruta.indexOf("..") < 0 && ruta.indexOf('/', 9) < 0 &&
           (ruta == "buscar.txt" || (ruta.startsWith("emisoras/") && ruta.length() > 9));
}

static bool desempaquetar(void)
{
    File paquete = LittleFS.open(TEMPORAL, FILE_READ);
    if (!paquete || paquete.readStringUntil('\n') != "RM1") {
        return false;
    }
    vaciar_carpeta();
    LittleFS.mkdir(CARPETA);
    LittleFS.mkdir(CARPETA "/emisoras");

    uint8_t *bloque = (uint8_t *)malloc(TAM_BLOQUE);
    bool bien = bloque != NULL;
    bool hay_indice = false, hay_buscar = false;
    while (bien && paquete.available()) {
        String cabecera = paquete.readStringUntil('\n');
        int tab = cabecera.indexOf('\t');
        String ruta = cabecera.substring(0, tab);
        size_t resto = (size_t)cabecera.substring(tab + 1).toInt();
        if (tab <= 0 || !ruta_valida(ruta)) {
            bien = false;
            break;
        }
        hay_indice |= ruta == "emisoras/indice.txt";
        hay_buscar |= ruta == "buscar.txt";
        File f = LittleFS.open(String(CARPETA "/") + ruta, FILE_WRITE);
        bien = (bool)f;
        while (bien && resto > 0) {
            size_t n = paquete.read(bloque, min(resto, (size_t)TAM_BLOQUE));
            bien = n > 0 && f.write(bloque, n) == n;
            resto -= n;
        }
        f.close();
        porcentaje = (uint8_t)(paquete.position() * 100 / paquete.size());
    }
    free(bloque);
    paquete.close();
    LittleFS.remove(TEMPORAL);
    if (!bien || !hay_indice || !hay_buscar) {
        vaciar_carpeta();
        return false;
    }
    File marca = LittleFS.open(MARCA_LISTO, FILE_WRITE);  /* a partir de aquí, la nueva */
    if (!marca) {
        return false;
    }
    marca.print(version_nueva);
    marca.close();
    return true;
}

static void instalar(void)
{
    estado = ACT_DESCARGANDO;
    porcentaje = 0;
    if (!descargar()) {
        estado = ACT_ERROR;
        return;
    }
    estado = ACT_INSTALANDO;
    porcentaje = 0;
    estado = desempaquetar() ? ACT_LISTO : ACT_ERROR;
}

static void tarea(void *parametro)
{
    if (parametro != NULL) {
        instalar();
    } else {
        comprobar();
    }
    ocupada = false;
    vTaskDelete(NULL);
}

static void lanzar(void *parametro)
{
    if (!ocupada) {
        ocupada = true;
        xTaskCreatePinnedToCore(tarea, "actualizar", PILA_TAREA, parametro, 1, NULL, 0);
    }
}

void hal_actualizar_comprobar(void)
{
    if (!ocupada) {
        estado = ACT_COMPROBANDO;
        lanzar(NULL);
    }
}

void hal_actualizar_instalar(void)
{
    if (!ocupada) {
        estado = ACT_DESCARGANDO;
        porcentaje = 0;
        lanzar((void *)1);
    }
}

EstadoActualizar hal_actualizar_estado(uint8_t *progreso)
{
    *progreso = porcentaje;
    return estado;
}

const char *hal_actualizar_fecha(void)
{
    return fecha_nueva;
}

void hal_reiniciar(void)
{
    ESP.restart();
}
