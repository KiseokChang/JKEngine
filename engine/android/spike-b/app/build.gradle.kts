// #125 스파이크-B — docs/101 §5 질문: "방향 B가 이 폰에서 실제로 되는가?"
// 핀은 jkapk 빌드 성립 원존(AGP 8.6.0 + Gradle 8.9 + NDK 28.2.13676358 +
// SDK cmake 3.22.1)을 승계 — 조달 재시험을 이 스파이크가 반복하지 않는다.
plugins {
    id("com.android.application") version "8.6.0"
}

android {
    namespace = "chang.spikeb"
    compileSdk = 35
    ndkVersion = "28.2.13676358"

    defaultConfig {
        applicationId = "chang.spikeb"
        minSdk = 28
        targetSdk = 34
        versionCode = 1
        versionName = "spike-0.10"
        ndk {
            abiFilters += "arm64-v8a"
        }
        externalNativeBuild {
            cmake {
                arguments += listOf(
                    "-DCMAKE_BUILD_TYPE=Release",
                    "-DANDROID_STL=c++_static",
                    // nativeLibraryDir exec 계약 — 실행 파일을 lib*.so로 포장.
                    // useLegacyPackaging과 짝(아래 packaging 블록).
                    "-DJK_SPIKE_EXEC_PACK=ON",
                )
            }
        }
    }

    // debug signing(AOSP 기본 키스토어) — 사이드로드 전용.
    externalNativeBuild {
        cmake {
            path = file("src/main/jni/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    packaging {
        // exec 계약: so를 압축하지 않고 실물 파일로 nativeLibraryDir에 풀린다
        // (W^X 우회 경유 — docs/101 §3에 근거, 이 실측이 그 문서의 열린 질문).
        jniLibs.useLegacyPackaging = true
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
}

dependencies {
    // 의존 신설 0 — jkapk v1 문화 승계(스파이크이므로 AndroidX도 불요).
}