// =====================================================================
// Kit de conexión FNK0104 -> DAC: TONO DE PRUEBA
//
// Manda audio por I2S (Inter-IC Sound) por el conector «IO2 IO3 IO14 IO21»
// al DAC externo, sin WiFi ni radio: solo comprueba los cables.
// Secuencia que se repite, con pitidos del mismo tono (660 Hz) para que se
// distinga el canal contando pitidos, aunque los altavoces no se vean:
//   1 pitido   solo por el canal IZQUIERDO   (LED rojo)
//   2 pitidos  solo por el canal DERECHO     (LED verde)
//   3 pitidos  por los DOS canales           (LED azul)
//   y 2 s de silencio entre grupos           (LED apagado)
// A -12 dBFS, con subida y bajada suaves para no dar golpes al altavoz.
// =====================================================================
#include <driver/i2s.h>
#include <math.h>

#include "comun.h"

constexpr i2s_port_t PUERTO = I2S_NUM_0;
constexpr int FS = 44100;
constexpr float AMPLITUD = 0.25f;   // -12 dBFS

void iniciarI2S() {
  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  cfg.sample_rate = FS;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.dma_buf_count = 8;
  cfg.dma_buf_len = 256;
  cfg.use_apll = false;
  cfg.tx_desc_auto_clear = true;
  i2s_driver_install(PUERTO, &cfg, 0, nullptr);

  i2s_pin_config_t pines = {};
  pines.mck_io_num = I2S_PIN_NO_CHANGE;   // sin MCLK: el DAC lo saca de BCK
  pines.bck_io_num = PIN_BCK;
  pines.ws_io_num = PIN_LCK;
  pines.data_out_num = PIN_DIN;
  pines.data_in_num = I2S_PIN_NO_CHANGE;
  i2s_set_pin(PUERTO, &pines);
}

// Toca «segundos» de un tono por los canales indicados
void tocar(float hz, bool izq, bool der, float segundos) {
  const int total = (int)(segundos * FS);
  const int rampa = FS / 50;   // 20 ms
  int16_t bloque[256 * 2];
  float fase = 0;
  const float paso = 2.0f * (float)M_PI * hz / FS;
  for (int hecho = 0; hecho < total;) {
    int n = min(256, total - hecho);
    for (int i = 0; i < n; i++) {
      int k = hecho + i;
      float env = 1.0f;
      if (k < rampa) env = (float)k / rampa;
      if (total - k < rampa) env = (float)(total - k) / rampa;
      int16_t s = hz > 0 ? (int16_t)(AMPLITUD * env * 32767.0f * sinf(fase)) : 0;
      fase += paso;
      if (fase > 2 * (float)M_PI) fase -= 2 * (float)M_PI;
      bloque[2 * i] = izq ? s : 0;
      bloque[2 * i + 1] = der ? s : 0;
    }
    size_t escritos;
    i2s_write(PUERTO, bloque, n * 4, &escritos, portMAX_DELAY);
    hecho += n;
  }
}

void setup() {
  prepararPlaca();
  Serial.begin(115200);
  iniciarI2S();
  delay(1500);
  Serial.println("\nKit de conexión FNK0104 -> DAC: tono de prueba");
  Serial.println("IO2 -> DIN   IO14 -> BCK   IO21 -> LCK   3V3 (I2C) -> 3.3V   GND (I2C) -> GND");
}

// «veces» pitidos cortos del mismo tono por los canales indicados
void pitidos(int veces, bool izq, bool der) {
  for (int i = 0; i < veces; i++) {
    tocar(660, izq, der, 0.25f);
    tocar(0, true, true, 0.25f);
  }
}

void loop() {
  led(40, 0, 0);
  Serial.println("1 pitido: canal IZQUIERDO");
  pitidos(1, true, false);
  led(0, 0, 0);
  tocar(0, true, true, 2);

  led(0, 40, 0);
  Serial.println("2 pitidos: canal DERECHO");
  pitidos(2, false, true);
  led(0, 0, 0);
  tocar(0, true, true, 2);

  led(0, 0, 40);
  Serial.println("3 pitidos: los DOS canales");
  pitidos(3, true, true);
  led(0, 0, 0);
  Serial.println("Silencio\n");
  tocar(0, true, true, 3);
}
