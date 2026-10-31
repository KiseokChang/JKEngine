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
// asset: ggml-small-komixv2.bin(등급 전환 배치 — 2026-10-31 사용자 결정 ①
// small 내장: tiny-ko가 실발화 결실 0(원장) → small 외부 재판 성공 원존 승계).
// 핀 sha256 a428f646…/487,601,984B — install_android_sdk.sh §7이 동일 핀으로
// 로컬 실물 사본(engine/tmp/stt_smallko_verify — 네트워크 조달 불요·실물 부재
// 시에만 HF fetch)을 무추적 build/android-sdk/jkapk-assets/에 조달, 부재 시
// 재조달+핀 불일치 정직 거부. ggml-tiny-ko.bin(T1 조달기 원존 핀 sha256
// dd6d9f46…/77,691,730) = 보존 원존 2순위(jkapk-assets-kept — 조달기
// JKAPK_MODEL=tiny-ko; 현재 gradle/MainActivity 핀은 small이므로 등급
// 재판정 시 3계층 동시 갱신 계약). asset dir 전체가 무추적(루트 build/
// gitignore — D4)이라 이 파일은 경로만 지정한다. 부재 = configuration
// fail-closed(조용한 누락 asset 금지).
val jkapkAssetsDir = System.getenv("JKAPK_ASSETS")
    ?: File(rootDir.parentFile.parentFile.parentFile, "build/android-sdk/jkapk-assets").absolutePath
val jkapkModelAsset = File(jkapkAssetsDir, "ggml-small-komixv2.bin")
check(jkapkModelAsset.isFile) {
    "jkapk v2 small-komixv2 모델 asset 부재: $jkapkModelAsset — " +
        "engine/tools/probes/install_android_sdk.sh(§7 small-komixv2) 재실행 필수"
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
                // #122 GPU leg — GGML_VULKAN=ON: vendored ggml-vulkan backend
                // 동일 .so 정적 링크(backend-reg 승계 — GPU 미탑재 기기 =
                // instance init 실패 → Vulkan 장비 0 → JNI 검출부 CPU 폴백 —
                // D2 fail-closed). GGML_VULKAN_GLSLC_DIR = NDK 내장 shader-tools
                // glslc(ndkDirectory 핀 — 호스트 빌드 도구, 산출물 APK 비포함 —
                // vulkan-shaders-gen 헤더 원존 주석). host-toolchain 재유 =
                // gradle 실행 환경 PATH(g++ 발견 가능성) — 조달기 §9 원문.
                arguments += listOf("-DANDROID_STL=c++_static",
                    "-DGGML_VULKAN=ON",
                    "-DGGML_VULKAN_GLSLC_DIR=" +
                        "${ndkDirectory.absolutePath}/shader-tools/windows-x86_64")
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