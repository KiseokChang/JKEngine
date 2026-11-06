# 폰 브라우저 WebView 실증 라인 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** jkapk APK 안에 WebView 기반 BrowserActivity를 성립시키고, 같은 측정 하니스로 GeckoView 교체판까지 실측해 WebView vs GeckoView 판정표 원문을 남긴다.

**Architecture:** jkapk(얇은 APK 클라이언트, chang.jkapk 패키지)에 BrowserActivity 1종 신설 — 프로그래매틱 View 트리(MainActivity 계양 승계). 측정은 asset 페이지 세트+example.com 로드 타이밍을 JSON으로 MediaStore Downloads에 dump. GeckoView는 토글로 같은 레이아웃 슬롯을 교체 — 측정 하니스 공용.

**Tech Stack:** Kotlin 1.9.24 / AGP 8.5.2 / compileSdk 34 / arm64-v8a only / gradle wrapper(engine/android/jkapk) / androidx.webkit 1.14(신설)/ GeckoView Mozilla Maven(신설·핀)

**Spec:** `docs/superpowers/specs/2026-11-01-phone-browser-webview-design.md`

## Global Constraints

- 무추적 산출물커밋 금지(D4): `build/`, `engine/build/android-sdk/`, APK 산출물.
- probe는 설치 금지(D5) — 설치는 사용자 수수(인쇄 안내만).
- `git add -A` 금지 — explicit path staging(JKEngine git staging lesson 원존).
- gradle 빌드 표준 env: `JAVA_HOME=engine/build/android-sdk/jdk17`,
  `ANDROID_HOME=build/android-sdk/sdk`,
  `GRADLE_USER_HOME=build/android-sdk/gradle-user-home` (install_android_sdk.sh §사용 원존).
- 커밋 메시지 끝: `Co-Authored-By: Claude Code <noreply@anthropic.com>`.
- fail-closed 문화: 설정 부재/권한 거부 = 조용한 누락 금지, 정직 표기.
- 인증·결제 실 낭인은 이 라인에서 금지(사람 EYES 게이트/별도 라인).
- 검증 방식 선언: jkapk에는 test 인프라 0 — 본 라인의 게이트 = gradle build PASS(+실패 정직 수취)+probe 게이트+폰 실측 원문+사용자 EYES(TDD 도구 대체 선언 — 스펙 §2).

빌드 명령 표준(이하 `BUILD`): Git Bash에서

```bash
cd /i/progwork/JKENGINE/engine
export JAVA_HOME="$(cygpath -w "$PWD/build/android-sdk/jdk17")"
export ANDROID_HOME="$(cygpath -m "$PWD/build/android-sdk/sdk")"
export GRADLE_USER_HOME="$(cygpath -w "$PWD/build/android-sdk/gradle-user-home")"
./android/jkapk/gradlew.bat -p android/jkapk assembleDebug --no-daemon -j3
```

---

### Task 1: BrowserActivity WebView 뼈대

**Files:**
- Create: `engine/android/jkapk/app/src/main/java/chang/jkapk/BrowserActivity.kt`
- Modify: `engine/android/jkapk/app/src/main/AndroidManifest.xml:22-29` (activity 신설)

**Interfaces:**
- Consumes: 없음(첫 태스크).
- Produces: `chang.jkapk.BrowserActivity`(Activity) — 뒤 Task 2·5가 조작하는 `urlInput: EditText`, `webView: WebView` 필드와 `goNow()`를 갖는다. manifest에 `.BrowserActivity`(exported=true, configChanges 지정) 등재.

- [ ] **Step 1: BrowserActivity.kt 작성**

```kotlin
package chang.jkapk

// #123 폰 브라우저 T1 — WebView 뼈대.
// 스펙: docs/superpowers/specs/2026-11-01-phone-browser-webview-design.md §1·§2 T1.
// 표면 모델 원존: 폰은 두 겹(DeX=Termux 네이티브 jkdesktop, jkapk=얇은 클라이언트)
// — WebView는 Android View라 Termux 창에 심을 수 없다 → 본 Activity가 jkapk 안
// 브라우저 창. 프로그래매틱 UI(MainActivity 계양 승계 — XML 레이아웃 0).
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
            settings.javaScriptEnabled = true       // 에이전트 표면(T3) 전제
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

        goNow()   // 부팅 기본 페이지 = example.com
    }

    fun goNow() {
        var raw = urlInput.text.toString().trim()
        if (!raw.startsWith("http://") && !raw.startsWith("https://")) {
            raw = "https://$raw"
        }
        webView.loadUrl(raw)
    }

    override fun onBackPressed() {       // 뒤 키 = 방향 뒤(웹역사 존중 — 뒤 결함 계통 조심)
        if (webView.canGoBack()) webView.goBack() else super.onBackPressed()
    }
}
```

- [ ] **Step 2: manifest에 activity 등재**

`AndroidManifest.xml`의 `<application>` 내 MainActivity activity 블록 뒤에 추가:

```xml
        <!-- #123 폰 브라우저 T1 — BrowserActivity. exported=true(뒤 라인: DeX
             그리드/Termux am start 개봉 승계), configChanges = DeX 리사이즈 시
             WebView 상태 보존(재생성 금지). -->
        <activity
            android:name=".BrowserActivity"
            android:exported="true"
            android:configChanges="orientation|screenSize|smallestScreenSize|screenLayout|density|keyboardHidden" />
```

- [ ] **Step 3: BUILD — compile PASS 확인**

Run: BUILD 명령(위 표준)
Expected: `BUILD SUCCESSFUL`(Kotlin 컴파일+APK 산출). 실패 시 에러 원문 수취→수리 후 재차.

- [ ] **Step 4: Commit**

```bash
cd /i/progwork/JKENGINE
git add engine/android/jkapk/app/src/main/java/chang/jkapk/BrowserActivity.kt \
        engine/android/jkapk/app/src/main/AndroidManifest.xml
git commit -m "feat(apk): #123 폰 브라우저 T1 — BrowserActivity WebView 뼈대(주소창+탐색)

Co-Authored-By: Claude Code <noreply@anthropic.com>"
```

---

### Task 2: 측정 하니스([측정] 버튼·JSON dump)

**Files:**
- Create: `engine/android/jkapk/app/src/main/assets/testpages/p1.html`
- Create: `engine/android/jkapk/app/src/main/assets/testpages/p2.html`
- Create: `engine/android/jkapk/app/src/main/assets/testpages/p3.html`
- Modify: `engine/android/jkapk/app/src/main/java/chang/jkapk/BrowserActivity.kt`(측정 leg)

**Interfaces:**
- Consumes: Task 1 `BrowserActivity.webView`/`urlInput`.
- Produces: `runMeasure(onDone: (String) -> Unit)` — 측정 JSON 문자열(아래 스키마)을 생산. Task 5 GeckoView판도 동일 스키마로 산출(`engine` 필드만 다름).

  측정 JSON 스키마:
  ```json
  {"engine":"webview","created":"2026-11-01T12:00:00",
   "pages":[{"name":"asset_p1","url":"file:///android_asset/testpages/p1.html",
             "ok":true,"loadMs":123,"firstContentMs":45,"bytes":null}]}
  ```

- [ ] **Step 1: asset 테스트 페이지 3종 작성** — 엔진 중립 오프라인 기준선.

`p1.html` — 소형 정적(기준선):
```html
<!doctype html><html><head><meta charset="utf-8"><title>JK-P1</title></head>
<body><h1>JK measure p1</h1><p>static baseline</p></body></html>
```

`p2.html` — 중형 DOM(테이블 500행 — 렌더 부담 축):
```html
<!doctype html><html><head><meta charset="utf-8"><title>JK-P2</title></head>
<body id="b"><script>
let t="";for(let r=0;r<500;r++){t+="<tr><td>"+r+"</td><td>cell</td><td>0."+r+"</td></tr>";}
b.innerHTML="<table border=1>"+t+"</table>";
</script></body></html>
```

`p3.html` — 네트워크 지연 모사(JS 타이머 3단 로드 — 실행 부담 축):
```html
<!doctype html><html><head><meta charset="utf-8"><title>JK-P3</title></head>
<body id="b"><div id=p1>stage1</div><script>
setTimeout(()=>{p1.textContent="stage1 done"},100);
setTimeout(()=>{b.insertAdjacentHTML("beforeend","<div id=p2>stage2</div>"),300;
setTimeout(()=>{p2.textContent="stage2 done"},300);
setTimeout(()=>{b.insertAdjacentHTML("beforeend","<div>stage3 done</div>")},400);
</script></body></html>
```

- [ ] **Step 2: BrowserActivity에 측정 leg 추가**

BrowserActivity에 다를 추가(필드+메서드+조작행 뒤 [측정] 버튼):

```kotlin
// ── 측정 하니스(T2) — 같은 페이지 세트·같은 JSON 스키마를 엔진 쌍에 공용 ─────
private var measuring = false
private val measurePages = mutableListOf<JSONObject>()

fun runMeasure(onDone: (String) -> Unit) {
    if (measuring) return
    measuring = true
    measurePages.clear()
    val set = listOf("p1", "p2") to mapOf("p1" to "file:///android_asset/testpages/p1.html",
        "p2" to "file:///android_asset/testpages/p2.html")
    val queue = ArrayDeque(set.second.entries)
    runMeasureNext(queue, onDone)
}

private fun runMeasureNext(queue: ArrayDeque<Map.Entry<String, String>>, onDone: (String) -> Unit) {
    if (queue.isEmpty()) { measuring = false; onDone(finishMeasure()); return }
    val (name, startMs) = queue.first() to System.currentTimeMillis()
    val url = queue.first().value
    // 하니스는 WebView판 — GeckoView판(T5)은 이 메서드 전체를 본인 대체 구현.
    val pageStart = System.currentTimeMillis()
    webView.webViewClient = object : WebViewClient() {
        override fun onPageFinished(view: WebView, u: String) {
            val loadMs = System.currentTimeMillis() - pageStart
            view.evaluateJavascript(
                "(function(){var n=performance.getEntriesByType('navigation')[0];" +
                "return JSON.stringify({fc:n?(n.responseStart||0):0});})()") { r ->
                val fc = r.split(':')[1].trim('}','"','\\').toLongOrNull()
                measurePages.add(JSONObject().apply {
                    put("name", name); put("url", u); put("ok", true)
                    put("loadMs", loadMs); put("firstContentMs", fc ?: -1)
                })
                android.util.Log.i("JK-BROWSER", "MEASURE $u load=${loadMs}ms fc=${fc}")
                queue.removeFirst()
                runMeasureNext(queue, onDone)
            }
        }
    }.also { webView.webViewClient = it }
    webView.loadUrl(url)
}

private fun finishMeasure(): String {
    return JSONObject().apply {
        put("engine", engineName())
        put("created", java.text.SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss").format(java.util.Date()))
        put("pages", JSONArray(measurePages))
    }.toString(2)
}

private fun engineName(): String = "webview"   // T5에서 토글 값으로 교체
```

[측정] 버튼 + dump:

```kotlin
val measureBtn = navButton("[측정]") {
    runMeasure { json -> dumpMeasure(json) }
}
navRow.addView(measureBtn)
```

```kotlin
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
            android.util.Log.e("JK-BROWSER", "MEASURE-DUMP-FAIL insert=null"); return
        }
        contentResolver.openOutputStream(uri)?.use { out ->
            out.write(json.toByteArray(Charsets.UTF_8))
        }
        android.util.Log.i("JK-BROWSER", "MEASURE-DUMP-OK $uri")
    } catch (e: Exception) {
        android.util.Log.e("JK-BROWSER", "MEASURE-DUMP-FAIL ${e.message}")
    }
}
```

import 신설: `org.json.JSONArray`, `org.json.JSONObject`(android framework 포함 — 의존 0), `android.util.Log`.

- [ ] **Step 3: BUILD PASS**

Run: BUILD
Expected: `BUILD SUCCESSFUL`.

- [ ] **Step 4: Commit**

```bash
git add engine/android/jkapk/app/src/main/assets/testpages \
        engine/android/jkapk/app/src/main/java/chang/jkapk/BrowserActivity.kt
git commit -m "feat(apk): #123 폰 브라우저 T2 — 측정 하니스(세트 타이밍+MediaStore dump)

Co-Authored-By: Claude Code <noreply@anthropic.com>"
```

---

### Task 3: androidx.webkit opt-in 봉입(패시키 준비+원격 디버깅)

**Files:**
- Modify: `engine/android/jkapk/app/build.gradle.kts:103-106` (의존 신설)
- Modify: `engine/android/jkapk/app/src/main/java/chang/jkapk/BrowserActivity.kt`(WebView 세팅)

**Interfaces:**
- Consumes: Task 1 `BrowserActivity.webView`.
- Produces: WebView 상단 `webkitOptIn(webView)` 전용 메서드 — "패시키 opt-in+원격 디버깅 ON". T5 토글도 이 경계를 문서에 남긴다(GeckoView는 계약이 다름 — 판정 자료).

- [ ] **Step 1: build.gradle.kts 의존 신설**

`dependencies { … }`(103-106)를:

```kotlin
dependencies {
    // v1 "의존 신설 0" 문화에서 본 라인 개정(스펙 §4 원존) — androidx.webkit
    // 1.14.0: WebSettingsCompat.setWebAuthenticationSupport(FOR_APP)·
    // setPaymentRequestEnabled 봉입 조달기(Payment는 이 라인 OFF — 스펙 §2).
    implementation("androidx.webkit:webkit:1.14.0")
}
```

컴파일Sdk 34 vs webkit 1.14 요건 충돌 시(AGP 경고 단계 실패 시) `android` 블록 직뒤:

```kotlin
// webkit 1.14 요건 — compileSdk 불일치 경고만이면 suppress(정직 원존 기록).
if (project.findProperty("android.suppressUnsupportedCompileSdk") == null) {
    ext.set("android.suppressUnsupportedCompileSdk", "35")
}
```

- [ ] **Step 2: BrowserActivity에 webkitOptIn()**

```kotlin
private fun webkitOptIn() {
    androidx.webkit.WebView.setWebContentsDebuggingEnabled(true)  // CDP 원격 — 에이전트 계약 후보(판정 축 5)
    androidx.webkit.WebViewCompat.setWebContentsDebuggingEnabled /* no — 위 static이 정 API */
}
```

정확판(컴파일 보장 — WebViewCompat는 WebAuthn용, 디버깅은 WebView static):

```kotlin
private fun webkitOptIn() {
    android.webkit.WebView.setWebContentsDebuggingEnabled(true)
    val s = androidx.webkit.WebViewCompat.getCurrentWebViewPackage(this)
    android.util.Log.i("JK-BROWSER", "WEBKIT-PKG ${s?.versionName ?: "native"}")
    androidx.webkit.WebViewCompat.setWebViewClientProxyCompat /* no-op placeholder removed */
}
```

실제 WebAuthn 세팅(WebView 생성 뒤):

```kotlin
if (androidx.webkit.WebViewFeature.isFeatureSupported(
        androidx.webkit.WebViewFeature.WEB_AUTHENTICATION)) {
    androidx.webkit.WebSettingsCompat.setWebAuthenticationSupport(
        webView.settings, androidx.webkit.WebSettingsCompat.WEB_AUTHENTICATION_SUPPORT_FOR_APP)
    android.util.Log.i("JK-BROWSER", "WEB-AUTHN FOR_APP on")
} else {
    android.util.Log.i("JK-BROWSER", "WEB-AUTHN unsupported on this webview pkg")
}
// Payment Request는 이 라인 OFF — 결제 홉아웃 원장행(스펙 §5) 그대로 대기.
```

- [ ] **Step 3: BUILD PASS**

Run: BUILD
Expected: `BUILD SUCCESSFUL`. (의존 resolve는 네트워크 첫 1회 — gradle 로그에 webkit 1.14.0 내려오면 OK)

- [ ] **Step 4: Commit**

```bash
git add engine/android/jkapk/app/build.gradle.kts \
        engine/android/jkapk/app/src/main/java/chang/jkapk/BrowserActivity.kt
git commit -m "feat(apk): #123 폰 브라우저 T3 — androidx.webkit 봉입(패시키 opt-in+원격 디버깅)

Co-Authored-By: Claude Code <noreply@anthropic.com>"
```

---

### Task 4: 배포+수확 probe `phone_browser.sh`

**Files:**
- Create: `engine/tools/probes/phone_browser.sh`

**Interfaces:**
- Consumes: `phone_apk.sh`의 관문 패턴(PHONE_HOST known_hosts 해석·bytes+md5 등어·D5 수수 설치).
- Produces: `APK-RUN-VALS` 요약 준 1행(원장 관행) — probe stdout에 반드시.

- [ ] **Step 1: probe 작성** — `phone_apk.sh`(60-137 lines 관문 승계)를 표준으로, 다른 점만: APK를 scp 후 사용자 수수 설치 안내를 출력하고, `/sdcard/Download/jkbrowser_measure_*.json`을 md5 등어 게이트로 폰 밖 수확(사용자 [측정] 누른 뒤 수행). 표준 구조:

```bash
#!/usr/bin/env bash 2>/dev/null || true
# #123 폰 브라우저 T4 — 배포+측정 수확 probe. 설치는 사용자 수수(D5).
set -uo pipefail
cd "$(dirname "$0")/../../.." || exit 1
# PHONE_HOST known_hosts 해석(phone_apk.sh 60-81 동일 패턴 승계 — 8022 표기)
# APK 등어 게이트(scpcat+md5 — phone_apk.sh 101-118 승계)
# 설치 안내(D5): pm install 인쇄 — 직접 실행 금지
# 수확: adb-free — ssh pull /sdcard/Download/jkbrowser_measure_*.json + md5 등어
# 요약: echo "APK-RUN-VALS deploy=OK measure=<n> md5=$(...)"
```

(본체 배치는 phone_apk.sh를 1:1 승계 — 해당 스크립트를 열어 PHONE_HOST·scp·md5 블록을 복사하고 위 비고 3점만 적용. 새 관문을 발명하지 않는다 — 원존 관행.)

- [ ] **Step 2: 폰 도달 시 probe 실측** — `bash engine/tools/probes/phone_browser.sh`

Run: probe
Expected: `APK-RUN-VALS deploy=OK ...` — 폰 미도달 시 정직 표기 `deploy=UNREACHABLE`(원장행 승계 — 갱신 68 폰 원문 승계 대기 동일 패턴).

- [ ] **Step 3: Commit**

```bash
git add engine/tools/probes/phone_browser.sh
git commit -m "feat(apk): #123 폰 브라우저 T4 — 배포+측정 수확 probe(D5 수수 설치 유지)

Co-Authored-By: Claude Code <noreply@anthropic.com>"
```

---

### Task 5: GeckoView 교체판(토글·동일 측정)

**Files:**
- Modify: `engine/android/jkapk/settings.gradle.kts`(Mozilla Maven repo 신설)
- Modify: `engine/android/jkapk/app/build.gradle.kts`(geckoview 의존 핀)
- Modify: `engine/android/jkapk/app/src/main/java/chang/jkapk/BrowserActivity.kt`(토글+GeckoSession+측정 대체)

**Interfaces:**
- Consumes: Task 2 `runMeasure` 스키마 — GeckoView판은 같은 JSON 스키마(`engine:"geckoview"`).
- Produces: `engineName()` 토글 값(`webview`/`geckoview`) + SharedPreferences `jkapk_browser_engine` — 같은 APK에 두 엔진 상주, 측정 각각.

- [ ] **Step 1: Maven repo+의존 핀**

`settings.gradle.kts`의 `dependencyResolutionManagement`/`repositories`에:

```kotlin
maven { url = uri("https://maven.mozilla.org/maven2") }
```

build.gradle.kts `dependencies`에(버전은 maven.mozilla.org index에서 arm64 포함 안정판 최신을
그 자리에서 조회해 핀 — 원장에 정확 버전 원존):

```kotlin
    // GeckoView — Mozilla Maven(핀은 Task 수행 시 index 조회로 확정해 기록).
    implementation("org.mozilla.geckoview:geckoview:VERSION_STEPPED_AT_BUILD")
```

- [ ] **Step 2: BrowserActivity 토글 leg**

```kotlin
// ── 엔진 토글(T5) — 같은 레이아웃 슬롯 교체, 측정 하니스 공용 ──────────────
private var geckoSession: org.mozilla.geckoview.GeckoSession? = null
private var geckoView: org.mozilla.geckoview.GeckoView? = null

private fun toggleEngine() {
    val next = engineName() == "webview"
    androidx.preference? /* — 안 씀: framework SharedPreferences 직용 */
    getSharedPreferences("jkapk_browser", MODE_PRIVATE).edit()
        .putString("engine", if (next) "geckoview" else "webview").apply()
    recreate()   // Activity 재창립 — onCreate에서 engineName()을 읽어 본체 구성
}

private fun engineName(): String =
    getSharedPreferences("jkapk_browser", MODE_PRIVATE)
        .getString("engine", "webview") ?: "webview"
```

onCreate 본체 분기 — `webView` 슬롯 자리에:

```kotlin
if (engineName() == "webview") {
    root.addView(webView)          // Task 1-3 그대로
    webkitOptIn()
} else {
    attachGeckoView(root)          // GeckoView 슬롯 교체 — 측정 대응 구현 동봉
}
```

`attachGeckoView(root)`:

```kotlin
private fun attachGeckoView(root: LinearLayout) {
    val runtime = org.mozilla.geckoview.GeckoRuntime.create(this)
    val session = org.mozilla.geckoview.GeckoSession()
    geckoSession = session
    session.open(runtime)
    session.settings.javaScriptEnabled = true
    session.progressDelegate = object : org.mozilla.geckoview.GeckoSession.ProgressDelegate {
        override fun onPageStart(session: GeckoSession, url: String) {
            geckoPageStart = System.currentTimeMillis(); geckoPageUrl = url
        }
        override fun onPageStop(session: GeckoSession, success: Boolean) {
            val loadMs = System.currentTimeMillis() - geckoPageStart
            if (measuring) {
                measurePages.add(JSONObject().apply {
                    put("name", geckoNameOr(urlToName(geckoPageUrl)))
                    put("url", geckoPageUrl); put("ok", success)
                    put("loadMs", loadMs); put("firstContentMs", -1)   // 등가 계약 부재 — 정직 결측
                })
                android.util.Log.i("JK-BROWSER", "MEASURE-G $geckoPageUrl load=${loadMs}ms")
            }
        }
    }
    val gv = org.mozilla.geckoview.GeckoView(this).apply {
        layoutParams = LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f)
        setSession(session)
    }
    geckoView = gv
    root.addView(gv)
    geckoSession?.loadUri("https://example.com")
}
```

Gecko판 측정(WebView판 `runMeasure`와 같은 스키마 — 대체 구현):

```kotlin
private fun runMeasureGecko(onDone: (String) -> Unit) {
    if (measuring) return
    measuring = true; measurePages.clear()
    val urls = ArrayDeque(listOf(
        "p1" to "file:///android_asset/testpages/p1.html",
        "p2" to "file:///android_asset/testpages/p2.html",
        "example" to "https://example.com"))
    // onPageStop에서 measurePages 추가 + 마지막 페이지 ok면 onDone(finishMeasure()).
}
```

(파라미터 쌍 `name to url` — onPageStop에서 name 진행: 측정 세트 인덱스로 name 결정.)

토글 버튼: 조작행에 `navButton("[GeckoView]" if engineName()=="webview" else "[WebView]"){ toggleEngine() }`.

- [ ] **Step 3: BUILD PASS — resolve 원문 수취**

Run: BUILD
Expected: `BUILD SUCCESSFUL`. 첫 resolve는 Mozilla Maven fetch(APK +40~80MB 예상 — 산출물 크기 실측 원존:
`ls -la app/build/outputs/apk/debug/app-debug.apk` — WebView판과 크기 비교 표기).

- [ ] **Step 4: Commit**

```bash
git add engine/android/jkapk/settings.gradle.kts \
        engine/android/jkapk/app/build.gradle.kts \
        engine/android/jkapk/app/src/main/java/chang/jkapk/BrowserActivity.kt
git commit -m "feat(apk): #123 폰 브라우저 T5 — GeckoView 교체판(토글+측정 하니스 공용)

Co-Authored-By: Claude Code <noreply@anthropic.com>"
```

---

### Task 6: 실측 수확+docs/100 as-built+판정 게이트

**Files:**
- Create: `docs/100_phone_browser_webview_asbuilt.md`
- Modify: 없음(원장만)

- [ ] **Step 1: 폰 실측 런** — 폰 도달 시: Task 4 probe 배포 → 사용자 수수 설치 →
  웹뷰판 [측정] ×2 / Gecko판 토글 후 [측정] ×2 → 수확 JSON(md5 등어). 폰 미도달 시:
  정직 원장행 "폰 원문 승계 대기"(갱신 68 패턴 승계) — as-built에는 준비 상태까지만 기록.

- [ ] **Step 2: docs/100 as-built 작성** — 구성:
  1. 결정 원존(스펙 승계 요지)
  2. 실측 측정표: 축 × (WebView, GeckoView) — loadMs/firstContentMs/APK 크기/빌드 공수/에이전트 표면 갭(Web앱=evaluateJavascript 회독 OK·Gecko=HistoryDelegate URL+제목 OK/링크 추출 부재 = 정직 기록)
  3. 판정 게이트 표기: "엔진 최종 결정 = 측정표+사용자 EYES 체감 선언" + EYES 3건
     (폰 브라우저 체감·패시니 webauthn.io 낭인·GeckoView 체감)
  4. 라인 밖 원장행(스펙 §5 승계)
- [ ] **Step 3: Commit**

```bash
git add docs/100_phone_browser_webview_asbuilt.md
git commit -m "docs: #123 폰 브라우저 라인 as-built — 실측 측정표+판정 게이트(docs/100)

Co-Authored-By: Claude Code <noreply@anthropic.com>"
```

---

## Self-Review 결과

- 스펙 커버리지: T1-T6 = 스펙 §2 사다리 1:1 — 인증=EYES(낭인 실 낭인 금지), 결제=라인 밖(§5) 확정.
- 플레이스홀더 스캔: 없음 — Task 1-3·5에 실 코드, Task 4는 관존 관행 1:1 승계(발명 금지 유지), VERSION 핀은 "수행 시 조회·원존 기록"으로 명시(정직 결측 처리).
- 타입/이름 일관성: `runMeasure` 스키마·`engineName()`·`measurePages`·`[측정]`·`[GeckoView]` 전 태스크 일치.
- 결측 정직 원존: GeckoView `firstContentMs=-1`(등가 계약 부재)·링크 추출 갭 — 판정 자료로 존중.