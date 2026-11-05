/**
 * Punto de entrada en la placa (Arduino): setup() y loop().
 */
#include "app.h"
#include "hal.h"

#include <Arduino.h>
#include <esp_sleep.h>

bool archivos_iniciar(void);
void red_iniciar(void);
void red_atender(void);
void audio_iniciar(void);

/* Informe de memoria por el puerto serie cada cierto tiempo */
#define PERIODO_INFORME 10000

static void informe_memoria(const char *paso)
{
    Serial.printf("[memoria] tras %s: libre %u, bloque mayor %u, DMA libre %u\n", paso,
                  (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap(),
                  (unsigned)heap_caps_get_free_size(MALLOC_CAP_DMA));
}

void setup()
{
    Serial.begin(115200);
    delay(200);
    Serial.println();
    Serial.println("=== Radio Mundial ===");
    Serial.printf("Motivo de arranque: reinicio %d, despertar %d\n", (int)esp_reset_reason(),
                  (int)esp_sleep_get_wakeup_cause());
    Serial.printf("Memoria libre al arrancar: %u bytes (bloque mayor %u)\n",
                  (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
    Serial.printf("PSRAM: %u bytes (libres %u)\n", (unsigned)ESP.getPsramSize(), (unsigned)ESP.getFreePsram());

    archivos_iniciar();
    informe_memoria("archivos");
    /* La interfaz va PRIMERO: LVGL reserva su memoria (96 KB seguidos) al
     * iniciarse y, sin PSRAM, después de arrancar la WiFi y el audio ya no
     * queda un bloque tan grande. */
    app_inicio();
    informe_memoria("interfaz");
    red_iniciar();
    informe_memoria("WiFi");
    audio_iniciar();
    informe_memoria("audio");
}

void loop()
{
    app_bucle();
    red_atender();

    static uint32_t ultimo = 0;
    if (millis() - ultimo > PERIODO_INFORME) {
        ultimo = millis();
        Serial.printf("[sistema] libre %u, mínimo %u, bloque mayor %u\n",
                      (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMinFreeHeap(),
                      (unsigned)ESP.getMaxAllocHeap());
    }
}
