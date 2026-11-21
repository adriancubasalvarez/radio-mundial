/**
 * Registro de reinicios inesperados: fallo del programa (panic), perro
 * guardián (watchdog: una tarea se quedó colgada) o bajada de tensión
 * (brownout). Sirve para saber si la radio se reinicia sola, por ejemplo de
 * noche, y por qué.
 *
 * Al arrancar se mira el motivo con esp_reset_reason(). Si es uno de esos,
 * se suma uno a la cuenta y se guardan el motivo y la hora en la NVS
 * (Non-Volatile Storage), en el espacio «reinicios».
 *
 * La hora: mientras la radio funciona, cada 20 s se apunta el minuto del día
 * en la memoria RTC (Real-Time Clock) con RTC_NOINIT_ATTR, que no se borra en
 * un reinicio por software ni por el perro guardián (sí al quitar la
 * corriente). Al arrancar tras un fallo, ese valor es la hora aproximada del
 * fallo. Un número mágico dice si el valor es de verdad o basura de un
 * encendido.
 */
#include "hal.h"

#include <Arduino.h>
#include <Preferences.h>
#include <esp_attr.h>
#include <esp_system.h>
#include <string.h>

#define MAGICO        0x52414449u  /* «RADI» */
#define PERIODO_HORA  20000        /* ms entre apuntes de la hora */

RTC_NOINIT_ATTR static uint32_t magico;
RTC_NOINIT_ATTR static int32_t minuto_rtc;

static Reinicios datos = {0, "", -1};

/* Texto corto del motivo, o NULL si el reinicio fue normal (encendido,
 * reinicio pedido, despertar del apagado, actualización...) */
static const char *motivo_inesperado(esp_reset_reason_t motivo)
{
    switch (motivo) {
    case ESP_RST_PANIC:    return "panic";
    case ESP_RST_INT_WDT:  return "int wdt";
    case ESP_RST_TASK_WDT: return "task wdt";
    case ESP_RST_WDT:      return "wdt";
    case ESP_RST_BROWNOUT: return "brownout";
    default:               return NULL;
    }
}

/* Lo llama setup() al principio */
void reinicios_iniciar(void)
{
    Preferences nvs;
    nvs.begin("reinicios", false);
    datos.veces = nvs.getUShort("veces", 0);
    if (nvs.isKey("motivo")) {  /* sin comprobar, la primera vez avisa de un error */
        nvs.getString("motivo", datos.motivo, sizeof(datos.motivo));
    }
    datos.minuto_del_dia = nvs.getInt("minuto", -1);

    const char *motivo = motivo_inesperado(esp_reset_reason());
    if (motivo != NULL) {
        datos.veces++;
        strncpy(datos.motivo, motivo, sizeof(datos.motivo) - 1);
        datos.motivo[sizeof(datos.motivo) - 1] = '\0';
        datos.minuto_del_dia = (magico == MAGICO) ? minuto_rtc : -1;
        nvs.putUShort("veces", datos.veces);
        nvs.putString("motivo", datos.motivo);
        nvs.putInt("minuto", datos.minuto_del_dia);
        if (datos.minuto_del_dia >= 0) {
            Serial.printf("[reinicio] INESPERADO: %s hacia las %02d:%02d (van %u)\n", motivo,
                          datos.minuto_del_dia / 60, datos.minuto_del_dia % 60, (unsigned)datos.veces);
        } else {
            Serial.printf("[reinicio] INESPERADO: %s, hora desconocida (van %u)\n", motivo,
                          (unsigned)datos.veces);
        }
    }
    nvs.end();

    magico = MAGICO;
    minuto_rtc = -1;  /* hasta que se sepa la hora */
}

/* Lo llama loop(): apunta la hora en la memoria RTC */
void reinicios_atender(void)
{
    static uint32_t ultimo = 0;
    if (millis() - ultimo < PERIODO_HORA) {
        return;
    }
    ultimo = millis();
    int hora, minuto;
    if (hal_hora(&hora, &minuto)) {
        minuto_rtc = hora * 60 + minuto;
    }
}

void hal_reinicios(Reinicios *r)
{
    *r = datos;
}

void hal_reinicios_borrar(void)
{
    Preferences nvs;
    nvs.begin("reinicios", false);
    nvs.clear();
    nvs.end();
    datos.veces = 0;
    datos.motivo[0] = '\0';
    datos.minuto_del_dia = -1;
    Serial.println("[reinicio] registro borrado");
}
