package chg.jkcontrol;

// JK Control v1 - 독립 컨트롤 앱 (사용자 백로그 원존 11-07: "컨트롤은 독립 앱이어야").
// DeX 모드 중 폰 화면의 컨트롤 판: MIC 탭 = SpeechRecognizer → 결과를
// localhost:8091/talk로 POST(jkdesktop 통일 APK의 in-process HTTP — 같은 폰
// 루프백) → 엔진 턴 → DeX 화면 반응. 서버와 라이프사이클 분리(독립 앱 근거
// 원존: 컨트롤이 서버 재시작 중에도 살아 있는다 + 크래시 격리).
// BACK/BROWSER 버튼 = 같은 /talk 도구 지시(keyevent 4 전달 · 브라우저 개봉 envelope
// 응답은 jkweb이 브라우저 intent로 소비하는 것과 동일 계약).
import android.app.Activity;
import android.content.Intent;
import android.view.Display;
import android.os.Bundle;
import android.speech.RecognitionListener;
import android.speech.RecognizerIntent;
import android.speech.SpeechRecognizer;
import android.widget.Button;
import android.widget.TextView;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;

public class ControlActivity extends Activity implements RecognitionListener {
    TextView status;
    SpeechRecognizer recognizer;
    Intent recognizerIntent;
    Thread post;
    int micRetries;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.control);
        status = findViewById(R.id.status);

        // [mic 권한 — 사용자 실측 2026-11-07 "recognizer error 9" 원존 —
        // logcat RecognitionService "caller without permission" 원문] —
        // 독립 앱이라 RECORD_AUDIO 런타임 요청이 뼈대에 없었다. 미권한 상태
        // 서비스 거부 = ERROR_RECOGNIZER_BUSY(9)로 보고된다(삼성 특정 계층).
        if (checkSelfPermission(android.Manifest.permission.RECORD_AUDIO)
            != android.content.pm.PackageManager.PERMISSION_GRANTED) {
            requestPermissions(
                new String[] { android.Manifest.permission.RECORD_AUDIO }, 1);
        }
        recognizer = SpeechRecognizer.createSpeechRecognizer(this);
        recognizerIntent = new Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH);
        recognizerIntent.putExtra(RecognizerIntent.EXTRA_LANGUAGE_MODEL,
            RecognizerIntent.LANGUAGE_MODEL_FREE_FORM);
        recognizerIntent.putExtra(RecognizerIntent.EXTRA_LANGUAGE, "ko-KR");
        recognizer.setRecognitionListener(this);

        findViewById(R.id.micBtn).setOnClickListener(v -> startMic());
    }

    void startMic() {
        // start 전 cancel(9번 BUSY 반복 방지 — 기존 세가 잡혀 있으면 빨리 푼다).
        try { recognizer.cancel(); } catch (Exception ignored) { }
        status.setText("listening...");
        recognizer.startListening(recognizerIntent);
        findViewById(R.id.desktopBtn).setOnClickListener(v -> {
            status.setText("starting desktop...");
            try {
                // [원존] jkdesktop = singleInstance 서버 — 기존 인스턴스의 태스크가
                // 폰 화면에 남는다. DeX(12)로 옮기려면 재개가 아니라 재시작:
                // force-stop → DeX 타깃 재시작 → SDL 창이 DeX 표면에 재성립
                // (fit-scale 원존: 서버 부팅 시 표면 크기에 맞춘다).
                Runtime.getRuntime().exec(new String[] { "am", "force-stop",
                    "chang.jkdesktop" });
                Thread.sleep(900);
            } catch (Exception e) {
                status.setText("stop failed: " + e.getMessage());
            }
            // [원존 실측 11-07] 앱이 보는 DeX 표면은 id=2(DeX 가상 Desktop —
            // 앱 DisplayManager에는 비가시, dumpsys로만 확인). getDisplays()가
            // 주는 1920x1080 'HDMI 화면'(id=12, 물리 RTK)은 DeX 합성 아래
            // 가려져 TV에 안 뜬다(실측: 창은 12에 프레젠트돼도 TV=런처). 그래서
            // 1순위 = am --display 2(shell 소스만 성공이었다가 앱 소스 수용
            // 여부를 실측), 실패 시 visible-HDMI(12) 폴백 → 최종 폴백=기본.
            String[] amArgs = { "am", "start", "-n",
                "chang.jkdesktop/.MainActivity", "--display", "2" };
            String amErr = "";
            try {
                Process p = Runtime.getRuntime().exec(amArgs);
                java.io.InputStream in = p.getInputStream();
                java.io.ByteArrayOutputStream bo =
                    new java.io.ByteArrayOutputStream();
                byte[] buf = new byte[4096];
                for (int n; (n = in.read(buf)) > 0; ) bo.write(buf, 0, n);
                in.close();
                p.waitFor();
                amErr = bo.toString("UTF-8");
                android.util.Log.i("jkctl", "am-out=" + amErr.trim());
            } catch (Exception e) {
                amErr = "am exec: " + e.getMessage();
            }
            if (amOutOk()) {
                // am --display 2 실측(11-07): 앱 소스는 "Starting"만 찍고 무시됨 —
                // 그래서 pidof로 실성립을 검증하고, 실패면 폴백으로 간다.
                status.setText("desktop launched (DeX display 2)");
                return;
            }
            try {
                android.hardware.display.DisplayManager dm = getSystemService(
                    android.hardware.display.DisplayManager.class);
                android.view.Display target = null;
                for (android.view.Display d : dm.getDisplays()) {
                    android.graphics.Point sz = new android.graphics.Point();
                    if (d != null) d.getRealSize(sz);
                    String nm = d != null ? d.getName() : null;
                    android.util.Log.i("jkctl", "enum id="
                        + (d != null ? d.getDisplayId() : -1)
                        + " name='" + nm + "' size=" + sz.x + "x" + sz.y);
                    String up = nm != null ? nm.toUpperCase() : "";
                    if (target == null && (up.contains("DESKTOP") || nm != null
                        && (nm.contains("Desktop") || nm.contains("HDMI"))
                        && d.getDisplayId() != Display.DEFAULT_DISPLAY && sz.x > 1600))
                        { target = d; }
                }
                android.util.Log.i("jkctl", "chosen="
                    + (target != null ? target.getDisplayId() : "default"));
                if (target != null) {
                    Intent desktop = new Intent();
                    desktop.setClassName("chang.jkdesktop",
                        "chang.jkdesktop.MainActivity");
                    desktop.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                    android.app.ActivityOptions opts =
                        android.app.ActivityOptions.makeBasic();
                    opts.setLaunchDisplayId(target.getDisplayId());
                    startActivity(desktop, opts.toBundle());
                    status.setText("desktop launched on visible display "
                        + target.getDisplayId());
                } else {
                    status.setText("DeX 화면은 폰 앱 권한으로 접근 불가 — "
                        + "TV의 DeX 홈에서 JK 아이콘을 탭하세요");
                }
            } catch (Exception e) {
                status.setText("jkdesktop not installed: " + e.getMessage());
            }
        });
        findViewById(R.id.backBtn).setOnClickListener(v ->
            // [수형 2026-11-08 — 결함 원존: {"kind":"BACK"} POST = 서버가
            // "text"만 읽어 빈 발화 거부(무동작) + 앱 단독 keyevent 주입은
            // 시스템 권한이라 성립 불가] — 브라우저/창 위의 화면을 바탕화면으로
            // 되돌리는 지시(라우터 AmpkClose 승천 — 브라우저 정실측 경로 재용).
            postTalk("{\"text\":\"바탕화면으로\"}"));
        findViewById(R.id.browserBtn).setOnClickListener(v ->
            postTalk("{\"text\":\"브라우저 열어줘\"}"));
    }

    boolean amOutOk() {
        try {
            Thread.sleep(2500);
            Process p = Runtime.getRuntime().exec(
                new String[] { "pidof", "chang.jkdesktop" });
            java.io.InputStream in = p.getInputStream();
            byte[] b = new byte[256];
            int n = in.read(b);
            in.close();
            p.waitFor();
            boolean ok = n > 0
                && new String(b, 0, n, "UTF-8").trim().length() > 0;
            android.util.Log.i("jkctl", "pidof-ok=" + ok);
            return ok;
        } catch (Exception e) {
            android.util.Log.i("jkctl", "pidof err " + e);
            return false;
        }
    }

    void postTalk(String body) {
        status.setText("sending...");
        if (post != null && post.isAlive()) return;
        post = new Thread(() -> {
            try {
                URL u = new URL("http://127.0.0.1:8091/talk");
                HttpURLConnection c = (HttpURLConnection) u.openConnection();
                c.setConnectTimeout(5000);
                c.setReadTimeout(90000);
                c.setDoOutput(true);
                c.setRequestMethod("POST");
                c.setRequestProperty("Content-Type", "application/json");
                byte[] b = body.getBytes(StandardCharsets.UTF_8);
                c.setFixedLengthStreamingMode(b.length);
                try (OutputStream o = c.getOutputStream()) { o.write(b); }
                int code = c.getResponseCode();
                java.io.InputStream in = c.getInputStream();
                java.io.ByteArrayOutputStream bo =
                    new java.io.ByteArrayOutputStream();
                byte[] buf = new byte[4096];
                for (int n; (n = in.read(buf)) > 0; ) bo.write(buf, 0, n);
                in.close();
                final String reply = bo.toString("UTF-8");
                runOnUiThread(() -> status.setText(
                    (code == 200 ? "ok: " : "err: ")
                    + reply.substring(0, Math.min(reply.length(), 160))));
            } catch (Exception e) {
                final String m = "post failed: " + e.getMessage();
                runOnUiThread(() -> status.setText(m));
            }
        });
        post.start();
    }

    @Override
    public void onResults(Bundle results) {
        micRetries = 0;  // 성공 — busy 재시도 카운터 리셋
        ArrayList<String> list =
            results.getStringArrayList(SpeechRecognizer.RESULTS_RECOGNITION);
        if (list != null && !list.isEmpty()) {
            String said = list.get(0);
            status.setText("heard: " + said);
            postTalk("{\"text\":\"" + said.replace("\"", "\\\"") + "\"}");
        }
    }

    @Override public void onReadyForSpeech(Bundle p) { status.setText("ready"); }
    @Override public void onBeginningOfSpeech() { status.setText("speaking..."); }
    @Override public void onRmsChanged(float rms) {}
    @Override public void onBufferReceived(byte[] b) {}
    @Override public void onEndOfSpeech() { status.setText("processing..."); }
    @Override public void onError(int err) {
        // 9 = ERROR_RECOGNIZER_BUSY — cancel+1회 재개(권한 거부도 9로 표출되는
        // 삼성 계층 실측 원존 — 재시도가 이뤄지려면 권한이 선행됨).
        if (err == 9 && micRetries < 3) {
            ++micRetries;
            try { recognizer.cancel(); } catch (Exception ignored) { }
            status.setText("busy - retrying...");
            new Thread(() -> {
                try { Thread.sleep(700); } catch (Exception ignored) { }
                runOnUiThread(() -> startMic());
            }).start();
            return;
        }
        status.setText("recognizer error " + err + " - tap to retry");
    }
    @Override public void onPartialResults(Bundle p) {}
    @Override public void onEvent(int t, Bundle p) {}
}
