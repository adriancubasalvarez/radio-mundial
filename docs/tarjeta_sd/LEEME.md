# Tarjeta microSD (opcional)

La radio funciona sin tarjeta. Si le pones una microSD (formato FAT32), puede usar:

| En la tarjeta | Para qué sirve |
|---|---|
| `mis_emisoras.csv` | Tus propias emisoras. Aparecen en **Buscar → Mis emisoras (SD)**. Ejemplo en esta carpeta. |
| `radio/` | Base de emisoras más grande. Si existe, la radio la usa en lugar de la interna. |
| `radio_mundial_ajustes.json` | Copia automática de tus favoritos y ajustes. Si reflasheas la placa, se recuperan solos. |

## Crear la base grande (`radio/`)

En el PC, desde la carpeta del proyecto:

```
python tools/build_stations.py --destino sd --max 100 --probar
```

Genera la carpeta `sd/radio/` (hasta 100 emisoras comprobadas por país). Copia esa carpeta `radio` a la raíz de la tarjeta.

## En el simulador del PC

La carpeta `sd/` del proyecto hace de tarjeta: si existe, el simulador actúa como si hubiera una SD puesta. Bórrala (o renómbrala) para probar sin tarjeta.
