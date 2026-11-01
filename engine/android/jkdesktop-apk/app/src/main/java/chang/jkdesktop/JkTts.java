// #126 jkdesktop-apk — 시스템 TTS 브리지 (스펙 §5 — TTS = SDK 단일 축).
// C++(서버 /talk 응답) → JNI speak(String) → TextToSpeech(메인 루퍼 hop —
// JkVoice와 동일 스레드 규약). init 실패 = 정직 false(무음 — fail-closed).
package chang.jkdesktop;

import android.content.Context;
import android.speech.tts.TextToSpeech;

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

    public static void speak(String text) {
        if (!ready || tts == null) begin(null);
        if (tts == null) return;
        new android.os.Handler(android.os.Looper.getMainLooper()).post(() -> {
            try {
                tts.stop();
                tts.speak(text, TextToSpeech.QUEUE_FLUSH, null, "jk-" + System.nanoTime());
            } catch (Exception e) {
                android.util.Log.w("jkdesk", "tts speak failed: " + e);
            }
        });
    }
}