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
//
// ── v2 T2(#121) — 구두 버튼 전사 사슬 ────────────────────────────────────
// 원존 승계: 데스크톱 #112 T4(ClientChatApp 구두 표면 — R2/R5)+#114 v1 조작
// 사슬(doTalk/postTalk **무변조** — 전사 텍스트=타이핑 동형 계약).
//
// 표면 재판정(버튼 토글/푸시 — R2 원존 "F6 핫키=hold-to-talk·[구두] 버튼=
// 토글"의 APK 판): 터치 표면에서 핫키의 "누름=시작·해제=마침"에 직접 대응하는
// 것은 **버튼 홀드(push-to-talk)**라 채택 — ACTION_DOWN=캡처 시작·ACTION_UP/
// CANCEL=마침. 마이크 실수 개방 방지(홀드 없는 열림 없음)+대응 근거 원존 유지.
// 토글형과의 등호 재판정은 사용자 EYES 몫(기각 시 즉시 수리 계약).
//
// 캡처: AudioRecord 16 kHz·모노·s16le(SttVoice.h wav 규약 상수 등호 수형)·
// VOICE_RECOGNITION 소스(발화 도메인 표준 — 재생 동시 성립). WAV 임시 =
// **표면 소유 진실원**(데스크톱 T3 "캡처 leg는 경로를 강요하지 않는다" —
// cacheDir+pid+발화 순서 접미 원존 수형)·캐넌 44바이트 RIFF 헤더(BuildWavBlob
// 수형). 마이크 권한 = 런타임 fail-closed(거부 = 정직 거부 표기 — 조용한
// 마이크 개방 금지). R5 전사 대기 UX = "녹음 중…" 구두상태행+"전사 중…" 구두
// 상태행(**경과 초 표기 1초 간격 갱신 — UX 수형(2026-10-28 사용자 체감):
// 30-47초 무변화가 "멈춤" 오판을 낳음 — "(Ns)"로 대기의 정직화)**+
// placeholder [나] 행 도착 후 **전사 도착 시 치환**(데스크톱 turns_[slot]
// 치환 수형 — 기록 append-only 유지). 전사 = voiceExec 별도 단일 스레드
// (전사 수십 초(tiny-ko 폰 실측 35-47s — T1 원장)가 조작 사슬을 막지 않음).
//
// 조달: 모델 = APK asset(ggml-small-komixv2.bin — 등급 전환 배치(2026-10-31
// 사용자 결정 ① small 내장) 조달기 핀 sha256 a428f646…; tiny-ko(dd6d9f46… —
// T1 원존)는 2순위 보존 — 등급 재판정 시 조달기+gradle+본 핀 3계층 동시 갱신)
// → filesDir 1회 조립+**sha256 핀 등호 fail-closed**(조용한 대체 금지 —
// 조달기 게이트의 폰 표면 수형).
//
// ── debug leg (2026-10-31 — APK 빈 전사 원진 확정용·일시) ─────────────────
// 사용자 실시간 진단 leg — 전체 사슬(R5 치환·/talk 전송·권한·해제 단일 소관)
// **원존 무변조** — 신설은 검증용 표기뿐(녹음/전사 사슬 접촉 0):
//   1. 녹음 결과물 외부 덤프: transcribe 직전(voiceExec 스레드) — wav를 폰
//      Download/jkapk_last_capture.wav로 복사(덮어쓰기 — 1파일 진실원).
//      **쓰기 실패 = 조용히 스킵**(전사 사슬 접촉 0 — D5 계통 — 기록/거부
//      무접촉). 성공 여부+bytes를 구두 상태행 1행 표기("녹음 N bytes 저장").
//      컨트롤러가 폰에서 md5/사운드 승인 — 외부 분석 몫.
//   2. 전사 결과 계기: JNI 반환 직후 — **반환 문자열 길이**를 구두 상태행
//      1행 표기("인식 완료 (N자)"). 빈 전사/예외 실패는 정확히 구분 표기 —
//      fail-closed 계약 무변조("인식 없음 (빈 전사…)"/"인식 실패").
//   ⚠ **철수 계획**: 본 leg는 진단용 일시 — 다음 라인(원진 확정 후)에서
//   디버그 leg 전량 철수(dumpWavForDiagnosis·dumpNote·withDump·rawLen
//   표기 소거, fail 분기 setVoiceStatus 원존("") 복원) — 제스처 잔상
//   방지. 철수 시 본 헤더 원장행도 함께 소각한다.

package chang.jkapk

import android.Manifest
import android.app.Activity
import android.content.pm.PackageManager
import android.media.AudioFormat
import android.media.AudioRecord
import android.media.MediaRecorder
import android.os.Bundle
import android.os.Environment
import android.os.Handler
import android.os.Looper
import android.os.Process
import android.os.SystemClock
import android.text.InputType
import android.view.Gravity
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.view.inputmethod.EditorInfo
import android.widget.Button
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import java.io.ByteArrayOutputStream
import java.io.File
import java.io.IOException
import java.net.HttpURLConnection
import java.net.URL
import java.nio.charset.StandardCharsets
import java.security.MessageDigest
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

    // ── v2 T2 구두 표면 상수/상태 ─────────────────────────────────────────
    // 캡처 규약 상수 — SttVoice.h wav::kVoiceSampleRate/Channels/BitsPerSample
    // 등호 수형(16 kHz·모노·s16le — JNI 판독부와 같은 규약).
    private val capSampleRate = 16000
    private val reqMicCode = 6102
    // pcm 수납 상한(fix r1) — 12 MB ≈ 6.25분(@32KB/s) — 이탈 홀드 무한 수납
    // 방지(원장행: 상한 도달 = 판독 스레드 절단+정직 거부 — 기록 반영).
    private val kMaxPcmBytes = 12 * 1024 * 1024
    // 모델 asset 핀 — 등급 전환 배치(2026-10-31 사용자 결정 ① small 내장):
    // 조달기(install_android_sdk.sh §7) small-komixv2 핀 승계 원존. tiny-ko
    // (dd6d9f46…/77,691,730 — T1 조달기 원존)는 2순위 보존 — 등급 재판정 시
    // 조달기+gradle+본 핀 3계층 동시 갱신 계약.
    private val modelAssetName = "ggml-small-komixv2.bin"
    private val modelAssetBytes = 487_601_984L
    private val modelAssetSha256 =
        "a428f6462d65c8f8942a2e1b9684910e5ff3a6873cc2b80856d549739627bba4"

    private lateinit var voiceButton: Button
    private lateinit var voiceStatusView: TextView
    private var audioRecord: AudioRecord? = null
    @Volatile private var capturing = false
    private var capThread: Thread? = null
    private var pcmBuf: ByteArrayOutputStream? = null
    private var voiceWavSeq = 0
    private val voiceExec: ExecutorService = Executors.newSingleThreadExecutor()

    // ── debug leg 상태(2026-10-31 진단용 일시 — 헤더 원장행 철수 계획 참조) ──
    // 녹음 외부 덤프 표기 문언(1파일 진실원 — 새 발화 개시마다 리셋). 기입은
    // 전부 메인 스레드(transcribe runOnUiThread 사슬) — 읽는 쪽도 상태행
    // 합성(메인 스레드)뿐. 전사 수렴 시 상태행 1행에 덤프 계기와 합성 표기
    // (행 분할 0 — "상태행 1행" 계약).
    private var dumpNote: String? = null
    // 기록 = 텍스트+오류플래그 행열(append-only — 치환만 허용(R5)). TextView
    // 직 append에서 행열+render로 확장한 이유: 데스크톱 turns_[slot] 치환 수형
    // 이 전사 도착 시 필요(placeholder 슬롯 안정성 — append-only 계약 유지).
    private val logLines = ArrayList<Pair<String, Boolean>>()

    // 전사 경과 표기(UX 수형 — 2026-10-28 사용자 체감): 상태행 "전사 중…"
    // 뒤 경과 초 갱신(1초 간격 — 핸들러 재량 선택: 코루틴 의존 0 원존(v1
    // 얇은판) 유지·틱/중지 전부 메인 스레드 접근(개시=터치 사슬·중지=
    // runOnUiThread 사슬 — UI 접근 원존 준수). 상한 표기 없음(30-47s 밴드 —
    // 대기의 정직 표기가 목적 — 원장행). 중지 소관 = 전사 수렴 1종
    // (transcribe runOnUiThread — 성공/실패 공통)+onDestroy.
    private val voiceUiHandler = Handler(Looper.getMainLooper())
    private var transcribeStartMs = 0L
    private val transcribeTickRun = object : Runnable {
        override fun run() {
            val sec = (SystemClock.elapsedRealtime() - transcribeStartMs) / 1000
            setVoiceStatus(withDump("전사 중… (${sec}s)"))
            voiceUiHandler.postDelayed(this, 1000)
        }
    }

    // 디버그 leg 표기 합성 — 덤프 계기가 실려 있으면 구두 상태행 1행 앞에
    // 첨가(1행 계약 유지 — 행 분할 0). dumpNote 미실림 = 원존 문언 그대로.
    private fun withDump(s: String): String =
        if (dumpNote != null) "$dumpNote · $s" else s

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
        // [구두] — push-to-talk(재판정 원문: 헤더 주석 — 홀드=녹음·해제=마침).
        voiceButton = Button(this).apply {
            text = "구두"
            setOnTouchListener { _, ev ->
                when (ev.actionMasked) {
                    MotionEvent.ACTION_DOWN -> {
                        startVoice(); true
                    }
                    MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                        finishVoice(); true
                    }
                    else -> true
                }
            }
        }
        // 구두 상태행(R5 수형 2행 원존 — "녹음 중…/전사 중…" — 대기 무더티
        // 원존 계약: 이 행은 상태 전이에만 쓴다).
        voiceStatusView = TextView(this).apply {
            textSize = 13f
            setPadding(0, row / 2, 0, 0)
        }
        val inputRow = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
        }
        inputRow.addView(inputView, LinearLayout.LayoutParams(
            0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        inputRow.addView(sendButton)
        inputRow.addView(voiceButton)

        root.addView(statusView)
        root.addView(scrollView, LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f))
        root.addView(voiceStatusView)
        root.addView(inputRow)
        setContentView(root)
        renderLog()
    }

    // 전송 1회 — [나] 기록 즉시 반영 후 HTTP 스레드로 POST /talk.
    private fun sendNow() {
        val raw = inputView.text ?: ""
        val text = raw.toString().trim()
        if (text.isEmpty()) return // 서버 400 "빈 발화입니다." 대신 로컬 무시

        inputView.setText("")
        appendChat("나", text, false)
        sendPrepared(text)
    }

    // 조작 사슬 꼬리 1종 — 타이핑(sendNow)과 전사 원문(구두 T2)이 같은 꼬리를
    // 쓴다("STT 텍스트==타이핑 텍스트" 규약 승계 — doTalk/postTalk 무변조).
    private fun sendPrepared(text: String) {
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
        logLines.add(Pair("[$tag] $text", err))
        renderLog()
    }

    // 행열 → TextView(append-only 렌더 — 유일한 변경점은 R5 슬롯 치환).
    private fun renderLog() {
        val sb = StringBuilder()
        for ((text, err) in logLines) {
            if (sb.isNotEmpty()) sb.append('\n')
            sb.append(text)
            if (err) sb.append("  (오류)")
        }
        logView.text = sb
        scrollView.post { scrollView.fullScroll(View.FOCUS_DOWN) }
    }

    // R5 치환 — 데스크톱 turns_[slot] 수형(append-only 행열에서 슬롯 안정 —
    // 슬롯 도달 없음 = 무이 치환(데스크톱 DrainVoice 가드 수형)).
    private fun replaceLine(slot: Int, text: String, err: Boolean) {
        if (slot !in logLines.indices) return
        logLines[slot] = Pair(text, err)
        renderLog()
    }

    // ---- 구두 표면 (v2 T2 — 데스크톱 #112 T4 StartVoice/FinishVoice 수형) ----

    private fun setVoiceStatus(s: String) {
        voiceStatusView.text = s
    }

    // 전사 경과 갱신 개시 — "전사 중…" 즉시 표기 후 1초 간격 틱(UX 수형).
    // 호출지 = finishVoice(터치 사슬 — 전부 메인 스레드). 이중 개시 봉쇄:
    // removeCallbacks 선행(voiceExec 단일 실행기로 중복 전사 자체가 없지만
    // 방어 1행 — 틱 이중 가동 시 소리 없는 초 가속).
    private fun transcribeProgressStart() {
        transcribeStartMs = SystemClock.elapsedRealtime()
        voiceUiHandler.removeCallbacks(transcribeTickRun)
        transcribeTickRun.run()
    }

    // 전사 경과 갱신 중지 1종 — 전사 수렴(transcribe runOnUiThread — 성공/
    // 실패 공통)과 onDestroy 공용. 상태 소각("전사 중…" 소거)은 기존 소관
    // 행(setVoiceStatus(""))에 그대로 두고 본 종은 틱 소거만 담당한다
    // (R5 치환 경로 무변조 — 기존 행열/치환 계약 접촉 0).
    private fun transcribeProgressStop() {
        voiceUiHandler.removeCallbacks(transcribeTickRun)
    }

    // 정직 거부 1종 — 데스크톱 status_="[!] "+reason 얼굴+[시스템] 기록 수형
    // (기록을 부풀리지 않는다 — ClientNotifyApp 토스트의 APK 대응은 기록).
    private fun failVoice(msg: String) {
        setVoiceStatus("[!] $msg")
        appendChat("시스템", "[!] $msg", true)
    }

    // [구두] 누름 — 마이크 권한 fail-closed 게이트 먼저(데스크톱 StartVoice
    // 사전 판정 수형: 권한 미성립 = 개방 낭비 없이 정직 거부로 수렴).
    private fun startVoice() {
        if (capturing) return
        dumpNote = null  // 디버그 leg — 새 발화 개시마다 덤프 표기 리셋(1파일 진실원 대응)
        if (checkSelfPermission(Manifest.permission.RECORD_AUDIO) !=
            PackageManager.PERMISSION_GRANTED) {
            setVoiceStatus("마이크 권한 대기…")
            requestPermissions(arrayOf(Manifest.permission.RECORD_AUDIO), reqMicCode)
            return  // 결과는 onRequestPermissionsResult — 승인해도 이 홀드는 개방
                    // 하지 않는다(fix r1 — 승인 즉시 자동 기동 철수): 권한
                    // 대기 중 손가락이 이미 떨어진 홀드가 무인 녹음으로 잡음
                    // 전사 경로에 유입되는 것을 소각. 다음 홀드에서 시작.
        }
        startCaptureActual()
    }

    override fun onRequestPermissionsResult(
        requestCode: Int, permissions: Array<out String>, grantResults: IntArray,
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode != reqMicCode) return
        val granted = permissions.isNotEmpty() &&
            permissions[0] == Manifest.permission.RECORD_AUDIO &&
            grantResults.isNotEmpty() &&
            grantResults[0] == PackageManager.PERMISSION_GRANTED
        if (granted) {
            // fix r1 — 승인 분기 = 상태 소거만(무홀드 자동 기동 없음 — 다음
            // 홀드에서 startVoice가 원존 계약대로 개방).
            setVoiceStatus("")
        } else {
            // fail-closed 정직 거부(원존: 미승인이면 fail-closed로 정직 표기 —
            // #112 스펙 §6b R3 계열).
            failVoice("마이크 권한 거부 — 구두 전사 불가: 설정에서 권한 허용 후 재조작")
        }
    }

    // AudioRecord 개방+판독 스레드 — 16 kHz·모노·s16le(개방 실패/시작 실패 =
    // 정직 거부 — 조용한 공백 녹음 금지).
    private fun startCaptureActual() {
        if (capturing) return
        val minBuf = AudioRecord.getMinBufferSize(
            capSampleRate,
            AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT)
        if (minBuf <= 0) {
            failVoice("구두 캡처 개방 실패 (버퍼 산출 실패)")
            return
        }
        val rec = try {
            AudioRecord(
                MediaRecorder.AudioSource.VOICE_RECOGNITION,
                capSampleRate,
                AudioFormat.CHANNEL_IN_MONO,
                AudioFormat.ENCODING_PCM_16BIT,
                maxOf(minBuf, capSampleRate * 2 * 8))
        } catch (e: Exception) {
            null
        }
        if (rec == null || rec.state != AudioRecord.STATE_INITIALIZED) {
            // 개방 실패 = 등록 전 자기해제 예외 — 해제 단일 소관(
            // releaseCaptureState)은 등록(audioRecord = rec) 이후 소관이라
            // 여기선 인라인 release — 소관 별도(batch4 원장행 보강).
            rec?.release()
            failVoice("구두 캡처 개방 실패 (AudioRecord 초기화 불성립)")
            return
        }
        pcmBuf = ByteArrayOutputStream()
        audioRecord = rec
        try { rec.startRecording() } catch (e: Exception) {
            // 개방 실패 = 등록 전 자기해제 예외 — 소관 별도(위 초기화 불성립
            // 분기와 같은 계보).
            rec.release(); audioRecord = null; pcmBuf = null
            failVoice("구두 캡처 개방 실패 (startRecording): ${e.message}")
            return
        }
        capturing = true
        setVoiceStatus("녹음 중… (버튼 해제로 마침)")   // R5 "녹음 중…" 수형
        capThread = Thread {
            val dst = pcmBuf
            val chunk = ShortArray(2048)
            val bytes = ByteArray(chunk.size * 2)
            var overflow = false
            while (capturing && dst != null) {
                if (dst.size() >= kMaxPcmBytes) {
                    // pcm 상한(fix r1 — 12 MB ≈ 6.25분): 이탈 홀드 무한 수납
                    // 방지 — 상한 도달 = 즉시 절단+정직 거부 표기(기록 반영).
                    overflow = true
                    capturing = false
                    break
                }
                val n = audioRecord?.read(chunk, 0, chunk.size) ?: 0
                if (n <= 0) break  // 판독 오류/개방 해제 — 마침 쪽에서 정직 판정
                // s16le 직기입 — 플랫폼 판독(Short)을 LE 바이트열로 표준화
                // (원존 캐넌 s16le 계약 — 암묵 리샘플/축소 0).
                for (i in 0 until n) {
                    bytes[i * 2] = (chunk[i].toInt() and 0xFF).toByte()
                    bytes[i * 2 + 1] = ((chunk[i].toInt() shr 8) and 0xFF).toByte()
                }
                dst.write(bytes, 0, n * 2)
            }
            if (overflow) {
                // fix r2 — 상한 도달도 캡처 해제 단일 소관을 친다(미해제 시
                // 마이크 계속 개방·다음 홀드 startCaptureActual이 구
                // AudioRecord를 만들어 덮어써 연쇄 실패 — 리뷰 Important).
                val recO = audioRecord
                audioRecord = null
                pcmBuf = null
                capThread = null
                releaseCaptureState(recO)  // finishVoice와 동일 해제 구조
                // 부품 단정(+1건 — 2zb 계열 확장): overflow → mic 해제·상태
                // 소각 불변식(fail-loud — 정직 거부 문화 수형; 위배 시 조용한
                // 마이크 개방 잔존 없이 즉사). 동어반복 단정(audioRecord는
                // 바로 위 소각) → release 후 재대입 검사 형태로 정정(batch4
                // 원장행 — fail-loud 의미 살리기: recO 부재 = 해제 미실시·
                // 재대입 잔존 = release 뒤 유실 재등록).
                check(recO != null && audioRecord == null) {
                    "jkapk overflow 해제 원존 위배 (mic 미해제·release 후 재대입)"
                }
                runOnUiThread {
                    failVoice("녹음 상한 도달 (${kMaxPcmBytes / 1024 / 1024}MB) — 조기 절단, 다시 발화해 주세요")
                }
            }
        }.also { it.start() }
    }

    // 캡처 해제 단일 소관 1종(fix r2 — finishVoice·overflow·onDestroy 공용):
    // AudioRecord 소관자가 아니라 소관자라 stop/release+상태 소각을 같은
    // 구조로 친다 — 소관 분산은 "마이크 계속 개방" 결함(리뷰 Important)의
    // 진원이므로 해제 구조를 1소관으로 봉한다. 호출 뒤 불변식 = audioRecord
    // == null(부품 단정 — 호출자의 check fail-loud가 수형).
    private fun releaseCaptureState(rec: AudioRecord?) {
        if (rec != null) {
            // 원장행(join-타임아웃 레이스 — fix r1, 코멘트만): 판독 스레드가
            // join 2s 내 종료하지 않았는데도 stop/release가 난다면 — 그 스레드의
            // 마지막 AudioRecord.read는 잘못된 상태(-1/0)로 수렴하거나
            // ConcurrentModification 계열 예외로 자사(자기 루프 break)한다.
            // 판독 스레드는 AudioRecord를 참조만 한다(개방 소유 아님) — 해제는
            // 본 단일 소관이므로 이중 release는 구조적으로 없고, 늦은 판독
            // 결실은 버려진다(부분 산물 — 정직 거부 계약이 이미 0.1s 미만을
            // 친다. 장파괴 레이스 실측은 폰 E2E 후 원장행 판정).
            try { rec.stop() } catch (_: IllegalStateException) {}
            try { rec.release() } catch (_: IllegalStateException) {}
        }
    }

    // [구두] 해제 — 판독 절단 → WAV 조립(표면 소유 경로) → 전사 스레드(R5
    // placeholder 도착). 데스크톱 FinishVoice 수형(빈 결실 = 정직 거부).
    private fun finishVoice() {
        if (!capturing) return
        capturing = false
        val rec = audioRecord
        audioRecord = null
        val pcm = pcmBuf
        pcmBuf = null
        capThread?.let { t -> try { t.join(2_000) } catch (_: InterruptedException) {} }
        capThread = null
        releaseCaptureState(rec)   // fix r2 — 해제 단일 소관 1행
        val bytes = pcm?.toByteArray() ?: ByteArray(0)
        // 0.1s 미만 사전 거부 — JNI 판독부 규약(결실 0.1s 미만 정직 거부)의
        // 표면 수형("눌렀다 바로 푼 실수 탭"이 placeholder 없이 정직 수렴).
        if (bytes.size < capSampleRate * 2 / 10) {
            failVoice("발화 결실 부족 — 누른 채 유지하고 해제로 마침")
            return
        }
        // WAV 임시 = 표면 소유 진실원(데스크톱 T3 원존 수형 — cacheDir+
        // pid+발화 순서 접미: 재발화가 앞 발화 산물을 덮지 않는다).
        val wav = File(cacheDir,
            "jkapk_voice_${Process.myPid()}_${voiceWavSeq++}.wav")
        try {
            wav.outputStream().use { it.write(buildWavBlob(bytes)) }
        } catch (e: IOException) {
            failVoice("구두 캡처 wav 쓰기 실패: ${e.message}")
            return
        }
        // R5 — placeholder 턴 [나] 도착+구두 상태행 "전사 중…"(데스크톱
        // FinishVoice 수형 — record 반영치환 도착 시 슬롯 치환 계약).
        val slot = logLines.size
        appendChat("나", "(전사 중…)", false)
        transcribeProgressStart()  // "전사 중…"+경과 초 틱 개시(UX 수형)
        voiceButton.isEnabled = false
        voiceExec.execute { transcribe(wav, slot) }
    }

    // ── 디버그 leg 1(2026-10-31 진단용 일시 — 헤더 원장행 철수 계획) ────────
    // 녹음 결과물 외부 덤프 — transcribe 직전(voiceExec 스레드 — 기존
    // finishVoice main 스레드 wav 쓰기 관행을 여기서는 UI 스레드 밖으로
    // 치켜세움). 폰 Download/jkapk_last_capture.wav 1파일 진실원(덮어쓰기
    // — 최후 발화 산물) → 컨트롤러가 폰에서 md5/사운드 승인(외부 분석 몫).
    // **쓰기 실패 = 조용히 스킵**(D5 계통 — 전사 사슬 접촉 0·기록/거부
    // 무접촉) — 성공 여부+bytes만 구두 상태행 1행에 정직 표기.
    private fun dumpWavForDiagnosis(wav: File) {
        val dst = File(
            Environment.getExternalStoragePublicDirectory(
                Environment.DIRECTORY_DOWNLOADS),
            "jkapk_last_capture.wav")
        try {
            wav.copyTo(dst, overwrite = true)
            val n = dst.length()
            runOnUiThread { dumpNote = "녹음 $n bytes 저장" }
        } catch (e: Exception) {
            // 조용히 스킵 — 실은 계기만(빈/실패 구분의 "실패" 축과 무관 —
            // 전사 판정 계약은 이 leg 결과를 전혀 읽지 않는다).
            runOnUiThread {
                dumpNote = "녹음 덤프 실패 (${e.javaClass.simpleName})"
            }
        }
    }

    // 전사 — voiceExec(단일 실행기 — 전사 수십 초(tiny-ko 폰 실측 35-47s —
    // T1 원장)가 조작 사슬을 막지 않는다). 성공 = R5 슬롯 치환(전사 원문
    // 그대로) → /talk 조작 사슬(타이핑 동형 — v1 사슬 무변조). 실패 = 슬롯을
    // 정직 오류 행으로 치환(기록 부풀림 0 — 데스크톱 DrainVoice 수형).
    private fun transcribe(wav: File, slot: Int) {
        var failText: String? = null
        var transcript: String? = null
        var rawLen = -1  // 디버그 leg 2 — JNI 반환 문자열 길이(예외로 미도달 = -1)
        dumpWavForDiagnosis(wav)  // 디버그 leg 1 — 전사 직전 녹음 산물 외부 덤프
        try {
            val model = resolveModel()
            val text = JniWhisper.jkapkTranscribe(
                wav.absolutePath, model.absolutePath, JniWhisper.kVoicePrompt)
            rawLen = text.length
            val safe = JniWhisper.sanitizeTranscript(text)  // 보조 원존 — 2차 게이트
            if (safe.isBlank()) {
                failText = "전사 결실 0 (빈 전사 — 정직 거부 계약)"
            } else {
                transcript = safe
            }
        } catch (e: Throwable) {
            failText = e.message ?: e.javaClass.simpleName  // RuntimeException 원문 유지 — 발명 0
        } finally {
            wav.delete()  // 캡처 임시 잔산 소각("부분 산물 잔산 금지" 원존 계열)
        }
        runOnUiThread {
            transcribeProgressStop()  // 경과 갱신 중지(UX 수형 — 성공/실패 공통)
            voiceButton.isEnabled = true
            if (failText != null) {
                replaceLine(slot, "[!] 전사 실패: $failText", true)
                // 디버그 leg 2 — 빈 전사/예외 실패 구분 표기(계기뿐 — 기록
                // 행열·fail-closed 판정 계약 무변조): rawLen >= 0 = JNI가
                // 반환했으나 정리 결과 빈 것(빈 전사) · rawLen == -1 = 예외
                // 수렴(실패). JNI 계약상 빈 반환은 RuntimeException으로 수렴
                // (승용 금지)이라 빈 분기는 방어 표기.
                setVoiceStatus(withDump(if (rawLen >= 0)
                    "인식 없음 (빈 전사 · 반환 ${rawLen}자)"
                else
                    "인식 실패"))
            } else {
                // 디버그 leg 2 — 반환 문자열 길이 계기 1행("인식 완료 (N자)"
                // — N = JNI 반환 원문 길이·sanitize는 단독 서러게이트 소각뿐
                // 이라 통상 등호). R5 치환 경로 무변조.
                replaceLine(slot, "[나] $transcript", false)  // R5 치환 — 전사 원문 그대로
                setVoiceStatus(withDump("인식 완료 (${rawLen}자)"))
                // (원장행: Kotlin은 한글도 식별자 문자로 파싱 — "$rawLen"+"자"가
                // "rawLen자" 최장 일치 변수로 확장되어 unresolved. 한글 접미는
                // "${rawLen}자" 중괄호 필수.)
                transcript?.let { sendPrepared(it) }  // /talk 조작 사슬 재용
            }
        }
    }

    // 모델 resolve — filesDir 사본 1회 조립+sha256 핀 등호 fail-closed(T1
    // 조달기 §7 게이트의 폰 표면 수형 — 조용한 대체/부적합 조용 진행 금지).
    // JNI fopen = 같은 uid 앱 프로세스 → filesDir 경로 직접 개방 성립(표면
    // 경로 진실원).
    private fun resolveModel(): File {
        val f = File(filesDir, modelAssetName)
        if (f.isFile && f.length() == modelAssetBytes) return f
        val part = File(filesDir, "$modelAssetName.part")
        val md = MessageDigest.getInstance("SHA-256")
        assets.open(modelAssetName).use { ins ->
            part.outputStream().use { out ->
                val buf = ByteArray(1 shl 20)
                while (true) {
                    val n = ins.read(buf)
                    if (n < 0) break
                    if (n > 0) { md.update(buf, 0, n); out.write(buf, 0, n) }
                }
            }
        }
        val sha = md.digest().joinToString("") { "%02x".format(it) }
        if (sha != modelAssetSha256 || part.length() != modelAssetBytes) {
            part.delete()
            throw RuntimeException(
                "모델 asset 핀 불일치 (sha256=$sha — 정직 거부 — 조용한 대체 금지)")
        }
        if (!part.renameTo(f)) {
            part.delete()
            throw RuntimeException("모델 asset 조립 실패 (rename)")
        }
        return f
    }

    // WAV 조립 — BuildWavBlob 수형(캐넌 44바이트 RIFF/WAVE+fmt16+data — LE
    // 직기입·16 kHz/모노/s16le 상수 등호 — SttVoice.h wav 규약).
    private fun buildWavBlob(pcm: ByteArray): ByteArray {
        val blob = ByteArray(44 + pcm.size)
        var at = 0
        fun putTag(t: String) {
            for (b in t.toByteArray(StandardCharsets.US_ASCII)) blob[at++] = b
        }
        fun putLe16(v: Int) {
            blob[at++] = (v and 0xFF).toByte()
            blob[at++] = ((v shr 8) and 0xFF).toByte()
        }
        fun putLe32(v: Int) {
            for (i in 0 until 4) blob[at++] = ((v shr (8 * i)) and 0xFF).toByte()
        }
        putTag("RIFF")
        putLe32(36 + pcm.size)
        putTag("WAVE")
        putTag("fmt ")
        putLe32(16)              // fmt 청크 크기(캐넌 16)
        putLe16(1)               // WAVE_FORMAT_PCM
        putLe16(1)               // 모노
        putLe32(capSampleRate)   // 16000
        putLe32(capSampleRate * 2)  // nAvgBytesPerSec = rate*ch*bits/8
        putLe16(2)               // blockAlign = ch*bits/8
        putLe16(16)              // s16le
        putTag("data")
        putLe32(pcm.size)
        System.arraycopy(pcm, 0, blob, at, pcm.size)
        return blob
    }

    // --- JSON 최소 헬퍼 (jkweb::JsonEsc/JsonGetString 동형 — 본 리포트 원문) ---

    override fun onDestroy() {
        // 구두 워커 join 수형(UserChatApp 캡처 절단 원문 계약 계열): 녹음
        // 중 단절이면 판독 루프를 먼저 절단하고 join — AudioRecord 조기 해제
        // 없음(판독 스레드가 개방을 소유하지 않는 구조 — 해제는 finishVoice/
        // onDestroy 단일 소관).
        capturing = false
        capThread?.let { t -> try { t.join(2_000) } catch (_: InterruptedException) {} }
        capThread = null
        releaseCaptureState(audioRecord)  // fix r2 — 해제 단일 소관 1행
        audioRecord = null
        pcmBuf = null
        transcribeProgressStop()  // 경과 갱신 중지(onDestroy 소관 — UX 수형)
        voiceExec.shutdown()
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
