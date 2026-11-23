package com.forecastport

import android.os.Bundle
import android.util.TypedValue
import android.view.Gravity
import android.view.ViewGroup
import android.widget.Button
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import androidx.activity.ComponentActivity
import androidx.activity.result.contract.ActivityResultContracts
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat

/**
 * Touch-only UI: every control is a large button or a scrollable text area.
 * No keyboard or hardware key is needed.
 */
class MainActivity : ComponentActivity() {

    private lateinit var output: TextView

    private fun dp(value: Int): Int =
        TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, value.toFloat(), resources.displayMetrics).toInt()

    // Opens any file so the user can pick forecast.bin from the device.
    private val pickForecast = registerForActivityResult(
        ActivityResultContracts.OpenDocument()
    ) { uri ->
        if (uri != null) {
            try {
                val bytes = contentResolver.openInputStream(uri)?.use { it.readBytes() } ?: ByteArray(0)
                output.text = ForecastNative.summarize(bytes, ByteArray(0))
            } catch (e: Exception) {
                output.text = "No se pudo leer el fichero: ${e.message}"
            }
        }
    }

    private fun bigButton(label: String, onClick: () -> Unit): Button =
        Button(this).apply {
            text = label
            textSize = 18f
            isAllCaps = false
            minimumHeight = dp(56)
            setOnClickListener { onClick() }
            layoutParams = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT
            ).apply { topMargin = dp(12) }
        }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val title = TextView(this).apply {
            text = "Forecast"
            textSize = 28f
            setTypeface(typeface, android.graphics.Typeface.BOLD)
            gravity = Gravity.START
        }

        val sample = bigButton("Probar con muestra") {
            try {
                val forecast = assets.open("forecast.bin").use { it.readBytes() }
                val short = assets.open("short.bin").use { it.readBytes() }
                output.text = ForecastNative.summarize(forecast, short)
            } catch (e: Exception) {
                output.text = "No se pudieron cargar los ficheros de muestra: ${e.message}"
            }
        }

        val open = bigButton("Abrir forecast.bin…") {
            pickForecast.launch(arrayOf("*/*"))
        }

        val clear = bigButton("Limpiar") {
            output.text = ""
        }

        output = TextView(this).apply {
            textSize = 16f
            setPadding(dp(4), dp(8), dp(4), dp(8))
            setTextIsSelectable(true) // long-press to select and copy text
        }

        val outputScroll = ScrollView(this).apply {
            isFillViewport = true
            addView(output, ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT
            ))
            layoutParams = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                0,
                1f
            ).apply { topMargin = dp(16) }
        }

        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            addView(title)
            addView(sample)
            addView(open)
            addView(clear)
            addView(outputScroll)
        }

        // Targets Android 15+ (edge-to-edge): keep content clear of the status and navigation bars.
        ViewCompat.setOnApplyWindowInsetsListener(root) { v, insets ->
            val bars = insets.getInsets(WindowInsetsCompat.Type.systemBars())
            v.setPadding(bars.left + dp(16), bars.top + dp(16), bars.right + dp(16), bars.bottom + dp(16))
            insets
        }

        setContentView(root, ViewGroup.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.MATCH_PARENT
        ))
    }
}
