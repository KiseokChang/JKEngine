// include/jkhooks_android.h — 안드로이드 JNI 훅 ABI 단일 선언 (docs/106 §4)
//
// 정의 = android/jkdesktop-apk/app/src/main/jni/jkvoice_bridge.cpp (libjkdesk
// 전용). 소비자 = jkweb(/talk 라우터)·서버·허브 TU.
//
// 왜 weak인가: 같은 TU가 클라 모듈(libjkc)에도 컴파일되지만 훅 정의는
// jkdesk에만 있다 — weak 선언이면 링크에서 미정의가 허용되고, 콜사이트는
// `if (JkXxx)` 선조회로 정직하게 건너뛴다(데스크톱/리눅스 빌드 = 훅 0).
//
// 규약: 모든 훅은 int 반환(0 = 실패/미적용, 1 = 전달됨). 인자 소유권 없음.
#ifndef JK_HOOKS_ANDROID_H
#define JK_HOOKS_ANDROID_H

#if defined(__GNUC__)
#define JK_WEAK_HOOK __attribute__((weak))
#else
#define JK_WEAK_HOOK
#endif

extern "C" {
// 브라우저 — BrowserActivity 개봉/닫기/뒤앞(방향 0=뒤 1=앞).
JK_WEAK_HOOK int JkStartBrowserActivity(const char* url);
JK_WEAK_HOOK int JkCloseBrowserActivity(void);
JK_WEAK_HOOK int JkBrowserNavigate(int dir);

// 음성/TTS — /voice 개시·정지, 응답 발화, 큐 자동 턴.
JK_WEAK_HOOK int JKAndroidVoiceBegin();
JK_WEAK_HOOK int JKTtsSpeak(const char* text);
JK_WEAK_HOOK int JkTtsStopAll(void);
JK_WEAK_HOOK int JKVoiceAutoturnStart();
}

#endif  // JK_HOOKS_ANDROID_H
