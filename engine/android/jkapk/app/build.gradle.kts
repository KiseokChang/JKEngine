plugins {
    id("com.android.application") version "8.5.2"
    id("org.jetbrains.kotlin.android") version "1.9.24"
}

android {
    namespace = "chang.jkapk"
    compileSdk = 34

    defaultConfig {
        applicationId = "chang.jkapk"
        minSdk = 24
        targetSdk = 34
        versionCode = 1
        versionName = "0.1"
    }

    // debug build type keeps the Gradle-default debug signing config
    // (auto-generated ~/.android/debug.keystore) — D5, side-load only.

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }
}

dependencies {
    // none — v1 thin plate: android framework + kotlin stdlib only
}