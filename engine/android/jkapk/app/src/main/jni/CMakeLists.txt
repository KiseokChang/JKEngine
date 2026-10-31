# CMakeLists.txt — #121 JK APK v2 (T1) — APK 내장 whisper 전사 JNI 안장.
# 스펙: docs/superpowers/specs/2026-10-25-jkapk-design.md §1(빌드 사슬 — "#114
#       설계 원문: JNI CMake(vendored whisper.cpp 재사용 — 상대 참조)")·§2 v2
#       원장 부기.
#
# 대상 1종: libwhisper_jni.so (whisper_jni.c — JNI 함수 계약 1종
#           Java_chang_jkapk_JniWhisper_jkapkTranscribe — 헤더 원존 주석 계약).
# vendored whisper.cpp (engine/third_party/whisper.cpp — T1 v1.9.5 원존 149파일)
# 를 상대 경로로 재용 — whisper 정적 lib 1종+그 ggml 사슬 전부 APK 내부 정적
# 소비(D4 무추적 원존 — 소스만 커밋, 산출물은 gradle 무추적 .cxx/APK).
# EXCLUDE_FROM_ALL: vendored 루트의 cli(common/cli) 원존 타깃은 APK 사슬에
# 불요 — whisper_jni가 링크하는 whisper(와 그 의존 ggml)만 빌드된다.
#
# 링크 결정 원장(정직 예외 — 근거) = whisper_jni.c 헤더 원존 주석
#   (cli 스폰 계약은 Android W^X에서 구조 불성립 — full API 직접 링크).

cmake_minimum_required(VERSION 3.22)
project(jkapk_jni LANGUAGES C CXX)

set(JK_WHISPER_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../third_party/whisper.cpp")
if(NOT EXISTS "${JK_WHISPER_ROOT}/include/whisper.h")
  message(FATAL_ERROR
    "jkapk_jni: vendored whisper.cpp 부재 — ${JK_WHISPER_ROOT} (T1 원존 149파일)")
endif()

# vendored 루트는 정적만 산출(BUILD_SHARED_LIBS OFF 원존 — 폰 #3117 backend-
# 공유-lib 탐색 segfault 계통 fail 면 소각 원존) — 라이브러리 타깃 1종 소비.
#
# #122 GPU leg — GGML_VULKAN=ON(gradle에서 옵션 주입 — CMake cache option,
# vendored ggml/CMakeLists.txt 옵션 원존)일 때 vendored ggml 사슬이
# ggml-vulkan backend(정적)를 동일 libwhisper_jni.so에 링크한다(backend-reg
# 승계 — GGML_USE_VULKAN 정의). Vulkan 소스+헤더 폐쇄 =
# third_party/whisper.cpp/ggml/src/ggml-vulkan(+deps — upstream v1.9.5
# verbatim 재이관 조달 원존 — 커밋 메시지 리포트). libvulkan.so = 안드로이드
# 시스템 동링커(DT_NEEDED — loader는 API 24+ 전량 상주 — GPU 미탑재 기기 =
# runtime instance init 실패 → ggml 레지스트리 Vulkan 장비 0 → JNI 검출부가
# CPU 폴백 leg로 수렴 — D2 fail-closed). glslc = NDK 내장 shader-tools
# (GGML_VULKAN_GLSLC_DIR — gradle가 지정).
add_subdirectory("${JK_WHISPER_ROOT}" "${CMAKE_BINARY_DIR}/whisper_build"
    EXCLUDE_FROM_ALL)

add_library(whisper_jni SHARED whisper_jni.c)
target_include_directories(whisper_jni PRIVATE "${JK_WHISPER_ROOT}/include"
    "${JK_WHISPER_ROOT}/ggml/include")
target_link_libraries(whisper_jni PRIVATE whisper log)  # log = #122 스탬프 leg(logcat 원문 채널)
# C — JNI 본체는 순수 C(whisper.h는 C에서 그대로 소비 가능 — 원존 include/whisper.h).
set_target_properties(whisper_jni PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)