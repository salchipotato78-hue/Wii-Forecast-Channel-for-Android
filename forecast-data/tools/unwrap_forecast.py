#!/usr/bin/env python3
"""Convierte la salida de RiiConnect24 File-Maker (forecast.bin / short.bin del servidor)
en un fichero plano que lee forecast-data/ y la app Android.

Formato de la salida (sign_file en Channels/Forecast_Channel/forecast.py):
  - 64 bytes de relleno a cero
  - 256 bytes de firma RSA-SHA1 (clave de 2048 bits)
  - cuerpo comprimido con LZ10 (nlzss)
Al descomprimirlo queda una cabecera de transporte de 12 bytes (0, tamaño, CRC-32)
seguida del fichero real. Este script quita firma, descomprime y quita esa cabecera.

Uso:
  python3 unwrap_forecast.py <entrada> <salida>
"""
import os
import sys
import tempfile

import nlzss

SIGNED_HEADER = 64 + 256   # relleno + firma RSA de 2048 bits
TRANSPORT_HEADER = 12      # u32 0, u32 tamaño, u32 CRC-32


def unwrap(data: bytes) -> bytes:
    if len(data) <= SIGNED_HEADER:
        raise ValueError("el archivo es demasiado corto para tener firma y datos")
    fd, comp_path = tempfile.mkstemp()
    out_fd, plain_path = tempfile.mkstemp()
    os.close(out_fd)
    try:
        with os.fdopen(fd, "wb") as f:
            f.write(data[SIGNED_HEADER:])
        nlzss.decode_file(comp_path, plain_path)
        with open(plain_path, "rb") as f:
            plain = f.read()
    finally:
        os.remove(comp_path)
        os.remove(plain_path)
    if len(plain) <= TRANSPORT_HEADER:
        raise ValueError("el contenido descomprimido es demasiado corto")
    return plain[TRANSPORT_HEADER:]


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(__doc__)
        sys.exit(1)
    with open(sys.argv[1], "rb") as f:
        raw = f.read()
    plain = unwrap(raw)
    with open(sys.argv[2], "wb") as f:
        f.write(plain)
    print("OK: %d bytes escritos en %s" % (len(plain), sys.argv[2]))
