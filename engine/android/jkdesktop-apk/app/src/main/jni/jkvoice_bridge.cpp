// #126 jkdesktop-apk — 음성 브리지 C 수신부 (스펙 §5 — STT 단일 축).
// ① JNI_OnLoad: SDLActivity의 loadLibrary 스레드(앱 클래스로더 보유)에서
//    JkVoice 클래스를 전역 참조로 캐시 — 네이티브 스레드 FindClass는 시스템
//    로더만 봐서 "JkVoice class missing"(02:2x 원문 — FatalError = 재기동
//    주기 원진) 함정 수형. ② Java 결과 → 큐 → 자동 턴(POST /talk 8091).
// ③ /voice(jkweb 경유) → begin(ctx) — SDL_AndroidGetActivity() = Context.
#include <algorithm>
#include <android/log.h>
#include <arpa/inet.h>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <jni.h>
#include <mutex>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include <SDL_system.h>  // SDL_AndroidGetJNIEnv / SDL_AndroidGetActivity
#include <SDL.h>
#ifdef main
#undef main
#endif

#include <agent/JKLlmTurnHttp.h>  // T7 — cloud transport 주입(SetHttpsPostTransport)

static JavaVM* g_vm = nullptr;
static jclass g_voiceClass = nullptr;
static jclass g_ttsClass = nullptr;
static jclass g_httpClass = nullptr;

// 전방 선언(정의는 g_voiceMtx 뒤 — JNI_OnLoad의 주입이 앞서 쓴다).
static jk::agent::LlmHttpPostResult JkHttpsTransportCall(
    const std::string& url, const std::string& bodyJson,
    const std::string& bearer, int timeoutMs);

extern "C" jint JNI_OnLoad(JavaVM* vm, void*) {
    g_vm = vm;
    JNIEnv* env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }
    jclass ttsLocal = env->FindClass("chang/jkdesktop/JkTts");
    if (ttsLocal) {
        g_ttsClass = static_cast<jclass>(env->NewGlobalRef(ttsLocal));
        env->DeleteLocalRef(ttsLocal);
    }
    if (!g_voiceClass) {
        jclass local = env->FindClass("chang/jkdesktop/JkVoice");
        if (local) {
            g_voiceClass = static_cast<jclass>(env->NewGlobalRef(local));
            env->DeleteLocalRef(local);
            __android_log_print(ANDROID_LOG_INFO, "jksrv",
                                "[jkdesk] JkVoice class cached");
        } else {
            env->ExceptionClear();
            __android_log_print(ANDROID_LOG_WARN, "jksrv",
                                "[jkdesk] JkVoice class not found at load");
        }
    }
    if (!g_httpClass) {
        // T7 — cloud TLS leg의 클래스 캐시(JkVoice 원존 계약과 동형 — 네이티브
        // 스레드 FindClass는 시스템 로더만 봐서 실패하는 함정의 승계 방어).
        jclass local = env->FindClass("chang/jkdesktop/JkHttp");
        if (local) {
            g_httpClass = static_cast<jclass>(env->NewGlobalRef(local));
            env->DeleteLocalRef(local);
        } else {
            env->ExceptionClear();
            __android_log_print(ANDROID_LOG_WARN, "jksrv",
                                "[jkdesk] JkHttp class not found at load");
        }
        // transport 주입 — 라이브러리 적재 때 1회(결정론). 주입 후 모든
        // llm_mode=cloud 턴이 이 브리지로 간다(주입 부재 = 정직 실패 계약이
        // 백업으로 남는다 — 데스크톱/클란 빌드 = 주입 0이라 정직 거부 원존).
        jk::agent::SetHttpsPostTransport(&JkHttpsTransportCall);
        __android_log_print(ANDROID_LOG_INFO, "jksrv",
                            "[jkdesk] https transport wired=%d", g_httpClass ? 1 : 0);
    }
    return JNI_VERSION_1_6;
}

static std::mutex g_voiceMtx;
static std::vector<std::string> g_voiceQueue;

// --- T7 cloud TLS leg: 네이티브 → Java HttpsURLConnection ------------------
// 폰의 cloud 턴이 지나는 유일 수형. 호출 스레드 = LLM 턴 워커(std::thread —
// SDL 메인 스레드의 env가 아니므로 Attach/Detach 필수 — GetEnv가 JNI_EDEDET
// ACHED를 주는 실측 원존 계약의 일반화). JkVoice env 원존(SDL_AndroidGetJNIEnv
// 는 SDL 시작 스레드 한정)과 다른 점 = 워커 스레드라 Attach+Detach 사거리가
// 턴당 왕복한다.
static JNIEnv* AttachTurnEnv() {
    if (!g_vm) return nullptr;
    JNIEnv* env = nullptr;
    const jint got = g_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (got == JNI_OK) return env;
    if (got != JNI_EDETACHED) return nullptr;
    if (g_vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return nullptr;
    return env;
}

static void DetachTurnEnv() {
    if (g_vm) g_vm->DetachCurrentThread();
}

// SetHttpsPostTransport에 심는 C 진입(torch 계약: jk::agent::HttpsPostFn).
// 실패(null 응답) = ok=false의 정직 전달 — 소비자 stub 폴백 계약과 동형.
static jk::agent::LlmHttpPostResult JkHttpsTransportCall(
    const std::string& url, const std::string& bodyJson,
    const std::string& bearer, int timeoutMs) {
    jk::agent::LlmHttpPostResult out;
    JNIEnv* env = AttachTurnEnv();
    if (!env || !g_httpClass) {
        out.status = "cloud: env/class missing";
        DetachTurnEnv();
        return out;
    }
    jmethodID mid =
        env->GetStaticMethodID(g_httpClass, "httpPost",
                               "(Ljava/lang/String;Ljava/lang/String;"
                               "Ljava/lang/String;I)Ljava/lang/String;");
    if (!mid) {
        env->ExceptionClear();
        out.status = "cloud: method missing";
        DetachTurnEnv();
        return out;
    }
    jstring jUrl = env->NewStringUTF(url.c_str());
    jstring jBody = env->NewStringUTF(bodyJson.c_str());
    jstring jBearer =
        env->NewStringUTF(bearer.empty() ? "" : bearer.c_str());
    jobject res = env->CallStaticObjectMethod(g_httpClass, mid, jUrl, jBody,
                                              jBearer,
                                              static_cast<jint>(timeoutMs));
    env->ExceptionClear();
    env->DeleteLocalRef(jUrl);
    env->DeleteLocalRef(jBody);
    env->DeleteLocalRef(jBearer);
    if (res) {
        const char* utf = env->GetStringUTFChars(
            static_cast<jstring>(res), nullptr);
        if (utf) {
            out.body = utf;
            env->ReleaseStringUTFChars(static_cast<jstring>(res), utf);
        }
        env->DeleteLocalRef(res);
        out.status = "200";
        out.ok = !out.body.empty();
    } else {
        out.status = "cloud: java http null (non-200/exception)";
    }
    DetachTurnEnv();
    return out;
}

static void VoiceEnqueue(const std::string& text) {
    std::lock_guard<std::mutex> lk(g_voiceMtx);
    g_voiceQueue.push_back(text);
}

// --- Java → C 콜백 (JkVoice의 native 명명 계약) -----------------------------
extern "C" JNIEXPORT void JNICALL
Java_chang_jkdesktop_JkVoice_nVoiceResult(JNIEnv* env, jclass, jstring text) {
    if (!text) return;
    const char* utf = env->GetStringUTFChars(text, nullptr);
    if (utf) {
        VoiceEnqueue(std::string(utf));
        env->ReleaseStringUTFChars(text, utf);
    }
    __android_log_print(ANDROID_LOG_INFO, "jksrv", "[jkdesk] voice result queued");
}

extern "C" JNIEXPORT void JNICALL
Java_chang_jkdesktop_JkVoice_nVoiceError(JNIEnv* env, jclass, jstring err) {
    std::string msg = "(null)";
    if (err) {
        const char* utf = env->GetStringUTFChars(err, nullptr);
        if (utf) {
            msg = utf;
            env->ReleaseStringUTFChars(err, utf);
        }
    }
    __android_log_print(ANDROID_LOG_WARN, "jksrv", "[jkdesk] voice error: %s",
                        msg.c_str());
}

// --- C → Java 개시 요청 (HTTP /voice 경유) ----------------------------------
extern "C" int JKAndroidVoiceBegin() {
    JNIEnv* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    if (!env || !g_voiceClass) {
        std::fprintf(stderr, "[jkdesk] voice begin: env/class 없음\n");
        return 0;
    }
    jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
    if (!activity) {
        std::fprintf(stderr, "[jkdesk] voice begin: activity 없음\n");
        return 0;
    }
    jmethodID mid = env->GetStaticMethodID(
        g_voiceClass, "begin", "(Landroid/content/Context;)Z");
    if (!mid) {
        env->ExceptionClear();
        std::fprintf(stderr, "[jkdesk] voice begin: begin(Context) 미검출\n");
        env->DeleteLocalRef(activity);
        return 0;
    }
    jboolean ok = env->CallStaticBooleanMethod(g_voiceClass, mid, activity);
    env->ExceptionClear();
    env->DeleteLocalRef(activity);
    std::fprintf(stderr, "[jkdesk] voice begin ok=%d\n", (int)ok);
    return ok ? 1 : 0;
}

// --- 자동 턴 스레드: 큐 drain → POST /talk --------------------------------
static std::string HttpPostTalk(const std::string& body) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return "";
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8091);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        ::close(fd);
        return "";
    }
    std::string req =
        "POST /talk HTTP/1.0\r\nHost: localhost:8091\r\n"
        "Content-Type: application/json\r\nContent-Length: " +
        std::to_string(body.size()) + "\r\n\r\n" + body;
    if (::send(fd, req.data(), req.size(), 0) < 0) {
        ::close(fd);
        return "";
    }
    char buf[512];
    const ssize_t n = ::recv(fd, buf, sizeof(buf) - 1, 0);
    ::close(fd);
    if (n <= 0) return "";
    buf[std::min<size_t>((size_t)n, sizeof(buf) - 1)] = '\0';
    return std::string(buf);
}

// --- T1.5: 앱 자체 브라우저 개봉(jkweb AmpkOpen 소비 leg) -----------------
#include <agent/JKLlmTurnHttp.h>
extern "C" int JkStartBrowserActivity(const char* url);
extern "C" int JkStartBrowserActivity(const char* url) {
    if (!url) url = "";   // 빈 = 마지막 주소 그대로(BrowserActivity 기본 페이지)
    JNIEnv* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
    if (!env || !activity) return 0;
    jclass cls = env->FindClass("chang/jkdesktop/BrowserActivity");
    if (!cls) { env->ExceptionClear(); env->DeleteLocalRef(activity); return 0; }
    jmethodID mid = env->GetStaticMethodID(
        cls, "openWithUrl", "(Landroid/content/Context;Ljava/lang/String;)V");
    if (!mid) {
        env->ExceptionClear();
        env->DeleteLocalRef(cls); env->DeleteLocalRef(activity);
        return 0;
    }
    jstring ju = env->NewStringUTF(url);
    env->CallStaticVoidMethod(cls, mid, activity, ju);
    env->ExceptionClear();
    env->DeleteLocalRef(ju);
    env->DeleteLocalRef(cls);
    env->DeleteLocalRef(activity);
    std::fprintf(stderr, "[browser] open delivered: %s\n", url);
    return 1;
}

// --- TTS speak (HTTP /talk 응답의 reply 본문) ------------------------------
extern "C" int JKTtsSpeak(const char*);
extern "C" int JKTtsSpeak(const char* text) {
    JNIEnv* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    if (!env || !g_ttsClass) return 0;
    jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
    if (!activity) return 0;
    jmethodID mid = env->GetStaticMethodID(g_ttsClass, "begin",
                                           "(Landroid/content/Context;)V");
    if (!mid) { env->ExceptionClear(); env->DeleteLocalRef(activity); return 0; }
    env->CallStaticVoidMethod(g_ttsClass, mid, activity);
    env->DeleteLocalRef(activity);
    jmethodID mid2 = env->GetStaticMethodID(g_ttsClass, "speak",
                                            "(Ljava/lang/String;)V");
    if (!mid2) { env->ExceptionClear(); return 0; }
    jstring s = env->NewStringUTF(text ? text : "");
    env->CallStaticVoidMethod(g_ttsClass, mid2, s);
    env->ExceptionClear();
    env->DeleteLocalRef(s);
    return 1;
}

extern "C" int JKVoiceAutoturnStart() {
    std::thread([] {
        while (true) {
            std::string got;
            {
                std::lock_guard<std::mutex> lk(g_voiceMtx);
                if (!g_voiceQueue.empty()) {
                    got = g_voiceQueue.front();
                    g_voiceQueue.erase(g_voiceQueue.begin());
                }
            }
            if (!got.empty()) {
                std::fprintf(stderr, "[jkdesk] voice turn: %s\n", got.c_str());
                std::string esc;
                for (char c : got) {
                    if (c == '"') esc += '\\';
                    else if (c == '\\') esc += '\\';
                    else if (c == '\n') { esc += ' '; continue; }
                    esc += c;
                }
                const std::string resp =
                    HttpPostTalk("{\"text\":\"" + esc + "\"}");
                // TTS: 응답 JSON의 reply 본문 발화(최소 파서 — 첫 "reply":"…").
                std::string reply;
                const std::string key = "\"reply\":\"";
                const size_t kp = resp.find(key);
                if (kp != std::string::npos) {
                    for (size_t i = kp + key.size(); i < resp.size(); ++i) {
                        if (resp[i] == '\\') {
                            ++i;
                            if (i < resp.size()) reply += resp[i];
                            continue;
                        }
                        if (resp[i] == '"') break;
                        reply += resp[i];
                    }
                }
                if (!reply.empty() && JKTtsSpeak(reply.c_str()) == 1) {
                    std::fprintf(stderr,
                                 "[jkdesk] voice turn tts: %s\n", reply.c_str());
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }
    }).detach();
    return 0;
}
