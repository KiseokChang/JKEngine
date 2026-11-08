// #126 jkdesktop-apk — 시스템 TTS 브리지 (스펙 §5 — TTS = SDK 단일 축).
// C++(서버 /talk 응답) → JNI speak(String) → TextToSpeech(메인 루퍼 hop —
// JkVoice와 동일 스레드 규약). init 실패 = 정직 false(무음 — fail-closed).
package chang.jkdesktop;

import android.content.Context;
import android.speech.tts.TextToSpeech;
import android.speech.tts.UtteranceProgressListener;

public class JkTts {
    private static TextToSpeech tts;
    private static boolean ready;

    private static native String notUsed();  // 미사용 — JNI 역방향 없음

    public static void begin(Context ctx) {
        try {
            if (tts != null) return;
            tts = new TextToSpeech(ctx, status -> {
                ready = (status == TextToSpeech.SUCCESS);
                if (ready) {
                    tts.setLanguage(new java.util.Locale("ko", "KR"));
                }
            });
        } catch (Exception e) {
            android.util.Log.w("jkdesk", "tts begin failed: " + e);
        }
    }

    // [안내 즉시 정지 leg — 2026-11-08 사용자 원존 "마이크를 누르면 안내
    // 멈추며 안돼요"] — jkweb /voice가 JNI로 부른다(마이크 탭 = 안내 정지 +
    // 듣기 개시). 정지 = 말 버림 + 마이크 재무장(JkVoice 뮤트 계약 승계).
    public static void stopAll() {
        new android.os.Handler(android.os.Looper.getMainLooper()).post(() -> {
            try {
                if (tts != null) tts.stop();
                JkVoice.MuteDoneThenRearm();
            } catch (Exception e) {
                android.util.Log.w("jkdesk", "tts stopAll failed: " + e);
            }
        });
    }

    public static void speak(String text) {
        if (!ready || tts == null) begin(null);
        if (tts == null) return;
        new android.os.Handler(android.os.Looper.getMainLooper()).post(() -> {
            try {
                tts.stop();
                // [에코 루프 뮤트 leg — 2026-11-08 폰 원존: voice turn 26회
                // 반복 = TTS 답변을 상시 마이크가 재청취 → LLM 재회신 → 재TTS]
                // 발화 전 마이크 뮤트·종료 후 자동 재무장 — 미완성 채널은
                // JkVoice가 소유(정직 계약 동형).
                JkVoice.MuteMicUntilDone();
                tts.setOnUtteranceProgressListener(
                    new UtteranceProgressListener() {
                        @Override public void onStart(String id) { }
                        @Override public void onDone(String id) {
                            JkVoice.MuteDoneThenRearm();
                        }
                        @Override public void onError(String id) {
                            JkVoice.MuteDoneThenRearm();
                        }
                        @Override public void onError(String id, int code) {
                            JkVoice.MuteDoneThenRearm();
                        }
                    });
                tts.speak(text, TextToSpeech.QUEUE_FLUSH, null, "jk-" + System.nanoTime());
            } catch (Exception e) {
                android.util.Log.w("jkdesk", "tts speak failed: " + e);
            }
        });
    }
}