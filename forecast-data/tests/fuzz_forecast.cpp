// Mutation fuzzer for forecast.bin. Fixed seed, so runs are reproducible.
// Usage: fuzz_forecast <dir with forecast.bin>
#include "forecast_data/forecast_data.h"
#include <cstdio>
#include <fstream>
#include <iterator>
#include <random>
#include <vector>
int main(int argc, char** argv) {
    if (argc != 2) { std::fprintf(stderr, "usage: %s <dir>\n", argv[0]); return 2; }
    std::ifstream f(std::string(argv[1]) + "/forecast.bin", std::ios::binary);
    std::vector<uint8_t> base((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    std::mt19937 rng(12345);
    int ok = 0, rejected = 0;
    for (int it = 0; it < 2000; ++it) {
        auto b = base;
        int n = 1 + rng() % 4;
        for (int k = 0; k < n; ++k) b[rng() % b.size()] = rng() & 0xFF;
        if (rng() % 3 == 0) b.resize(rng() % (b.size() + 1));
        // Recompute size and CRC so the fuzzed bytes reach the table and string code.
        if (b.size() >= 0x58 && rng() % 2) {
            uint32_t sz = b.size();
            b[4] = sz >> 24; b[5] = sz >> 16; b[6] = sz >> 8; b[7] = sz;
            uint32_t c = forecast::Crc32(b.data() + 0xC, b.size() - 0xC);
            b[8] = c >> 24; b[9] = c >> 16; b[10] = c >> 8; b[11] = c;
        }
        forecast::ForecastFile out; forecast::Error e;
        if (forecast::ParseForecast(b.data(), b.size(), forecast::ParseOptions{}, out, e)) ++ok; else ++rejected;
    }
    std::printf("fuzz done: %d accepted, %d rejected, no crash\n", ok, rejected);
    return ok + rejected == 2000 ? 0 : 1;
}
