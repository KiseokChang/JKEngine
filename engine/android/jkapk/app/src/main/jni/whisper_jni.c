// whisper_jni.c — #121 JK APK v2 (T1) — APK 내장 whisper 전사 JNI 안장.
// 스펙: docs/superpowers/specs/2026-10-25-jkapk-design.md §2 v2 원장 부기(D4-D7)
//      + task-1-brief (v2 T1).
//
// **링크 결정 원장(구현자 재량 결정 — 근거 필기 계약)**:
//   PC/폰 원존(#112 SttVoice.cpp run leg)은 whisper-cli **별도 프로세스 스폰**
//   이고 라이브러리 링크는 "폰 #3117 segfault 원존"으로 금지되어 있다 — 그
//   금지의 본질은 "호스트 프로세스가 whisper 결함으로 죽지 않는 격리"다.
//   v2 APK에서 cli 스폰을 승계하려면 (1) cli 바이너리를 APK에 동봉해 runtime
//   시 exec해야 한다 — Android 10+ W^X(app_data exec 금지)와 targetSdk 34의
//   untrusted_app domain은 앱 자기 데이터 dir로부터 exec를 막고, (2) jniLibs의
//   .so는 exec 불가(동적 linker만 로드). 즉 cli 스폰 계약은 APK에서 구조적으로
//   성립하지 않는다. APK는 **자체 프로세스**이므로 whisper 결함이 죽이는 것=
//   APK 본인 — 격리 본질(Every-process-isolated)은 원존 계약과 동일하게 성립.
//   → 따라서 본 안장은 whisper **full API 직접 링크**(vendored whisper.cpp
//   정적 lib — engine/android/jkapk/app/src/main/jni/CMakeLists.txt가
//   ../../../../../../third_party/whisper.cpp 상대 재용 — D4 무추적 원존)를
//   채택한다. 이것은 원존 링크 금지의 **정직 예외**이며 예외 근거는 이 주석과
//   v2 T1 리포트에 원문으로 남는다.
//
// **stdout 파서 규약 재판정(원존 승계)**: PC/폰 cli 원존은 cli stdout의
//   세그먼트 행을 transcript::ParseLastText(최후 텍스트 절단 — SttVoice.h)로
//   쳤다. 직접 링크에서는 stdout이 존재하지 않고, cli stdout의 진실원 자체가
//   whisper 세그먼트 열이므로 세그먼트 열을 API로 직순회한다
//   (whisper_full_n_segments / whisper_full_get_segment_text — cli.cpp의
//   whisper_print_segment_callback이 인쇄하는 것과 **동일 진실원**) — 새 파서
//   계층이 아니라 원존 파서의 데이터 출처 승격. 세그먼트 전 연결 후 전주
//   trim — cli 원존 행마다 선행 공백(+ " "(cli print 형식))과 ParseLastText
//   절단의 합성 결과와 내용 등호를 목표로 한다.
//
// JNI 함수 계약 1종:
//   Java(chang.jkapk — T2에서 Kotlin 측 계약 수형):
//     System.loadLibrary("whisper_jni")
//     public static native String jkapkTranscribe(
//         String wavPath, String modelPath, String prompt);
//   성공   = non-null jstring(전사 원문 — trim 완료).
//   실패   = NULL + RuntimeException("jkapk 전사 실패: <err>") pending
//            (정직 거부 — 조용한 빈 문자열 대체 금지 — 원존 "정직" 문화 승계).
//   prompt = kVoicePrompt(SttVoice.cpp) 어휘 힌트의 jstring 전달 계약 —
//   PC/폰 cli의 --prompt(라우터 어휘 단일 진실원 승계)와 같은 자리.
//
// 언어 핀 = "ko"(SttVoice.cpp kVoiceLanguage 원존 수형 — cli 기본 "en"은
// 한국어 발화 암묵 망각 — 무지정 금지 원존). 스레드 핀 = 4(cli 원존
// std::min(4, hardware_concurrency()) 수형 — 폰 aarch64 8코어라 상한이 산다).
//
// **UTF-8 정리 계약(v2 T2 — T1 결함 원장행 수형)**: T1 폰 실측 원문(전사
// R2·R3)에서 전사 원문에 UTF-8 orphan 바이트 실측(R2 선두 `a5 bc`·R3 중간
// `eb a7` — byte-fallback 토큰 파열). 기존 코드는 이 원문 바이트열을
// NewStringUTF(modified UTF-8)에 그대로 수입하므로 ART가 invalid 수입 시
// RuntimeException 수렴(OOM 유사 표기) — 유용 전사까지 함께 죽는 결함.
// 수형 계약:
//   1) 전사 세그먼트 연결 후 · trim 전에 **jk_utf8_clean** 1회 — 유효
//      UTF-8 시퀀스만 생존(orphan 바이트 소각 — 발명 0·대체 문자 U+FFFD
//      주입도 금지: 소각은 침묵의 발명이 아니라 "파열 바이트의 제거"이고
//      전사 본문은 whisper 산출 원문 그대로 유지된다).
//   2) 소각 후 결실 0 = **정직 거부**(빈 전사 — 빈 문자열 대체 금지 계약
//      승계). 소각 바이트 수 = err 원문에 병기(정직 증거).
//   3) 4바이트 시퀀스(U+10000+)는 modified UTF-8이 받지 않으므로 **서러게이트
//      쌍 6바이트로 치환**(NewStringUTF 계약 — 소각 아님·내용 보존).
//   4) 3바이트 CESU-8 서러게이트(ED A0-BF …)는 invalid — 소각.
//   5) Java(Kotlin) 측 보조 원존 = JniWhisper.sanitizeTranscript(2차 게이트 —
//      modified UTF-8 디코딩 뒤 잔여 단독 서러게이트 소각 — 계층 방어).
// 이 정리부는 pure C(jni/whisper 의존 0)라 단독 컴파일 자기검증이 산다:
//   -DJKSANITIZE_SELFTEST_MAIN — Win/폰 양축 컴파일(부품 등호 — 2zb 계열
//   확장 라벨 2zb-5a..5h; JNI 전사 축은 비결정이라 어셜션 금지 원존 계약
//   준수 — 결정 부품만 검증).
//
// ── #122 GPU leg 진행/스템프 동봉(2026-10-31 — 같은 커밋·빌드 계약) ──────
// 이 섹션은 **보고 leg뿐** — 기존 사슬(캡처·해제·권한·/talk·덤프·정리) 원존
// 무변조(신설 = 표기/보고 채널과 use_gpu 분기 1행뿐).
//
// ① GPU 검출 leg(D2 fail-closed): 진실원 = vendored ggml backend 레지스트리
//    (GGML_USE_VULKAN — vendored ggml-vulkan이 vulkan 장비를 등록). C 층에서
//    ggml_backend_reg_by_name("Vulkan")+dev_count로 검출 — reg 부재/장비 0 =
//    **CPU 폴백**(cparams.use_gpu=false — 기존 경로와 동일 — 원존 주석 유지
//    문언 갱신). reg() 자체가 try/catch 밀폐(vendored ggml-vulkan 원문)라 C
//    측 예외 유입 0. 성공 = use_gpu=true(whisper_backend_init_gpu 원존 분기 —
//    GPU/IGPU 장비 탐색 — ggml_registry가 진단 원문).
// ② 진행 leg(2026-10-31 사용자 증상 "23초 넘겨도 결과 없음 — 카운터만 증가"
//    대응 — 원진 분절 원문): whisper_full progress_callback(%) → Java로
//    보고(pct 정수 — (전사 중…, pct) 계약) → MainActivity 상태행 "전사 중…
//    (Ns · N%)" 갱신(% 진행이 100까지 가는데도 결과가 안 나오면 = 변환 직후
//    수수 문제·%가 안 올라가면 = 마지막 단계(init/inference) 문제).
// ③ init 스템프 leg: whisper_init_from_file 전후 원문(모델 파일 파인 경로+
//    실제 bytes — asset 추출본 존재/사이즈 원문 — Java 측이 핀 등호 어설션
//    선수행)·로드 경과 ms·logcat 채널("jkapk" 태그 — 원문 수취 계약).
//    원문 채널 2종: __android_log_print(폰)+Java 상태행 스탬프(일시 표기 —
//    1행 계약 — 틱 합성이 수초 내 회수). env NULL(데스크톱 selftest main)=
//    Java 채널 스킵(log 채널은 stderr 폴백 — 결정 축만 진입).

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ---- UTF-8 정리부 (pure C — jni/whisper 의존 0 — Win/폰 양축 단독 컴파일) --
// 규약(헤더 원존 주석 위 계약의 구현):
//   · 유효 시퀀스만 생존: 1바이트(01-7F)·2바이트(C2-DF+cont)·3바이트
//     (E0-A0..·cont2 — E0은 A0-9F overlong 배제·ED는 서러게이트 배제)·
//     4바이트(F0-90..·cont3 — F0은 90-8F overlong 배제·U+10FFFF 상한).
//   · 무효 = **orphan 소각**: 불완전 시퀀스는 선두 바이트만(연속 바이트는
//     개별 재판정 — 좌우 유효 시퀀스는 항상 생존하는 결정론), 서러게이트
//     CESU-8(E0 수임 D800-DFFF)은 3바이트 통째 소각, 4바이트는 아래의
//     modified UTF-8 치환(내용 보존 — 소각 아님).
//   · *removed = 소각 바이트 수(정직 증거 — 호출자 err 병기 소관).
//   · 입출력 모두 C 문자열(NUL 종단 — NUL 자체는 경유 0). 최악 4→6바이트
//     확장이므로 버퍼 = 6n+6.
static int jk_utf8_clean(const char* in, char** out, size_t* removed) {
    *out = NULL;
    *removed = 0;
    if (in == NULL) return 1;
    size_t n = strlen(in);
    unsigned char* buf = (unsigned char*)malloc(n * 6 + 6);
    if (buf == NULL) return 1;
    size_t r = 0, w = 0;
    while (r < n) {
        unsigned char c = (unsigned char)in[r];
        if (c < 0x80) {  // 1바이트(ASCII — NUL은 C 문자열 계약상 경유 0)
            buf[w++] = c; r++; continue;
        }
        if (c >= 0xC2 && c <= 0xDF && r + 1 < n &&
            ((unsigned char)in[r + 1] & 0xC0) == 0x80) {
            buf[w++] = c; buf[w++] = (unsigned char)in[r + 1];
            r += 2; continue;
        }
        if (c >= 0xE0 && c <= 0xEF && r + 2 < n &&
            ((unsigned char)in[r + 1] & 0xC0) == 0x80 &&
            ((unsigned char)in[r + 2] & 0xC0) == 0x80) {
            // E0은 후속 1바이트가 A0-9F면 overlong(E0 9F = 0x800 미만) — 소각.
            // ED는 후속이 A0-BF면 CESU-8 서러게이트 — modified UTF-8 수입
            // 불가라 3바이트 통째 소각(계약 4).
            if ((c == 0xE0 && (unsigned char)in[r + 1] < 0xA0) ||
                (c == 0xED && (unsigned char)in[r + 1] >= 0xA0)) {
                r += (c == 0xED) ? 3 : 1;
                *removed += (c == 0xED) ? 3 : 1;
                continue;
            }
            buf[w++] = c; buf[w++] = (unsigned char)in[r + 1];
            buf[w++] = (unsigned char)in[r + 2];
            r += 3; continue;
        }
        if (c >= 0xF0 && c <= 0xF4 && r + 3 < n &&
            ((unsigned char)in[r + 1] & 0xC0) == 0x80 &&
            ((unsigned char)in[r + 2] & 0xC0) == 0x80 &&
            ((unsigned char)in[r + 3] & 0xC0) == 0x80) {
            // F0은 후속 1바이트 90-8F면 overlong — 소각(선두만).
            if (c == 0xF0 && (unsigned char)in[r + 1] < 0x90) {
                r++; (*removed)++; continue;
            }
            // 유효 보충평면 → modified UTF-8 서러게이트 쌍 6바이트 치환
            // (계약 3 — NewStringUTF가 받는 형태. 내용 보존).
            unsigned cp = ((unsigned)(c & 0x07) << 18) |
                          ((unsigned)(in[r + 1] & 0x3F) << 12) |
                          ((unsigned)(in[r + 2] & 0x3F) << 6) |
                          (unsigned)(in[r + 3] & 0x3F);
            if (cp > 0x10FFFF) { r++; (*removed)++; continue; }
            cp -= 0x10000;
            unsigned hi = 0xD800u + (cp >> 10);
            unsigned lo = 0xDC00u + (cp & 0x3FFu);
            buf[w++] = (unsigned char)(0xE0 | (hi >> 12));
            buf[w++] = (unsigned char)(0x80 | ((hi >> 6) & 0x3F));
            buf[w++] = (unsigned char)(0x80 | (hi & 0x3F));
            buf[w++] = (unsigned char)(0xE0 | (lo >> 12));
            buf[w++] = (unsigned char)(0x80 | ((lo >> 6) & 0x3F));
            buf[w++] = (unsigned char)(0x80 | (lo & 0x3F));
            r += 4; continue;
        }
        // 그 외(연속 바이트 낙하·C0/C1 overlong·F5-FF·잘린 꼬리) = 선두
        // 1바이트 소각 — 연속 바이트는 다음 루프에서 개별 재판정.
        r++; (*removed)++;
    }
    buf[w] = '\0';
    char* shrunk = (char*)realloc(buf, w + 1);
    *out = shrunk != NULL ? shrunk : (char*)buf;
    return 0;
}

// 자기검증(부품 등호 — 2zb 계열 확장 라벨 — 결정 부품만, JNI 전사 비결정
// 축은 어셜션 금지 원존 계약 준수). stdout 라벨 원문 = 리포트 수취 계약.
static int jk_selftest_sanitize(void) {
    int fails = 0;
    #define JKCHK(label, cond) do { \
        if (cond) printf("%s-OK\n", label); \
        else { printf("%s-FAIL\n", label); fails++; } } while (0)
    char* out = NULL; size_t removed = 0;

    // 5a: 유효 한국어 = 원문 등호·소각 0
    JKCHK("2zb-5a", jk_utf8_clean("지뢰찾기 켜줘", &out, &removed) == 0 &&
        strcmp(out, "지뢰찾기 켜줘") == 0 && removed == 0 &&
        strlen(out) == strlen("지뢰찾기 켜줘"));
    free(out); out = NULL; removed = 0;
    // 5b: T1 R2 실측 orphan(선두 a5 bc) 소각 — 좌우 유효 시퀀스 생존
    JKCHK("2zb-5b", jk_utf8_clean("깍" "\xA5\xBC" "이", &out, &removed) == 0 &&
        strcmp(out, "깍이") == 0 && removed == 2);
    free(out); out = NULL; removed = 0;
    // 5c: 3바이트 잘린 꼬리(E3 단독) — 선두 소각
    JKCHK("2zb-5c", jk_utf8_clean("가" "\xEB", &out, &removed) == 0 &&
        strcmp(out, "가") == 0 && removed == 1);
    free(out); out = NULL; removed = 0;
    // 5d: CESU-8 서러게이트(ED A0 BC) — 3바이트 통째 소각
    JKCHK("2zb-5d", jk_utf8_clean("a" "\xED\xA0\xBC" "b", &out, &removed) == 0 &&
        strcmp(out, "ab") == 0 && removed == 3);
    free(out); out = NULL; removed = 0;
    // 5e: overlong(C0 80) — 소각
    JKCHK("2zb-5e", jk_utf8_clean("x" "\xC0\x80" "y", &out, &removed) == 0 &&
        strcmp(out, "xy") == 0 && removed == 2);
    free(out); out = NULL; removed = 0;
    // 5f: 4바이트 보충평면(U+1F31A F0 9F 8C 9A) — 서러게이트 쌍 6바이트 치환
    {   // U+1F31A = 서러게이트 D83C·DF1A → modified UTF-8 ED A0 BC·ED BC 9A
        const char want[] = "\xED\xA0\xBC\xED\xBC\x9A";
        JKCHK("2zb-5f", jk_utf8_clean("\xF0\x9F\x8C\x9A", &out, &removed) == 0 &&
            out != NULL && memcmp(out, want, 6) == 0 && out[6] == '\0' &&
            removed == 0);
        free(out); out = NULL; removed = 0;
    }
    // 5g: ASCII = 무변형
    JKCHK("2zb-5g", jk_utf8_clean("ok talk", &out, &removed) == 0 &&
        strcmp(out, "ok talk") == 0 && removed == 0);
    free(out); out = NULL; removed = 0;
    // 5h: 전면 orphan = 결실 0(정직 거부 경로의 입력 형태)
    JKCHK("2zb-5h", jk_utf8_clean("\xA5\xBC\xA1", &out, &removed) == 0 &&
        out != NULL && out[0] == '\0' && removed == 3);
    free(out); out = NULL; removed = 0;
    #undef JKCHK
    printf("%s (2zb 계열 확장 — 부품 등호 %s)\n",
           fails == 0 ? "JKSANITIZE-PASS" : "JKSANITIZE-FAIL",
           fails == 0 ? "8/8" : "위 원문 FAIL 행");
    return fails == 0 ? 0 : 1;
}

#ifdef JKSANITIZE_SELFTEST_MAIN
// Win/폰 양축 단독 자기검증 — 순수 정리부만(jni/whisper 링크 0).
int main(void) { return jk_selftest_sanitize(); }
#else
#include <jni.h>
#include <whisper.h>
#include "ggml.h"
#include "ggml-backend.h"
#ifdef __ANDROID__
#include <android/log.h>
#endif

#define JKJNI_ERR_MAX 512
#define JKJNI_STAGE_MAX 224

// ---- 원문 로그 채널(#122 스탬프 leg) — __ANDROID__ = logcat("jkapk" 태그),
// 그 외 = stderr 폴백(데스크톱 selftest main 결정 축 수취).
static void jk_log(const char* msg) {
#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_INFO, "jkapk", "%s", msg);
#else
    fprintf(stderr, "[jkapk] %s\n", msg);
#endif
}

// ---- Java 스탬프 채널(#122 보고 leg뿐 — 실패해도 전사 사슬 접촉 0) ----
// 계약(JniWhisper.kt 원존 주석과 정합): voiceStage(text, pct) —
//   pct >= 0 = 진행 보고(text == "전사 중…" — Kotlin tick 합성 소관),
//   pct == -1 = 스탬프 표기(text 원문 그대로 1행),
//   pct == 0|1 + text == "전사 leg" = GPU/CPU leg 라벨 보고(Kotlin 소관).
// 리스너 부재 = 조용 소각(보고 leg뿐 — 사슬 접촉 0 계약).
static void jk_stage(JNIEnv* env, const char* msg, int pct) {
    if (env == NULL) return;  // 데스크톱 selftest — log 채널 몫
    jclass cls = (*env)->FindClass(env, "chang/jkapk/JniWhisper");
    if (cls == NULL) {
        if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
        return;
    }
    jmethodID mid = (*env)->GetStaticMethodID(env, cls, "voiceStage",
                                              "(Ljava/lang/String;I)V");
    if (mid == NULL) {
        if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
        return;
    }
    jstring js = (*env)->NewStringUTF(env, msg);
    if (js != NULL) {
        (*env)->CallStaticVoidMethod(env, cls, mid, js, (jint)pct);
    }
    // 보고 채널 예외는 절대 전사 사슬로 전파하지 않는다(보고 leg뿐 계약).
    if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
}

static int jk_mono_ms(void) {  // 스테이지 경과 계기(CLOCK_MONOTONIC 원문)
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return (int)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

// ---- GPU 검출 leg(D2 fail-closed — 헤더 원존 주석 계약) ----
// 진실원 = vendored ggml backend 레지스트리. 성공 1 + verdict = "GPU 가속 …"
// / 실패 0 + verdict = "CPU 변환 …". ggml_backend_reg_by_name은 등록되지 않은
// 이름에 NULL(등록 전 try/catch 밀폐 — C 측 예외 유입 0).
static int jk_probe_vulkan(char* verdict, size_t verdictSz) {
    ggml_backend_reg_t reg = ggml_backend_reg_by_name("Vulkan");
    if (reg == NULL) {
        snprintf(verdict, verdictSz,
                 "CPU 변환 (Vulkan 미성립 — 레지스트리 부재: instance init 실패 계통)");
        return 0;
    }
    size_t ndev = ggml_backend_reg_dev_count(reg);
    if (ndev == 0) {
        snprintf(verdict, verdictSz, "CPU 변환 (Vulkan 장비 0 — 드라이버/GPU 미탑재)");
        return 0;
    }
    ggml_backend_dev_t dev = ggml_backend_reg_dev_get(reg, 0);
    const char* d = dev ? ggml_backend_dev_description(dev) : NULL;
    if (d == NULL || *d == '\0') d = "(desc 결실 0)";
    snprintf(verdict, verdictSz, "GPU 가속 (Vulkan: %.160s — 장비 %zu)", d, ndev);
    return 1;
}

// ---- 진행 콜백 leg — whisper_full의 호출 스레드(voiceExec)에서만 불린다
// (vendored whisper.cpp main loop 원문 — 스레드 안전: 보고는 jk_stage가
// Java 측 runOnUiThread로 UI 수렴 계약 — C 측 로컬 상태 접촉 0).
struct jk_progress_ctx { JNIEnv* env; };

static void jk_progress_cb(struct whisper_context* ctx,
                           struct whisper_state* state, int progress,
                           void* user_data) {
    (void)ctx; (void)state;
    struct jk_progress_ctx* pc = (struct jk_progress_ctx*)user_data;
    if (pc == NULL) return;
    // pct = whisper 원문 진행(0-100 — vendored whisper.cpp 진행 산식 승계)
    jk_stage(pc->env, "전사 중…", progress);
}

static const char kJniLanguage[] = "ko";
static const int kJniThreads = 4;

// ---- wav — 16 kHz/모노/s16le PCM 판독 (SttVoice.h wav::Validate16kMono 규약
// 수형 + 몸통 소비 승격). 원존 = ffmpeg 정규화 산출 캐논 44바이트 헤더
// (RIFF+fmt16+data) 초석 + 보조 청크(LIST 등) 스킵(2바이트 정렬 패딩) —
// 비적합(16k 아님·스테레오·비PCM·빈 data) = 정직 거부(암묵 리샘플 금지 —
// "리샘플 단순 계약"). s16le → f32 = /32768.0f(whisper pcmf32 계약).
static int jk_le16(const unsigned char* p) {
    return (int)p[0] | ((int)p[1] << 8);
}
static unsigned jk_le32(const unsigned char* p) {
    return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) |
           ((unsigned)p[3] << 24);
}

static int jk_jstr_to_utf8(JNIEnv* env, jstring js, const char** out,
                           char* err, size_t errsz) {
    *out = NULL;
    if (js == NULL) {
        snprintf(err, errsz, "jkapk 인자 NULL (jstring)");
        return 1;
    }
    const char* s = (*env)->GetStringUTFChars(env, js, NULL);
    if (s == NULL) {
        snprintf(err, errsz, "jkapk GetStringUTFChars 실패");
        return 1;
    }
    *out = s;
    return 0;
}

static void jk_free_jstr(JNIEnv* env, jstring js, const char* s) {
    if (s != NULL) (*env)->ReleaseStringUTFChars(env, js, s);
}

// 원문 전체 로드(무추적 asset/복사본 wav — 수 MB 규모) → 헤더 판정 → f32 변환.
// 성공 0 / 실패 1(err 기입). *out_samples는 mallocf — 호출자 free(*out_samples).
static int jk_load_wav_f32(const char* path, float** out_samples,
                           size_t* out_n, char* err, size_t errsz) {
    FILE* f = fopen(path, "rb");
    if (f == NULL) {
        snprintf(err, errsz, "jkapk wav 개방 실패: %s", path);
        return 1;
    }
    // 말미 seek로 원문 크기 → 1회 로드(스트리밍 재합성 계층 신설 금지 —
    // 캡처/asset wav는 단발성 본제).
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 44) {
        snprintf(err, errsz, "jkapk wav 부적합 (44B 미만): %s", path);
        fclose(f);
        return 1;
    }
    unsigned char* p = (unsigned char*)malloc((size_t)sz);
    if (p == NULL) {
        snprintf(err, errsz, "jkapk wav 로드 malloc 실패 (%ldB)", sz);
        fclose(f);
        return 1;
    }
    size_t n = fread(p, 1, (size_t)sz, f);
    int fail = 0;
    if (n != (size_t)sz) fail = 1;
    fclose(f);
    if (fail) {
        snprintf(err, errsz, "jkapk wav 판독 실패 (read %zu / %ld)", n, sz);
        free(p);
        return 1;
    }
    if (memcmp(p, "RIFF", 4) != 0 || memcmp(p + 8, "WAVE", 4) != 0) {
        snprintf(err, errsz, "jkapk wav 부적합 (RIFF/WAVE 아님): %s", path);
        free(p);
        return 1;
    }
    size_t off = 12;
    int fmtOk = 0, dataOk = 0;
    size_t dataOff = 0, dataSize = 0;
    while (off + 8 <= n) {
        const unsigned char* id = p + off;
        unsigned csize = jk_le32(p + off + 4);
        if (csize == 0xFFFFFFFFu || csize > n - (off + 8)) {
            snprintf(err, errsz, "jkapk wav 부적합 (잘린 청크: %u@%zu)", csize, off);
            free(p);
            return 1;
        }
        if (memcmp(id, "fmt ", 4) == 0) {
            if (csize < 16) {
                snprintf(err, errsz, "jkapk wav 부적합 (fmt <16B)");
                free(p);
                return 1;
            }
            const unsigned char* f2 = p + off + 8;
            unsigned tag = (unsigned)jk_le16(f2 + 0);
            unsigned ch = (unsigned)jk_le16(f2 + 2);
            unsigned rate = jk_le32(f2 + 4);
            unsigned brate = jk_le32(f2 + 8);
            unsigned align = (unsigned)jk_le16(f2 + 12);
            unsigned bits = (unsigned)jk_le16(f2 + 14);
            // 16 kHz/모노/s16le 규약(원존 상수 수형) — 이외 정직 거부.
            if (tag != 1u || ch != 1u || rate != 16000u || bits != 16u ||
                align != 2u || brate != rate * align) {
                snprintf(err, errsz,
                         "jkapk wav 부적합 (16 kHz 모노 s16le 아님 — "
                         "tag=%u ch=%u rate=%u bits=%u)",
                         tag, ch, rate, bits);
                free(p);
                return 1;
            }
            fmtOk = 1;
        } else if (memcmp(id, "data", 4) == 0) {
            if (csize == 0) {
                snprintf(err, errsz, "jkapk wav 부적합 (data 결실 0)");
                free(p);
                return 1;
            }
            dataOk = 1;
            dataOff = off + 8;
            dataSize = csize;
        }
        off += 8 + csize + (csize & 1);  // 청크 2바이트 정렬 패딩(RIFF 규약)
    }
    if (!fmtOk || !dataOk) {
        snprintf(err, errsz, "jkapk wav 부적합 (fmt=%d data=%d)", fmtOk, dataOk);
        free(p);
        return 1;
    }
    // 16비트 몸통 → f32(/32768.0f). 홀수 몸통 = 절단(가드 — 계약 위배는
    // 생산자 소관이고 여기선 결실 짝수 바이트만 소비).
    size_t count = dataSize / 2;
    if (count == 0 || count < (size_t)(16000 / 10)) {
        // 0.1s 미만 — whisper에 무의미(원존 "결실 0" 정직 거부 계열).
        snprintf(err, errsz, "jkapk wav 부적합 (결실 %.2fs 미만)", (double)count / 16000.0);
        free(p);
        return 1;
    }
    float* samples = (float*)malloc(count * sizeof(float));
    if (samples == NULL) {
        snprintf(err, errsz, "jkapk sample malloc 실패 (%zu samples)", count);
        free(p);
        return 1;
    }
    for (size_t i = 0; i < count; ++i) {
        int16_t v = (int16_t)jk_le16(p + dataOff + i * 2);
        samples[i] = (float)v / 32768.0f;
    }
    free(p);
    *out_samples = samples;
    *out_n = count;
    return 0;
}

// ---- transcribe core — JNI 층과 자기검증 main에서 공유하는 본체 1개 ----
// 성공 0 + *out(malloc, trim 완료) / 실패 1 + *err(malloc).
// 모델 로드 실패·whisper_full rc!=0·전사 trim 결실 0 — 전부 정직 거부.
// env = 스탬프/진행 보고 채널용(데스크톱 selftest main = NULL — log 채널 몫).
// #122: 보고 leg뿐 — 사슬 계약(판독 규약·정리·trim) 무변조.
static int jk_transcribe_core(JNIEnv* env, const char* wavPath,
                              const char* modelPath, const char* prompt,
                              char** out, char** err) {
    *out = NULL;
    *err = NULL;
    char errbuf[JKJNI_ERR_MAX];
    errbuf[0] = '\0';

    float* samples = NULL;
    size_t nSamples = 0;
    {
        int t0 = jk_mono_ms();
        if (jk_load_wav_f32(wavPath, &samples, &nSamples, errbuf, sizeof(errbuf)) != 0) {
            goto fail;
        }
        // 스탬프 leg — wav 판독 원문(정직 계기 — %가 안 올라갈 때 wav 측 분절
        // 증거). 표기는 즉시 틱 합성으로 회수(1행 계약 — MainActivity 원존 주석).
        char stamp[JKJNI_STAGE_MAX];
        snprintf(stamp, sizeof(stamp), "wav 로드 완료 (%zu samples — %d ms)",
                 nSamples, jk_mono_ms() - t0);
        jk_log(stamp);
        jk_stage(env, stamp, -1);
    }

    // #122 GPU 검출 leg(D2 fail-closed) — vendored ggml 레지스트리가
    // 진실원. 실패 = use_gpu=false(기존 경로 무변조)·성공 = true.
    // reg()/instance init 실패 = reg 부재 또는 장비 0 → CPU 폴백 원존
    // (whisper_backend_init_gpu 원존 분기 — "no GPU found" → CPU).
    char verdict[JKJNI_STAGE_MAX];
    int gpu;
    {
        gpu = jk_probe_vulkan(verdict, sizeof(verdict));
        jk_log(verdict);
        jk_stage(env, "전사 leg", gpu ? 1 : 0);
        jk_stage(env, verdict, -1);  // 원문 1행 표기(틱이 수초 내 회수)
    }

    // 스탬프 leg — init 직전 원문(파인 경로+실사이즈 — Java가 핀 등호
    // 어설션 선수행 — 추출본 존재/사이즈 원문). whisper_init 실패 = 정직
    // 거부(원존 계약 — 문언에 파인 bytes 병기).
    long modelBytes;
    int modelLoadT0;
    struct whisper_context* ctx;
    {
        modelBytes = -1;
        modelLoadT0 = jk_mono_ms();
        FILE* mf = fopen(modelPath, "rb");
        if (mf != NULL) {
            fseek(mf, 0, SEEK_END);
            modelBytes = ftell(mf);
            fclose(mf);
        }
        char stamp[JKJNI_STAGE_MAX];
        if (modelBytes >= 0) {
            snprintf(stamp, sizeof(stamp), "모델 로드 시작 (%ldB)", modelBytes);
        } else {
            snprintf(stamp, sizeof(stamp), "모델 로드 시작 (파인 실패)");
        }
        jk_log(stamp);
        jk_stage(env, stamp, -1);
    }

    struct whisper_context_params cparams = whisper_context_default_params();
    if (gpu) {
        cparams.use_gpu = true;  // D2 — 검출 성립 한정(원존 CPU 주석 승계)
    } else {
        cparams.use_gpu = false;  // CPU 원존 계약(GPU 실패 시 동작 = 기존과 동일)
    }
    ctx = whisper_init_from_file_with_params(modelPath, cparams);
    if (ctx == NULL) {
        snprintf(errbuf, sizeof(errbuf),
                 "jkapk 모델 로드 실패: %s (파인 bytes=%ld — "
                 "핀 등호 어설션은 Java 측 선수행)", modelPath, modelBytes);
        goto fail_with_samples;
    }
    // 스탬프 leg — init 지연 원문(원진 분절: 로드 지연 vs inference 지연).
    {
        char stamp[JKJNI_STAGE_MAX];
        snprintf(stamp, sizeof(stamp), "모델 로드 완료 (%d ms)",
                 jk_mono_ms() - modelLoadT0);
        jk_log(stamp);
        jk_stage(env, stamp, -1);
    }

    {
        struct whisper_full_params wparams =
            whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
        wparams.n_threads = kJniThreads;
        wparams.language = kJniLanguage;
        wparams.translate = false;
        wparams.print_progress = false;
        wparams.print_special = false;
        wparams.print_realtime = false;
        wparams.print_timestamps = false;
        if (prompt != NULL && *prompt != '\0') {
            wparams.initial_prompt = prompt;
            wparams.carry_initial_prompt = true;  // 장발화에서도 어휘 힌트 소멸 방지
        }
        // #122 진행 leg — whisper 원문 진행(%) 보고(vendored whisper.cpp 진행
        // 산식 승계 — (seek-seek_start)/(seek_end-seek_start)). env 경로 =
        // Java 상태행 "전사 중… (Ns · N%)" 합성(MainActivity 소관). selftest
        // main(env NULL) = 무음 — 콘솔 축 원존 유지.
        struct jk_progress_ctx pctx = { env };
        wparams.progress_callback = jk_progress_cb;
        wparams.progress_callback_user_data = &pctx;
        if (whisper_full(ctx, wparams, samples, (int)nSamples) != 0) {
            snprintf(errbuf, sizeof(errbuf),
                     "jkapk 전사 실패 (whisper_full rc!=0 — 모델=%s)", modelPath);
            goto fail_with_ctx;
        }

        // 세그먼트 열 직순회 — cli stdout의 진실원 승격(헤더 원존 주석).
        size_t cap = 256;
        char* text = (char*)malloc(cap);
        if (text == NULL) {
            snprintf(errbuf, sizeof(errbuf), "jkapk 전사 malloc 실패");
            goto fail_with_ctx;
        }
        size_t len = 0;
        const int nSeg = whisper_full_n_segments(ctx);
        for (int i = 0; i < nSeg; ++i) {
            const char* seg = whisper_full_get_segment_text(ctx, i);
            if (seg == NULL) continue;
            size_t l = strlen(seg);
            while (len + l + 1 > cap) {
                cap *= 2;
                char* grew = (char*)realloc(text, cap);
                if (grew == NULL) {
                    free(text);
                    snprintf(errbuf, sizeof(errbuf), "jkapk 전사 realloc 실패");
                    goto fail_with_ctx;
                }
                text = grew;
            }
            memcpy(text + len, seg, l);
            len += l;
            text[len] = '\0';
        }
        whisper_free(ctx);
        free(samples);
        samples = NULL;

        // UTF-8 정리 1회 — T1 결함 원장행 수형(orphan 소각 — 헤더 계약).
        // 소각은 조용해도 정직하다: 소각은 파열 바이트의 제거이고 전사 본문은
        // whisper 산출 원문 그대로다. 소각 증거는 "전면 소각" 경로의 err에만
        // 병기한다(본문 생존 케이스에서 전사 표면을 오염시키지 않는다).
        char* cleaned = NULL;
        size_t removed = 0;
        if (jk_utf8_clean(text, &cleaned, &removed) != 0) {
            free(text);
            snprintf(errbuf, sizeof(errbuf), "jkapk 전사 UTF-8 정리 실패 (malloc)");
            goto fail;
        }
        free(text);
        text = cleaned;
        len = strlen(text);

        // 전주 trim — cli 원존 행 선행 공백+ParseLastText 절단의 합성 등호 목표.
        size_t b = 0, e = len;
        while (b < e && (text[b] == ' ' || text[b] == '\n' || text[b] == '\r' ||
                         text[b] == '\t')) b++;
        while (e > b && (text[e - 1] == ' ' || text[e - 1] == '\n' ||
                         text[e - 1] == '\r' || text[e - 1] == '\t')) e--;
        if (b == e) {
            free(text);
            snprintf(errbuf, sizeof(errbuf),
                     "jkapk 빈 전사 (whisper full API 결실 0 — 원존 빈 전사 계약 수형"
                     "%s: orphan 소각 %zu바이트)",
                     removed > 0 ? " — 정리 후 결실 0" : "", removed);
            goto fail;
        }
        memmove(text, text + b, e - b);
        text[e - b] = '\0';
        *out = text;
        return 0;
    }

fail_with_ctx:
    whisper_free(ctx);
fail_with_samples:
    free(samples);
fail:
    *err = (char*)malloc(strlen(errbuf) + 1);
    if (*err != NULL) strcpy(*err, errbuf);
    return 1;
}

// ---- JNI 계약 1종 — Java_chang_jkapk_JniWhisper_jkapkTranscribe ----
// (헤더 원존 주석의 Java 계약 수형 — Kotlin 측 클래스는 T2 몫)
JNIEXPORT jstring JNICALL
Java_chang_jkapk_JniWhisper_jkapkTranscribe(JNIEnv* env, jclass clazz,
                                            jstring jWavPath, jstring jModelPath,
                                            jstring jPrompt) {
    (void)clazz;
    char errbuf[JKJNI_ERR_MAX];
    errbuf[0] = '\0';
    const char *wavPath = NULL, *modelPath = NULL, *prompt = NULL;
    int failed = 0;
    if (!failed && jk_jstr_to_utf8(env, jWavPath, &wavPath, errbuf, sizeof(errbuf)) != 0) failed = 1;
    if (!failed && jk_jstr_to_utf8(env, jModelPath, &modelPath, errbuf, sizeof(errbuf)) != 0) failed = 1;
    if (!failed && jk_jstr_to_utf8(env, jPrompt, &prompt, errbuf, sizeof(errbuf)) != 0) failed = 1;
    jstring result = NULL;
    if (!failed) {
        char* out = NULL;
        char* err = NULL;
        if (jk_transcribe_core(env, wavPath, modelPath, prompt, &out, &err) != 0) {
            snprintf(errbuf, sizeof(errbuf), "jkapk 전사 실패: %s",
                     err != NULL ? err : "(err 누락 — 정직 거부 계약 위배)");
            (*env)->ThrowNew(env,
                             (*env)->FindClass(env, "java/lang/RuntimeException"),
                             errbuf);
        } else {
            result = (*env)->NewStringUTF(env, out);
            if (result == NULL) {  // 새 문자열 실패(메모리) = OOM 정직 거부
                (*env)->ThrowNew(env,
                                 (*env)->FindClass(env, "java/lang/RuntimeException"),
                                 "jkapk NewStringUTF 실패 (OOM)");
            }
            free(out);
        }
        free(err);
    }
    jk_free_jstr(env, jWavPath, wavPath);
    jk_free_jstr(env, jModelPath, modelPath);
    jk_free_jstr(env, jPrompt, prompt);
    return result;  // 실패 = NULL + pending exception(정직 거부 계약)
}

// ---- 자기검증 main(JKJNI_SELFTEST_MAIN 한정 — JNI 경로 없이 같은 core 1개
// 소비). 폰 실측(#121 v2 T1 — CANON 밖)에서 이 TU를 NDK로 빌드한 실행 파일로
// 합성 16k wav+폰 모델(fixture/tiny-ko) 1회 전사 — stdout 결실 원문 수취.
// APK에 넘어가지 않는다(매크로 미정의 — 라이브러리 본체 무접점).
#ifdef JKJNI_SELFTEST_MAIN
int main(int argc, char** argv) {
    // argv: 1=wav 2=model 3=prompt(생략 가능 — kVoicePrompt 계약의 argv 승계)
    // v2 T2 — 먼저 결정 부품 등호(UTF-8 정리 2zb-5a..h — 비결정 전사 축
    // 어셜션 금지 계약 준수: 전사 결실은 stdout 수취(원문)만).
    if (jk_selftest_sanitize() != 0) {
        fprintf(stderr, "jkapk 정리부 selftest FAIL — 중단\n");
        return 3;
    }
    if (argc < 3) {
        fprintf(stderr, "usage: %s <wav> <model> [prompt]\n", argv[0]);
        return 2;
    }
    const char* prompt = (argc > 3) ? argv[3] : "";
    char* out = NULL;
    char* err = NULL;
    if (jk_transcribe_core(NULL, argv[1], argv[2], prompt, &out, &err) != 0) {
        fprintf(stderr, "%s\n", err ? err : "(err 누락)");
        free(out);
        free(err);
        return 1;
    }
    fputs(out, stdout);
    fputc('\n', stdout);
    free(out);
    return 0;
}
#endif  // JKJNI_SELFTEST_MAIN
#endif  // JKSANITIZE_SELFTEST_MAIN — 정리부 단독 자기검증(Win/폰 양축)
