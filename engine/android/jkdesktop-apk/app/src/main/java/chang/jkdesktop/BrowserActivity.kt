// #124 폰 브라우저 T1 — WebView 뼈대 (플랜 2026-11-01-phone-browser-webview
// Task 1 — 패키지 승계: 본 APK 통일(chang.jkdesktop — 플랜 원문 = chang.jkapk).
// 개봉 2길: ①외부 am start(adb·DeX 그리드) ②jkweb /talk AmpkOpen 회신을 소비하는
// JS intent-URI leg(embedded HTML — Task 1.5 같은 배치). 프로그매틱 UI(MainActivity
// 계양 — XML 0). 정직 URL 표기(최종 URL = onPageFinished 진실원).
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
            settings.javaScriptEnabled = true
            settings.domStorageEnabled = true
            @SuppressLint("SetJavaScriptEnabled")
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

        // 개봉 URL 봉입: intent extra "url"(jkweb AmpkOpen 회신 계약 — app 필드)
        // 있으면 그것, 없으면 기본 페이지.
        val given = intent.getStringExtra("url") ?: ""
        if (given.isNotEmpty()) {
            urlInput.setText(given)
            goNow()
        } else {
            urlInput.setText("https://example.com")
            goNow()
        }
    }

    fun goNow() {
        var raw = urlInput.text.toString().trim()
        if (!raw.startsWith("http://") && !raw.startsWith("https://") &&
            !raw.startsWith("file://")) {
            raw = "https://$raw"
        }
        webView.loadUrl(raw)
    }

    override fun onBackPressed() {       // 뒤 키 = 방향 뒤(웹역사 존중)
        if (webView.canGoBack()) webView.goBack() else super.onBackPressed()
    }

    companion object {
        // jkweb /talk AmpkOpen 회신 소비용 custom action(Task 1.5 — JS
        // intent-URI leg가 이 액션으로 개봉한다).
        const val ACTION_OPEN = "chang.jkdesktop.OPEN_BROWSER"
        const val EXTRA_URL = "url"
    }
}
