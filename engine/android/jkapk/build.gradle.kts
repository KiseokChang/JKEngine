// #114 jkapk (T1) — thin-typing APK v1 (design docs/superpowers/specs/2026-10-25-jkapk-design.md §1/D4-D5).
// AGP + Kotlin android plugins are resolved from google()/mavenCentral()/gradlePluginPortal()
// via the repositories in settings.gradle.kts — network only on first absence.
// The Android SDK / Gradle / JDK are provisioned by engine/tools/probes/install_android_sdk.sh
// into the untracked build/android-sdk/ and injected as env (ANDROID_HOME, JAVA_HOME,
// GRADLE_USER_HOME) — no local.properties is ever written into this tracked tree.