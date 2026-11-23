# forecast-data

Parser de `forecast.bin` y `short.bin` del Forecast Channel de Wii, en C++17 sin dependencias.
Es el primer paso del port a Android (ver `forecast-android-analisis.md`, sección 9): no necesita GPU
y se puede probar en el PC.

## Uso

```sh
make test
```

Genera muestras con `tests/make_sample.py`, compila y ejecuta los tests.

## Integración continua

El workflow `.github/workflows/forecast-data.yml` (en la raíz del repo) corre en cada push que toque
`forecast-data/`:

- **host**, con g++ y clang: tests, ASan + UBSan y fuzzing de 2000 mutaciones.
- **android-arm64**: compila la librería y el test para `arm64-v8a` (NDK r27c, API 29) con enlace
  estático, ejecuta el test bajo QEMU y sube `libforecast_data.a` y `test_arm64` como artefactos.

Todo se puede reproducir en local con `make test`, `make sanitize` y `make fuzz`.

## Qué hace

- Lee todos los campos en **big-endian**, byte a byte, con comprobación de límites. Así da el mismo
  resultado en el PC (x86, little-endian) que en Android (ARM, little-endian), y no depende del padding de los structs.
- Comprueba lo mismo que `CheckForecastData` del canal: versión, tamaño, CRC-32, idioma (opcional),
  caducidad (opcional, como estado), tipo de unidades, offsets y alineación de las tablas, mensaje del
  servidor, y que cada previsión tenga exactamente un lugar.
- Lee los textos UTF-16BE de los offsets del fichero, con límite de longitud.
- Convierte minutos desde 2000 a fecha.

## Lo verificado

- CRC-32 igual al valor de referencia estándar (`123456789` → `0xCBF43926`) y al de zlib.
- Los datos de muestra se escriben con un script independiente (struct big-endian + zlib) y se leen de vuelta:
  73 comprobaciones.
- Pruebas de mutación: CRC con otro polinomio, lectura little-endian y desactivar la comprobación de
  límites de tablas. En los tres casos los tests fallan.
- ASan y UBSan limpios; fuzzing de 2000 mutaciones (con CRC recalculado) sin cierres.
- El job arm64 aún no se ha ejecutado en GitHub: la cadena NDK → static → QEMU es la parte sin verificar.

## Lo que falta o no está verificado

- **No se ha probado con un `forecast.bin` real.** Solo con muestras generadas a partir de la estructura
  del código. Hace falta uno real para confirmar los offsets y los campos no documentados (`unk*`).
- **Las comprobaciones por día** (`CheckDayForecast` y `CheckWeekForecast` en `ForecastCheck.cpp`) no están portadas.
  Un fichero con datos de día inválidos puede pasar este parser y ser rechazado por el canal.
- **La zona horaria de "minutos desde 2000"** se asume UTC. No está confirmado en el código decompilado.
- Los campos marcados como `unk` se leen pero no se interpretan.
- No se ha comprobado el mensaje del servidor (`messageOffset != 0`) con un fichero real.
