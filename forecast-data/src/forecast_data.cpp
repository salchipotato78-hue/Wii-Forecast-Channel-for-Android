#include "forecast_data/forecast_data.h"

#include <cstring>

namespace forecast {
namespace {

// ---------------------------------------------------------------------------
// Big-endian readers with bounds checks. Every read goes through these, so an
// offset past the end of the buffer becomes an error instead of a crash.
// ---------------------------------------------------------------------------

inline bool InRange(size_t size, uint64_t offset, uint64_t length) {
    return offset <= size && length <= size - offset;
}

inline uint8_t ReadU8(const uint8_t* p) { return p[0]; }
inline int8_t ReadS8(const uint8_t* p) { return static_cast<int8_t>(p[0]); }
inline uint16_t ReadU16(const uint8_t* p) {
    return static_cast<uint16_t>((p[0] << 8) | p[1]);
}
inline uint32_t ReadU32(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}

// Reads a null-terminated UTF-16BE string starting at `offset`. The string
// must end before the end of the file. Returns false if it does not.
bool ReadString(const uint8_t* data, size_t size, uint64_t offset, std::u16string& out) {
    out.clear();
    if (!InRange(size, offset, 0) || offset >= size) {
        return false;
    }
    for (uint64_t pos = offset; pos + 2 <= size; pos += 2) {
        const char16_t unit = static_cast<char16_t>(ReadU16(data + pos));
        if (unit == 0) {
            return true;
        }
        out.push_back(unit);
    }
    return false;  // ran off the end without a terminator
}

void SetError(Error& error, Error::Kind kind, const char* detail) {
    error.kind = kind;
    error.detail = detail;
}

// ---------------------------------------------------------------------------
// Struct readers. Field order and offsets follow ForecastData.h.
// ---------------------------------------------------------------------------

ForecastHeader ReadForecastHeader(const uint8_t* p) {
    ForecastHeader h;
    h.version = ReadU32(p + 0x0);
    h.size = ReadU32(p + 0x4);
    h.crc = ReadU32(p + 0x8);
    h.openTime = ReadU32(p + 0xC);
    h.time = ReadU32(p + 0x10);
    h.country = ReadU8(p + 0x14);
    h.language = ReadU8(p + 0x18);
    h.unitType = ReadU8(p + 0x19);
    h.unk1A = ReadU8(p + 0x1A);
    h.messageOffset = ReadU32(p + 0x1C);
    h.numForecasts = ReadU32(p + 0x20);
    h.forecastOffset = ReadU32(p + 0x24);
    h.numSummaries = ReadU32(p + 0x28);
    h.summaryOffset = ReadU32(p + 0x2C);
    h.numWeatherTypes = ReadU32(p + 0x30);
    h.weatherTypeOffset = ReadU32(p + 0x34);
    h.numUVIndices = ReadU32(p + 0x38);
    h.uvIndexOffset = ReadU32(p + 0x3C);
    h.numLaundryIndices = ReadU32(p + 0x40);
    h.laundryIndexOffset = ReadU32(p + 0x44);
    h.numPollenIndices = ReadU32(p + 0x48);
    h.pollenIndexOffset = ReadU32(p + 0x4C);
    h.numPlaces = ReadU32(p + 0x50);
    h.placeOffset = ReadU32(p + 0x54);
    return h;
}

ShortHeader ReadShortHeader(const uint8_t* p) {
    ShortHeader h;
    h.version = ReadU32(p + 0x0);
    h.size = ReadU32(p + 0x4);
    h.crc = ReadU32(p + 0x8);
    h.openTime = ReadU8(p + 0xC);
    h.time = ReadU32(p + 0x10);
    h.country = ReadU32(p + 0x14);
    h.language = ReadU8(p + 0x18);
    h.numEntries = ReadU32(p + 0x1C);
    return h;
}

Day ReadDay(const uint8_t* p) {
    Day d;
    d.weather = ReadU16(p + 0x0);
    for (size_t i = 0; i < 4; ++i) {
        d.weatherParts[i] = ReadU16(p + 0x2 + 2 * i);
    }
    d.maxC = ReadS8(p + 0xA);
    d.maxDiffC = ReadS8(p + 0xB);
    d.minC = ReadS8(p + 0xC);
    d.minDiffC = ReadS8(p + 0xD);
    d.maxF = ReadS8(p + 0xE);
    d.maxDiffF = ReadS8(p + 0xF);
    d.minF = ReadS8(p + 0x10);
    d.minDiffF = ReadS8(p + 0x11);
    for (size_t i = 0; i < 4; ++i) {
        d.percent[i] = ReadU8(p + 0x12 + i);
    }
    d.windDirection = ReadU8(p + 0x16);
    d.windSpeedKmh = ReadU8(p + 0x17);
    d.windSpeedMph = ReadU8(p + 0x18);
    d.uvIndex = ReadU8(p + 0x19);
    d.laundryIndex = ReadU8(p + 0x1A);
    d.pollenIndex = ReadU8(p + 0x1B);
    return d;
}

Week ReadWeek(const uint8_t* p) {
    Week w;
    w.weather = ReadU16(p + 0x0);
    w.maxC = ReadS8(p + 0x2);
    w.minC = ReadS8(p + 0x3);
    w.maxF = ReadS8(p + 0x4);
    w.minF = ReadS8(p + 0x5);
    w.percent = ReadU8(p + 0x6);
    return w;
}

// Reads the place's strings too. A zero offset means "no string".
bool ReadPlace(const uint8_t* data, size_t size, const uint8_t* p, Place& place, Error& error) {
    place.id = ReadU32(p + 0x0);
    place.longitude = ReadU16(p + 0x10);
    place.latitude = ReadU16(p + 0x12);
    const uint32_t nameOffset = ReadU32(p + 0x4);
    const uint32_t regionOffset = ReadU32(p + 0x8);
    const uint32_t countryOffset = ReadU32(p + 0xC);
    if (!ReadString(data, size, nameOffset, place.name)) {
        SetError(error, Error::kBadString, "place name");
        return false;
    }
    if (regionOffset != 0 && !ReadString(data, size, regionOffset, place.region)) {
        SetError(error, Error::kBadString, "place region");
        return false;
    }
    if (countryOffset != 0 && !ReadString(data, size, countryOffset, place.country)) {
        SetError(error, Error::kBadString, "place country");
        return false;
    }
    return true;
}

// Checks that `count` entries of `entrySize` bytes fit in the file at `offset`,
// and that the offset is 4-byte aligned, as the channel requires.
bool CheckTable(size_t size, uint32_t offset, uint32_t count, size_t entrySize, Error& error,
                const char* name) {
    if (count == 0) {
        return true;
    }
    if (offset == 0) {
        error.kind = Error::kBadField;
        error.detail = std::string(name) + ": offset is 0 but count is not";
        return false;
    }
    if ((offset & 3) != 0) {
        error.kind = Error::kTableMisaligned;
        error.detail = name;
        return false;
    }
    if (!InRange(size, offset, static_cast<uint64_t>(count) * entrySize)) {
        error.kind = Error::kTableOutOfRange;
        error.detail = name;
        return false;
    }
    return true;
}

// Forecast/summary days in the channel are checked by CheckDayForecast and
// CheckWeekForecast (ForecastCheck.cpp). Those are not ported yet.

// Converts a text table (weather types, UV, laundry, pollen) entry's text.
bool ReadIndexTexts(const uint8_t* data, size_t size, uint32_t tableOffset, uint32_t count,
                    std::vector<IndexText>& out, Error& error) {
    out.clear();
    out.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        const uint8_t* p = data + tableOffset + i * kIndexTextSize;
        IndexText t;
        t.code = ReadU8(p);
        const uint32_t textOffset = ReadU32(p + 0x4);
        if (textOffset != 0 && !ReadString(data, size, textOffset, t.text)) {
            SetError(error, Error::kBadString, "index text");
            return false;
        }
        out.push_back(t);
    }
    return true;
}

}  // namespace

// ---------------------------------------------------------------------------
// CRC-32, table-driven. Equivalent to NETCalcCRC32 in the channel.
// ---------------------------------------------------------------------------

uint32_t Crc32(const uint8_t* data, size_t size) {
    static uint32_t table[256];
    static bool ready = false;
    if (!ready) {
        for (uint32_t n = 0; n < 256; ++n) {
            uint32_t c = n;
            for (int k = 0; k < 8; ++k) {
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            }
            table[n] = c;
        }
        ready = true;
    }
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < size; ++i) {
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return ~crc;
}

// ---------------------------------------------------------------------------
// forecast.bin
// ---------------------------------------------------------------------------

bool ParseForecast(const uint8_t* data, size_t size, const ParseOptions& options, ForecastFile& out,
                   Error& error) {
    error = Error{};
    out = ForecastFile{};

    if (data == nullptr || size < kForecastHeaderSize) {
        SetError(error, Error::kTooSmall, "forecast.bin shorter than its header");
        return false;
    }

    out.header = ReadForecastHeader(data);
    const ForecastHeader& h = out.header;

    if ((h.version & 0xFFFF0000u) != 0) {
        SetError(error, Error::kBadVersion, "version high bits set");
        return false;
    }
    if (h.size != size) {
        SetError(error, Error::kSizeMismatch, "header size differs from buffer size");
        return false;
    }
    if (h.crc != Crc32(data + 0xC, size - 0xC)) {
        SetError(error, Error::kBadCrc, "CRC-32 mismatch");
        return false;
    }
    if (options.checkLanguage && h.language != options.language) {
        SetError(error, Error::kLanguageMismatch, "language differs from the console's");
        return false;
    }
    if (h.unitType > 2) {
        SetError(error, Error::kBadField, "unitType > 2");
        return false;
    }
    if (h.unk1A > 1) {
        SetError(error, Error::kBadField, "unk1A > 1");
        return false;
    }
    if (options.checkExpiry && h.time < options.nowMinutes) {
        out.status = Status::kExpired;
    }

    // A server message replaces the forecast. The channel checks the offset
    // is inside the file and even.
    if (h.messageOffset != 0) {
        if (h.messageOffset >= size || (h.messageOffset & 1) != 0) {
            SetError(error, Error::kBadField, "messageOffset out of range or odd");
            return false;
        }
        if (!ReadString(data, size, h.messageOffset, out.message)) {
            SetError(error, Error::kBadString, "message");
            return false;
        }
        out.status = Status::kMessage;
        return true;
    }

    if (h.numForecasts == 0 && h.numSummaries == 0) {
        SetError(error, Error::kBadField, "no forecasts and no summaries");
        return false;
    }
    if (h.numWeatherTypes == 0 || h.numPlaces == 0) {
        SetError(error, Error::kBadField, "no weather types or no places");
        return false;
    }

    if (!CheckTable(size, h.forecastOffset, h.numForecasts, kForecastEntrySize, error, "forecasts") ||
        !CheckTable(size, h.summaryOffset, h.numSummaries, kSummaryEntrySize, error, "summaries") ||
        !CheckTable(size, h.weatherTypeOffset, h.numWeatherTypes, kWeatherTypeSize, error, "weather types") ||
        !CheckTable(size, h.uvIndexOffset, h.numUVIndices, kIndexTextSize, error, "uv") ||
        !CheckTable(size, h.laundryIndexOffset, h.numLaundryIndices, kIndexTextSize, error, "laundry") ||
        !CheckTable(size, h.pollenIndexOffset, h.numPollenIndices, kIndexTextSize, error, "pollen") ||
        !CheckTable(size, h.placeOffset, h.numPlaces, kPlaceEntrySize, error, "places")) {
        return false;
    }

    // Places first: forecasts are matched against them.
    out.places.resize(h.numPlaces);
    for (uint32_t i = 0; i < h.numPlaces; ++i) {
        const uint8_t* p = data + h.placeOffset + i * kPlaceEntrySize;
        if (!ReadPlace(data, size, p, out.places[i], error)) {
            return false;
        }
    }

    out.forecasts.resize(h.numForecasts);
    for (uint32_t i = 0; i < h.numForecasts; ++i) {
        const uint8_t* p = data + h.forecastOffset + i * kForecastEntrySize;
        Forecast& f = out.forecasts[i];
        f.id = ReadU32(p + 0x0);
        f.time = ReadU32(p + 0x4);
        f.offset = ReadU32(p + 0x8);
        f.unkC = ReadU8(p + 0xC);
        for (size_t d = 0; d < 2; ++d) {
            f.days[d] = ReadDay(p + 0x10 + d * kDayForecastSize);
        }
        for (size_t w = 0; w < 7; ++w) {
            f.week[w] = ReadWeek(p + 0x48 + w * kWeekForecastSize);
        }

        // Each forecast must match exactly one place.
        size_t matches = 0;
        for (const Place& place : out.places) {
            if (place.id == f.id) {
                ++matches;
            }
        }
        if (matches != 1) {
            SetError(error, Error::kUnknownPlace, "forecast id does not match exactly one place");
            return false;
        }
        if (f.unkC > 5 && f.unkC != 0xFF) {
            SetError(error, Error::kBadField, "forecast unkC out of range");
            return false;
        }
    }

    out.summaries.resize(h.numSummaries);
    for (uint32_t i = 0; i < h.numSummaries; ++i) {
        const uint8_t* p = data + h.summaryOffset + i * kSummaryEntrySize;
        Summary& s = out.summaries[i];
        s.id = ReadU32(p + 0x0);
        s.time = ReadU32(p + 0x4);
        s.unkC = ReadU8(p + 0xC);
        for (size_t d = 0; d < 2; ++d) {
            s.days[d] = ReadDay(p + 0x10 + d * kDayForecastSize);
        }
    }

    out.weatherTypes.resize(h.numWeatherTypes);
    for (uint32_t i = 0; i < h.numWeatherTypes; ++i) {
        const uint8_t* p = data + h.weatherTypeOffset + i * kWeatherTypeSize;
        WeatherType& t = out.weatherTypes[i];
        t.code = ReadU16(p + 0x0);
        t.icon = ReadU16(p + 0x2);
        const uint32_t textOffset = ReadU32(p + 0x4);
        if (textOffset == 0) {
            SetError(error, Error::kBadField, "weather type without text");
            return false;
        }
        if (!ReadString(data, size, textOffset, t.text)) {
            SetError(error, Error::kBadString, "weather type text");
            return false;
        }
    }

    if (!ReadIndexTexts(data, size, h.uvIndexOffset, h.numUVIndices, out.uvIndices, error) ||
        !ReadIndexTexts(data, size, h.laundryIndexOffset, h.numLaundryIndices, out.laundryIndices, error) ||
        !ReadIndexTexts(data, size, h.pollenIndexOffset, h.numPollenIndices, out.pollenIndices, error)) {
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// short.bin
// ---------------------------------------------------------------------------

bool ParseShort(const uint8_t* data, size_t size, const ParseOptions& options, ShortFile& out, Error& error) {
    error = Error{};
    out = ShortFile{};

    if (data == nullptr || size < kShortHeaderSize) {
        SetError(error, Error::kTooSmall, "short.bin shorter than its header");
        return false;
    }

    out.header = ReadShortHeader(data);
    const ShortHeader& h = out.header;

    if ((h.version & 0xFFFF0000u) != 0) {
        SetError(error, Error::kBadVersion, "short version high bits set");
        return false;
    }
    if (h.size != size) {
        SetError(error, Error::kSizeMismatch, "short header size differs from buffer size");
        return false;
    }
    if (h.crc != Crc32(data + 0xC, size - 0xC)) {
        SetError(error, Error::kBadCrc, "short CRC-32 mismatch");
        return false;
    }
    if (options.checkLanguage && h.language != options.language) {
        SetError(error, Error::kLanguageMismatch, "short language differs from the console's");
        return false;
    }

    const uint32_t entryOffset = ReadU32(data + 0x20);
    if (!CheckTable(size, entryOffset, h.numEntries, kShortEntrySize, error, "short entries")) {
        return false;
    }

    out.entries.resize(h.numEntries);
    for (uint32_t i = 0; i < h.numEntries; ++i) {
        const uint8_t* p = data + entryOffset + i * kShortEntrySize;
        Now& n = out.entries[i];
        n.id = ReadU32(p + 0x0);
        n.time = ReadU32(p + 0x4);
        n.weather = ReadU16(p + 0xC);
        n.tempC = ReadS8(p + 0xF);
        n.tempF = ReadS8(p + 0x10);
        n.windDirection = ReadU8(p + 0x11);
        n.windSpeedKmh = ReadU8(p + 0x12);
        n.windSpeedMph = ReadU8(p + 0x13);
    }
    return true;
}

// ---------------------------------------------------------------------------
// Dates
// ---------------------------------------------------------------------------

// Days since 1970-01-01 to civil date (Howard Hinnant's algorithm).
static void CivilFromDays(int64_t z, int& year, int& month, int& day) {
    z += 719468;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const int64_t doe = z - era * 146097;
    const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int64_t y = yoe + era * 400;
    const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const int64_t mp = (5 * doy + 2) / 153;
    day = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
    month = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
    year = static_cast<int>(y + (month <= 2 ? 1 : 0));
}

DateTime MinutesSince2000ToDateTime(uint32_t minutes) {
    // 2000-01-01 is 10957 days after 1970-01-01.
    constexpr int64_t kDaysBetween1970And2000 = 10957;
    const int64_t days = static_cast<int64_t>(minutes / 1440);
    const uint32_t minuteOfDay = minutes % 1440;

    DateTime dt{};
    CivilFromDays(days + kDaysBetween1970And2000, dt.year, dt.month, dt.day);
    dt.hour = static_cast<int>(minuteOfDay / 60);
    dt.minute = static_cast<int>(minuteOfDay % 60);
    return dt;
}

}  // namespace forecast
