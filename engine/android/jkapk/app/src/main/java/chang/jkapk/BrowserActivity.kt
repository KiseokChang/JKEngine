package chang.jkapk

// #123 폰 브라우저 T1 — WebView 뼈대.
// 스펙: docs/superpowers/specs/2026-11-01-phone-browser-webview-design.md §1·§2 T1.
// 표면 모델 원존: 폰은 두 겹(DeX 그리드=Termux 네이티브 jkdesktop, jkapk=얇은
// APK 클라이언트) — WebView는 Android View라 Termux 네이티브 창에 심을 수
// 없다 → 본 Activity가 jkapk 안 브라우저 창, DeX에서는 별도 Android 윈도우.
// 프로그래매틱 UI(MainActivity 계양 승계 — XML 레이아웃 0).
import android.annotation.SuppressLint
import android.app.Activity
import android.os.Bundle
import android.view.ViewGroup
import android.view.inputmethod.EditorInfo
import android.webkit.WebView
import android.webkit.WebViewClient
import android.widget.Button
import android.widget.EditText
import android.widget.LinearLayout

@SuppressLint("SetJavaScriptEnabled")
class BrowserActivity : Activity() {
    lateinit var urlInput: EditText
        private set
    lateinit var webView: WebView
        private set

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val pad = (8 * resources.displayMetrics.density).toInt()
        val row = (4 * resources.displayMetrics.density).toInt()

        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(pad, pad / 2, pad, pad / 2)
        }

        // 주소행 — 정직 URL 표기(개봉 후 onPageFinished가 실 최종 URL로 갱신).
        urlInput = EditText(this).apply {
            setSingleLine(true)
            imeOptions = EditorInfo.IME_ACTION_GO
            hint = "https://…"
            setText("https://example.com")
            setOnEditorActionListener { _, actionId, _ ->
                if (actionId == EditorInfo.IME_ACTION_GO) { goNow(); true } else false
            }
        }

        // 조작행 — 뒤·앞·새로고침·[Go].
        val navRow = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL }
        fun navButton(label: String, onClick: () -> Unit) = Button(this).apply {
            text = label
            textSize = 12f
            minimumWidth = 0
            setPadding(row * 2, 0, row * 2, 0)
            setOnClickListener { onClick() }
        }
        val backBtn = navButton("뒤") { webView.goBack() }
        val fwdBtn = navButton("앞") { webView.goForward() }
        val reloadBtn = navButton("새로고침") { webView.reload() }
        val goBtn = navButton("Go") { goNow() }
        listOf(backBtn, fwdBtn, reloadBtn, goBtn).forEach { navRow.addView(it) }

        // 본체 — 화면 남은 영역 전부(죽은 마진 0 — fit 원칙 승계).
        webView = WebView(this).apply {
            layoutParams = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f)
            settings.javaScriptEnabled = true   // 에이전트 표면(T3) 전제
            settings.domStorageEnabled = true
            webViewClient = object : WebViewClient() {
                override fun onPageFinished(view: WebView, url: String) {
                    urlInput.setText(url)   // 리다이렉트 최종 URL = 진실원
                }
            }
        }

        root.addView(urlInput)
        root.addView(navRow)
        root.addView(webView)
        setContentView(root)

        goNow()   // 부팅 기본 페이지
    }

    fun goNow() {
        var raw = urlInput.text.toString().trim()
        if (!raw.startsWith("http://") && !raw.startsWith("https://")) {
            raw = "https://$raw"
        }
        webView.loadUrl(raw)
    }

    // 뒤 키 = 방향 뒤(웹역사 존중 — 뒤 결함 계통 조심).
    override fun onBackPressed() {
        if (webView.canGoBack()) webView.goBack() else super.onBackPressed()
    }
}