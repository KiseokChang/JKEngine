// #124 폰 브라우저 T1 — WebView 뼈대 (Java 수형 — 통일 APK에 Kotlin 플러그인
// 부재 원존(플랜 원문 = jkapk Kotlin 환경 승계 — 통일 APK = AGP 8.6.0만), 뼈대
// 동작 계약 동일). 개봉 2길: ①외부 am start(+DeX) ②/talk AmpkOpen 회신을
// 소비하는 JS intent-URI leg(embedded HTML — 같은 배치).
// [T1.6 영상 풀스크린 수형 — 2026-11-07, 사용자 EYES 원존 "유튜브 영상 최대화
// 안됨"]: WebView의 HTML5 풀스크린은 WebChromeClient onShowCustomView 없이
// 조용히 무시된다(표 레시피) — 수형 = FrameLayout 루트 전환 개봉/복귀.
package chang.jkdesktop;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.EditorInfo;
import android.webkit.WebChromeClient;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.Button;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

public class BrowserActivity extends Activity {
    int pad, row;
    EditText urlInput;
    WebView webView;
    FrameLayout root;
    View fullscreenView;
    android.webkit.WebChromeClient.CustomViewCallback fullscreenCallback;

    @SuppressLint("SetJavaScriptEnabled")
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        pad = (int) (8 * getResources().getDisplayMetrics().density);
        row = (int) (4 * getResources().getDisplayMetrics().density);

        root = new FrameLayout(this);
        LinearLayout inner = new LinearLayout(this);
        inner.setOrientation(LinearLayout.VERTICAL);
        inner.setPadding(pad, pad / 2, pad, pad / 2);

        // 주소행 — 정직 URL 표기(최종 URL = onPageFinished 진실원).
        urlInput = new EditText(this);
        urlInput.setSingleLine(true);
        urlInput.setImeOptions(EditorInfo.IME_ACTION_GO);
        urlInput.setHint("https://…");
        urlInput.setOnEditorActionListener((v, actionId, e) -> {
            if (actionId == EditorInfo.IME_ACTION_GO) { goNow(); return true; }
            return false;
        });

        // 조작행 — 뒤·앞·새로고침·[Go]·[측정].
        LinearLayout navRow = new LinearLayout(this);
        navRow.setOrientation(LinearLayout.HORIZONTAL);
        Button backBtn = navButton("뒤", () -> webView.goBack());
        Button fwdBtn = navButton("앞", () -> webView.goForward());
        Button reloadBtn = navButton("새로고침", () -> webView.reload());
        Button goBtn = navButton("Go", () -> goNow());
        Button measureBtn = navButton("측정", () -> runMeasure(null));
        navRow.addView(backBtn); navRow.addView(fwdBtn);
        navRow.addView(reloadBtn); navRow.addView(goBtn);
        navRow.addView(measureBtn);

        // 본체 — 화면 남은 영역 전부(죽은 마진 0 — fit 원칙 승계).
        webView = new WebView(this);
        webView.setLayoutParams(new LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f));
        webView.getSettings().setJavaScriptEnabled(true);
        webView.getSettings().setDomStorageEnabled(true);
        webView.getSettings().setMediaPlaybackRequiresUserGesture(false);
        webView.setWebViewClient(new WebViewClient() {
            @Override
            public void onPageFinished(WebView view, String url) {
                urlInput.setText(url);   // 리다이렉트 최종 URL = 진실원
            }
        });
        // 영상 풀스크린 콜백 — 유튜브 "전체화면" 버튼이 이 길로 온다.
        webView.setWebChromeClient(new WebChromeClient() {
            @Override
            public void onShowCustomView(View view,
                                         CustomViewCallback callback) {
                if (fullscreenView != null) {
                    callback.onCustomViewHidden();
                    return;
                }
                fullscreenView = view;
                fullscreenCallback = callback;
                webView.setVisibility(View.GONE);
                inner.setVisibility(View.GONE);
                root.addView(view, new FrameLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT,
                    ViewGroup.LayoutParams.MATCH_PARENT));
            }

            @Override
            public void onHideCustomView() {
                if (fullscreenView == null) return;
                root.removeView(fullscreenView);
                fullscreenView = null;
                inner.setVisibility(View.VISIBLE);
                webView.setVisibility(View.VISIBLE);
                if (fullscreenCallback != null) {
                    fullscreenCallback.onCustomViewHidden();
                    fullscreenCallback = null;
                }
            }
        });

        inner.addView(urlInput);
        inner.addView(navRow);
        inner.addView(webView);
        root.addView(inner, new FrameLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.MATCH_PARENT));
        setContentView(root);
        webkitOptIn();

        // 닫기 브로드캐스트 수신 — 수신 시 자기 finish(AmpkClose 계약).
        closeRx = new android.content.BroadcastReceiver() {
            @Override
            public void onReceive(android.content.Context c, android.content.Intent i) {
                runOnUiThread(() -> finish());
            }
        };
        // RECEIVER_NOT_EXPORTED — Android 14 요구 실측 원존(SecurityException:
        // RECEIVER_EXPORTED|RECEIVER_NOT_EXPORTED 지정 요구 — 폰 크래시 버퍼).
        registerReceiver(closeRx,
            new android.content.IntentFilter(ACTION_CLOSE),
            android.content.Context.RECEIVER_NOT_EXPORTED);

        // 개봉 URL 봉입: intent extra "url"(jkweb AmpkOpen 회신 app 필드 계약)
        // 있으면 그것, 없으면 기본 페이지.
        String given = getIntent() != null
            ? getIntent().getStringExtra("url") : null;
        if (given == null) given = "";
        urlInput.setText(given.isEmpty() ? "https://example.com" : given);
        goNow();
    }

    // ── 브라우저 T3 — androidx.webkit opt-in 봉입(플랜 Task 3 계약)───────
    // 원격 디버깅(CDP — 에이전트 계약 후보·판정 축)+설치 webkit 패키지 표기+
    // WebAuthn FOR_APP 지원 개봉. PaymentRequest는 라인 OFF(결제 홉아웃
    // 원장행 — 스펙 §5 대기 유지).
    void webkitOptIn() {
        android.webkit.WebView.setWebContentsDebuggingEnabled(true);
        try {
            // AAR 원존 실측: getCurrentWebViewPackage 반환형 = (Context)Landroid/
            // content/pm/PackageInfo — android.jar 표시 API(android.jar에
            // WebViewPackageInfo 부재 실측 — jar tf 원문).
            android.content.pm.PackageInfo s =
                androidx.webkit.WebViewCompat.getCurrentWebViewPackage(this);
            android.util.Log.i("JK-BROWSER", "WEBKIT-PKG "
                + (s != null ? s.versionName : "native"));
        } catch (Throwable t) {
            android.util.Log.i("JK-BROWSER", "WEBKIT-PKG unknown");
        }
        try {
            boolean ok = androidx.webkit.WebViewFeature.isFeatureSupported(
                androidx.webkit.WebViewFeature.WEB_AUTHENTICATION);
            android.util.Log.i("JK-BROWSER",
                "WEB-AUTHN supported=" + ok
                + (ok ? " -> FOR_APP on" : " on this webview pkg"));
            if (ok) {
                androidx.webkit.WebSettingsCompat.setWebAuthenticationSupport(
                    webView.getSettings(),
                    androidx.webkit.WebSettingsCompat
                        .WEB_AUTHENTICATION_SUPPORT_FOR_APP);
            }
        } catch (Throwable t) {
            android.util.Log.i("JK-BROWSER", "WEB-AUTHN probe failed");
        }
    }

    void goNow() {
        String raw = urlInput.getText().toString().trim();
        if (!raw.startsWith("http://") && !raw.startsWith("https://")
            && !raw.startsWith("file://")) {
            raw = "https://" + raw;
        }
        webView.loadUrl(raw);
    }

    @Override
    public void onBackPressed() {   // 뒤 키 = 풀스크린 복귀 우선(기존 웹역사)
        if (fullscreenView != null) {
            webView.getWebChromeClient().onHideCustomView();
            return;
        }
        if (webView.canGoBack()) webView.goBack(); else super.onBackPressed();
    }

    // jkweb /talk AmpkOpen 회신 소비용 custom action(JS intent-URI leg 계약).
    public static final String ACTION_OPEN = "chang.jkdesktop.OPEN_BROWSER";
    public static final String EXTRA_URL = "url";

    // ── 측정 하니스(브라우저 T2) — 같은 페이지 세트·JSON 스키마를 GeckoView판
    // (T5)에도 공용(스키마 계약 원존 — 플랜 Task 2). 산출 = MediaStore
    // Downloads dump(exportSrvLog 패턴 승계). 폰이 offline일 때도 asset
    // 페이지(로컬 file://)는 전부 측정 가능 — 네트워크 대기 없음.
    boolean measuring = false;
    org.json.JSONArray measurePages = new org.json.JSONArray();
    Runnable measureDoneExtra;

    void runMeasure(Runnable onDoneExtra) {
        if (measuring) return;
        measuring = true;
        measurePages = new org.json.JSONArray();
        measureDoneExtra = onDoneExtra;
        java.util.ArrayDeque<String[]> queue = new java.util.ArrayDeque<>();
        queue.add(new String[] { "asset_p1",
            "file:///android_asset/testpages/p1.html" });
        queue.add(new String[] { "asset_p2",
            "file:///android_asset/testpages/p2.html" });
        queue.add(new String[] { "asset_p3",
            "file:///android_asset/testpages/p3.html" });
        runMeasureNext(queue);
    }

    void runMeasureNext(final java.util.ArrayDeque<String[]> queue) {
        if (queue.isEmpty()) {
            measuring = false;
            // 원복 — url 갱신 계약(측정 전 원 클라 계약 승계).
            webView.setWebViewClient(new WebViewClient() {
                @Override
                public void onPageFinished(WebView view, String url) {
                    urlInput.setText(url);
                }
            });
            String json = finishMeasure();
            android.util.Log.i("JK-BROWSER", "MEASURE-DONE " + json);
            java.util.Map<String, String> saved = dumpMeasureJson(json);
            android.widget.Toast.makeText(this,
                saved.get("ok") != null && saved.get("ok").equals("1")
                    ? "측정 완료 → " + saved.get("path")
                    : "측정 dump 실패",
                android.widget.Toast.LENGTH_LONG).show();
            if (measureDoneExtra != null) measureDoneExtra.run();
            return;
        }
        final String[] head = queue.pollFirst();
        final long pageStart = android.os.SystemClock.uptimeMillis();
        webView.setWebViewClient(new WebViewClient() {
            @Override
            public void onPageFinished(WebView view, String url) {
                urlInput.setText(url);
                final long loadMs = android.os.SystemClock.uptimeMillis() - pageStart;
                view.evaluateJavascript(
                    "(function(){var n=performance.getEntriesByType('navigation')[0];"
                    + "return JSON.stringify({fc:n?(n.responseStart||0):0});})()",
                    new android.webkit.ValueCallback<String>() {
                        @Override public void onReceiveValue(String r) {
                            long fc = -1;
                            try { fc = Long.parseLong(r.replace("\"", "")
                                .replace("}", "").split(":")[1]); }
                            catch (Exception ignored) { }
                            org.json.JSONObject row = new org.json.JSONObject();
                            try {
                                row.put("name", head[0]);
                                row.put("url", head[1]);
                                row.put("ok", true);
                                row.put("loadMs", loadMs);
                                row.put("firstContentMs", fc);
                            } catch (Exception ignored) { }
                            measurePages.put(row);
                            android.util.Log.i("JK-BROWSER", "MEASURE "
                                + head[0] + " load=" + loadMs + "ms fc=" + fc);
                            runMeasureNext(queue);
                        }
                    });
            }
        });
        webView.loadUrl(head[1]);
    }

    String finishMeasure() {
        String created = new java.text.SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss")
            .format(new java.util.Date());
        String pages = measurePages.toString();
        if (pages.equals("null")) pages = "[]";
        return "{\"engine\":\"webview\",\"created\":\"" + created
            + "\",\"pages\":" + pages + "}";
    }

    // MediaStore Downloads dump — exportSrvLog 복사 패턴(원존 승계).
    java.util.Map<String, String> dumpMeasureJson(String json) {
        java.util.Map<String, String> out = new java.util.HashMap<>();
        out.put("ok", "0");
        out.put("path", "");
        try {
            String name = "jk_measure_" + System.currentTimeMillis() + ".json";
            android.net.Uri uri = null;
            if (android.os.Build.VERSION.SDK_INT >= 29) {
                android.content.ContentValues v = new android.content.ContentValues();
                v.put(android.provider.MediaStore.Downloads.DISPLAY_NAME, name);
                v.put(android.provider.MediaStore.Downloads.IS_PENDING, 1);
                uri = getContentResolver().insert(
                    android.provider.MediaStore.Downloads.EXTERNAL_CONTENT_URI, v);
                if (uri == null) return out;
                try (java.io.OutputStream os = getContentResolver()
                        .openOutputStream(uri)) {
                    os.write(json.getBytes(java.nio.charset.StandardCharsets.UTF_8));
                }
                v.clear();
                v.put(android.provider.MediaStore.Downloads.IS_PENDING, 0);
                getContentResolver().update(uri, v, null, null);
            } else {
                java.io.File dir = android.os.Environment
                    .getExternalStoragePublicDirectory(
                        android.os.Environment.DIRECTORY_DOWNLOADS);
                java.io.File f = new java.io.File(dir, name);
                try (java.io.OutputStream os = new java.io.FileOutputStream(f)) {
                    os.write(json.getBytes(java.nio.charset.StandardCharsets.UTF_8));
                }
            }
            out.put("ok", "1");
            out.put("path", uri != null ? uri.toString() : name);
        } catch (Exception e) {
            android.util.Log.e("JK-BROWSER", "dumpMeasureJson failed", e);
        }
        return out;
    }

    private Button navButton(String label, Runnable onClick) {
        Button b = new Button(this);
        b.setText(label);
        b.setTextSize(12f);
        b.setMinimumWidth(0);
        b.setPadding(row * 2, 0, row * 2, 0);
        b.setOnClickListener(v -> onClick.run());
        return b;
    }

    // 폰 브라우저 닫기(브라우저 플랜 review 갱신 2 — 사용자 mic 원존): 브로드캐스트
    // 수신 leg — 다중 인스턴스도 각자 수신해 자기 finish(액티비티 static 참조
    // 보유 금지 원칙). jkvoice_bridge의 JkCloseBrowserActivity JNI 훅이 발사.
    public static final String ACTION_CLOSE = "chang.jkdesktop.CLOSE_BROWSER";
    android.content.BroadcastReceiver closeRx;

    @Override
    protected void onDestroy() {
        if (closeRx != null) {
            unregisterReceiver(closeRx);
            closeRx = null;
        }
        super.onDestroy();
    }

    public static void closeAllBroadcast(android.content.Context ctx) {
        ctx.sendBroadcast(new android.content.Intent(ACTION_CLOSE)
            .setPackage(ctx.getPackageName()));
    }

    // jkweb AmpkOpen 소비 leg(jkvoice_bridge의 JNI 훅이 부른다) — 같은 앱
    // 프로세스의 activity open이라 background 규제 밖(원존 — docs/102).
    public static void openWithUrl(android.content.Context ctx, String url) {
        android.content.Intent i = new android.content.Intent(ctx, BrowserActivity.class);
        i.putExtra("url", url == null ? "" : url);
        i.addFlags(android.content.Intent.FLAG_ACTIVITY_NEW_TASK);
        ctx.startActivity(i);
    }
}
