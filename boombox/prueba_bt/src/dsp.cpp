#include "dsp.h"

#include <math.h>

static constexpr float PI_F = 3.14159265358979f;

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

void Biquad::pico(float fs, float f0, float ganancia_db, float q) {
  const float A = powf(10.0f, ganancia_db / 40.0f);
  const float w = 2 * PI_F * f0 / fs;
  const float alfa = sinf(w) / (2 * q);
  const float c = cosf(w);
  normalizar(1 + alfa * A, -2 * c, 1 - alfa * A, 1 + alfa / A, -2 * c, 1 - alfa / A);
}

void CorreccionDSP::configurar(float fs, const AjustesDSP& a) {
  activo_ = a.activo;
  ganancia_ = powf(10.0f, a.ganancia_db / 20.0f);
  const float nyquist = fs * 0.45f;   // frecuencias por encima no tienen sentido
  for (int c = 0; c < 2; c++) {
    Biquad* f = filtros_[c];
    if (a.pasoalto_hz > 0 && a.pasoalto_hz < nyquist) {
      f[0].pasoAlto(fs, a.pasoalto_hz, 0.7071f);   // Butterworth
    } else {
      f[0].neutro();
    }
    if (a.joroba_hz > 0 && a.joroba_hz < nyquist && a.joroba_q > 0) {
      f[1].pico(fs, a.joroba_hz, a.joroba_db, a.joroba_q);
    } else {
      f[1].neutro();
    }
    if (a.grave_hz > 0 && a.grave_hz < nyquist && a.grave_q > 0) {
      f[2].pico(fs, a.grave_hz, a.grave_db, a.grave_q);
    } else {
      f[2].neutro();
    }
    for (int i = 0; i < N_FILTROS; i++) f[i].reiniciar();
  }
}

void CorreccionDSP::procesar(int16_t* m, size_t n_tramas) {
  for (size_t i = 0; i < n_tramas; i++) {
    for (int c = 0; c < 2; c++) {
      float x = m[2 * i + c] * ganancia_;
      if (activo_) {
        for (int k = 0; k < N_FILTROS; k++) x = filtros_[c][k].procesar(x);
      }
      if (x > 32767.0f) {
        x = 32767.0f;
        recortes_++;
      } else if (x < -32768.0f) {
        x = -32768.0f;
        recortes_++;
      }
      m[2 * i + c] = (int16_t)lrintf(x);
    }
  }
}
