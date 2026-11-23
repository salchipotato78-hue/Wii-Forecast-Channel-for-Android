plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "com.forecastport"
    compileSdk = 35
    // Same NDK as the other ports in this project. CI installs this exact version.
    ndkVersion = "27.0.12077973"

    defaultConfig {
        applicationId = "com.forecastport"
        // The parser needs no more than this; it matches the static test build in CI.
        minSdk = 29
        targetSdk = 35
        versionCode = (System.getenv("VERSION_CODE") ?: "1").toInt()
        versionName = "0.1.0"

        ndk {
            abiFilters += listOf("arm64-v8a")
        }
        externalNativeBuild {
            cmake {
                cppFlags += "-std=c++17"
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions {
        jvmTarget = "17"
    }

    sourceSets {
        getByName("main") {
            // Sample forecast.bin and short.bin from the parser's tests, for the "Probar con muestra" button.
            assets.srcDirs("../../forecast-data/tests/sample")
        }
    }
}

dependencies {
    // ComponentActivity provides registerForActivityResult (the file picker).
    implementation("androidx.activity:activity:1.9.2")
    // WindowInsetsCompat for touch-safe padding around the system bars.
    implementation("androidx.core:core:1.13.1")
}
