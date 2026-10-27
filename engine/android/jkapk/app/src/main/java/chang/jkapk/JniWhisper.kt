// #121 JK APK v2 (T2) — JNI 계약 클래스(T1 whisper_jni.c 헤더 원존 주석의
// Java 계약 수형) + 전사 텍스트 보조 정리(Java 측 보조 원존).
//
// 계약(원존 승계 — whisper_jni.c 헤더 주석):
//   - System.loadLibrary("whisper_jni") 1회 — libwhisper_jni.so(gradle
//     jniLibs 패키지 — T1 assembleDebug 원문 산출).
//   - @JvmStatic external fun jkapkTranscribe(wavPath, modelPath, prompt) =
//     Java_chang_jkapk_JniWhisper_jkapkTranscribe(JNIEnv*, jclass, ...) —
//     object의 @JvmStatic이 static 심볼을 낸다(JNI 시그니처 jclass 수형).
//     성공 = 전사 원문(UTF-8 정리+trim 완료 — v2 T2 정리 계약 적용본),
//     실패 = RuntimeException(한국어 이유 — 정직 거부 계약, 빈 문자열
//     승용 금지). 호출은 UI 스레드 금지(MainActivity voiceExec 스레드 소관).
//
// kVoicePrompt 수형 = SttVoice.cpp(#112) 원존 리터럴 동일 문자열(#115b 갱신
// + #118 T2 창 배지 어휘 포함 — 단일 진실원의 문자 수형 — 원존 주석과 같은
// 정합 계약: 표와의 정합은 리포트 원문 대조 몫). 언어 핀 "ko"는 JNI 층
// kJniLanguage가 소유(T1 원존 계약 — Java 측 이중 승계 0).
package chang.jkapk

object JniWhisper {

    init {
        // fail-loud(UnsatisfiedLinkError) — 원존 "정직 거부" 문화: 로드 실패를
        // 조용히 삼키면 전사 사슬이 조용한 공백으로 사망한다.
        System.loadLibrary("whisper_jni")
    }

    @JvmStatic
    external fun jkapkTranscribe(wavPath: String, modelPath: String,
                                 prompt: String): String

    // kVoicePrompt — SttVoice.cpp 원존 리터럴 수형(#112 EYESfix Step A +
    // #115b T1 + #118 T2 어휘 — prompt는 디코딩 편향 리스크 원장행도 승계:
    // 무발화 환각이 어휘 조합형 환각으로 이어질 수 있다 — 실발화 EYES 몫).
    const val kVoicePrompt: String =
        "지뢰찾기 테트리스 알림 실행해줘 실행 열어줘 켜줘 닫아줘 꺼줘 앞으로 가져와 " +
        "포커스 창 목록 창목록 뭐 떠 있어 멈춰줘 멈춰 정지해줘 정지 일시정지 " +
        "재생해줘 재생 완전히 정지해줘 보여줘 창 번호 보여줘 번호 보여줘 번 창"

    // Java 측 보조 원존(v2 T2 — 정리 계약의 2차 게이트). 1차 게이트 = JNI 층
    // jk_utf8_clean(orphan 소각 — modified UTF-8 수입 전). 이 함수는 문자열로
    // 디코딩된 뒤 남을 수 있는 **단독(unpaired) 서러게이트만 소각**한다 —
    // NewStringUTF가 서러게이트 쌍은 정상 수입하지만, 원본 byte-fallback가
    // 쌍 밖에서 열리면 String 안에 단독 서러게이트가 남을 수 있고 그대로
    // /talk JSON(StandardCharsets.UTF_8 = 3바이트 '?' 치환 — 발명 근접)이나
    // TextView에 유입될 수 있다. 발명 0 — 단독 서러게이트 소각만.
    fun sanitizeTranscript(s: String): String {
        if (!s.any { Character.isSurrogate(it) }) return s
        val sb = StringBuilder(s.length)
        var i = 0
        while (i < s.length) {
            val ch = s[i]
            when {
                Character.isHighSurrogate(ch) &&
                    i + 1 < s.length && Character.isLowSurrogate(s[i + 1]) -> {
                    sb.append(ch).append(s[i + 1]); i += 2
                }
                Character.isSurrogate(ch) -> i += 1  // 단독 서러게이트 = 소각
                else -> { sb.append(ch); i += 1 }
            }
        }
        return sb.toString()
    }
}
