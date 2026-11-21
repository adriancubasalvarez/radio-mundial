# Radio Mundial — case with DAC and battery («backpack»)

*[Español más abajo](#español)* · **Work in progress: not printed or tested yet.**

A deeper back cover for the [small case](../pequena/) that holds a **PCM5102A DAC
with a line-out jack** and a **battery**. The front is **unchanged**: the backpack
replaces the flat back cover and uses the same 4 screw holes.

![Inside, with the battery (blue) and the DAC (purple)](img/mochila_con_bateria.png)

| File | What it is |
|---|---|
| `mochila_dac.step` / `.stl` / `.3mf` | The backpack, in print orientation (bottom on the bed) |
| `mochila_dac_montaje.step` | The same part in the front's coordinates, to check the assembly in CAD |
| `mochila_dac.py` | Parametric model (CadQuery): every size is a variable at the top |
| `vistas.py` | Renders the views in `img/` |

**Inside** (from the USB-C side):
- **DAC:** WCMCU-5102 (32.5 × 24 mm) in a low frame, with its jack at the top end, flush against the wall on the USB-C side, so every cable comes out on the same side. Round **6 mm** hole for the plug, centred 20 mm below the inside of the top wall and 4 mm above the bottom of the board (measured on the first print).
- **Round light tube** (Ø30 mm) behind the board's RGB LED (46 mm from the USB-C edge of the board), with **19 holes of 4 mm** in rings (like the original back cover, but fewer and bigger), all inside the tube: the LED still shows through the back.
- **Battery:** 2 × 18650 **in parallel** (1S, 68 × 38 × 22 mm, with 1 mm clearance), lying between the light tube and the speaker-side wall. The tube and the side wall stop it lengthwise, and small stops above and below keep it in place.
- **Centring rim:** the same 2 mm rim as the original back cover, which goes into the front.
- **Two rear feet:** low wedges (0.1 to 1 mm) in the same 5° plane as the front's feet: the four feet sit flat on the table and the radio leans back 5°, like the original case. Centre of gravity checked with the battery: it does not tip over.

**Size:** the whole radio is about 130 × 69 × 44.5 mm (front 17 mm + backpack 27.5 mm).

**Holding the parts:** **two zip ties** (up to 4.8 mm wide) around the battery: each one goes under a small bridge on each side of the battery (4 bridges inside, nothing shows outside). The DAC: a few dots of hot glue on the long edges of the board, not on the jack or the pins. No hot glue on the battery.

**Screws:** 4 × M3 × 10 mm from the back, in counterbores (5 mm of the screw goes into the front).

**Print:** PETG, bottom of the backpack on the bed, open side up, no supports.

**First print (21 Nov):** the backpack was modelled on the wrong side of the front (mirrored), so the feet and the jack hole came out wrong. Fixed in this version, which also adds the centring rim and the round jack hole. **To check on the next print:** the jack hole, the rim fit and how the radio stands.

Wiring: [`boombox/fnk_conexion`](../../boombox/fnk_conexion/) (PDF guide).

Rebuild: `python mochila_dac.py` and `python vistas.py` (needs CadQuery).

License: **CC BY-NC-SA 4.0**.

---

## Español

**En desarrollo: aún sin imprimir ni probar.**

Tapa trasera más profunda para la [carcasa pequeña](../pequena/), con sitio para un
**DAC PCM5102A con salida de jack** y una **batería**. El **frontal no cambia**: la
mochila sustituye a la tapa plana y usa los mismos 4 tornillos.

| Archivo | Qué es |
|---|---|
| `mochila_dac.step` / `.stl` / `.3mf` | La mochila, ya orientada para imprimir (fondo sobre la cama) |
| `mochila_dac_montaje.step` | La misma pieza en los ejes del frontal, para comprobar el montaje en CAD |
| `mochila_dac.py` | Modelo paramétrico (CadQuery): todas las medidas son variables al principio |
| `vistas.py` | Genera las vistas de `img/` |

**Dentro** (desde el lado del USB-C):
- **DAC:** WCMCU-5102 (32,5 × 24 mm) en un marco bajo, con el jack en el extremo de arriba, a ras de la pared del lado del USB-C: todos los cables salen por el mismo lado. Agujero **redondo de 6 mm** para la clavija, con el centro a 20 mm de la cara interior de arriba y a 4 mm de la cara de abajo de la placa (medido en la primera impresión).
- **Tubo de luz redondo** (Ø30 mm) detrás del LED RGB de la placa (a 46 mm del borde del USB-C), con **19 agujeros de 4 mm** en anillos (como la tapa original, pero menos y más grandes), todos dentro del tubo: el LED se sigue viendo por detrás.
- **Batería:** 2 × 18650 **en paralelo** (1S, 68 × 38 × 22 mm, con 1 mm de holgura), tumbada entre el tubo de luz y la pared del lado del altavoz. El tubo y la pared la frenan a lo largo, y unos topes arriba y abajo la sujetan.
- **Borde de centrado:** el mismo marco de 2 mm de la tapa original, que entra en el frontal.
- **Dos patas traseras:** tacos bajos en cuña (de 0,1 a 1 mm) en el mismo plano de 5° que las patas del frontal: las cuatro apoyan enteras en la mesa y la radio se echa 5° hacia atrás, como la carcasa original. Centro de gravedad comprobado con la batería: no se vuelca.

**Tamaño:** la radio entera mide unos 130 × 69 × 44,5 mm (frontal de 17 mm + mochila de 27,5 mm).

**Sujeción:** **dos bridas** (de hasta 4,8 mm de ancho) alrededor de la batería: cada una pasa por debajo de un puentecito a cada lado (4 puentes por dentro, no se ve nada por fuera). El DAC, con unos puntos de pegamento caliente en los bordes largos de la placa, sin tocar el jack ni los pines. Nada de pegamento caliente en la batería.

**Tornillos:** 4 × M3 × 10 mm por detrás, en cajeados (entran 5 mm en el frontal).

**Impresión:** PETG, el fondo de la mochila sobre la cama, abierta hacia arriba, sin soportes.

**Primera impresión (21-nov):** la mochila estaba modelada en el lado equivocado del frontal (en espejo), así que las patas y el agujero del jack salieron mal. Corregido en esta versión, que además añade el borde de centrado y el agujero redondo del jack. **A comprobar en la siguiente impresión:** el agujero del jack, el ajuste del borde y cómo apoya la radio.

Conexiones: [`boombox/fnk_conexion`](../../boombox/fnk_conexion/) (guía en PDF).

Regenerar: `python mochila_dac.py` y `python vistas.py` (hace falta CadQuery).

Licencia: **CC BY-NC-SA 4.0**.
