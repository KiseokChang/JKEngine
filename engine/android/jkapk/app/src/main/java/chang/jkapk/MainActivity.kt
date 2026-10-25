// #114 jkapk (T2) — 타이핑 표면 (v1 얇은판 — 스펙
// docs/superpowers/specs/2026-10-25-jkapk-design.md §2·D2/D3/D5).
//
// UI: EditText 1줄+[보내기]·대화 목록([나]=보낸 텍스트·[시스템]=답신/오류 —
// TextView append 동형(append-only — chat 앱 문화 승계)·스크롤 앵커)·상태행
// (서버 연결 정직). IME 음성입력은 EditText에서 자연 작동(별도 권한 0 — 외부
// STT 앱 몫 = D7 계약). 전사 원문 그대로 기록(외부 STT가 넣은 텍스트 포함).
//
// 조작: POST http://localhost:8090/talk — jkweb HandleTalk 계약 원문
// (engine/tools/jkweb/main.cpp) — 몸통 {"text":"..."}(JsonEsc 동형 이스케이프),
// 응답 필드 ok/reply/kind/app(server)의 최소 파싱(JsonGetString 동형 — 새
// 파서 계층 아님), 타임아웃 10s. 서버 부재(연결 실패)="서버 미기동 — DeX
// 세션 필요" 정직 표기+기록 반영. HTTP 쓰레드 = 단일 스레드 실행기 — UI
// 스레드 비접촉(framework 전용 — 코루틴 의존 0 = v1 얇은판).

package chang.jkapk

import android.app.Activity
import android.os.Bundle
import android.text.InputType
import android.view.Gravity
import android.view.KeyEvent
import android.view.View
import android.view.ViewGroup
import android.view.inputmethod.EditorInfo
import android.widget.Button
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import java.io.IOException
import java.net.HttpURLConnection
import java.net.URL
import java.nio.charset.StandardCharsets
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors

class MainActivity : Activity() {

    // D3 조작 사슬 — jkweb 8090 loopback(루프백 기본 — LAN 무인증 노출 수수
    // 원장 승계). cleartext는 manifest usesCleartextTraffic로 허용(전부 루프백).
    private val talkUrl = "http://localhost:8090/talk"
    private val statusIdle = "서버: 미확인 — [보내기]로 연결 시도"
    private val statusOff = "서버: 미기동 — DeX 세션 필요"

    private lateinit var logView: TextView
    private lateinit var statusView: TextView
    private lateinit var inputView: EditText
    private lateinit var sendButton: Button
    private lateinit var scrollView: ScrollView
    private val httpExec: ExecutorService = Executors.newSingleThreadExecutor()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val pad = (16 * resources.displayMetrics.density).toInt()
        val row = (10 * resources.displayMetrics.density).toInt()

        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(pad, pad / 2, pad, pad / 2)
        }

        // 상태행 — 서버 연결 정직 표기(아래 updateStatus).
        statusView = TextView(this).apply {
            text = statusIdle
            textSize = 13f
            setPadding(0, row / 2, 0, row)
        }

        // 대화 목록 — append-only TextView(chat 앱 문화 승계[나]/[시스템]).
        logView = TextView(this).apply {
            textSize = 15f
            setPadding(0, 0, 0, 0)
        }
        scrollView = ScrollView(this).apply {
            addView(logView, ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT))
        }

        // 입력행 — EditText 1줄 + [보내기].
        inputView = EditText(this).apply {
            setSingleLine(true)
            inputType = InputType.TYPE_CLASS_TEXT
            // IME 음성입력이 그대로 유입되는 표면(D7 — 외부 STT 앱의 정상
            // 유입 경로). 전사 원문은 그대로 기록된다.
            imeOptions = EditorInfo.IME_ACTION_SEND
            hint = "구두·타이핑 발화 입력"
            setOnEditorActionListener { _, actionId, event ->
                val sendKey = actionId == EditorInfo.IME_ACTION_SEND ||
                    (event?.keyCode == KeyEvent.KEYCODE_ENTER &&
                        event.action == KeyEvent.ACTION_DOWN)
                if (sendKey) { sendNow(); true } else false
            }
        }
        sendButton = Button(this).apply {
            text = "보내기"
            setOnClickListener { sendNow() }
        }
        val inputRow = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
        }
        inputRow.addView(inputView, LinearLayout.LayoutParams(
            0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        inputRow.addView(sendButton)

        root.addView(statusView)
        root.addView(scrollView, LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f))
        root.addView(inputRow)
        setContentView(root)
    }

    // 전송 1회 — [나] 기록 즉시 반영 후 HTTP 스레드로 POST /talk.
    private fun sendNow() {
        val raw = inputView.text ?: ""
        val text = raw.toString().trim()
        if (text.isEmpty()) return // 서버 400 "빈 발화입니다." 대신 로컬 무시

        inputView.setText("")
        appendChat("나", text, false)
        setSending(true)
        updateStatus("서버: 연결 시도 (localhost:8090/talk …)")
        httpExec.execute { doTalk(text) }
    }

    // POST /talk 1회 — UI 스레드 밖(단일 실행기 직렬화).
    private fun doTalk(text: String) {
        val reply = try {
            postTalk(text)
        } catch (e: IOException) {
            null // 서버 부재·타임아웃 — 정직 표기로 수렴
        }
        runOnUiThread {
            setSending(false)
            if (reply == null) {
                markServerOff()
            } else {
                updateStatus("서버: 연결 (localhost:8090 · HTTP ${reply.httpCode})")
                if (reply.ok) {
                    appendChat("시스템", describeReply(reply), false)
                } else {
                    appendChat("시스템", describeReply(reply), true)
                }
            }
        }
    }

    // jkweb HandleTalk 원문 실측(POST /talk 파서 — 새 파서 0):
    //   성공  {"ok":true,"reply":"...","kind":"...","app":"...","server":"..."}
    //   실패  {"ok":false,"reply":"...한국어 이유...","kind":"Error","app":""}
    //   (400 빈 발화·500 데스크톱 창 서버 파이프 미기동 — fail-loud 계약)
    private class TalkReply(
        val httpCode: Int,
        val ok: Boolean,
        val reply: String,
        val kind: String,
        val app: String,
    )

    private fun postTalk(text: String): TalkReply {
        // JsonEsc 동형(jkweb/jktalk — ClientChatApp 규약): 쌍따옴표·역슬래시
        // 백슬래시 이스케이프·제어문자 \uXXXX·나머지(한글 포함) 원문.
        val body = "{\"text\":\"" + jsonEsc(text) + "\"}"
        val bytes = body.toByteArray(StandardCharsets.UTF_8)
        val conn = URL(talkUrl).openConnection() as HttpURLConnection
        try {
            conn.requestMethod = "POST"
            conn.connectTimeout = 10_000  // 타임아웃 10s 계약
            conn.readTimeout = 10_000
            conn.doOutput = true
            conn.setRequestProperty("Content-Type", "application/json; charset=utf-8")
            conn.setFixedLengthStreamingMode(bytes.size)
            conn.outputStream.use { it.write(bytes) }

            val code = conn.responseCode
            val stream = if (code in 200..299) conn.inputStream else conn.errorStream
            val respBody = stream.use { it.readBytes().toString(StandardCharsets.UTF_8) }

            // 최소 파싱(JsonGetString 동형 — jkweb 응답 원문 승계). "reply" 키
            // 탐색 불가 시 = 응답 원문 그대로(판정 못 하면 성공으로 부르지 않음).
            val okFlag = respBody.contains("\"ok\":true")
            val replyText = jsonGetString(respBody, "reply") ?: respBody
            val kind = jsonGetString(respBody, "kind") ?: ""
            val app = jsonGetString(respBody, "app") ?: ""
            return TalkReply(code, okFlag, replyText, kind, app)
        } finally {
            conn.disconnect()
        }
    }

    // 응답 표기 — "kind"·"app"은 사람 표기 뒤에 원문 그대로 병기(최소 파싱).
    private fun describeReply(r: TalkReply): String {
        var out = r.reply
        if (r.kind.isNotEmpty() || r.app.isNotEmpty()) {
            out += "  [${r.kind}"
            if (r.app.isNotEmpty()) out += "·${r.app}"
            out += "]"
        }
        if (!r.ok && r.httpCode != 200) out = "HTTP ${r.httpCode} — $out"
        return out
    }

    private fun markServerOff() {
        updateStatus(statusOff)
        appendChat("시스템", statusOff, true)
    }

    private fun updateStatus(s: String) {
        statusView.text = s
    }

    private fun setSending(on: Boolean) {
        sendButton.isEnabled = !on
        inputView.isEnabled = !on
    }

    // [나]/[시스템] append — append-only 계약+스크롤 앵커(합상면 아래로 고정).
    private fun appendChat(tag: String, text: String, err: Boolean) {
        if (logView.text.isNotEmpty()) logView.append("\n")
        logView.append("[$tag] $text")
        if (err) logView.append("  (오류)")
        scrollView.post { scrollView.fullScroll(View.FOCUS_DOWN) }
    }

    // --- JSON 최소 헬퍼 (jkweb::JsonEsc/JsonGetString 동형 — 본 리포트 원문) ---

    override fun onDestroy() {
        httpExec.shutdown()
        super.onDestroy()
    }

    private fun jsonEsc(s: String): String {
        val out = StringBuilder(s.length + 8)
        for (ch in s) {
            when (ch) {
                '"' -> out.append("\\\"")
                '\\' -> out.append("\\\\")
                '\b' -> out.append("\\u0008")
                '\u000C' -> out.append("\\u000c")
                '\n' -> out.append("\\n")
                '\r' -> out.append("\\r")
                '\t' -> out.append("\\t")
                else -> {
                    val c = ch.code
                    if (c < 0x20) {
                        val n = Character.forDigit((c shr 4) and 0xF, 16)
                        val m = Character.forDigit(c and 0xF, 16)
                        out.append("\\u00").append(n).append(m)
                    } else out.append(ch)
                }
            }
        }
        return out.toString()
    }

    // 키 탐색 단일 문자열 추출 — jkweb::JsonGetString 동형(순차 find — 중첩
    // 개체 없는 단일 키 몸통). \u 서러게이트 쌍까지 해석.
    private fun jsonGetString(body: String, key: String): String? {
        val want = "\"$key\""
        var at = body.indexOf(want)
        while (at >= 0) {
            var v = at + want.length
            while (v < body.length && (body[v] == ' ' || body[v] == '\t')) v++
            if (v < body.length && body[v] == ':') {
                v++
                while (v < body.length && (body[v] == ' ' || body[v] == '\t')) v++
                if (v < body.length && body[v] == '"') {
                    v++
                    return decodeJsonString(body, v)
                }
            }
            at = body.indexOf(want, at + want.length)
        }
        return null
    }

    private fun decodeJsonString(body: String, start: Int): String {
        val out = StringBuilder()
        var v = start
        while (v < body.length) {
            val ch = body[v]
            when {
                ch == '"' -> return out.toString()
                ch == '\\' && v + 1 < body.length -> {
                    v++
                    when (val e = body[v]) {
                        '"' -> out.append('"')
                        '\\' -> out.append('\\')
                        '/' -> out.append('/')
                        'b' -> out.append('\b')
                        'f' -> out.append('\u000C')
                        'n' -> out.append('\n')
                        'r' -> out.append('\r')
                        't' -> out.append('\t')
                        'u' -> {
                            if (v + 4 < body.length) {
                                val hex = body.substring(v + 1, v + 5)
                                val cp = hex.toIntOrNull(16) ?: 0
                                v += 4
                                if (Character.isHighSurrogate(cp.toChar()) &&
                                    v + 5 < body.length &&
                                    body[v + 1] == '\\' && body[v + 2] == 'u'
                                ) {
                                    val lowHex = body.substring(v + 3, v + 7)
                                    val low = lowHex.toIntOrNull(16) ?: 0
                                    if (Character.isLowSurrogate(low.toChar())) {
                                        out.appendCodePoint(
                                            Character.toCodePoint(
                                                cp.toChar(), low.toChar()))
                                        v += 6
                                    } else out.append(cp.toChar())
                                } else out.append(cp.toChar())
                            }
                        }
                        else -> out.append(e)
                    }
                }
                else -> out.append(ch)
            }
            v++
        }
        return out.toString()
    }
}