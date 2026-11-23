# Forecast Channel para Android

Port del Forecast Channel de Wii (decompilación `forecast-main`, versión USA/NTSC v7).

Estado actual: **base de datos y primera app**. La app lee `forecast.bin` y `short.bin` y muestra
el pronóstico. El canal todavía no se ejecuta: faltan el shim de GX, OS y NAND, y el WAD del canal.

## Estructura

| Carpeta | Contenido |
|---|---|
| `forecast-data/` | Parser C++ de `forecast.bin` y `short.bin`, con tests, sanitizers y fuzzing |
| `android/` | App Android (Kotlin + JNI) que compila el parser con el NDK |
| `.github/workflows/` | `forecast-data.yml` (tests en host y arm64) y `android.yml` (APK de depuración) |

## Build

Todo se compila en GitHub Actions. No hace falta instalar nada en local:

- **forecast-data**: tests, sanitizers y fuzzing en g++ y clang, y compilación para `arm64-v8a`.
- **android**: APK de depuración para `arm64-v8a` (minSdk 29). Se sube como artefacto.

En local, si quieres: `cd forecast-data && make test`.

## Uso de la app

- **Probar con muestra**: carga los ficheros de ejemplo incluidos en la app.
- **Abrir forecast.bin…**: elige un `forecast.bin` del dispositivo. Se muestra el resumen.

## Lo que no está hecho

- Ejecutar el canal (GX, OS, NAND, NWC24 y entrada táctil). Ver `forecast-data/README.md` y el documento de análisis.
- Comprobaciones por día del parser (`CheckDayForecast`, `CheckWeekForecast`).
- Zona horaria de las fechas (se muestran como UTC, por verificar).
- Probar con un `forecast.bin` real del servidor.

## Licencia

El código del parser y de la app es propio. El repo de origen (`forecast-main`) está bajo CC0 1.0.
Este repo no incluye ningún fichero del juego ni del WAD del canal.
