// =====================================================================
// Radio Mundial Boombox — PRUEBA: altavoz Bluetooth (Fase A)
//
// El ESP32 aparece en el móvil como "Radio Mundial BT". Lo que suena en
// el móvil sale por I2S (Inter-IC Sound) al DAC PCM5102A y de ahí al
// amplificador TPA3116D2. El volumen se controla desde el móvil.
//
// Conexión (ver CONEXIONADO_PRUEBA_BT.pdf):
//   DAC BCK -> GPIO 26   DAC LCK -> GPIO 25   DAC DIN -> GPIO 22
//   DAC VCC -> VIN (5 V) DAC GND -> GND
//   DAC FLT, DMP, SCL, FMT -> GND      DAC XMT -> 3.3V del propio DAC
// =====================================================================
#include <Arduino.h>
#include "AudioTools.h"
#include "BluetoothA2DPSink.h"

// Pines I2S: ninguno es de arranque (strapping) en el ESP32
constexpr int PIN_BCK  = 26;  // reloj de bit
constexpr int PIN_LCK  = 25;  // canal izquierdo/derecho (word select)
constexpr int PIN_DIN  = 22;  // datos de audio

constexpr const char* NOMBRE_BT = "Radio Mundial BT";

I2SStream i2s;                       // salida I2S hacia el DAC
BluetoothA2DPSink a2dp(i2s);         // receptor de audio Bluetooth

// Se llama al conectar o desconectar un móvil
void alCambiarConexion(esp_a2d_connection_state_t estado, void*) {
  Serial.printf("Bluetooth: %s\n", a2dp.to_str(estado));
}

// Se llama cuando el móvil cambia el volumen (0–127)
void alCambiarVolumen(int volumen) {
  Serial.printf("Volumen del móvil: %d/127\n", volumen);
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\nRadio Mundial Boombox - prueba Bluetooth");

  auto cfg = i2s.defaultConfig(TX_MODE);
  cfg.pin_bck  = PIN_BCK;
  cfg.pin_ws   = PIN_LCK;
  cfg.pin_data = PIN_DIN;
  i2s.begin(cfg);

  a2dp.set_on_connection_state_changed(alCambiarConexion);
  a2dp.set_on_volumechange(alCambiarVolumen);
  a2dp.set_auto_reconnect(true);     // vuelve a conectar con el último móvil
  a2dp.start(NOMBRE_BT);

  Serial.printf("Listo: busca \"%s\" en el Bluetooth del móvil\n", NOMBRE_BT);
}

void loop() {
  delay(1000);  // todo el trabajo lo hace la librería en sus tareas
}
