# Radio Mundial ESP32 — Briefing del proyecto

> Archivo de contexto para Claude Code. Léelo entero antes de proponer nada.

## Reglas de trabajo (obligatorias)

1. **Primero propón un plan y espera mi autorización explícita** antes de escribir código o tocar archivos. Una fase cada vez.
2. **Responde siempre en español.**
3. **Cada vez que aparezcan siglas, escribe su significado** la primera vez en cada respuesta (por ejemplo: I2S (Inter-IC Sound)).
4. **No inventes pines ni datos del hardware.** Compruébalos en la documentación oficial de Freenove (tutorial y esquemas del FNK0104). Si no los encuentras, pregúntame.
5. Cambios pequeños y verificables: cada fase debe compilar y poder probarse en la placa real antes de pasar a la siguiente.

## Qué es

Una radio por Internet de escritorio con **mapamundi táctil a color**:
tocas una región del mapa → ves sus países → tocas un país → lista de emisoras → tocas una emisora → suena.

Está pensada como **proyecto para compartir con la comunidad maker** (manual paso a paso, carcasa 3D propia en Cults3D, enlaces de afiliado a la placa). Por eso prioriza:
- **Mínima fricción para el usuario final**: flashear, conectar al WiFi desde el móvil y listo.
- **Cero servidores propios** y cero claves de API.
- Código claro y comentado en español.

Inspirado en el concepto de Radio Garden, pero **con nombre, logo y estética propios**: no usar su nombre, marca ni diseño.

## Hardware

- **Placa:** Freenove ESP32-S3 CYD (Cheap Yellow Display) 2,8", SKU (Stock Keeping Unit) **FNK0104**.
  - ESP32-S3 doble núcleo a 240 MHz, WiFi.
  - Pantalla IPS (In-Plane Switching) 240×320, **táctil capacitivo**.
  - Audio integrado con altavoz (preparada para asistentes de voz). **Verificar** en el esquema qué códec y amplificador lleva (probablemente ES8311 + NS4150) y qué pines usa.
  - **Verificar** tamaño de flash y PSRAM (Pseudo-Static RAM). El diseño asume PSRAM disponible; si no la hay, avísame porque cambia el tamaño de los búferes.
  - Documentación: http://freenove.com/fnk0104 y el repositorio de Freenove en GitHub.
- **Salida de audio opcional (fase posterior):** módulo **PCM5102A** (DAC (Digital-to-Analog Converter) estéreo I2S) para salida de línea hi-fi. El firmware debe permitir elegir en la configuración: altavoz integrado o PCM5102A externo.

## Arquitectura propuesta

### En el PC (herramientas en Python, carpeta `tools/`)
1. **Generador de la base de emisoras** (`tools/build_stations.py`):
   - Descarga emisoras de la API pública de radio-browser.info (usar un servidor espejo, sin clave).
   - Filtra: códec MP3 o AAC (Advanced Audio Coding), bitrate ≤ 192 kbps, `lastcheckok = 1`, URL HTTP o HTTPS directa. **Excluir HLS (HTTP Live Streaming) y DASH (Dynamic Adaptive Streaming over HTTP)**, que el ESP32 no reproduce.
   - Ordena por popularidad (votos y clics) y limita a un máximo configurable por país (por ejemplo, 30).
   - Genera un archivo compacto por país (JSON (JavaScript Object Notation) mínimo o CSV) más un índice de países y regiones.
2. **Generador de mapas** (`tools/build_maps.py`):
   - A partir de datos de dominio público (por ejemplo, Natural Earth), genera las imágenes del mapamundi y de cada región a 320×240 (o 240×320, según la orientación elegida), en el formato que mejor cargue la librería gráfica (RGB565 o JPEG).
   - Genera también el **mapa de zonas táctiles**: polígonos o rectángulos simplificados de cada región y país, con zonas grandes porque se toca con el dedo.
3. Todo lo generado se guarda en `data/` y se sube a la placa (LittleFS).

### En el ESP32 (firmware, PlatformIO + Arduino)
- **Audio:** librería ESP32-audioI2S (schreibfaul1) o equivalente probada en ESP32-S3. Streaming en un núcleo y la interfaz en el otro.
- **Gráficos y táctil:** LVGL (Light and Versatile Graphics Library) sobre LovyanGFX o TFT_eSPI, según lo que mejor soporte esta placa. Seguir los ejemplos de Freenove.
- **Pantallas:**
  1. Mapamundi con unas 8–10 regiones táctiles.
  2. Mapa de la región con sus países.
  3. Lista de emisoras del país (desplazable).
  4. Reproduciendo: emisora, país, título de la canción si llega (metadatos ICY), volumen, pausa y atrás.
  5. Favoritos y "emisora aleatoria del mundo".
- **Configuración:** portal cautivo WiFi en el primer arranque, igual que en mi dashboard de YouTube: el usuario se conecta desde el móvil y elige su red. Página web de ajustes con volumen por defecto, brillo, salida de audio y favoritos.
- **Persistencia:** última emisora, volumen y favoritos en NVS (Non-Volatile Storage).
- **Robustez:** reconexión automática del WiFi y del stream; si una emisora falla, mostrar un aviso y volver a la lista sin colgarse.

## Fases (proponer el plan detallado de cada una antes de implementarla)

1. **Base del proyecto:** estructura de carpetas, `platformio.ini`, tabla de particiones, pantalla funcionando con un "hola mundo" y el táctil leyendo coordenadas.
2. **Audio:** reproducir una URL fija por el altavoz integrado, con control de volumen.
3. **Herramientas en el PC:** `build_stations.py` y `build_maps.py`, con datos de prueba para pocas regiones.
4. **Navegación:** mapa → región → país → lista → reproducir, leyendo de LittleFS.
5. **Portal WiFi y web de ajustes.**
6. **Pulido:** favoritos, aleatorio, metadatos, animaciones, pantalla de reposo.
7. **Opcional:** salida PCM5102A seleccionable.
8. **Documentación para la comunidad:** manual en español (y luego en inglés) para Instructables, lista de materiales y guía de flasheo.

## Estructura de carpetas sugerida

```
radio-mundial/
├── CLAUDE.md
├── platformio.ini
├── partitions.csv
├── src/          firmware
├── include/
├── data/         emisoras y mapas generados (LittleFS)
├── tools/        scripts de Python del PC
└── docs/         manual, esquemas y notas
```

## Limitaciones conocidas a tener en cuenta

- El táctil es pequeño (2,8"): zonas grandes, sin zoom libre.
- No todas las emisoras funcionan aunque vayan en VLC. La base se filtra en el PC y se prueba una muestra.
- El ESP32-S3 no tiene Bluetooth clásico, así que no puede enviar audio a altavoces Bluetooth (A2DP, Advanced Audio Distribution Profile).
- Las emisoras cambian de URL con el tiempo: el script del PC debe poder regenerar la base fácilmente, y más adelante se puede añadir una actualización desde la web de ajustes.
