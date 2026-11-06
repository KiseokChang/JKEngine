// #124 폰 브라우저 T1 — WebView 뼈대 (Java 수형 — 통일 APK에 Kotlin 플러그인
// 부재 원존(플랜 원문 = jkapk Kotlin 환경 승계 — 통일 APK = AGP 8.6.0만), 뼈대
// 동작 계약 동일). 개봉 2길: ①외부 am start(+DeX) ②/talk AmpkOpen 회신을
// 소비하는 JS intent-URI leg(embedded HTML — 같은 배치).
package chang.jkdesktop;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.os.Bundle;
import android.view.ViewGroup;
import android.view.inputmethod.EditorInfo;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;

public class BrowserActivity extends Activity {
    int pad, row;  // onCreate Scale - navButton consumes (local->field promotion fix)
    EditText urlInput;
    WebView webView;

    @SuppressLint("SetJavaScriptEnabled")
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        int pad = (int) (8 * getResources().getDisplayMetrics().density);
        int row = (int) (4 * getResources().getDisplayMetrics().density);

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(pad, pad / 2, pad, pad / 2);

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
        webView.setWebViewClient(new WebViewClient() {
            @Override
            public void onPageFinished(WebView view, String url) {
                urlInput.setText(url);   // 리다이렉트 최종 URL = 진실원
            }
        });

        root.addView(urlInput);
        root.addView(navRow);
        root.addView(webView);
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
    public void onBackPressed() {   // 뒤 키 = 웹역사 존중(뒤 결함 계통 조심)
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
}
