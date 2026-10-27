plugins {
    id("com.android.application") version "8.5.2"
    id("org.jetbrains.kotlin.android") version "1.9.24"
}

// ── v2 T1(#121) — JNI 안장+모델 asset 배선 ─────────────────────────────────
// 스펙: docs/superpowers/specs/2026-10-25-jkapk-design.md §1(빌드 사슬 —
// externalNativeBuild CMake+vendored whisper.cpp 상대 참조)·§2 v2 원장 부기.
//
// JNI: src/main/jni/CMakeLists.txt — vendored whisper.cpp
// (engine/third_party/whisper.cpp — T1 v1.9.5 원존) 상대 재용 정적 링크 →
// libwhisper_jni.so 1종. 링크 결정 원장(cli 스폰 계약의 정직 예외) =
// src/main/jni/whisper_jni.c 헤더 원존 주석.
// NDK: install_android_sdk.sh v2 T1 섹션(무추적 build/android-sdk/sdk/ndk/
// 28.2.13676358) — r28 계열(aarch64 triple — brief 원문)·android-24(minSdk).
//
// asset: ggml-tiny-ko.bin(#112 조달기 install_stt_models.sh 원존 핀
// sha256 dd6d9f46… — install_android_sdk.sh §7이 동일 핀으로 무추적
// build/android-sdk/jkapk-assets/에 조달, 부재 시 재다운로드+핀 불일치 정직
// 거부). asset dir 전체가 무추적(루트 build/ gitignore — D4)이라 이 파일은
// 경로만 지정한다. 부재 = configuration fail-closed(조용한 누락 asset 금지).
val jkapkAssetsDir = System.getenv("JKAPK_ASSETS")
    ?: File(rootDir.parentFile.parentFile.parentFile, "build/android-sdk/jkapk-assets").absolutePath
val jkapkTinyKoAsset = File(jkapkAssetsDir, "ggml-tiny-ko.bin")
check(jkapkTinyKoAsset.isFile) {
    "jkapk v2 tiny-ko 모델 asset 부재: $jkapkTinyKoAsset — " +
        "engine/tools/probes/install_android_sdk.sh(v2 T1 §7) 재실행 필수"
}

android {
    namespace = "chang.jkapk"
    compileSdk = 34
    ndkVersion = "28.2.13676358"

    defaultConfig {
        applicationId = "chang.jkapk"
        minSdk = 24
        targetSdk = 34
        versionCode = 1
        versionName = "0.1"

        // 폰 aarch64 1축만(D4 최소 산출 — x86/x64 에뮬레이터 산출은 비-목표).
        ndk {
            abiFilters += "arm64-v8a"
        }
        externalNativeBuild {
            cmake {
                // c++_static — ggml/whisper(C++) 사슬을 libwhisper_jni.so 내부
                // 정적 소비(libc++_shared.so 별도 패키지 0 — D4 최소 산출).
                arguments += listOf("-DANDROID_STL=c++_static")
            }
        }
    }

    // debug build type keeps the Gradle-default debug signing config
    // (auto-generated ~/.android/debug.keystore) — D5, side-load only.

    // v2 asset 배선 — 무추적 조달 dir 위(위 fail-closed check).
    sourceSets["main"].assets.srcDir(jkapkAssetsDir)

    externalNativeBuild {
        cmake {
            path = file("src/main/jni/CMakeLists.txt")
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
}

dependencies {
    // none — v1 얇은판 문화 유지: android framework + kotlin stdlib only
    // (JNI 안장은 CMake 직링크 — gradle 의존 신설 0)
}