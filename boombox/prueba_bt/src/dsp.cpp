#include "dsp.h"

#include <math.h>

static constexpr float PI_F = 3.14159265358979f;

static float db_a_lineal(float db) { return powf(10.0f, db / 20.0f); }
static float lineal_a_db(float x) { return x > 1e-9f ? 20.0f * log10f(x) : -180.0f; }
// Coeficiente de un filtro de un polo con constante de tiempo «segundos»
static float coef_tiempo(float fs, float segundos) { return 1.0f - expf(-1.0f / (segundos * fs)); }

void Biquad::normalizar(float b0, float b1, float b2, float a0, float a1, float a2) {
  b0_ = b0 / a0;
  b1_ = b1 / a0;
  b2_ = b2 / a0;
  a1_ = a1 / a0;
  a2_ = a2 / a0;
}

void Biquad::neutro() { normalizar(1, 0, 0, 1, 0, 0); }

void Biquad::pasoAlto(float fs, float f0, float q) {
  const float w = 2 * PI_F * f0 / fs;
  const float alfa = sinf(w) / (2 * q);
  const float c = cosf(w);
  normalizar((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + alfa, -2 * c, 1 - alfa);
}

void Biquad::pasoBajo(float fs, float f0, float q) {
  const float w = 2 * PI_F * f0 / fs;
  const float alfa = sinf(w) / (2 * q);
  const float c = cosf(w);
  normalizar((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + alfa, -2 * c, 1 - alfa);
}

void Biquad::pico(float fs, float f0, float ganancia_db, float q) {
  const float A = powf(10.0f, ganancia_db / 40.0f);
  const float w = 2 * PI_F * f0 / fs;
  const float alfa = sinf(w) / (2 * q);
  const float c = cosf(w);
  normalizar(1 + alfa * A, -2 * c, 1 - alfa * A, 1 + alfa / A, -2 * c, 1 - alfa / A);
}

void CorreccionDSP::configurar(float fs, const AjustesDSP& a) {
  const bool fs_nueva = fs != fs_;
  fs_ = fs;
  activo_ = a.activo;
  limitador_ = a.limitador;
  ganancia_obj_ = db_a_lineal(a.ganancia_db);
  if (fs_nueva) ganancia_ = ganancia_obj_;
  paso_ganancia_ = coef_tiempo(fs, 0.02f);     // la ganancia cambia en ~20 ms
  graves_max_ = db_a_lineal(a.graves_max_db);
  grave_G_ = db_a_lineal(a.grave_db > 0 ? a.grave_db : 0.0f);
  techo_ = db_a_lineal(a.techo_db);
  c_ataque_ = coef_tiempo(fs, 0.005f);
  c_retorno_ = coef_tiempo(fs, 0.3f);
  c_env_ = expf(-1.0f / (0.05f * fs));          // la envolvente cae en ~50 ms
  c_lim_ = coef_tiempo(fs, 0.1f);

  const float nyquist = fs * 0.45f;            // frecuencias por encima no tienen sentido
  for (int c = 0; c < 2; c++) {
    if (a.pasoalto_hz > 0 && a.pasoalto_hz < nyquist) {
      hp_[c].pasoAlto(fs, a.pasoalto_hz, 0.7071f);   // Butterworth
    } else {
      hp_[c].neutro();
    }
    if (a.joroba_hz > 0 && a.joroba_hz < nyquist && a.joroba_q > 0) {
      joroba_[c].pico(fs, a.joroba_hz, a.joroba_db, a.joroba_q);
    } else {
      joroba_[c].neutro();
    }
    if (a.grave_hz > 0 && a.grave_hz < nyquist && a.grave_q > 0) {
      grave_[c].pico(fs, a.grave_hz, a.grave_db, a.grave_q);
    } else {
      grave_[c].neutro();
    }
    detector_[c].pasoBajo(fs, 150.0f, 0.7071f);       // mide solo el grave
    if (fs_nueva) {
      hp_[c].reiniciar();
      joroba_[c].reiniciar();
      grave_[c].reiniciar();
      detector_[c].reiniciar();
    }
  }
}

void CorreccionDSP::procesar(int16_t* m, size_t n_tramas) {
  for (size_t i = 0; i < n_tramas; i++) {
    ganancia_ += (ganancia_obj_ - ganancia_) * paso_ganancia_;
    float y[2];
    float graves = 0.0f;
    for (int c = 0; c < 2; c++) {
      float x = m[2 * i + c] * (ganancia_ / 32768.0f);
      if (activo_) {
        x = hp_[c].procesar(x);
        x = joroba_[c].procesar(x);
        const float refuerzo = grave_[c].procesar(x) - x;   // lo que añade el filtro
        x += (limitador_ ? k_ : 1.0f) * refuerzo;
        const float g = fabsf(detector_[c].procesar(x));
        if (g > graves) graves = g;
      }
      y[c] = x;
    }

    if (limitador_) {
      // Refuerzo dinámico del grave
      env_ = graves > env_ ? graves : env_ * c_env_;
      if (env_ > graves_max_) {
        k_ -= k_ * c_ataque_;
      } else {
        k_ += (1.0f - k_) * c_retorno_;
      }
      if (k_ < k_min_) k_min_ = k_;
      // Limitador final: baja al instante y vuelve en ~100 ms
      const float pico = fmaxf(fabsf(y[0]), fabsf(y[1]));
      if (pico * g_lim_ > techo_) {
        g_lim_ = techo_ / pico;
      } else {
        g_lim_ += (1.0f - g_lim_) * c_lim_;
      }
      if (g_lim_ < g_lim_min_) g_lim_min_ = g_lim_;
      y[0] *= g_lim_;
      y[1] *= g_lim_;
    }

    for (int c = 0; c < 2; c++) {
      float s = y[c] * 32768.0f;
      const float a = fabsf(y[c]);
      if (a > pico_) pico_ = a;
      if (s > 32767.0f) {
        s = 32767.0f;
        recortes_++;
      } else if (s < -32768.0f) {
        s = -32768.0f;
        recortes_++;
      }
      m[2 * i + c] = (int16_t)lrintf(s);
    }
  }
}

EstadoDSP CorreccionDSP::leerEstado() {
  EstadoDSP e;
  e.pico_dbfs = lineal_a_db(pico_);
  // Reducción del refuerzo en la frecuencia del filtro de grave: con el
  // refuerzo completo la ganancia es G; con el factor k queda 1 + k·(G − 1)
  e.reduccion_graves_db = lineal_a_db(grave_G_) - lineal_a_db(1.0f + k_min_ * (grave_G_ - 1.0f));
  e.reduccion_limitador_db = -lineal_a_db(g_lim_min_);
  e.recortes = recortes_;
  pico_ = 0;
  k_min_ = k_;
  g_lim_min_ = g_lim_;
  recortes_ = 0;
  return e;
}
