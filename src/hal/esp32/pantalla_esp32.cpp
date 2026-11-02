/**
 * Pantalla y táctil en la placa.
 *
 * Pantalla: LovyanGFX (misma configuración que el contador de YouTube en
 * esta placa) y LVGL encima. Táctil: lectura por SPI software del XPT2046.
 */
#include "hal.h"
#include "placa.h"

#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <algorithm>
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
        b.spi_host = SPI2_HOST;  // HSPI
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
        p.invert = false;
        p.rgb_order = false;
        p.bus_shared = false;
        panel.config(p);

        auto l = luz.config();
        l.pin_bl = PIN_TFT_BL;
        l.freq = 12000;
        l.pwm_channel = 7;
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

/* ---------- Táctil XPT2046 por SPI software ---------- */

static uint8_t transferir(uint8_t salida)
{
    uint8_t entrada = 0;
    for (int bit = 7; bit >= 0; bit--) {  // modo SPI 0, primero el bit más alto
        digitalWrite(PIN_TOUCH_MOSI, (salida >> bit) & 1);
        delayMicroseconds(1);
        digitalWrite(PIN_TOUCH_SCLK, HIGH);
        delayMicroseconds(1);
        entrada = (entrada << 1) | digitalRead(PIN_TOUCH_MISO);
        digitalWrite(PIN_TOUCH_SCLK, LOW);
    }
    return entrada;
}

/* Lectura en bruto (mediana de 7 medidas); false si no se está tocando */
static bool leer_tactil_bruto(int &rx, int &ry)
{
    uint8_t d[57];
    memset(d, 0, sizeof(d));
    for (int j = 0; j < 7; j++) {
        d[j * 8 + 0] = 0x91;  // Y
        d[j * 8 + 2] = 0xB1;  // Z1
        d[j * 8 + 4] = 0xD1;  // X
        d[j * 8 + 6] = 0xC1;  // Z2
    }
    d[56] = 0x80;  // al final, a reposo
    digitalWrite(PIN_TOUCH_CS, LOW);
    for (auto &b : d) {
        b = transferir(b);
    }
    digitalWrite(PIN_TOUCH_CS, HIGH);

    int xt[7], yt[7], zt[7], ix = 0, iy = 0, iz = 0;
    for (int j = 0; j < 7; j++) {
        const uint8_t *v = &d[j * 8];
        int x = (v[5] << 8 | v[6]) >> 3;
        int y = (v[1] << 8 | v[2]) >> 3;
        int z = 0x3200 + y - x + (((v[3] << 8 | v[4]) - (v[7] << 8 | v[8])) >> 1);
        if (x > 128 && x <= 3968) xt[ix++] = x;
        if (y > 128 && y <= 3968) yt[iy++] = y;
        if (z > 0) zt[iz++] = z;
    }
    if (ix < 3 || iy < 3 || iz < 3) {
        return false;
    }
    std::sort(xt, xt + ix);
    std::sort(yt, yt + iy);
    std::sort(zt, zt + iz);
    if ((zt[iz >> 1] >> 8) == 0) {
        return false;
    }
    rx = xt[ix >> 1];
    ry = yt[iy >> 1];
    return true;
}

static void leer_tactil(lv_indev_drv_t *drv, lv_indev_data_t *datos)
{
    (void)drv;
    int rx, ry;
    if (!leer_tactil_bruto(rx, ry)) {
        datos->state = LV_INDEV_STATE_RELEASED;
        return;
    }
    /* De la calibración (panel vertical) a la pantalla horizontal con el
     * USB a la izquierda (setRotation(1)) */
    int px = (rx - TOUCH_X_MIN) * 239 / (TOUCH_X_MAX - TOUCH_X_MIN);
    int py = (ry - TOUCH_Y_MIN) * 319 / (TOUCH_Y_MAX - TOUCH_Y_MIN);
    datos->point.x = constrain(py, 0, PANTALLA_ANCHO - 1);
    datos->point.y = constrain(239 - px, 0, PANTALLA_ALTO - 1);
    datos->state = LV_INDEV_STATE_PRESSED;
}

/* ---------- HAL ---------- */

void hal_pantalla_init(void)
{
    /* Si venimos de hal_apagar(), soltar los pines que se quedaron fijos */
    gpio_hold_dis((gpio_num_t)PIN_TFT_BL);
    gpio_hold_dis((gpio_num_t)PIN_AUDIO_EN);
    gpio_hold_dis((gpio_num_t)PIN_TOUCH_CS);
    gpio_hold_dis((gpio_num_t)PIN_TOUCH_SCLK);
    gpio_hold_dis((gpio_num_t)PIN_TOUCH_MOSI);
    for (uint8_t pin : {PIN_LED_R, PIN_LED_G, PIN_LED_B}) {
        gpio_hold_dis((gpio_num_t)pin);
    }
    gpio_deep_sleep_hold_dis();

    lcd.init();
    lcd.setRotation(1);  // horizontal, con el USB a la izquierda
    lcd.setBrightness(255);
    lcd.fillScreen(TFT_BLACK);

    pinMode(PIN_TOUCH_SCLK, OUTPUT);
    pinMode(PIN_TOUCH_MOSI, OUTPUT);
    pinMode(PIN_TOUCH_MISO, INPUT);
    pinMode(PIN_TOUCH_CS, OUTPUT);
    digitalWrite(PIN_TOUCH_CS, HIGH);
    digitalWrite(PIN_TOUCH_SCLK, LOW);

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
    /* Amplificador y retroiluminación apagados, y que sigan así mientras el
     * ESP32 duerme (si no, los pines quedan sueltos) */
    pinMode(PIN_AUDIO_EN, OUTPUT);
    digitalWrite(PIN_AUDIO_EN, HIGH);  // activo a nivel bajo
    lcd.setBrightness(0);
    lcd.sleep();
    pinMode(PIN_TFT_BL, OUTPUT);
    digitalWrite(PIN_TFT_BL, LOW);
    gpio_hold_en((gpio_num_t)PIN_AUDIO_EN);
    gpio_hold_en((gpio_num_t)PIN_TFT_BL);
    /* El táctil también: con CS suelto el XPT2046 podría tomar ruido por
     * órdenes y dejar de avisar de los toques */
    digitalWrite(PIN_TOUCH_CS, HIGH);
    digitalWrite(PIN_TOUCH_SCLK, LOW);
    digitalWrite(PIN_TOUCH_MOSI, LOW);
    gpio_hold_en((gpio_num_t)PIN_TOUCH_CS);
    gpio_hold_en((gpio_num_t)PIN_TOUCH_SCLK);
    gpio_hold_en((gpio_num_t)PIN_TOUCH_MOSI);
    /* LED apagado (ánodo común: a nivel alto) */
    for (uint8_t pin : {PIN_LED_R, PIN_LED_G, PIN_LED_B}) {
        ledcDetachPin(pin);
        pinMode(pin, OUTPUT);
        digitalWrite(pin, HIGH);
        gpio_hold_en((gpio_num_t)pin);
    }
    gpio_deep_sleep_hold_en();

    /* Esperar a que se levante el dedo del deslizador: si no, despertaría
     * nada más dormirse. Cada lectura del táctil deja el XPT2046 en reposo
     * con el aviso de toque activado (orden 0x80). */
    pinMode(PIN_TOUCH_IRQ, INPUT);
    while (digitalRead(PIN_TOUCH_IRQ) == LOW) {
        delay(20);
    }
    delay(300);

    Serial.println("Apagada: toca la pantalla para encenderla");
    Serial.flush();
    esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_TOUCH_IRQ, 0);
    esp_deep_sleep_start();
}
