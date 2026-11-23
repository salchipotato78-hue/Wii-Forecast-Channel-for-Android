// Tests for the forecast.bin / short.bin parser.
// Reads the files written by tests/make_sample.py and checks the values in
// tests/expected.h, which that script generated.
//
// Usage: test_forecast_data <dir with forecast.bin and short.bin>
#include "forecast_data/forecast_data.h"
#include "expected.h"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace forecast;

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond)                                                                 \
    do {                                                                            \
        ++g_checks;                                                                 \
        if (!(cond)) {                                                              \
            ++g_failures;                                                           \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);   \
        }                                                                           \
    } while (0)

static bool ReadFile(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        return false;
    }
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return true;
}

static std::string Narrow(const std::u16string& s) {
    std::string out;
    for (char16_t c : s) {
        out.push_back(c < 0x80 ? static_cast<char>(c) : '?');
    }
    return out;
}

static void TestCrc() {
    const char* v = "123456789";
    CHECK(Crc32(reinterpret_cast<const uint8_t*>(v), 9) == 0xCBF43926u);  // standard CRC-32 check value
    CHECK(Crc32(nullptr, 0) == 0);
}

static void TestForecastSample(const std::string& dir) {
    std::vector<uint8_t> buf;
    CHECK(ReadFile(dir + "/forecast.bin", buf));
    CHECK(buf.size() == expected::kForecastSize);

    ForecastFile f;
    Error err;
    const bool ok = ParseForecast(buf.data(), buf.size(), ParseOptions{}, f, err);
    if (!ok) {
        std::fprintf(stderr, "parse error: kind=%d %s\n", static_cast<int>(err.kind), err.detail.c_str());
    }
    CHECK(ok);
    if (!ok) {
        return;
    }

    CHECK(f.status == Status::kOk);
    CHECK(f.header.crc == expected::kForecastCrc);
    CHECK(f.header.time == expected::kClosingMinutes);
    CHECK(f.header.openTime == expected::kOpeningMinutes);
    CHECK(f.header.unitType == 2);

    CHECK(f.places.size() == expected::kNumPlaces);
    CHECK(f.forecasts.size() == expected::kNumForecasts);
    CHECK(f.summaries.size() == expected::kNumSummaries);
    CHECK(f.weatherTypes.size() == expected::kNumWeatherTypes);
    CHECK(f.uvIndices.size() == expected::kNumUV);
    CHECK(f.laundryIndices.size() == expected::kNumLaundry);
    CHECK(f.pollenIndices.size() == expected::kNumPollen);

    // Strings: UTF-16BE read correctly, including a non-ASCII character.
    CHECK(f.places[0].id == expected::kMadridId);
    CHECK(Narrow(f.places[0].name) == "Madrid");
    CHECK(Narrow(f.places[0].region) == "Comunidad de Madrid");
    CHECK(f.places[0].country == u"Espa\u00F1a");
    CHECK(f.places[1].id == expected::kSevillaId);
    CHECK(f.places[1].region.empty());  // offset 0 means no string

    CHECK(f.weatherTypes[0].code == 1);
    CHECK(Narrow(f.weatherTypes[0].text) == "Soleado");
    CHECK(Narrow(f.weatherTypes[1].text) == "Nublado");
    CHECK(Narrow(f.uvIndices[1].text) == "Alto");
    CHECK(f.uvIndices[1].code == 7);

    const Forecast& madrid = f.forecasts[0];
    CHECK(madrid.id == expected::kMadridId);
    CHECK(madrid.unkC == 1);
    CHECK(madrid.days[0].weather == expected::kDay0Weather);
    CHECK(madrid.days[0].maxC == expected::kDay0MaxC);
    CHECK(madrid.days[0].minC == expected::kDay0MinC);
    CHECK(madrid.days[0].percent[0] == expected::kDay0Percent0);
    CHECK(madrid.days[0].percent[1] == expected::kDay0Percent1);  // 0xFF = none
    CHECK(madrid.days[0].windSpeedKmh == expected::kDay0WindKmh);
    CHECK(madrid.days[1].weather == expected::kDay1Weather);
    CHECK(madrid.days[1].maxC == expected::kDay1MaxC);
    CHECK(madrid.days[1].percent[3] == expected::kDay1Percent3);
    CHECK(madrid.week[0].weather == expected::kWeek0Weather);
    CHECK(madrid.week[0].maxC == expected::kWeek0MaxC);
    CHECK(madrid.week[6].percent == expected::kWeek6Percent);

    CHECK(f.summaries[0].id == expected::kMadridId);
    CHECK(f.summaries[0].days[0].maxC == expected::kDay0MaxC);
}

static void TestShortSample(const std::string& dir) {
    std::vector<uint8_t> buf;
    CHECK(ReadFile(dir + "/short.bin", buf));
    CHECK(buf.size() == expected::kShortSize);

    ShortFile s;
    Error err;
    const bool ok = ParseShort(buf.data(), buf.size(), ParseOptions{}, s, err);
    CHECK(ok);
    if (!ok) {
        return;
    }
    CHECK(s.entries.size() == 2);
    CHECK(s.entries[0].id == expected::kMadridId);
    CHECK(s.entries[0].weather == expected::kNowMadridWeather);
    CHECK(s.entries[0].tempC == expected::kNowMadridTempC);
    CHECK(s.entries[1].id == expected::kSevillaId);
    CHECK(s.entries[1].tempC == expected::kNowSevillaTempC);  // negative: signed read
}

static void TestErrors(const std::string& dir) {
    std::vector<uint8_t> good;
    CHECK(ReadFile(dir + "/forecast.bin", good));
    ForecastFile f;
    Error err;

    // Flipping one body byte breaks the CRC.
    {
        auto bad = good;
        bad[0x60] ^= 0x01;
        CHECK(!ParseForecast(bad.data(), bad.size(), ParseOptions{}, f, err));
        CHECK(err.kind == Error::kBadCrc);
    }

    // Truncated file: header size no longer matches.
    {
        CHECK(!ParseForecast(good.data(), good.size() - 4, ParseOptions{}, f, err));
        CHECK(err.kind == Error::kSizeMismatch);
    }

    // Shorter than the header.
    {
        CHECK(!ParseForecast(good.data(), 0x10, ParseOptions{}, f, err));
        CHECK(err.kind == Error::kTooSmall);
    }

    // Language check: the sample is language 1.
    {
        ParseOptions opt;
        opt.checkLanguage = true;
        opt.language = 2;
        CHECK(!ParseForecast(good.data(), good.size(), opt, f, err));
        CHECK(err.kind == Error::kLanguageMismatch);
        opt.language = 1;
        CHECK(ParseForecast(good.data(), good.size(), opt, f, err));
    }

    // Expiry is a status, not an error: the data still parses.
    {
        ParseOptions opt;
        opt.checkExpiry = true;
        opt.nowMinutes = expected::kClosingMinutes + 1;
        CHECK(ParseForecast(good.data(), good.size(), opt, f, err));
        CHECK(f.status == Status::kExpired);
    }

    // A table offset past the end, with the CRC recomputed so only the bounds check fails.
    {
        auto bad = good;
        // numForecasts stays 1; point the forecast table at the last byte.
        const uint32_t badOffset = static_cast<uint32_t>(bad.size() - 4);
        bad[0x24] = static_cast<uint8_t>(badOffset >> 24);
        bad[0x25] = static_cast<uint8_t>(badOffset >> 16);
        bad[0x26] = static_cast<uint8_t>(badOffset >> 8);
        bad[0x27] = static_cast<uint8_t>(badOffset);
        const uint32_t crc = Crc32(bad.data() + 0xC, bad.size() - 0xC);
        bad[0x8] = static_cast<uint8_t>(crc >> 24);
        bad[0x9] = static_cast<uint8_t>(crc >> 16);
        bad[0xA] = static_cast<uint8_t>(crc >> 8);
        bad[0xB] = static_cast<uint8_t>(crc);
        CHECK(!ParseForecast(bad.data(), bad.size(), ParseOptions{}, f, err));
        CHECK(err.kind == Error::kTableOutOfRange);
    }

    // A forecast id with no matching place: change the Madrid id in the forecast entry.
    {
        // Forecast table offset is in the header at 0x24; the entry id is its first field.
        const size_t entry = (static_cast<size_t>(good[0x24]) << 24) | (static_cast<size_t>(good[0x25]) << 16) |
                             (static_cast<size_t>(good[0x26]) << 8) | good[0x27];
        auto bad = good;
        bad[entry + 3] ^= 0x7F;  // change the low byte of the id
        const uint32_t crc = Crc32(bad.data() + 0xC, bad.size() - 0xC);
        bad[0x8] = static_cast<uint8_t>(crc >> 24);
        bad[0x9] = static_cast<uint8_t>(crc >> 16);
        bad[0xA] = static_cast<uint8_t>(crc >> 8);
        bad[0xB] = static_cast<uint8_t>(crc);
        CHECK(!ParseForecast(bad.data(), bad.size(), ParseOptions{}, f, err));
        CHECK(err.kind == Error::kUnknownPlace);
    }
}

static void TestDates() {
    // Sample opening is 2026-11-22 00:00, closing 2026-11-23 00:00 (both in minutes since 2000).
    DateTime d = MinutesSince2000ToDateTime(expected::kOpeningMinutes);
    CHECK(d.year == 2026 && d.month == 11 && d.day == 22 && d.hour == 0 && d.minute == 0);
    d = MinutesSince2000ToDateTime(expected::kClosingMinutes);
    CHECK(d.year == 2026 && d.month == 11 && d.day == 23);
    d = MinutesSince2000ToDateTime(0);
    CHECK(d.year == 2000 && d.month == 1 && d.day == 1 && d.hour == 0);
    d = MinutesSince2000ToDateTime(60 * 24 * 366);  // 2000 is a leap year
    CHECK(d.year == 2001 && d.month == 1 && d.day == 1);
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <dir with forecast.bin and short.bin>\n", argv[0]);
        return 2;
    }
    const std::string dir = argv[1];
    TestCrc();
    TestForecastSample(dir);
    TestShortSample(dir);
    TestErrors(dir);
    TestDates();

    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
