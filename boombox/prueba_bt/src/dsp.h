// =====================================================================
// Corrección DSP (Digital Signal Processing) de las cápsulas del boombox
//
// Cadena, igual en los dos canales:
//   ganancia previa -> paso alto -> corte de la joroba -> refuerzo del grave
//
// Cada filtro es un biquad (filtro digital de 2º orden) calculado con las
// fórmulas del «Audio EQ Cookbook» de Robert Bristow-Johnson, las mismas
// que usa boombox/medidas/disenar_eq.py para predecir el resultado.
//
// No depende de ninguna librería: se puede llevar tal cual al firmware de
// la radio (ESP32-S3).
// =====================================================================
#pragma once

#include <stddef.h>
#include <stdint.h>

struct AjustesDSP {
  bool activo = true;          // false: solo la ganancia previa (para comparar)
  float ganancia_db = -7.0f;   // margen para que el refuerzo no recorte
  float pasoalto_hz = 65.0f;   // 0 = sin paso alto
  float joroba_hz = 167.0f;    // filtro de pico que baja la resonancia
  float joroba_db = -9.8f;
  float joroba_q = 2.11f;
  float grave_hz = 75.0f;      // filtro de pico que refuerza el grave
  float grave_db = 9.0f;
  float grave_q = 0.91f;
};

// Filtro biquad en forma directa II transpuesta, en coma flotante
class Biquad {
 public:
  void pasoAlto(float fs, float f0, float q);
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

class CorreccionDSP {
 public:
  // Recalcula los filtros (al cambiar la frecuencia de muestreo o los ajustes)
  void configurar(float fs, const AjustesDSP& ajustes);

  // Procesa audio estéreo de 16 bits intercalado (I, D, I, D...) en el sitio
  void procesar(int16_t* muestras, size_t n_tramas);

  // Muestras que llegaron al máximo y se recortaron desde el último reinicio
  uint32_t recortes() const { return recortes_; }
  void borrarRecortes() { recortes_ = 0; }

 private:
  static constexpr int N_FILTROS = 3;
  Biquad filtros_[2][N_FILTROS];   // [canal][filtro]
  float ganancia_ = 1.0f;
  bool activo_ = true;
  uint32_t recortes_ = 0;
};
