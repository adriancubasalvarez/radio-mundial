# Speaker measurements / Medidas de los altavoces

*English first — versión en español más abajo.*

Tools to measure the Radio Mundial Boombox speaker pods with an
inexpensive wireless microphone (DJI Mic) and the Bluetooth test rig
(ESP32 + PCM5102A + TPA3116D2). Only `numpy` and `matplotlib` are needed.

| File | What it does |
|---|---|
| `generar_senales.py` | Creates the test signals in `senales/`: a 69 s session (3 exponential sweeps 16 Hz–22 kHz + 17 bass tones 30–190 Hz), left/right/both versions, and pink noise for level setting. |
| `procedimiento.py` | Creates `PROCEDIMIENTO_MEDIDAS.pdf`, the step-by-step measuring guide (Spanish). |
| `analizar.py` | Analyses one recording: frequency response (1/6 octave), sealed-box fit (fc, Qtc), F3/F6/F10, Linkwitz-transform EQ table, harmonic distortion vs frequency, and THD/THD+N of the bass tones. Writes a PDF report, PNG plots and CSV data to `resultados/<name>/`. |
| `medir.py` | Measures straight from a PC: plays the session over Bluetooth to the ESP32 while recording the DJI Mic receiver over USB, then analyses it (`pip install sounddevice`). `--nivel` checks the level, `--atenuacion` lowers the signal from the PC if the mic clips. |
| `comparar.py` | Overlays several measurements, or splices a near-field and a 50 cm gated measurement into one 20 Hz–20 kHz curve. |

```bash
python medir.py --nivel --atenuacion 12
python medir.py --nombre jvc_cerca_I --tipo cerca --canal I --atenuacion 12
# or, with a recording made elsewhere:
python generar_senales.py
python analizar.py recordings/near_L.wav --nombre jvc_cerca_I --tipo cerca
python analizar.py recordings/50cm_L.wav --nombre jvc_50cm_I --tipo lejos
python comparar.py --unir jvc_cerca_I jvc_50cm_I --salida jvc_completa_I
```

**Method.** Exponential sine sweeps deconvolved by regularised spectral
division (Farina's method), so harmonic distortion separates from the linear
response. The clock difference between the player (ESP32) and the recorder
is measured from the sweep spacing and corrected. Near-field (1 cm) gives
the bass without the room; the 50 cm measurement is time-gated before the
first reflection and is valid above ~1000/gate(ms) Hz.

**Limits.** The microphone is not calibrated and rolls off below ~50 Hz, so
levels are relative and the deepest bass reads slightly low. Comparisons
between measurements are reliable.

---

## En español

Herramientas para medir las cápsulas del Radio Mundial Boombox con un
micrófono inalámbrico barato (DJI Mic) y el montaje Bluetooth de prueba
(ESP32 + PCM5102A + TPA3116D2). Solo hacen falta `numpy` y `matplotlib`.

| Archivo | Qué hace |
|---|---|
| `generar_senales.py` | Crea las señales en `senales/`: una sesión de 69 s (3 barridos exponenciales de 16 Hz a 22 kHz + 17 tonos graves de 30 a 190 Hz), en versión izquierda, derecha y ambos, y ruido rosa para ajustar el nivel. |
| `procedimiento.py` | Crea `PROCEDIMIENTO_MEDIDAS.pdf`, la guía paso a paso para medir. |
| `analizar.py` | Analiza una grabación: respuesta en frecuencia (1/6 de octava), ajuste de caja cerrada (fc, Qtc), F3/F6/F10, tabla de ecualización (transformada de Linkwitz), distorsión armónica según la frecuencia y THD/THD+N de los tonos graves. Deja un informe PDF, gráficos PNG y datos CSV en `resultados/<nombre>/`. |
| `medir.py` | Mide directamente desde el PC: reproduce la sesión por Bluetooth hacia el ESP32 mientras graba el receptor del DJI Mic por USB, y la analiza (`pip install sounddevice`). `--nivel` comprueba el volumen y `--atenuacion` baja la señal desde el PC si el micrófono satura. |
| `comparar.py` | Superpone varias medidas, o une la de campo cercano con la de 50 cm en una curva de 20 Hz a 20 kHz. |

**Método.** Barridos exponenciales deconvolucionados por división espectral
regularizada (método de Farina): la distorsión armónica queda separada de la
respuesta lineal. La diferencia de relojes entre el ESP32 y la grabadora se
mide con la separación entre barridos y se corrige. El campo cercano (1 cm)
da el grave sin la habitación; la medida a 50 cm se recorta antes del primer
rebote y vale por encima de ~1000/ventana(ms) Hz.

**Límites.** El micrófono no está calibrado y pierde nivel por debajo de
~50 Hz: los niveles son relativos y el grave más profundo sale algo bajo.
Las comparaciones entre medidas sí son fiables.
