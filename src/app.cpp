/**
 * Bucle principal común al PC y a la placa.
 */
#include "app.h"
#include "hal.h"
#include "ui/ui.h"

#include <lvgl.h>

void app_inicio(void)
{
    lv_init();
    hal_pantalla_init();
    ui_inicio();
}

bool app_bucle(void)
{
    if (!hal_atender()) {
        return false;
    }
    /* LVGL redibuja, lee el táctil y ejecuta sus temporizadores */
    uint32_t espera = lv_timer_handler();
    hal_esperar(espera < 5 ? espera : 5);
    return true;
}
