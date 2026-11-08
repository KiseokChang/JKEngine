// #126 jkdesktop-apk — SpeechRecognizer 브리지 (스펙 §5 — STT 단일 축).
// C++(서버 스레드)가 JNI로 begin()을 부르고, 인식 결과는 네이티브 콜백
// (libjkdesk.so의 Java_chang_jkdesktop_JkVoiceService_n*)으로 돌아간다.
// 콜백의 C++ 수신부 = jksrv_main.cpp의 큐+자동 턴(->localhost:8091/talk).
package chang.jkdesktop;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.speech.RecognitionListener;
import android.speech.RecognizerIntent;
import android.speech.SpeechRecognizer;

public class JkVoice {
    private static SpeechRecognizer recognizer;

    private static native void nVoiceResult(String text);
    private static native void nVoiceError(String err);

    public static boolean begin(Context ctx) {
        // libjkdesk.so 로드 보증(크래시 원존 11-07: force-stop→냉재기동에서
        // PhoneControlActivity와 MainActivity가 병렬 부팅하면 SDL 부하 경로의
        // loadLibrary보다 SpeechRecognizer onError가 먼저 터져 UnsatisfiedLinkError로
        // 프로세스가 죽어 DeX 태스크가 소멸했다). loadLibrary는 멱등 — 이미
        // 로드돼 있으면 no-op. 콜백도 가드(방어): 음성 실패가 프로세스를 죽이지 않는다.
        try { System.loadLibrary("jkdesk"); } catch (Throwable t) { }
        // SpeechRecognizer는 main 스레드 전용 런타임 규약(원문: RuntimeException
        // "should be used only from the application's main thread") — C 네이티브
        // 스레드에서 온 개시 요청을 메인 루퍼로 hop시킨다.
        final Context c = ctx;
        new android.os.Handler(android.os.Looper.getMainLooper()).post(() -> {
            listenerCtx = c.getApplicationContext();  // 재무장 leg의 Context 진실원
            rearmFails = 0;   // 수동 개시 = 새 세션(실패 카운터 리셋)
            try {
                if (recognizer == null) {
                    recognizer = SpeechRecognizer.createSpeechRecognizer(c);
                    recognizer.setRecognitionListener(listener);
                }
                recognizer.startListening(makeIntent());
                // 개시 성공 유무는 결과/오류 콜백으로 보고된다(nVoice*).
            } catch (Exception e) {
                nVoiceError("start 실패: " + e);
            } catch (Throwable t) {
                // 네이티브 미로드 상태 콜백은 프로세스 사망이 아니라 포기(음성=옵션).
            }
        });
        return true;
    }

    private static final RecognitionListener listener = new RecognitionListener() {
        @Override public void onResults(Bundle results) {
            java.util.ArrayList<String> list =
                    results.getStringArrayList(SpeechRecognizer.RESULTS_RECOGNITION);
            if (list != null && !list.isEmpty()) {
                rearmFails = 0;   // 성공 — 재무장 카운터 리셋
                try { nVoiceResult(list.get(0)); } catch (Throwable t) { }
            } else {
                try { nVoiceError("결과 없음"); } catch (Throwable t) { }
            }
            ScheduleRearm();
        }
        @Override public void onError(int error) {
            if (rearmFails < 6) rearmFails++;   // 카운트는 오류마다(REARM에서 하지 않는다 — 1판 결함 원존)
            try { nVoiceError("인식 오류 code=" + error); } catch (Throwable t) { }
            ScheduleRearm();
        }
        @Override public void onPartialResults(Bundle partialResults) {}
        @Override public void onRmsChanged(float rmsdB) {}
        @Override public void onReadyForSpeech(Bundle params) {}
        @Override public void onBeginningOfSpeech() {}
        @Override public void onEndOfSpeech() {}
        @Override public void onBufferReceived(byte[] buffer) {}
        @Override public void onEvent(int eventType, Bundle params) {}
    };

    // [상시 구두 재무장 leg — 2026-11-08] 인식기는 1발성 후 정지한다(결과·
    // 무발화 에러 code=7 모두) — 룸 UX(구두 상시 준비)에는 자동 재무장이
    // 전제. **1판 카운터 결함 원존(실측: code=5 0.8초 무한 반복 — 첫 스킵에서
    // 카운터가 안 오르는 논리)** → 정확 계약: onError마다 실패 카운트++·
    // 간격 1.5초(삼성 서비스 전환 최소 소화기 — 짧은 간격=code=5 원존)·
    // 5회 초과 = 스스로 멈춘다(정직 포기 — /voice·컨트롤 앱 재개시 채널).
    public static Context listenerCtx;
    public static int rearmFails;
    public static final java.lang.Runnable REARM = () -> {
        if (listenerCtx == null || recognizer == null || rearmFails >= 5) return;
        try {
            recognizer.startListening(makeIntent());
        } catch (Throwable t) { }
    };

    private static void ScheduleRearm() {
        new android.os.Handler(android.os.Looper.getMainLooper())
            .postDelayed(REARM, 1500);
    }

    // [에코 루프 뮤트 leg — 2026-11-08 폰 원존: voice turn 26회 반복] TTS가
    // 말하는 동안 마이크 정지(자기 답변을 재청취하는 자체 에코 방해) —
    // UtteranceProgressListener onDone/onError 지시로 재무장(JkTts가 부름).
    public static void MuteMicUntilDone() {
        try {
            if (recognizer != null) recognizer.stopListening();
        } catch (Throwable t) { }
    }

    public static void MuteDoneThenRearm() {
        android.os.Handler h = new android.os.Handler(
            android.os.Looper.getMainLooper());
        h.postDelayed(() -> {
            try {
                if (listenerCtx != null && recognizer != null
                    && rearmFails < 5) {
                    recognizer.startListening(makeIntent());
                }
            } catch (Throwable t) { }
        }, 600);
    }

    private static Intent makeIntent() {
        Intent it = new Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH);
        it.putExtra(RecognizerIntent.EXTRA_LANGUAGE_MODEL,
                RecognizerIntent.LANGUAGE_MODEL_FREE_FORM);
        it.putExtra(RecognizerIntent.EXTRA_LANGUAGE, "ko-KR");
        it.putExtra(RecognizerIntent.EXTRA_PARTIAL_RESULTS, false);
        return it;
    }
}
