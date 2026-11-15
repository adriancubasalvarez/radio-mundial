// =====================================================================
// Corrección DSP (Digital Signal Processing) de las cápsulas del boombox
//
// Cadena, igual en los dos canales:
//   ganancia previa -> paso alto -> corte de la joroba
//     -> refuerzo del grave DINÁMICO -> limitador final
//
// Refuerzo dinámico: el refuerzo del grave se suma aparte y se escala con
// un factor k (1 = refuerzo completo, 0 = sin refuerzo). Un detector mide
// el grave a la salida; si pasa de «graves_max», k baja deprisa (unos
// 5 ms) y luego vuelve despacio (unos 300 ms). Así, a volumen normal el
// grave va reforzado y a volumen alto el cono no se fuerza.
//
// Limitador final: red de seguridad para que la señal nunca pase de
// «techo» ni se recorte.
//
// Los filtros son biquads (filtros digitales de 2º orden) calculados con
// las fórmulas del «Audio EQ Cookbook» de Robert Bristow-Johnson, las
// mismas que usa boombox/medidas/disenar_eq.py.
//
// No depende de ninguna librería: se puede llevar tal cual al firmware de
// la radio (ESP32-S3).
// =====================================================================
#pragma once

#include <stddef.h>
#include <stdint.h>

struct AjustesDSP {
  bool activo = true;          // false: solo ganancia previa y limitador (para comparar)
  float ganancia_db = -7.0f;   // margen para que el refuerzo no recorte
  float pasoalto_hz = 65.0f;   // 0 = sin paso alto
  float joroba_hz = 167.0f;    // filtro de pico que baja la resonancia
  float joroba_db = -9.8f;
  float joroba_q = 2.11f;
  float grave_hz = 75.0f;      // filtro de pico que refuerza el grave
  float grave_db = 9.0f;
  float grave_q = 0.91f;
  bool limitador = true;       // refuerzo dinámico + limitador final
  float graves_max_db = -6.0f; // nivel de grave (dBFS) a partir del que se reduce el refuerzo
  float techo_db = -1.0f;      // nivel máximo de salida (dBFS)
};

// Filtro biquad en forma directa II transpuesta, en coma flotante
class Biquad {
 public:
  void pasoAlto(float fs, float f0, float q);
  void pasoBajo(float fs, float f0, float q);
  void pico(float fs, float f0, float ganancia_db, float q);
  void neutro();               // deja pasar la señal sin cambios
  void reiniciar() { z1_ = z2_ = 0.0f; }

  inline float procesar(float x) {
    float y = b0_ * x + z1_;
    z1_ = b1_ * x - a1_ * y + z2_;
    z2_ = b2_ * x - a2_ * y;
    return y;
  }

 private:
  void normalizar(float b0, float b1, float b2, float a0, float a1, float a2);
  float b0_ = 1, b1_ = 0, b2_ = 0, a1_ = 0, a2_ = 0;
  float z1_ = 0, z2_ = 0;
};

// Lo que ha pasado desde la última consulta (para «ver»)
struct EstadoDSP {
  float pico_dbfs;             // pico de la salida
  float reduccion_graves_db;   // lo máximo que se ha quitado al refuerzo del grave
  float reduccion_limitador_db;
  uint32_t recortes;           // muestras recortadas (debería ser 0)
};

class CorreccionDSP {
 public:
  // Recalcula los filtros (al cambiar la frecuencia de muestreo o los
  // ajustes). No borra la memoria de los filtros salvo si cambia la
  // frecuencia, para que el cambio no dé un chasquido.
  void configurar(float fs, const AjustesDSP& ajustes);

  // Procesa audio estéreo de 16 bits intercalado (I, D, I, D...) en el sitio
  void procesar(int16_t* muestras, size_t n_tramas);

  // Devuelve el estado desde la última llamada y lo pone a cero
  EstadoDSP leerEstado();

 private:
  Biquad hp_[2], joroba_[2], grave_[2], detector_[2];
  float fs_ = 0;
  bool activo_ = true, limitador_ = true;
  float ganancia_ = 1.0f, ganancia_obj_ = 1.0f, paso_ganancia_ = 0.001f;
  float k_ = 1.0f;                       // fracción del refuerzo del grave
  float env_ = 0.0f;                     // envolvente del grave a la salida
  float graves_max_ = 0.5f, techo_ = 0.89f;
  float grave_G_ = 1.0f;                 // refuerzo completo del grave, en lineal
  float g_lim_ = 1.0f;                   // ganancia del limitador final
  float c_ataque_ = 0, c_retorno_ = 0, c_env_ = 0, c_lim_ = 0;
  // Estadísticas (se leen desde otra tarea: valores sueltos, sin cerrojo)
  float pico_ = 0, k_min_ = 1.0f, g_lim_min_ = 1.0f;
  uint32_t recortes_ = 0;
};
