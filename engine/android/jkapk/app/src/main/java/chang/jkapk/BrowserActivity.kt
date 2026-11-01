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
import org.mozilla.geckoview.GeckoSession

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
        val backBtn = navButton("뒤") { navBack() }
        val fwdBtn = navButton("앞") { navFwd() }
        val reloadBtn = navButton("새로고침") { navReload() }
        val goBtn = navButton("Go") { goNow() }
        val measureBtn = navButton("[측정]") { runMeasure() }
        val engineBtn = navButton(if (engineName() == "webview") "[GeckoView]" else "[WebView]") {
            toggleEngine()
        }
        listOf(backBtn, fwdBtn, reloadBtn, goBtn, measureBtn, engineBtn)
            .forEach { navRow.addView(it) }

        // 본체 — 화면 남은 영역 전부(죽은 마진 0 — fit 원칙 승계).
        // T5 엔진 토글: webview | geckoview — 같은 레이아웃 슬롯 교체, 측정 공용.
        if (engineName() == "geckoview") {
            attachGeckoView(root)
        } else {
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
        }

        root.addView(urlInput)
        root.addView(navRow)
        if (engineName() == "webview") root.addView(webView)  // gecko는 attach가 root에 추가
        setContentView(root)

        if (engineName() == "webview") {
            webkitOptIn()
            goNow()   // 부팅 기본 페이지
        }   // gecko 부팅 로드는 attachGeckoView가 수행
    }

    fun goNow() {
        var raw = urlInput.text.toString().trim()
        if (!raw.startsWith("http://") && !raw.startsWith("https://")) {
            raw = "https://$raw"
        }
        if (engineName() == "geckoview") geckoSession?.loadUri(raw)
        else webView.loadUrl(raw)
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
        if (engineName() == "geckoview") {
            // Gecko는 /android_asset 별칭 부재 — 앱 내부 파일 복사본으로 대응 개봉
            // (차단 시 onPageStop ok=false 정직 기록 — 판정 자료).
            val url = head.second.replace("file:///android_asset/", "file://$filesDir/")
            geckoSession?.loadUri(url)
        } else webView.loadUrl(head.second)
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

    // ── 엔진 토글+탐색 dispatch(#123 T5, 스펙 §3 판정 실측 축) ──────────────
    // 같은 APK에 두 엔진 상주 — 토글은 SharedPreferences+jrecreate()로 창 재구성.
    // GeckoView 판단 채점은 ProgressDelegate(loadMs) — firstContentMs는 등가
    // 계약 부재로 -1 정직 결측(스펙 §3 축 2 원존).
    private var geckoSession: org.mozilla.geckoview.GeckoSession? = null
    private var geckoRuntime: org.mozilla.geckoview.GeckoRuntime? = null

    private fun engineName(): String =
        getSharedPreferences("jkapk_browser", MODE_PRIVATE)
            .getString("engine", "webview") ?: "webview"

    private fun toggleEngine() {
        val next = if (engineName() == "webview") "geckoview" else "webview"
        getSharedPreferences("jkapk_browser", MODE_PRIVATE).edit()
            .putString("engine", next).apply()
        recreate()   // Activity 재창립 — onCreate가 engineName()을 읽어 본체 재구성
    }

    private fun navBack() {
        if (engineName() == "geckoview") geckoSession?.goBack()
        else webView.goBack()
    }

    private fun navFwd() {
        if (engineName() == "geckoview") geckoSession?.goForward()
        else webView.goForward()
    }

    private fun navReload() {
        if (engineName() == "geckoview") geckoSession?.reload()
        else webView.reload()
    }

    private fun attachGeckoView(root: LinearLayout) {
        val runtime = geckoRuntime ?: org.mozilla.geckoview.GeckoRuntime.create(this)
        geckoRuntime = runtime
        val session = org.mozilla.geckoview.GeckoSession()
        geckoSession = session
        session.open(runtime)
        // 측정 세트의 file:///android_asset 계약을 gecko에도 개봉 시도 — 차단 시
        // ok=false 정직 기록(판정 자료 — 스펙 §3 축 2 결측 정직 원존).
        session.progressDelegate = object : org.mozilla.geckoview.GeckoSession.ProgressDelegate {
            override fun onPageStart(session: GeckoSession, url: String) {
                measurePageStart = System.currentTimeMillis()
            }
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                val loadMs = System.currentTimeMillis() - measurePageStart
                if (measuring) {
                    val head = measureQueue.removeFirstOrNull()
                    measurePages.put(org.json.JSONObject().apply {
                        put("name", head?.first ?: "?")
                        put("url", head?.second ?: "?")
                        put("ok", success)
                        put("loadMs", loadMs)
                        put("firstContentMs", -1)   // 등가 계약 부재 — 정직 결측
                    })
                    Log.i("JK-BROWSER", "MEASURE-G ${head?.second} load=${loadMs}ms ok=$success")
                    startNextMeasurePage()
                }
            }
        }
        // asset 세트 → 앱 내부 파일 복사(Gecko file:// 대응 — 대응 경로 등어 계약)
        runCatching {
            for (p in listOf("p1.html", "p2.html", "p3.html")) {
                val dst = java.io.File(filesDir, "testpages/$p")
                dst.parentFile?.mkdirs()
                if (!dst.exists()) {
                    assets.open("testpages/$p").use { inp ->
                        dst.outputStream().use { inp.copyTo(it) }
                    }
                }
            }
            Log.i("JK-BROWSER", "GECKO-ASSETS-COPIED filesDir/testpages")
        }.onFailure {
            Log.e("JK-BROWSER", "GECKO-ASSETS-COPY-FAIL ${it.message}")
        }
        val gv = org.mozilla.geckoview.GeckoView(this).apply {
            layoutParams = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f)
            setSession(session)
        }
        root.addView(gv)
        goNow()   // 부팅 기본 페이지(WebView판 goNow 동형 — loadUri 분기 내장)
    }

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