package chang.jkapk

// #123 폰 브라우저 T1 — WebView 뼈대.
// 스펙: docs/superpowers/specs/2026-11-01-phone-browser-webview-design.md §1·§2 T1.
// 표면 모델 원존: 폰은 두 겹(DeX 그리드=Termux 네이티브 jkdesktop, jkapk=얇은
// APK 클라이언트) — WebView는 Android View라 Termux 네이티브 창에 심을 수
// 없다 → 본 Activity가 jkapk 안 브라우저 창, DeX에서는 별도 Android 윈도우.
// 프로그래매틱 UI(MainActivity 계양 승계 — XML 레이아웃 0).
import android.annotation.SuppressLint
import android.app.Activity
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.Bundle
import android.util.Log
import android.view.MotionEvent
import android.view.ViewGroup
import android.view.inputmethod.EditorInfo
import android.webkit.WebView
import android.webkit.WebViewClient
import android.widget.Button
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.Toast
import androidx.core.content.ContextCompat
import org.mozilla.geckoview.GeckoSession

@SuppressLint("SetJavaScriptEnabled")
class BrowserActivity : Activity() {
    lateinit var urlInput: EditText
        private set
    lateinit var webView: WebView
        private set

    // #124 — 구두 서비스 회신 표기(브라우저 창은 채팅 진실원이 없어 토스트).
    private val voiceReceiver = object : BroadcastReceiver() {
        override fun onReceive(ctx: Context?, i: Intent?) {
            fun toast(d: String?) = android.widget.Toast.makeText(
                this@BrowserActivity, d.orEmpty(), android.widget.Toast.LENGTH_LONG).show()
            when (i?.action) {
                JkVoiceService.BC_STATUS ->
                    if (i.getStringExtra(JkVoiceService.EXTRA_STATE) == "error")
                        toast(i.getStringExtra(JkVoiceService.EXTRA_DETAIL))
                JkVoiceService.BC_REPLY -> {
                    // #124 r3 — 개봉지시 스스로 적용: 이 창이 화면 앞(사용자가
                    // [구두] 홀드 중)이므로 규제 밖 직통 — 서비스 startActivity
                    // leg(BAL 엣지 원존 r2) 대체. r5 계측판(2026-11-01 — 토스트
                    // 3종으로 정체 지점 화면 노출 — EYES 직독):
                    //   [적용] 토스트 = 브랜치 도달 → goNow 호출 뒤 엔진 표기
                    //   [미도달] 토스트(구회신판대로) = EXTRA_KIND 비거주 — 서비스
                    //   파서 leg 원진 — 다음 수형 축
                    if (i.getStringExtra(JkVoiceService.EXTRA_KIND) == "AmpkOpen") {
                        // 이미 이 창(브라우저) 자체가 떠 있으므로 "개봉만" 지시는
                        // 적용 상태 그대로(가짜 이동 금지 — 무대응 정직).
                        val u = i.getStringExtra(JkVoiceService.EXTRA_URL).orEmpty()
                        Log.i("JK-BROWSER", "VOICE-RECV kind=AmpkOpen url=$u")   // #124 r5 — 사슬 계측
                        Toast.makeText(this@BrowserActivity,
                            "①지시 수신: URL=$u (엔진=${engineName()})",
                            Toast.LENGTH_LONG).show()
                        if (u.isNotEmpty()) {
                            urlInput.setText(u)
                            goNow()
                        }
                        return
                    }
                    Log.i("JK-BROWSER",
                        "VOICE-RECV kind=${i.getStringExtra(JkVoiceService.EXTRA_KIND)} url=미지시")
                    toast(i.getStringExtra(JkVoiceService.EXTRA_DETAIL))
                }
            }
        }
    }

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

        // 조작행 — 뒤·앞·새로고침·[Go]. #124 r3 UX 원존(2026-11-01 사용자
        // "폰화면 좁아서 버튼 다 안보인다·구두도 화면 밖"): 좁은 폰 폭에서
        // 7버튼 1행은 화면 밖 유출 — **2행 배치**(탐색행+도구행). DeX 광폭에서도
        // 2행의 죽은 공간은 표층 좁음(본체가 1f로 남은 영역 전부 취함).
        val navRow = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL }
        val toolRow = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL }
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
        // [구두] — 홀드 푸시토크(계약 그대로): 홀드 Down=서비스 마이크 개방,
        // Up=마침. #124 서비스 분리로 브라우저 창 겹침과 무관하게 작동.
        val voiceBtn = Button(this).apply {
            text = "구두"
            textSize = 12f
            minimumWidth = 0
            setPadding(row * 2, 0, row * 2, 0)
            setOnTouchListener { _, ev ->
                when (ev.actionMasked) {
                    MotionEvent.ACTION_DOWN -> {
                        val i = Intent(this@BrowserActivity, JkVoiceService::class.java)
                            .setAction(JkVoiceService.ACTION_START)
                        if (android.os.Build.VERSION.SDK_INT >= 26)
                            startForegroundService(i)
                        else startService(i)
                        true
                    }
                    MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                        runCatching {   // 배경 전이 제한 정직 무시
                            startService(Intent(this@BrowserActivity, JkVoiceService::class.java)
                                .setAction(JkVoiceService.ACTION_STOP))
                        }
                        true
                    }
                    else -> true
                }
            }
        }
        val engineBtn = navButton(if (engineName() == "webview") "[GeckoView]" else "[WebView]") {
            toggleEngine()
        }
        listOf(backBtn, fwdBtn, reloadBtn, goBtn).forEach { navRow.addView(it) }
        listOf(voiceBtn, measureBtn, engineBtn).forEach { toolRow.addView(it) }

        // 주소 편집 — 도구행과 공급폭 균형(좁은 화면에서도 주로 타이핑 가능).
        urlInput.layoutParams = LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.WRAP_CONTENT)

        // 본체 — 화면 남은 영역 전부(죽은 마진 0 — fit 원칙 승계).
        // T5 엔진 토글: webview | geckoview — 같은 레이아웃 슬롯 교체, 측정 공용.
        if (engineName() == "geckoview") {
            attachGeckoView(root)
            applyIntentUrl(intent)   // #124 — 구두 URL (gecko판도 동일 계약)
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
        root.addView(toolRow)   // #124 r3 — 2행(좁은 화면 버튼 유출 소각)
        if (engineName() == "webview") root.addView(webView)  // gecko는 attach가 root에 추가
        setContentView(root)

        if (engineName() == "webview") {
            webkitOptIn()
            if (!applyIntentUrl(intent)) goNow()   // 구두 URL 없으면 부팅 기본
        }   // gecko 부팅 로드는 attachGeckoView가 수행
    }

    // #124 — 구두 leg(am start -e jkUrl)에서 온 주소. 이 Activity가 이미 살아
    // 있으면 onNewIntent로 재구성 없이 주소만 교체(측정·엔진 토글 상태 보존).
    private fun applyIntentUrl(i: Intent?): Boolean {
        val u = i?.getStringExtra("jkUrl")?.trim()
        if (u.isNullOrEmpty()) return false
        urlInput.setText(u)
        goNow()
        return true
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        setIntent(intent)
        applyIntentUrl(intent)
    }

    fun goNow() {
        var raw = urlInput.text.toString().trim()
        if (!raw.startsWith("http://") && !raw.startsWith("https://")) {
            raw = "https://$raw"
        }
        if (engineName() == "geckoview") {
            val s = geckoSession
            if (s == null) {
                Toast.makeText(this, "②Gecko 세션 준비 안 됨 — 창 재개봉 필요",
                    Toast.LENGTH_LONG).show()
                return
            }
            Toast.makeText(this, "②이동 개시(Gecko): $raw", Toast.LENGTH_SHORT).show()
            s.loadUri(raw)
        } else {
            Toast.makeText(this, "②이동 개시(WebView): $raw", Toast.LENGTH_SHORT).show()
            webView.loadUrl(raw)
        }
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

    // #124 구두 수신기 — **떠 있는 창 1곳만 수신** 계약(Main 동형): onResume
    // 등록(개봉지시 스스로 적용 leg)·onPause 해제(이중 로드 소각).
    override fun onResume() {
        super.onResume()
        ContextCompat.registerReceiver(this, voiceReceiver,
            IntentFilter().apply {
                addAction(JkVoiceService.BC_STATUS)
                addAction(JkVoiceService.BC_REPLY)
            }, ContextCompat.RECEIVER_NOT_EXPORTED)
    }

    override fun onPause() {
        super.onPause()
        try { unregisterReceiver(voiceReceiver) } catch (_: Exception) {}
    }

    override fun onDestroy() {
        geckoSession?.close()
        geckoSession = null
        super.onDestroy()
    }
}