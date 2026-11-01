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
        try {
            if (recognizer != null) {
                recognizer.destroy();
                recognizer = null;
            }
            recognizer = SpeechRecognizer.createSpeechRecognizer(ctx);
            recognizer.setRecognitionListener(new RecognitionListener() {
                @Override public void onResults(Bundle results) {
                    java.util.ArrayList<String> list =
                            results.getStringArrayList(SpeechRecognizer.RESULTS_RECOGNITION);
                    if (list != null && !list.isEmpty()) {
                        nVoiceResult(list.get(0));
                    } else {
                        nVoiceError("결과 없음");
                    }
                }
                @Override public void onError(int error) {
                    nVoiceError("인식 오류 code=" + error);
                }
                @Override public void onPartialResults(Bundle partialResults) {}
                @Override public void onRmsChanged(float rmsdB) {}
                @Override public void onReadyForSpeech(Bundle params) {}
                @Override public void onBeginningOfSpeech() {}
                @Override public void onEndOfSpeech() {}
                @Override public void onBufferReceived(byte[] buffer) {}
                @Override public void onEvent(int eventType, Bundle params) {}
            });
            Intent it = new Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH);
            it.putExtra(RecognizerIntent.EXTRA_LANGUAGE_MODEL,
                    RecognizerIntent.LANGUAGE_MODEL_FREE_FORM);
            it.putExtra(RecognizerIntent.EXTRA_LANGUAGE, "ko-KR");
            it.putExtra(RecognizerIntent.EXTRA_PARTIAL_RESULTS, false);
            recognizer.startListening(it);
            return true;
        } catch (Exception e) {
            nVoiceError("begin 실패: " + e);
            return false;
        }
    }
}