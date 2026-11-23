package com.forecastport

/** Calls the C++ parser (forecast-data) through the JNI bridge in forecast_jni.cpp. */
object ForecastNative {
    init {
        System.loadLibrary("forecast_jni")
    }

    /**
     * Returns a text summary of the forecast. [shortBytes] may be empty; then only the
     * forecast is shown. An empty [forecastBytes] gives a "missing file" message.
     */
    @JvmStatic
    external fun summarize(forecastBytes: ByteArray, shortBytes: ByteArray): String
}
