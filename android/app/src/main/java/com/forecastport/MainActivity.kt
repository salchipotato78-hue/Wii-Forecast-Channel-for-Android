package com.forecastport

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.result.contract.ActivityResultContracts
import android.view.ViewGroup
import android.widget.Button
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView

class MainActivity : ComponentActivity() {

    private lateinit var output: TextView

    // Opens any file so the user can pick forecast.bin from the device.
    private val pickForecast = registerForActivityResult(
        ActivityResultContracts.OpenDocument()
    ) { uri ->
        if (uri != null) {
            val bytes = contentResolver.openInputStream(uri)?.use { it.readBytes() } ?: ByteArray(0)
            output.text = ForecastNative.summarize(bytes, ByteArray(0))
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        output = TextView(this).apply {
            textSize = 15f
            setPadding(32, 32, 32, 32)
        }

        val sample = Button(this).apply {
            text = "Probar con muestra"
            setOnClickListener {
                val forecast = assets.open("forecast.bin").use { it.readBytes() }
                val short = assets.open("short.bin").use { it.readBytes() }
                output.text = ForecastNative.summarize(forecast, short)
            }
        }

        val open = Button(this).apply {
            text = "Abrir forecast.bin…"
            setOnClickListener { pickForecast.launch(arrayOf("*/*")) }
        }

        val column = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            addView(sample)
            addView(open)
            addView(output)
        }

        setContentView(ScrollView(this).apply {
            addView(column, ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT))
        })
    }
}
