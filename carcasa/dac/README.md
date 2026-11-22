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
- **DAC:** WCMCU-5102 (32.5 × 24 mm) in a low frame, with its jack at the bottom end (the feet side), flush against the wall on the USB-C side, so every cable comes out on the same side. Round **10 mm** hole, so the plug's 8 mm rubber body goes in, centred 20 mm above the inside of the bottom wall and 4 mm above the bottom of the board (measured on the first print).
- **Round light tube** (Ø30 mm) behind the board's RGB LED (46 mm from the USB-C edge of the board), with **19 holes of 4 mm** in rings (like the original back cover, but fewer and bigger), all inside the tube: the LED still shows through the back.
- **Battery:** 2 × 18650 **in parallel** (1S, 68 × 38 × 22 mm, with 1 mm clearance), lying between the light tube and the speaker-side wall. The tube and the side wall stop it lengthwise, and small stops above and below keep it in place.
- **Battery switch:** a small rocker switch (body 20 × 13 mm, 23 mm deep) lying sideways in the back, above the light tube. It cuts the battery's + wire: switched off, nothing drains the battery, but USB-C cannot charge it either. Snap-in, 20.4 × 13.4 mm hole. If it has a light, leave its third pin unconnected.
- **Centring rim:** the same 2 mm rim as the original back cover, which goes into the front. It sits on a 3.2 mm ledge around the opening (flat on top, 45° underneath, no supports). The opening is 119.6 × 54.6 mm: put the battery in slightly tilted, speaker end first.
- **Two rear feet:** low wedges (0.1 to 1 mm) in the same 5° plane as the front's feet: the four feet sit flat on the table and the radio leans back 5°, like the original case. Centre of gravity checked with the battery: it does not tip over.

**Size:** the whole radio is about 130 × 69 × 44.5 mm (front 17 mm + backpack 27.5 mm).

**Holding the parts:** **two zip ties** (up to 4.8 mm wide) around the battery: each one goes under a small bridge on each side of the battery (4 bridges inside, nothing shows outside). The DAC: a few dots of hot glue on the long edges of the board, not on the jack or the pins. No hot glue on the battery.

**Screws:** 4 × M3 × 10 mm from the back, in counterbores open towards the corner (5 mm of the screw goes into the front).

**Print:** PETG, bottom of the backpack on the bed, open side up, no supports.

**First print (21 Nov):** the backpack was modelled on the wrong side of the front (mirrored), so the feet and the jack hole came out wrong. Fixed in this version, which also adds the centring rim and the round jack hole. **To check on the next print:** the jack hole, the rim fit and how the radio stands.

Wiring: [`kit_dac`](../../kit_dac/) (PDF guide).

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
- **DAC:** WCMCU-5102 (32,5 × 24 mm) en un marco bajo, con el jack en el extremo de abajo (el lado de las patas), a ras de la pared del lado del USB-C: todos los cables salen por el mismo lado. Agujero **redondo de 10 mm**, para que entre la goma de 8 mm de la clavija, con el centro a 20 mm de la cara interior de abajo y a 4 mm de la cara de abajo de la placa (medido en la primera impresión).
- **Tubo de luz redondo** (Ø30 mm) detrás del LED RGB de la placa (a 46 mm del borde del USB-C), con **19 agujeros de 4 mm** en anillos (como la tapa original, pero menos y más grandes), todos dentro del tubo: el LED se sigue viendo por detrás.
- **Batería:** 2 × 18650 **en paralelo** (1S, 68 × 38 × 22 mm, con 1 mm de holgura), tumbada entre el tubo de luz y la pared del lado del altavoz. El tubo y la pared la frenan a lo largo, y unos topes arriba y abajo la sujetan.
- **Interruptor de la batería:** un basculante pequeño (cuerpo de 20 × 13 mm y 23 mm de fondo) tumbado en el fondo, encima del tubo de luz. Corta el cable + de la batería: apagado no gasta nada, pero tampoco carga por USB-C. Va a presión en un agujero de 20,4 × 13,4 mm. Si tiene luz, la tercera pata se deja sin conectar.
- **Borde de centrado:** el mismo marco de 2 mm de la tapa original, que entra en el frontal. Se apoya en una repisa de 3,2 mm alrededor de la boca (plana por arriba y en chaflán de 45° por debajo, sin soportes). La boca queda de 119,6 × 54,6 mm: la batería entra algo inclinada, primero por el lado del altavoz.
- **Dos patas traseras:** tacos bajos en cuña (de 0,1 a 1 mm) en el mismo plano de 5° que las patas del frontal: las cuatro apoyan enteras en la mesa y la radio se echa 5° hacia atrás, como la carcasa original. Centro de gravedad comprobado con la batería: no se vuelca.

**Tamaño:** la radio entera mide unos 130 × 69 × 44,5 mm (frontal de 17 mm + mochila de 27,5 mm).

**Sujeción:** **dos bridas** (de hasta 4,8 mm de ancho) alrededor de la batería: cada una pasa por debajo de un puentecito a cada lado (4 puentes por dentro, no se ve nada por fuera). El DAC, con unos puntos de pegamento caliente en los bordes largos de la placa, sin tocar el jack ni los pines. Nada de pegamento caliente en la batería.

**Tornillos:** 4 × M3 × 10 mm por detrás, en cajeados abiertos hacia la esquina (entran 5 mm en el frontal).

**Impresión:** PETG, el fondo de la mochila sobre la cama, abierta hacia arriba, sin soportes.

**Primera impresión (21-nov):** la mochila estaba modelada en el lado equivocado del frontal (en espejo), así que las patas y el agujero del jack salieron mal. Corregido en esta versión, que además añade el borde de centrado y el agujero redondo del jack. **A comprobar en la siguiente impresión:** el agujero del jack, el ajuste del borde y cómo apoya la radio.

Conexiones: [`kit_dac`](../../kit_dac/) (guía en PDF).

Regenerar: `python mochila_dac.py` y `python vistas.py` (hace falta CadQuery).

Licencia: **CC BY-NC-SA 4.0**.
