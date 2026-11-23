// Parser for the Forecast Channel data files forecast.bin and short.bin.
//
// The layout and the validation rules come from the decompiled channel
// (include/channel/ForecastData.h and src/ForecastCheck.cpp of forecast-main).
//
// All multi-byte fields in the files are big-endian (the files were made for a
// PowerPC console). This parser reads them one byte at a time, so it gives the
// same result on little-endian hosts such as Android/ARM and does not depend on
// struct padding or alignment.
//
// C++17, no dependencies outside the standard library.
#ifndef FORECAST_DATA_H
#define FORECAST_DATA_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace forecast {

// Sizes of the on-disk structs (see ForecastData.h).
constexpr size_t kForecastHeaderSize = 0x58;
constexpr size_t kShortHeaderSize = 0x24;
constexpr size_t kDayForecastSize = 0x1C;
constexpr size_t kWeekForecastSize = 0x08;
constexpr size_t kForecastEntrySize = 0x80;
constexpr size_t kSummaryEntrySize = 0x48;
constexpr size_t kWeatherTypeSize = 0x08;
constexpr size_t kIndexTextSize = 0x08;
constexpr size_t kPlaceEntrySize = 0x18;
constexpr size_t kShortEntrySize = 0x18;

// Value the channel uses for "no data" in some fields.
constexpr uint8_t kNoPercent = 0xFF;
constexpr int8_t kNoTemperature = -128;

// Result of the file's own checks, same meaning as the channel's.
enum class Status {
    kOk,       // usable data
    kMessage,  // the server sent a message instead of forecasts (messageOffset != 0)
    kExpired,  // closing time is in the past (channel returns -2)
};

// Error from a failed check.
struct Error {
    enum Kind {
        kNone = 0,
        kTooSmall,           // buffer shorter than the header
        kBadVersion,         // high 16 bits of version are set
        kSizeMismatch,       // header.size != buffer size
        kBadCrc,             // CRC-32 over [0xC, size) does not match
        kLanguageMismatch,   // header language differs from the requested one
        kTableOutOfRange,    // a table would run past the end of the file
        kTableMisaligned,    // a 4-byte table is not 4-byte aligned
        kBadField,           // a field has a value the channel rejects
        kBadString,          // a string offset is outside the file or not terminated
        kUnknownPlace,       // a forecast id does not match exactly one place
    };
    Kind kind = kNone;
    std::string detail;
};

struct Day {                       // DayForecast, 0x1C bytes
    uint16_t weather = 0;
    std::array<uint16_t, 4> weatherParts{};
    int8_t maxC = 0, maxDiffC = 0, minC = 0, minDiffC = 0;
    int8_t maxF = 0, maxDiffF = 0, minF = 0, minDiffF = 0;
    std::array<uint8_t, 4> percent{};  // 0xFF = none
    uint8_t windDirection = 0;
    uint8_t windSpeedKmh = 0;
    uint8_t windSpeedMph = 0;
    uint8_t uvIndex = 0;
    uint8_t laundryIndex = 0;
    uint8_t pollenIndex = 0;
};

struct Week {                      // WeekForecast, 0x08 bytes
    uint16_t weather = 0;
    int8_t maxC = 0, minC = 0, maxF = 0, minF = 0;
    uint8_t percent = 0;
};

struct Place {                     // PlaceEntry, 0x18 bytes, plus its strings
    uint32_t id = 0;
    std::u16string name;
    std::u16string region;         // empty if the offset is 0
    std::u16string country;        // empty if the offset is 0
    uint16_t longitude = 0;
    uint16_t latitude = 0;
};

struct Forecast {                  // ForecastEntry, 0x80 bytes
    uint32_t id = 0;
    uint32_t time = 0;             // minutes since 2000
    uint32_t offset = 0;
    uint8_t unkC = 0;
    std::array<Day, 2> days{};
    std::array<Week, 7> week{};
};

struct Summary {                   // SummaryEntry, 0x48 bytes
    uint32_t id = 0;
    uint32_t time = 0;
    uint8_t unkC = 0;
    std::array<Day, 2> days{};
};

struct WeatherType {               // WeatherType, 0x08 bytes, plus its text
    uint16_t code = 0;
    uint16_t icon = 0;             // 0x8000 = night variant
    std::u16string text;
};

struct IndexText {                 // IndexText, 0x08 bytes, plus its text
    uint8_t code = 0;
    std::u16string text;
};

struct ShortHeader {
    uint32_t version = 0;
    uint32_t size = 0;
    uint32_t crc = 0;
    uint8_t openTime = 0;
    uint32_t time = 0;             // minutes since 2000
    uint32_t country = 0;
    uint8_t language = 0;
    uint32_t numEntries = 0;
};

struct Now {                       // ShortEntry, 0x18 bytes
    uint32_t id = 0;
    uint32_t time = 0;
    uint16_t weather = 0;
    int8_t tempC = 0;
    int8_t tempF = 0;
    uint8_t windDirection = 0;
    uint8_t windSpeedKmh = 0;
    uint8_t windSpeedMph = 0;
};

struct ForecastHeader {            // forecast.bin header, 0x58 bytes
    uint32_t version = 0;
    uint32_t size = 0;
    uint32_t crc = 0;
    uint32_t openTime = 0;         // minutes since 2000
    uint32_t time = 0;             // closing time, minutes since 2000
    uint8_t country = 0;
    uint8_t language = 0;
    uint8_t unitType = 0;          // 0 = Japan, 1 = Fahrenheit, 2 = Celsius
    uint8_t unk1A = 0;
    uint32_t messageOffset = 0;
    uint32_t numForecasts = 0;
    uint32_t forecastOffset = 0;
    uint32_t numSummaries = 0;
    uint32_t summaryOffset = 0;
    uint32_t numWeatherTypes = 0;
    uint32_t weatherTypeOffset = 0;
    uint32_t numUVIndices = 0;
    uint32_t uvIndexOffset = 0;
    uint32_t numLaundryIndices = 0;
    uint32_t laundryIndexOffset = 0;
    uint32_t numPollenIndices = 0;
    uint32_t pollenIndexOffset = 0;
    uint32_t numPlaces = 0;
    uint32_t placeOffset = 0;
};

struct ForecastFile {
    ForecastHeader header;
    Status status = Status::kOk;
    std::u16string message;        // set when status == kMessage
    std::vector<Place> places;
    std::vector<Forecast> forecasts;
    std::vector<Summary> summaries;
    std::vector<WeatherType> weatherTypes;
    std::vector<IndexText> uvIndices;
    std::vector<IndexText> laundryIndices;
    std::vector<IndexText> pollenIndices;
};

struct ShortFile {
    ShortHeader header;
    std::vector<Now> entries;
};

// Options for parsing. Leave the defaults to skip the optional checks.
struct ParseOptions {
    // If set, the header language must equal this (channel: gLanguage).
    bool checkLanguage = false;
    uint8_t language = 0;
    // If set, a closing time before `nowMinutes` gives Status::kExpired
    // (channel: header->mTime < GetCurrentMinutes()).
    bool checkExpiry = false;
    uint32_t nowMinutes = 0;
};

// Standard CRC-32 (reflected 0xEDB88320, init and final xor 0xFFFFFFFF).
// Same algorithm as NETCalcCRC32 in the channel. zlib.crc32 gives the same result.
uint32_t Crc32(const uint8_t* data, size_t size);

// Parses forecast.bin. On failure returns false and fills `error`.
bool ParseForecast(const uint8_t* data, size_t size, const ParseOptions& options, ForecastFile& out, Error& error);

// Parses short.bin. The CRC and size are checked; the language is checked only if requested.
bool ParseShort(const uint8_t* data, size_t size, const ParseOptions& options, ShortFile& out, Error& error);

// Converts "minutes since 2000" to a broken-down date.
// Assumes UTC; the channel's own conversion is not in the decompiled sources yet, so verify.
struct DateTime {
    int year, month, day, hour, minute;
};
DateTime MinutesSince2000ToDateTime(uint32_t minutes);

}  // namespace forecast

#endif  // FORECAST_DATA_H
