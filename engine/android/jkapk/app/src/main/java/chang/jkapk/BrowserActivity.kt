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
import android.util.Log
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
        val measureBtn = navButton("[측정]") { runMeasure() }
        listOf(backBtn, fwdBtn, reloadBtn, goBtn, measureBtn).forEach { navRow.addView(it) }

        // 본체 — 화면 남은 영역 전부(죽은 마진 0 — fit 원칙 승계).
        webView = WebView(this).apply {
            layoutParams = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f)
            settings.javaScriptEnabled = true   // 에이전트 표면(T3) 전제
            settings.domStorageEnabled = true
            webViewClient = object : WebViewClient() {
                override fun onPageFinished(view: WebView, url: String) {
                    urlInput.setText(url)   // 리다이렉트 최종 URL = 진실원
                    if (measuring) onMeasurePageFinished(view, url)
                }
            }
        }

        root.addView(urlInput)
        root.addView(navRow)
        root.addView(webView)
        setContentView(root)

        webkitOptIn()
        goNow()   // 부팅 기본 페이지
    }

    fun goNow() {
        var raw = urlInput.text.toString().trim()
        if (!raw.startsWith("http://") && !raw.startsWith("https://")) {
            raw = "https://$raw"
        }
        webView.loadUrl(raw)
    }

    // ── 측정 하니스(#123 T2, 스펙 §3 축 1·2) — 같은 페이지 세트·같은 JSON 스키마를
    //    엔진 쌍에 공용(T5 GeckoView판도 이 스키마 산출). asset 오프라인 3종 +
    //    example.com. dump는 MediaStore Downloads(D4 무추적 산출물과 동일 축). ──
    private var measuring = false
    private var measureQueue = ArrayDeque<Pair<String, String>>()
    private var measurePageStart = 0L
    private val measurePages = org.json.JSONArray()

    private fun runMeasure() {
        if (measuring) return
        measuring = true
        measurePages.let { while (it.length() > 0) it.remove(0) }
        measureQueue = ArrayDeque(listOf(
            "p1" to "file:///android_asset/testpages/p1.html",
            "p2" to "file:///android_asset/testpages/p2.html",
            "p3" to "file:///android_asset/testpages/p3.html",
            "example" to "https://example.com"))
        startNextMeasurePage()
    }

    private fun startNextMeasurePage() {
        val head = measureQueue.firstOrNull()
        if (head == null) {
            measuring = false
            dumpMeasure(finishMeasureJson())
            return
        }
        measurePageStart = System.currentTimeMillis()
        webView.loadUrl(head.second)
    }

    private fun onMeasurePageFinished(view: WebView, url: String) {
        val loadMs = System.currentTimeMillis() - measurePageStart
        view.evaluateJavascript(
            "(function(){var n=performance.getEntriesByType('navigation')[0];" +
            "return (n&&n.responseStart)?String(Math.round(n.responseStart)):'-1';})()") { r ->
            val fc = r.trim().removeSurrounding("\"").toLongOrNull() ?: -1L
            val head = measureQueue.removeFirstOrNull()
            measurePages.put(org.json.JSONObject().apply {
                put("name", head?.first ?: "?")
                put("url", url)
                put("ok", true)
                put("loadMs", loadMs)
                put("firstContentMs", fc)
            })
            Log.i("JK-BROWSER", "MEASURE $url load=${loadMs}ms firstContent=${fc}ms")
            startNextMeasurePage()
        }
    }

    private fun finishMeasureJson(): String {
        return org.json.JSONObject().apply {
            put("engine", engineName())
            put("created", java.text.SimpleDateFormat(
                "yyyy-MM-dd'T'HH:mm:ss", java.util.Locale.US).format(java.util.Date()))
            put("pages", measurePages)
        }.toString(2)
    }

    private fun engineName(): String = "webview"   // T5에서 토글 값으로 교체

    private fun dumpMeasure(json: String) {
        try {
            val values = android.content.ContentValues().apply {
                put(android.provider.MediaStore.Downloads.DISPLAY_NAME,
                    "jkbrowser_measure_${engineName()}_${System.currentTimeMillis()}.json")
                put(android.provider.MediaStore.Downloads.MIME_TYPE, "application/json")
            }
            val uri = contentResolver.insert(
                android.provider.MediaStore.Downloads.EXTERNAL_CONTENT_URI, values)
            if (uri == null) {
                Log.e("JK-BROWSER", "MEASURE-DUMP-FAIL insert=null")
                return
            }
            contentResolver.openOutputStream(uri)?.use { out ->
                out.write(json.toByteArray(Charsets.UTF_8))
            }
            Log.i("JK-BROWSER", "MEASURE-DUMP-OK $uri")
        } catch (e: Exception) {
            Log.e("JK-BROWSER", "MEASURE-DUMP-FAIL ${e.message}")
        }
    }

    // ── webkit 봉입(#123 T3, 스펙 §3 축 4·5) ──────────────────────────────
    // ① 원격 디버깅(DevTools CDP 소켓) — 에이전트 브라우저 계약 후보(폰은 adb 중계.
    //    크로스프로세스 에이전트 연결은 /talk 라인 승계 — 현재는 개방만).
    // ② 패스키 WebAuthn FOR_APP opt-in — 인증 실측 축. 지원 부재 = 정직 표기
    //    (조용한 누락 금지 — fail-closed 문화). FOR_BROWSER는 특권 앱 등급 필요.
    // ③ Payment Request는 이 라인 OFF — 결제 홉아웃 원장행(스펙 §5) 대기.
    private fun webkitOptIn() {
        WebView.setWebContentsDebuggingEnabled(true)
        val pkg = androidx.webkit.WebViewCompat.getCurrentWebViewPackage(this)
        Log.i("JK-BROWSER", "WEBKIT-PKG ${pkg?.versionName ?: "native-null"}")
        try {
            if (androidx.webkit.WebViewFeature.isFeatureSupported(
                    androidx.webkit.WebViewFeature.WEB_AUTHENTICATION)) {
                androidx.webkit.WebSettingsCompat.setWebAuthenticationSupport(
                    webView.settings,
                    androidx.webkit.WebSettingsCompat.WEB_AUTHENTICATION_SUPPORT_FOR_APP)
                Log.i("JK-BROWSER", "WEB-AUTHN FOR_APP on")
            } else {
                Log.i("JK-BROWSER", "WEB-AUTHN unsupported on this webview pkg")
            }
        } catch (e: Exception) {
            Log.e("JK-BROWSER", "WEB-AUTHN set fail ${e.message}")
        }
    }

    override fun onBackPressed() {
        if (webView.canGoBack()) webView.goBack() else super.onBackPressed()
    }
}