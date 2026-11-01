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

static JavaVM* g_vm = nullptr;
static jclass g_voiceClass = nullptr;
static jclass g_ttsClass = nullptr;

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
    return JNI_VERSION_1_6;
}

static std::mutex g_voiceMtx;
static std::vector<std::string> g_voiceQueue;

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
        return false;
    }
    char buf[512];
    const ssize_t n = ::recv(fd, buf, sizeof(buf) - 1, 0);
    ::close(fd);
    if (n <= 0) return false;
    buf[std::min<size_t>((size_t)n, sizeof(buf) - 1)] = '\0';
    return std::strstr(buf, "\"ok\":true") != nullptr;
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
                if (!HttpPostTalk("{\"text\":\"" + esc + "\"}")) {
                    std::fprintf(stderr, "[jkdesk] voice turn POST failed\n");
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }
    }).detach();
    return 0;
}
