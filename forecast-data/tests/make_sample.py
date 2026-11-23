#!/usr/bin/env python3
"""Writes sample forecast.bin and short.bin files with the channel's layout, plus
tests/expected.h with the values the C++ test must read back.

The writer is independent of src/forecast_data.cpp: it uses struct (big-endian)
and zlib.crc32 to build the files, so a mismatch in the parser shows up as a
test failure instead of being hidden by shared code.

Usage: python3 tests/make_sample.py [outdir]
"""
import struct
import sys
import zlib
from datetime import datetime
from pathlib import Path

OUT = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).parent / "sample"
OUT.mkdir(parents=True, exist_ok=True)
HERE = Path(__file__).parent


def minutes_since_2000(dt):
    return int((dt - datetime(2000, 1, 1)).total_seconds() // 60)


def utf16be_z(text):
    return text.encode("utf-16-be") + b"\x00\x00"


class Blob:
    """Growing byte buffer with 4-byte aligned allocation."""

    def __init__(self, start):
        self.buf = bytearray(start)

    def align4(self):
        while len(self.buf) % 4:
            self.buf.append(0)

    def add(self, data):
        self.align4()
        off = len(self.buf)
        self.buf += data
        return off

    def add_string(self, text):
        return self.add(utf16be_z(text))


def day(weather, parts, max_c, min_c, percent, wind_dir, wind_kmh, wind_mph, uv, laundry, pollen):
    # DayForecast, 0x1C bytes. Diff fields are set to 0 for the sample.
    return struct.pack(
        ">H4H4b4b4B6B",
        weather, *parts,
        max_c, 0, min_c, 0,          # maxC, maxDiffC, minC, minDiffC
        int(max_c * 9 / 5 + 32), 0, int(min_c * 9 / 5 + 32), 0,  # F values
        *percent,
        wind_dir, wind_kmh, wind_mph, uv, laundry, pollen,
    )


def week(weather, max_c, min_c, percent):
    # WeekForecast, 0x08 bytes.
    return struct.pack(
        ">HbbbbBB",
        weather, max_c, min_c, int(max_c * 9 / 5 + 32), int(min_c * 9 / 5 + 32), percent, 0,
    )


def build_forecast():
    now_dt = datetime(2026, 11, 22, 0, 0)
    closing = minutes_since_2000(datetime(2026, 11, 23, 0, 0))  # in the future
    opening = minutes_since_2000(datetime(2026, 11, 22, 0, 0))

    HEADER = 0x58
    body = Blob(HEADER)

    # Strings first, so the tables can point at them.
    text_sunny = body.add_string("Soleado")
    text_cloudy = body.add_string("Nublado")
    text_uv_low = body.add_string("Bajo")
    text_uv_high = body.add_string("Alto")
    text_laundry = body.add_string("Secar al aire")
    text_pollen = body.add_string("Moderado")

    place_madrid_name = body.add_string("Madrid")
    place_madrid_region = body.add_string("Comunidad de Madrid")
    place_madrid_country = body.add_string("España")
    place_sevilla_name = body.add_string("Sevilla")
    place_sevilla_region = 0  # no region: offset 0 means "no string"

    # Weather types: code, icon (0x8000 = night), text offset.
    wt_entries = [
        struct.pack(">HHI", 1, 0x0001, text_sunny),
        struct.pack(">HHI", 2, 0x0002, text_cloudy),
    ]
    # IndexText tables.
    uv_entries = [struct.pack(">B3xI", 1, text_uv_low), struct.pack(">B3xI", 7, text_uv_high)]
    laundry_entries = [struct.pack(">B3xI", 1, text_laundry)]
    pollen_entries = [struct.pack(">B3xI", 2, text_pollen)]

    # Places: id, name, region, country, longitude, latitude, unk.
    places = [
        struct.pack(">IIIIHHBBH", 1001, place_madrid_name, place_madrid_region, place_madrid_country,
                    0xFFE0, 0x0FF4, 0, 0, 0),
        struct.pack(">IIIIHHBBH", 1002, place_sevilla_name, place_sevilla_region, 0,
                    0xFFF8, 0x0F97, 0, 0, 0),
    ]

    # Forecast for Madrid (id 1001). Two days, seven weeks. unkC = 1.
    days_madrid = [
        day(1, (1, 2, 1, 1), 21, 12, (10, 0xFF, 0xFF, 0xFF), 4, 15, 9, 3, 2, 1),
        day(2, (2, 2, 2, 2), 18, 10, (60, 70, 50, 40), 6, 20, 12, 5, 1, 1),
    ]
    week_madrid = [week(1 + (i % 2), 20 - i, 9 - i, 10 * i) for i in range(7)]
    # Layout inside ForecastEntry (0x80 bytes): id, time, offset, unkC, unkD[3], days[2] at 0x10, week[7] at 0x48.
    entry = bytearray(0x80)
    struct.pack_into(">IIIB", entry, 0x0, 1001, closing, 0, 1)
    entry[0x10:0x10 + 2 * 0x1C] = b"".join(days_madrid)
    entry[0x48:0x48 + 7 * 0x08] = b"".join(week_madrid)
    assert len(entry) == 0x80

    # Forecast for Sevilla (id 1002) is absent on purpose, so the file has one forecast.
    # Summary entry (0x48 bytes): id, time, unkC, days[2] at 0x10.
    summary = bytearray(0x48)
    struct.pack_into(">II", summary, 0x0, 1001, closing)
    summary[0x10:0x10 + 2 * 0x1C] = b"".join(days_madrid)

    # Build tables, aligned.
    off_places = body.add(b"".join(places))
    off_forecasts = body.add(bytes(entry))
    off_summaries = body.add(bytes(summary))
    off_wt = body.add(b"".join(wt_entries))
    off_uv = body.add(b"".join(uv_entries))
    off_laundry = body.add(b"".join(laundry_entries))
    off_pollen = body.add(b"".join(pollen_entries))

    buf = body.buf
    size = len(buf)

    # Header. Fields follow ForecastHeader (0x58 bytes).
    hdr = bytearray(HEADER)
    struct.pack_into(">I", hdr, 0x0, 0x00000003)            # version (high 16 bits zero)
    struct.pack_into(">I", hdr, 0x4, size)                  # size
    struct.pack_into(">I", hdr, 0x8, 0)                     # crc, filled below
    struct.pack_into(">I", hdr, 0xC, opening)               # openTime
    struct.pack_into(">I", hdr, 0x10, closing)              # time
    hdr[0x14] = 0                                           # country
    hdr[0x18] = 1                                           # language (sample value)
    hdr[0x19] = 2                                           # unitType: Celsius
    hdr[0x1A] = 0                                           # unk1A
    struct.pack_into(">I", hdr, 0x1C, 0)                    # messageOffset (none)
    struct.pack_into(">II", hdr, 0x20, 1, off_forecasts)    # numForecasts, offset
    struct.pack_into(">II", hdr, 0x28, 1, off_summaries)    # numSummaries, offset
    struct.pack_into(">II", hdr, 0x30, len(wt_entries), off_wt)
    struct.pack_into(">II", hdr, 0x38, len(uv_entries), off_uv)
    struct.pack_into(">II", hdr, 0x40, len(laundry_entries), off_laundry)
    struct.pack_into(">II", hdr, 0x48, len(pollen_entries), off_pollen)
    struct.pack_into(">II", hdr, 0x50, len(places), off_places)
    buf[0:HEADER] = hdr

    # CRC-32 over bytes [0xC, size), stored at 0x8.
    crc = zlib.crc32(bytes(buf[0xC:])) & 0xFFFFFFFF
    struct.pack_into(">I", buf, 0x8, crc)

    (OUT / "forecast.bin").write_bytes(bytes(buf))
    return {
        "size": size,
        "crc": crc,
        "closing": closing,
        "opening": opening,
        "off_forecasts": off_forecasts,
        "off_places": off_places,
    }


def build_short(closing_minutes):
    HEADER = 0x24
    body = Blob(HEADER)
    entries = [
        struct.pack(">II", 1001, closing_minutes) + b"\x00" * 4 + struct.pack(">HBbbBBB", 1, 0,
                                                                              14, 57, 180, 15, 9) + b"\x00" * 4,
        struct.pack(">II", 1002, closing_minutes) + b"\x00" * 4 + struct.pack(">HBbbBBB", 2, 0,
                                                                              -3, 27, 90, 8, 5) + b"\x00" * 4,
    ]
    off_entries = body.add(b"".join(entries))
    buf = body.buf
    size = len(buf)

    hdr = bytearray(HEADER)
    struct.pack_into(">I", hdr, 0x0, 0x00000001)
    struct.pack_into(">I", hdr, 0x4, size)
    struct.pack_into(">I", hdr, 0x8, 0)
    hdr[0xC] = 0
    struct.pack_into(">I", hdr, 0x10, closing_minutes)
    struct.pack_into(">I", hdr, 0x14, 0)
    hdr[0x18] = 1
    struct.pack_into(">II", hdr, 0x1C, len(entries), off_entries)
    buf[0:HEADER] = hdr
    crc = zlib.crc32(bytes(buf[0xC:])) & 0xFFFFFFFF
    struct.pack_into(">I", buf, 0x8, crc)
    (OUT / "short.bin").write_bytes(bytes(buf))
    return size


info = build_forecast()
short_size = build_short(info["closing"])

expected = f"""// GENERATED by tests/make_sample.py. Do not edit by hand.
#ifndef FORECAST_EXPECTED_H
#define FORECAST_EXPECTED_H
#include <cstdint>
namespace expected {{
constexpr uint32_t kForecastSize = {info['size']};
constexpr uint32_t kForecastCrc = 0x{info['crc']:08X};
constexpr uint32_t kClosingMinutes = {info['closing']};
constexpr uint32_t kOpeningMinutes = {info['opening']};
constexpr uint32_t kNumPlaces = 2;
constexpr uint32_t kNumForecasts = 1;
constexpr uint32_t kNumSummaries = 1;
constexpr uint32_t kNumWeatherTypes = 2;
constexpr uint32_t kNumUV = 2;
constexpr uint32_t kNumLaundry = 1;
constexpr uint32_t kNumPollen = 1;
constexpr uint32_t kMadridId = 1001;
constexpr uint32_t kSevillaId = 1002;
constexpr uint16_t kDay0Weather = 1;
constexpr int8_t kDay0MaxC = 21;
constexpr int8_t kDay0MinC = 12;
constexpr uint8_t kDay0Percent0 = 10;
constexpr uint8_t kDay0Percent1 = 0xFF;
constexpr uint8_t kDay0WindKmh = 15;
constexpr uint16_t kDay1Weather = 2;
constexpr int8_t kDay1MaxC = 18;
constexpr uint8_t kDay1Percent3 = 40;
constexpr uint16_t kWeek0Weather = 1;
constexpr int8_t kWeek0MaxC = 20;
constexpr uint8_t kWeek6Percent = 60;
constexpr uint16_t kNowMadridWeather = 1;
constexpr int8_t kNowMadridTempC = 14;
constexpr int8_t kNowSevillaTempC = -3;
constexpr uint32_t kShortSize = {short_size};
}}
#endif
"""
(HERE / "expected.h").write_text(expected, encoding="utf-8")
print("forecast.bin:", info["size"], "bytes, crc 0x%08X" % info["crc"])
print("short.bin:", short_size, "bytes")
print("wrote", OUT, "and tests/expected.h")
