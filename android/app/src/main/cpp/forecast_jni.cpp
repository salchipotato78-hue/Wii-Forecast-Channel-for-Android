// JNI bridge: turns forecast.bin and short.bin into a text summary for the UI.
// Parsing is done by forecast-data (C++, no dependencies). This file only formats.
#include <jni.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "forecast_data/forecast_data.h"

namespace {

std::vector<uint8_t> CopyBytes(JNIEnv* env, jbyteArray array) {
    std::vector<uint8_t> out;
    if (array == nullptr) {
        return out;
    }
    const jsize n = env->GetArrayLength(array);
    if (n <= 0) {
        return out;
    }
    out.resize(static_cast<size_t>(n));
    jbyte* p = env->GetByteArrayElements(array, nullptr);
    std::memcpy(out.data(), p, static_cast<size_t>(n));
    env->ReleaseByteArrayElements(array, p, JNI_ABORT);
    return out;
}

// Names in the file are UTF-16. Characters above U+07FF show as '?'.
std::string ToUtf8(const std::u16string& s) {
    std::string out;
    for (char16_t c : s) {
        if (c < 0x80) {
            out.push_back(static_cast<char>(c));
        } else if (c < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (c >> 6)));
            out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        } else {
            out.push_back('?');
        }
    }
    return out;
}

const forecast::Place* FindPlace(const forecast::ForecastFile& f, uint32_t id) {
    for (const auto& p : f.places) {
        if (p.id == id) {
            return &p;
        }
    }
    return nullptr;
}

std::string WeatherText(const forecast::ForecastFile& f, uint16_t code) {
    for (const auto& t : f.weatherTypes) {
        if (t.code == code) {
            return ToUtf8(t.text);
        }
    }
    return "código " + std::to_string(code);
}

// Temperature in the unit the console uses. kNoTemperature (-128) means no value.
std::string Temp(const forecast::ForecastFile& f, int8_t c, int8_t fahrenheit) {
    const bool useF = f.header.unitType == 1;
    const int8_t v = useF ? fahrenheit : c;
    if (v == forecast::kNoTemperature) {
        return "--";
    }
    return std::to_string(static_cast<int>(v)) + (useF ? " F" : " C");
}

std::string Rain(uint8_t percent) {
    if (percent == forecast::kNoPercent) {
        return "--";
    }
    return std::to_string(static_cast<int>(percent)) + "%";
}

std::string DayLine(const forecast::ForecastFile& f, const forecast::Day& d, const char* label) {
    std::string line = "  ";
    line += label;
    line += ": " + WeatherText(f, d.weather);
    line += ", máx " + Temp(f, d.maxC, d.maxF);
    line += " / mín " + Temp(f, d.minC, d.minF);
    line += ", lluvia " + Rain(d.percent[0]);
    return line + "\n";
}

std::string DateLine(uint32_t minutes) {
    const forecast::DateTime dt = forecast::MinutesSince2000ToDateTime(minutes);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d", dt.year, dt.month, dt.day, dt.hour, dt.minute);
    return buf;
}

std::string SummarizeForecast(const std::vector<uint8_t>& bytes) {
    forecast::ForecastFile f;
    forecast::Error err;
    if (!forecast::ParseForecast(bytes.data(), bytes.size(), forecast::ParseOptions{}, f, err)) {
        return "forecast.bin no válido: " + err.detail + "\n";
    }

    std::string out;
    if (f.status == forecast::Status::kMessage) {
        return "El servidor envió un mensaje:\n" + ToUtf8(f.message) + "\n";
    }
    out += f.status == forecast::Status::kExpired ? "Estado: caducado\n" : "Estado: correcto\n";
    out += "Cierre de datos: " + DateLine(f.header.time) + " (UTC, por verificar)\n";
    out += "Unidades: " + std::string(f.header.unitType == 0 ? "Japón" : f.header.unitType == 1 ? "Fahrenheit" : "Celsius") + "\n";
    out += "Lugares: " + std::to_string(f.places.size()) + ", previsiones: " + std::to_string(f.forecasts.size()) + "\n\n";

    for (const auto& fc : f.forecasts) {
        const forecast::Place* place = FindPlace(f, fc.id);
        out += (place ? ToUtf8(place->name) : std::string("(sin nombre)")) + "\n";
        out += DayLine(f, fc.days[0], "Hoy");
        out += DayLine(f, fc.days[1], "Mañana");
    }
    return out;
}

std::string SummarizeShort(const forecast::ForecastFile& f, const std::vector<uint8_t>& bytes) {
    forecast::ShortFile s;
    forecast::Error err;
    if (!forecast::ParseShort(bytes.data(), bytes.size(), forecast::ParseOptions{}, s, err)) {
        return "short.bin no válido: " + err.detail + "\n";
    }
    std::string out = "\nAhora:\n";
    for (const auto& n : s.entries) {
        const forecast::Place* place = FindPlace(f, n.id);
        out += "  " + (place ? ToUtf8(place->name) : std::string("(sin nombre)")) + ": " + WeatherText(f, n.weather);
        out += ", " + std::to_string(static_cast<int>(n.tempC)) + " C\n";
    }
    return out;
}

}  // namespace

extern "C" JNIEXPORT jstring JNICALL
Java_com_forecastport_ForecastNative_summarize(JNIEnv* env, jclass, jbyteArray forecastBytes,
                                               jbyteArray shortBytes) {
    const std::vector<uint8_t> fb = CopyBytes(env, forecastBytes);
    const std::vector<uint8_t> sb = CopyBytes(env, shortBytes);

    std::string text;
    forecast::ForecastFile f;
    forecast::Error err;
    if (fb.empty()) {
        text = "Falta forecast.bin.\n";
    } else {
        text = SummarizeForecast(fb);
        if (!sb.empty() && forecast::ParseForecast(fb.data(), fb.size(), forecast::ParseOptions{}, f, err) &&
            f.status == forecast::Status::kOk) {
            text += SummarizeShort(f, sb);
        }
    }
    return env->NewStringUTF(text.c_str());
}
