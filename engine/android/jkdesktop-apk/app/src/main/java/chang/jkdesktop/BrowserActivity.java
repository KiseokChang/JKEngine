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

        // 조작행 — 뒤·앞·새로고침·[Go].
        LinearLayout navRow = new LinearLayout(this);
        navRow.setOrientation(LinearLayout.HORIZONTAL);
        Button backBtn = navButton("뒤", () -> webView.goBack());
        Button fwdBtn = navButton("앞", () -> webView.goForward());
        Button reloadBtn = navButton("새로고침", () -> webView.reload());
        Button goBtn = navButton("Go", () -> goNow());
        navRow.addView(backBtn); navRow.addView(fwdBtn);
        navRow.addView(reloadBtn); navRow.addView(goBtn);

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

        // 개봉 URL 봉입: intent extra "url"(jkweb AmpkOpen 회신 app 필드 계약)
        // 있으면 그것, 없으면 기본 페이지.
        String given = getIntent() != null
            ? getIntent().getStringExtra("url") : null;
        if (given == null) given = "";
        urlInput.setText(given.isEmpty() ? "https://example.com" : given);
        goNow();
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

    private Button navButton(String label, Runnable onClick) {
        Button b = new Button(this);
        b.setText(label);
        b.setTextSize(12f);
        b.setMinimumWidth(0);
        b.setPadding(row * 2, 0, row * 2, 0);
        b.setOnClickListener(v -> onClick.run());
        return b;
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
