// =====================================================================
// Kit de conexión FNK0104 -> DAC: IDENTIFICAR LOS CABLES
//
// Pone a 3,3 V un pin de audio cada vez, durante 5 segundos, y los demás
// a 0 V. El LED de la placa dice cuál está activo:
//   ROJO  -> IO2  (irá a DIN)
//   VERDE -> IO14 (irá a BCK)
//   AZUL  -> IO21 (irá a LCK)
//   BLANCO tenue -> todos a 0 V (pausa de 3 s)
//
// Con el polímetro en tensión continua: punta negra en el cable GND del
// conector «I2C» (o del «UART», es la misma masa) y punta roja en cada
// cable del conector «IO2 IO3 IO14 IO21». El que marca ~3,3 V es el del
// color del LED.
// =====================================================================
#include "comun.h"

struct Paso {
  int pin;
  const char* nombre;
  const char* destino;
  uint8_t r, g, b;
};

const Paso PASOS[] = {
    {PIN_DIN, "IO2", "DIN", 40, 0, 0},
    {PIN_BCK, "IO14", "BCK", 0, 40, 0},
    {PIN_LCK, "IO21", "LCK", 0, 0, 40},
};

void todosAbajo() {
  for (const Paso& p : PASOS) digitalWrite(p.pin, LOW);
}

void setup() {
  prepararPlaca();
  Serial.begin(115200);
  for (const Paso& p : PASOS) pinMode(p.pin, OUTPUT);
  todosAbajo();
  delay(1500);
  Serial.println("\nKit de conexión FNK0104 -> DAC: identificar los cables");
  Serial.println("Polímetro: negro en GND (conector I2C), rojo en cada cable del conector IO2 IO3 IO14 IO21.");
}

void loop() {
  for (const Paso& p : PASOS) {
    todosAbajo();
    digitalWrite(p.pin, HIGH);
    led(p.r, p.g, p.b);
    Serial.printf("%s a 3,3 V (LED %s) -> ese cable va a %s del DAC\n", p.nombre,
                  p.r ? "ROJO" : p.g ? "VERDE" : "AZUL", p.destino);
    delay(5000);
  }
  todosAbajo();
  led(8, 8, 8);
  Serial.println("Todos a 0 V\n");
  delay(3000);
}
