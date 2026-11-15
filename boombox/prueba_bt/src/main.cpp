// =====================================================================
// Radio Mundial Boombox — PRUEBA: altavoz Bluetooth con corrección DSP
//
// El ESP32 aparece como "Radio Mundial BT". Lo que llega por Bluetooth
// pasa por la corrección DSP (dsp.h) y sale por I2S (Inter-IC Sound) al
// DAC PCM5102A y de ahí al amplificador TPA3116D2.
//
// Fase A: altavoz Bluetooth. Fase B: corrección DSP con refuerzo de
// grave dinámico y limitador, ajustable por el puerto serie (115200
// baudios). Escribe «ayuda» en el monitor serie, o usa
// boombox/medidas/dsp.py desde el PC.
//
// Conexión (ver CONEXIONADO_PRUEBA_BT.pdf):
//   DAC BCK -> GPIO 26   DAC LCK -> GPIO 25   DAC DIN -> GPIO 22
//   DAC VCC -> VIN (5 V) DAC GND -> GND
//   DAC FLT, DMP, SCL, FMT -> GND      DAC XMT -> 3.3V del propio DAC
// =====================================================================
#include <Arduino.h>
#include <Preferences.h>
#include "AudioTools.h"
#include "BluetoothA2DPSink.h"
#include "dsp.h"

// Pines I2S: ninguno es de arranque (strapping) en el ESP32
constexpr int PIN_BCK  = 26;  // reloj de bit
constexpr int PIN_LCK  = 25;  // canal izquierdo/derecho (word select)
constexpr int PIN_DIN  = 22;  // datos de audio

constexpr const char* NOMBRE_BT = "Radio Mundial BT";

// ---------------------------------------------------------------- DSP

AjustesDSP ajustes;                  // los que se ven y se cambian por serie
CorreccionDSP correccion;            // los usa la tarea de audio
portMUX_TYPE cerrojo = portMUX_INITIALIZER_UNLOCKED;
volatile bool ajustes_cambiados = true;
Preferences memoria;

// Flujo intermedio: recibe el audio del Bluetooth, lo corrige y lo manda
// al DAC. También se entera de la frecuencia de muestreo de la conexión.
class DspStream : public AudioStream {
 public:
  explicit DspStream(AudioStream& salida) : salida_(salida) {}

  void setAudioInfo(AudioInfo info) override {
    AudioStream::setAudioInfo(info);
    salida_.setAudioInfo(info);
    fs_ = info.sample_rate;
    ajustes_cambiados = true;
    Serial.printf("Audio: %d Hz, %d canales, %d bits\n", info.sample_rate, info.channels,
                  info.bits_per_sample);
  }

  size_t write(const uint8_t* datos, size_t len) override {
    if (ajustes_cambiados) {
      AjustesDSP copia;
      portENTER_CRITICAL(&cerrojo);
      copia = ajustes;
      ajustes_cambiados = false;
      portEXIT_CRITICAL(&cerrojo);
      correccion.configurar((float)fs_, copia);
    }
    // Se procesa por trozos en un búfer propio (los datos llegan como const)
    size_t hecho = 0;
    while (hecho < len) {
      size_t n = min(len - hecho, sizeof(bufer_));
      n -= n % 4;                    // tramas enteras: 2 canales × 16 bits
      if (n == 0) break;
      memcpy(bufer_, datos + hecho, n);
      const int64_t t0 = esp_timer_get_time();
      correccion.procesar(bufer_, n / 4);
      us_proceso_ += esp_timer_get_time() - t0;
      us_audio_ += (int64_t)(n / 4) * 1000000 / fs_;
      salida_.write((const uint8_t*)bufer_, n);
      hecho += n;
    }
    return len;
  }

  int frecuencia() const { return fs_; }

  // Porcentaje del tiempo de audio que el procesador dedica al DSP (y lo reinicia)
  float carga() {
    const float c = us_audio_ > 0 ? 100.0f * (float)us_proceso_ / (float)us_audio_ : 0.0f;
    us_proceso_ = us_audio_ = 0;
    return c;
  }

 private:
  AudioStream& salida_;
  int fs_ = 44100;
  int16_t bufer_[512];
  volatile int64_t us_proceso_ = 0, us_audio_ = 0;
};

I2SStream i2s;                       // salida I2S hacia el DAC
DspStream dsp(i2s);
BluetoothA2DPSink a2dp(dsp);         // receptor de audio Bluetooth

void cargarAjustes() {
  memoria.begin("dsp", true);
  if (memoria.isKey("ganancia")) {
    ajustes.activo = memoria.getBool("activo", ajustes.activo);
    ajustes.ganancia_db = memoria.getFloat("ganancia", ajustes.ganancia_db);
    ajustes.pasoalto_hz = memoria.getFloat("pasoalto", ajustes.pasoalto_hz);
    ajustes.joroba_hz = memoria.getFloat("joroba_f", ajustes.joroba_hz);
    ajustes.joroba_db = memoria.getFloat("joroba_g", ajustes.joroba_db);
    ajustes.joroba_q = memoria.getFloat("joroba_q", ajustes.joroba_q);
    ajustes.grave_hz = memoria.getFloat("grave_f", ajustes.grave_hz);
    ajustes.grave_db = memoria.getFloat("grave_g", ajustes.grave_db);
    ajustes.grave_q = memoria.getFloat("grave_q", ajustes.grave_q);
    ajustes.limitador = memoria.getBool("limitador", ajustes.limitador);
    ajustes.graves_max_db = memoria.getFloat("graves_max", ajustes.graves_max_db);
    ajustes.techo_db = memoria.getFloat("techo", ajustes.techo_db);
    Serial.println("DSP: ajustes cargados de la memoria");
  } else {
    Serial.println("DSP: ajustes de fábrica");
  }
  memoria.end();
}

void guardarAjustes() {
  memoria.begin("dsp", false);
  memoria.putBool("activo", ajustes.activo);
  memoria.putFloat("ganancia", ajustes.ganancia_db);
  memoria.putFloat("pasoalto", ajustes.pasoalto_hz);
  memoria.putFloat("joroba_f", ajustes.joroba_hz);
  memoria.putFloat("joroba_g", ajustes.joroba_db);
  memoria.putFloat("joroba_q", ajustes.joroba_q);
  memoria.putFloat("grave_f", ajustes.grave_hz);
  memoria.putFloat("grave_g", ajustes.grave_db);
  memoria.putFloat("grave_q", ajustes.grave_q);
  memoria.putBool("limitador", ajustes.limitador);
  memoria.putFloat("graves_max", ajustes.graves_max_db);
  memoria.putFloat("techo", ajustes.techo_db);
  memoria.end();
  Serial.println("ok guardado");
}

void mostrar() {
  Serial.printf("dsp %s | ganancia %.1f dB | pasoalto %.0f Hz | joroba %.0f Hz %.1f dB Q %.2f"
                " | grave %.0f Hz %.1f dB Q %.2f | limitador %s, gravesmax %.1f dB, techo %.1f dB"
                " | %d Hz\n",
                ajustes.activo ? "on" : "off", ajustes.ganancia_db, ajustes.pasoalto_hz,
                ajustes.joroba_hz, ajustes.joroba_db, ajustes.joroba_q, ajustes.grave_hz,
                ajustes.grave_db, ajustes.grave_q, ajustes.limitador ? "on" : "off",
                ajustes.graves_max_db, ajustes.techo_db, dsp.frecuencia());
}

// Lo que ha pasado desde la última consulta
void estado() {
  const EstadoDSP e = correccion.leerEstado();
  Serial.printf("pico %.1f dBFS | grave reducido hasta %.1f dB | limitador hasta %.1f dB"
                " | recortes %u | carga %.1f %%\n",
                e.pico_dbfs, e.reduccion_graves_db, e.reduccion_limitador_db,
                (unsigned)e.recortes, dsp.carga());
}

void ayuda() {
  Serial.println(
      "Comandos:\n"
      "  ver                     ajustes y estado desde la última consulta\n"
      "  estado                  picos, reducción del limitador y carga del procesador\n"
      "  dsp on | dsp off        corrección activada / solo ganancia previa y limitador\n"
      "  ganancia DB             ganancia previa (negativa, p. ej. -7)\n"
      "  pasoalto HZ             paso alto de protección (0 = quitar)\n"
      "  joroba HZ DB Q          filtro que baja la resonancia (p. ej. 167 -9.8 2.1)\n"
      "  grave HZ DB Q           filtro que refuerza el grave (p. ej. 75 9 0.9)\n"
      "  limitador on | off      refuerzo de grave dinámico y limitador final\n"
      "  gravesmax DB            grave a partir del que se reduce el refuerzo (p. ej. -6)\n"
      "  techo DB                nivel máximo de salida (p. ej. -1)\n"
      "  guardar                 guarda los ajustes (se mantienen al apagar)\n"
      "  fabrica                 vuelve a los ajustes de fábrica (sin guardar)");
}

// Interpreta una línea recibida por el puerto serie
void orden(String linea) {
  linea.trim();
  if (linea.isEmpty()) return;
  char cmd[16] = {0};
  float a = 0, b = 0, c = 0;
  int n = sscanf(linea.c_str(), "%15s %f %f %f", cmd, &a, &b, &c);
  String s(cmd);
  AjustesDSP nuevo = ajustes;
  if (s == "ver") {
    mostrar();
    estado();
    return;
  } else if (s == "estado") {
    estado();
    return;
  } else if (s == "ayuda" || s == "help") {
    ayuda();
    return;
  } else if (s == "dsp") {
    nuevo.activo = linea.endsWith("on");
  } else if (s == "ganancia" && n >= 2) {
    nuevo.ganancia_db = constrain(a, -30.0f, 6.0f);
  } else if (s == "pasoalto" && n >= 2) {
    nuevo.pasoalto_hz = constrain(a, 0.0f, 500.0f);
  } else if (s == "joroba" && n >= 4) {
    nuevo.joroba_hz = a;
    nuevo.joroba_db = constrain(b, -20.0f, 12.0f);
    nuevo.joroba_q = constrain(c, 0.1f, 10.0f);
  } else if (s == "grave" && n >= 4) {
    nuevo.grave_hz = a;
    nuevo.grave_db = constrain(b, -20.0f, 12.0f);
    nuevo.grave_q = constrain(c, 0.1f, 10.0f);
  } else if (s == "limitador") {
    nuevo.limitador = linea.endsWith("on");
  } else if (s == "gravesmax" && n >= 2) {
    nuevo.graves_max_db = constrain(a, -40.0f, 0.0f);
  } else if (s == "techo" && n >= 2) {
    nuevo.techo_db = constrain(a, -20.0f, 0.0f);
  } else if (s == "guardar") {
    guardarAjustes();
    return;
  } else if (s == "fabrica") {
    nuevo = AjustesDSP();
  } else {
    Serial.println("error: comando desconocido (escribe «ayuda»)");
    return;
  }
  portENTER_CRITICAL(&cerrojo);
  ajustes = nuevo;
  ajustes_cambiados = true;
  portEXIT_CRITICAL(&cerrojo);
  Serial.print("ok ");
  mostrar();
}

// ------------------------------------------------------------ Bluetooth

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
  Serial.println("\nRadio Mundial Boombox - prueba Bluetooth con DSP");
  cargarAjustes();

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
  mostrar();
  Serial.println("Escribe «ayuda» para ver los comandos del DSP");
}

void loop() {
  static String linea;
  while (Serial.available()) {
    char ch = (char)Serial.read();
    if (ch == '\n' || ch == '\r') {
      orden(linea);
      linea = "";
    } else if (linea.length() < 80) {
      linea += ch;
    }
  }
  delay(10);
}
