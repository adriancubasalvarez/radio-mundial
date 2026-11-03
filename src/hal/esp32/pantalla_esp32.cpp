/**
 * Pantalla y táctil en la placa.
 *
 * Pantalla: LovyanGFX (en la CYD, la misma configuración que el contador de
 * YouTube) y LVGL encima. El táctil de cada placa está en tactil_*.cpp.
 */
#include "hal.h"
#include "placa.h"
#include "tactil.h"

#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <driver/gpio.h>
#include <esp_sleep.h>
#include <lvgl.h>

class Pantalla : public lgfx::LGFX_Device {
    lgfx::Panel_ILI9341 panel;
    lgfx::Bus_SPI bus;
    lgfx::Light_PWM luz;

public:
    Pantalla()
    {
        auto b = bus.config();
        b.spi_host = TFT_SPI_HOST;
        b.spi_mode = 0;
        b.freq_write = 40000000;
        b.freq_read = 16000000;
        b.pin_sclk = PIN_TFT_SCLK;
        b.pin_mosi = PIN_TFT_MOSI;
        b.pin_miso = PIN_TFT_MISO;
        b.pin_dc = PIN_TFT_DC;
        bus.config(b);
        panel.setBus(&bus);

        auto p = panel.config();
        p.pin_cs = PIN_TFT_CS;
        p.pin_rst = -1;  // va al reset de la placa
        p.panel_width = 240;
        p.panel_height = 320;
        p.readable = true;
        p.invert = TFT_INVERTIR;
        p.rgb_order = false;
        p.bus_shared = false;
        panel.config(p);

        auto l = luz.config();
        l.pin_bl = PIN_TFT_BL;
        l.freq = 12000;
        l.pwm_channel = 7;  // el LED RGB usa los canales 0-2
        luz.config(l);
        panel.setLight(&luz);

        setPanel(&panel);
    }
};

static Pantalla lcd;
static uint32_t ultimo_tick;

/* Búfer de dibujo de LVGL en RAM interna; cada línea son 640 bytes. La
 * placa puede fijar otro valor en platformio.ini. */
#ifndef LINEAS_BUFER
#define LINEAS_BUFER 20
#endif
static lv_color_t bufer[PANTALLA_ANCHO * LINEAS_BUFER];

static void volcar_pantalla(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color)
{
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    lcd.startWrite();
    lcd.setAddrWindow(area->x1, area->y1, w, h);
    lcd.writePixels((lgfx::rgb565_t *)&color->full, w * h);
    lcd.endWrite();
    lv_disp_flush_ready(drv);
}

static void leer_tactil(lv_indev_drv_t *drv, lv_indev_data_t *datos)
{
    (void)drv;
    int x, y;
    if (!tactil_leer(&x, &y)) {
        datos->state = LV_INDEV_STATE_RELEASED;
        return;
    }
    datos->point.x = x;
    datos->point.y = y;
    datos->state = LV_INDEV_STATE_PRESSED;
}

/* Pines que se quedan fijos mientras la radio duerme (ver hal_apagar) */
static void soltar_pines(void)
{
    gpio_hold_dis((gpio_num_t)PIN_TFT_BL);
#ifdef PIN_AUDIO_EN
    gpio_hold_dis((gpio_num_t)PIN_AUDIO_EN);
#endif
#ifdef PIN_LED_R
    for (uint8_t pin : {PIN_LED_R, PIN_LED_G, PIN_LED_B}) {
        gpio_hold_dis((gpio_num_t)pin);
    }
#endif
    tactil_soltar();
    gpio_deep_sleep_hold_dis();
}

/* ---------- HAL ---------- */

void hal_pantalla_init(void)
{
    soltar_pines();  // por si venimos de hal_apagar()

    lcd.init();
    lcd.setRotation(TFT_ROTACION);  // horizontal, con el USB a la izquierda
    lcd.setBrightness(255);
    lcd.fillScreen(TFT_BLACK);

    tactil_iniciar();

    static lv_disp_draw_buf_t bufer_dibujo;
    lv_disp_draw_buf_init(&bufer_dibujo, bufer, NULL, PANTALLA_ANCHO * LINEAS_BUFER);

    static lv_disp_drv_t drv_pantalla;
    lv_disp_drv_init(&drv_pantalla);
    drv_pantalla.hor_res = PANTALLA_ANCHO;
    drv_pantalla.ver_res = PANTALLA_ALTO;
    drv_pantalla.flush_cb = volcar_pantalla;
    drv_pantalla.draw_buf = &bufer_dibujo;
    lv_disp_drv_register(&drv_pantalla);

    static lv_indev_drv_t drv_tactil;
    lv_indev_drv_init(&drv_tactil);
    drv_tactil.type = LV_INDEV_TYPE_POINTER;
    drv_tactil.read_cb = leer_tactil;
    lv_indev_drv_register(&drv_tactil);

    ultimo_tick = millis();
}

bool hal_atender(void)
{
    uint32_t ahora = millis();
    lv_tick_inc(ahora - ultimo_tick);
    ultimo_tick = ahora;
    return true;
}

uint32_t hal_millis(void)
{
    return millis();
}

void hal_esperar(uint32_t ms)
{
    delay(ms);
}

void hal_brillo(uint8_t porcentaje)
{
    if (porcentaje > 100) {
        porcentaje = 100;
    }
    lcd.setBrightness(porcentaje * 255 / 100);
}

void hal_apagar(void)
{
    /* Amplificador, retroiluminación y LED apagados, y que sigan así mientras
     * el ESP32 duerme (si no, los pines quedan sueltos) */
#ifdef PIN_AUDIO_EN
    pinMode(PIN_AUDIO_EN, OUTPUT);
    digitalWrite(PIN_AUDIO_EN, HIGH);  // activo a nivel bajo
    gpio_hold_en((gpio_num_t)PIN_AUDIO_EN);
#endif
    lcd.setBrightness(0);
    lcd.sleep();
    pinMode(PIN_TFT_BL, OUTPUT);
    digitalWrite(PIN_TFT_BL, LOW);
    gpio_hold_en((gpio_num_t)PIN_TFT_BL);
#ifdef PIN_LED_R
    /* LED de ánodo común: apagado a nivel alto */
    for (uint8_t pin : {PIN_LED_R, PIN_LED_G, PIN_LED_B}) {
        ledcDetachPin(pin);
        pinMode(pin, OUTPUT);
        digitalWrite(pin, HIGH);
        gpio_hold_en((gpio_num_t)pin);
    }
#endif
    tactil_dormir();
    gpio_deep_sleep_hold_en();

    /* Esperar a que se levante el dedo del deslizador: si no, despertaría
     * nada más dormirse */
    while (tactil_tocando()) {
        delay(20);
    }
    delay(300);

    Serial.println("Apagada: toca la pantalla para encenderla");
    Serial.flush();
    esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_TOUCH_IRQ, 0);
    esp_deep_sleep_start();
}
