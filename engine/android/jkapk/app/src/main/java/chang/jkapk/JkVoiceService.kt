package chang.jkapk

// #124 구두 서비스 분리 T1 — 마이크 FGS(기본 STT 엔진 담당).
// 스펙: docs/superpowers/specs/2026-11-01-voice-service-browser-v1-design.md §4.
// 근본 수형 원존: 구두가 본창(Main)에만 붙어 브라우저 창이 겹치면 구두 버튼
// 손이 안 닿음 → 마이크+기본 STT+/talk POST를 서비스 단일 소관으로 이층.
// 계약(사용자 승인): 푸시토크 그대로 — **홀드(START) 동안만 듣고 떼기(STOP)=마침,
// 평시 녹음 0**, 발화 후 = 마이크 완전 개방 해제(stopSelf — 상시 마이크 아님).
// Whisper 엔진은 본창에 그대로 예비 유지(스코프 정직 — v1 서비스는 기본만).
import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.content.Intent
import android.content.pm.ServiceInfo
import android.graphics.Color
import android.os.Build
import android.os.Bundle
import android.os.IBinder
import android.speech.RecognitionListener
import android.speech.RecognizerIntent
import android.speech.SpeechRecognizer
import android.util.Log
import java.net.HttpURLConnection
import java.net.URL
import java.nio.charset.StandardCharsets

class JkVoiceService : Service() {
    companion object {
        const val ACTION_START = "chang.jkapk.VOICE_START"   // 홀드 Down
        const val ACTION_STOP = "chang.jkapk.VOICE_STOP"     // 홀드 Up (마침)
        const val BC_STATUS = "chang.jkapk.VOICE_STATUS"     // partial/상태/오류
        const val BC_REPLY = "chang.jkapk.VOICE_REPLY"       // 최종 텍스트+/talk 회신
        const val EXTRA_TEXT = "text"
        const val EXTRA_STATE = "state"     // listening|recognizing|result|error
        const val EXTRA_DETAIL = "detail"
        const val EXTRA_OK = "ok"           // /talk 회신 성공 여부(표기 색 결정)
        const val TAG = "JK-BROWSER"        // 기존 런 원문 tag 승계(정직 원문 단일 축)
        const val VOICE_CH = "jk_voice"
        const val VOICE_NID = 41
        const val TALK_URL = "http://localhost:8090/talk"
        // #124 세션 기록 계약(jkengine-89 요청 — 사용 저널 세션 축): 앱 프로세스
        // 시작 시 1회 생성 짧은 id — 서버 측 /talk 수취·기록 소관(필드 부재=
        // 서버 정상 동작 — 순서 무관 계약 원존).
        val SESSION_ID: String =
            "ap" + java.util.UUID.randomUUID().toString()
                .replace("-", "").take(10)
    }

    private var recognizer: SpeechRecognizer? = null
    @Volatile private var sysListening = false
    private var sttStartMs = 0L
    private val httpExec = java.util.concurrent.Executors.newSingleThreadExecutor()

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onCreate() {
        super.onCreate()
        val nm = getSystemService(NotificationManager::class.java)
        val ch = NotificationChannel(VOICE_CH, "JK 구두",
            NotificationManager.IMPORTANCE_LOW).apply {
            setShowBadge(false)
            enableLights(false)
            enableVibration(false)
            lightColor = Color.GRAY
        }
        nm.createNotificationChannel(ch)
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        when (intent?.action) {
            ACTION_START -> {
                // 포그라운드 인터랙션에서만 도달하는 것을 계약(스펙 §3 — Android
                // 14+ 뒤에서 mic 시작 = SecurityException — fail-closed 원존).
                val n = buildNotice("듣는 중… (버튼 해제로 마침)")
                if (Build.VERSION.SDK_INT >= 29) {
                    startForeground(VOICE_NID, n,
                        ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE)
                } else {
                    startForeground(VOICE_NID, n)
                }
                startStt()
            }
            ACTION_STOP -> stopStt()
            else -> stopSelf()
        }
        return START_NOT_STICKY   // 계약: 시스템이 부활시켜도 평시 마이킹 금지
    }

    private fun buildNotice(text: String): Notification =
        Notification.Builder(this, VOICE_CH).apply {
            setContentTitle("JK 구두")
            setContentText(text)
            setSmallIcon(android.R.drawable.ic_btn_speak_now)
            setOngoing(true)
        }.build()

    private fun startStt() {
        if (sysListening) return
        if (!SpeechRecognizer.isRecognitionAvailable(this)) {
            emitStatus("error", "기본 음성 인식 서비스 부재 (이 기기에서 구두 불가)")
            finishSelf()
            return
        }
        val sr = recognizer ?: SpeechRecognizer.createSpeechRecognizer(this).also {
            it.setRecognitionListener(sttListener)
            recognizer = it
        }
        val it = Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH).apply {
            putExtra(RecognizerIntent.EXTRA_LANGUAGE_MODEL,
                RecognizerIntent.LANGUAGE_MODEL_FREE_FORM)
            putExtra(RecognizerIntent.EXTRA_LANGUAGE, "ko-KR")
            putExtra(RecognizerIntent.EXTRA_PARTIAL_RESULTS, true)
            putExtra(RecognizerIntent.EXTRA_MAX_RESULTS, 1)
            putExtra(RecognizerIntent.EXTRA_CALLING_PACKAGE, packageName)
        }
        sysListening = true
        sttStartMs = android.os.SystemClock.elapsedRealtime()
        emitStatus("listening", "듣는 중… (버튼 해제로 마침)")
        try {
            sr.startListening(it)
        } catch (e: Exception) {
            sysListening = false
            emitStatus("error", "기본 음성 인식 시작 실패: ${e.message}")
            finishSelf()
        }
    }

    private fun stopStt() {
        // 떼기 = 마침 — 리스너 결과(onResults/onError)를 받아 뒤에 finishSelf().
        // 여기서 즉시 stopSelf하면 결과 수취 전 사망하는 함정 원존(설계 회피 leg).
        if (sysListening) {
            runCatching { recognizer?.stopListening() }
        } else {
            finishSelf()
        }
    }

    private fun finishSelf() {
        sysListening = false
        stopForeground(STOP_FOREGROUND_REMOVE)
        stopSelf()
    }

    private val sttListener = object : RecognitionListener {
        override fun onReadyForSpeech(params: Bundle?) {}
        override fun onBeginningOfSpeech() {}
        override fun onRmsChanged(rmsdB: Float) {}
        override fun onBufferReceived(buffer: ByteArray?) {}
        override fun onEndOfSpeech() {
            if (sysListening) emitStatus("recognizing", "인식 중…")
        }
        override fun onEvent(eventType: Int, params: Bundle?) {}

        override fun onPartialResults(partialResults: Bundle?) {
            val t = partialResults
                ?.getStringArrayList(SpeechRecognizer.RESULTS_RECOGNITION)
                ?.firstOrNull()?.trim().orEmpty()
            if (t.isNotEmpty()) emitStatus("listening", "듣는 중… $t")
        }

        override fun onResults(results: Bundle?) {
            sysListening = false
            val t = results
                ?.getStringArrayList(SpeechRecognizer.RESULTS_RECOGNITION)
                ?.firstOrNull()?.trim().orEmpty()
            val ms = android.os.SystemClock.elapsedRealtime() - sttStartMs
            if (t.isEmpty()) {
                emitStatus("error", "인식 결과 없음 — 다시 말해 주세요")
                finishSelf()
                return
            }
            emitStatus("result", "인식 완료 (기본 · 누름부터 ${ms / 100 / 10.0}s)")
            // /talk POST는 이 서비스가 유일 소관(계약 — 어느 창이 앞이든 발화
            // 사슬이 한 곳). 회신은 BC_REPLY로 양창 전파(Main=채팅 진실원,
            // Browser=토스트).
            httpExec.execute { talkPost(t) }
        }

        override fun onError(error: Int) {
            sysListening = false
            emitStatus("error", "기본 음성 인식 실패: ${sttErrorText(error)}")
            finishSelf()
        }
    }

    private fun sttErrorText(code: Int): String = when (code) {
        SpeechRecognizer.ERROR_NO_MATCH -> "알아들은 말 없음 (다시 말해 주세요)"
        SpeechRecognizer.ERROR_SPEECH_TIMEOUT -> "말소리가 들리지 않음"
        SpeechRecognizer.ERROR_NETWORK, SpeechRecognizer.ERROR_NETWORK_TIMEOUT ->
            "네트워크 오류 (서버 쪽 확인 필요)"
        SpeechRecognizer.ERROR_AUDIO -> "마이크 녹음 오류"
        SpeechRecognizer.ERROR_INSUFFICIENT_PERMISSIONS -> "마이크 권한 없음"
        SpeechRecognizer.ERROR_RECOGNIZER_BUSY -> "인식기가 사용 중 — 잠시 후 다시"
        SpeechRecognizer.ERROR_CLIENT -> "취소됨"
        else -> "오류 코드 $code"
    }

    // ---- POST /talk (MainActivity postTalk 사슬 1:1 승계 — 무변조 계약) ----
    // #124 세션 기록 바디 확장(jkengine-89 요청): "session"(프로세스 수명 짧은
    // id)+"stt":"system"(이 서비스·Whisper/typed는 각 소관 leg가 표기 —
    // MainActivity 쪽 = 원장행)+"stt_ms"(누름~결과 ms — 실측 원문 열).
    private fun talkPost(text: String) {
        val ms = android.os.SystemClock.elapsedRealtime() - sttStartMs
        val body = "{\"text\":\"" + jsonEsc(text) + "\"" +
            ",\"session\":\"$SESSION_ID\"" +
            ",\"stt\":\"system\"" +
            ",\"stt_ms\":$ms}"
        val bytes = body.toByteArray(StandardCharsets.UTF_8)
        val conn = URL(TALK_URL).openConnection() as HttpURLConnection
        try {
            conn.requestMethod = "POST"
            conn.connectTimeout = 10_000
            conn.readTimeout = 60_000   // 74409f2 — LLM 냉시작 여유 원존 승계
            conn.doOutput = true
            conn.setRequestProperty("Content-Type", "application/json; charset=utf-8")
            conn.setFixedLengthStreamingMode(bytes.size)
            conn.outputStream.use { it.write(bytes) }
            val code = conn.responseCode
            val stream = if (code in 200..299) conn.inputStream else conn.errorStream
            val resp = stream.use { it.readBytes().toString(StandardCharsets.UTF_8) }
            val okFlag = resp.contains("\"ok\":true")
            val replyText = jsonGetString(resp, "reply") ?: resp
            val kind = jsonGetString(resp, "kind") ?: ""
            val app = jsonGetString(resp, "app") ?: ""
            var out = replyText
            if (kind.isNotEmpty() || app.isNotEmpty()) {
                out += "  [$kind"
                if (app.isNotEmpty()) out += "·$app"
                out += "]"
            }
            var okAll = okFlag
            if (!okFlag && code != 200) { out = "HTTP $code — $out"; okAll = false }
            emitReply(text, out, okAll)
        } catch (e: java.net.SocketTimeoutException) {
            emitReply(text, "응답 지연… (최대 60초 기다렸습니다 — 서버 확인)", false)
            Log.w(TAG, "TALK-SLOW ${e.message}")
        } catch (e: java.io.IOException) {
            emitReply(text, "서버 미기동 — DeX 세션 필요 (localhost:8090)", false)
        } finally {
            conn.disconnect()
        }
        finishSelf()
    }

    private fun emitStatus(state: String, detail: String) {
        val i = Intent(BC_STATUS).setPackage(packageName)
            .putExtra(EXTRA_STATE, state).putExtra(EXTRA_DETAIL, detail)
        sendBroadcast(i)
    }

    private fun emitReply(text: String, reply: String, ok: Boolean) {
        val i = Intent(BC_REPLY).setPackage(packageName)
            .putExtra(EXTRA_TEXT, text).putExtra(EXTRA_DETAIL, reply)
            .putExtra(EXTRA_OK, ok)
        sendBroadcast(i)
    }

    override fun onDestroy() {
        sysListening = false
        recognizer?.destroy()
        recognizer = null
        httpExec.shutdown()
        super.onDestroy()
    }

    private fun jsonEsc(s: String): String = buildString {
        for (c in s) {
            when (c) {
                '"' -> append("\\\"")
                '\\' -> append("\\\\")
                '\n' -> append("\\n")
                '\r' -> append("\\r")
                '\t' -> append("\\t")
                in '\u0000'..'\u001F' -> append("\\u%04x".format(c.code))
                else -> append(c)
            }
        }
    }

    // JsonGetString — MainActivity 동형(파서 무변조 승계).
    private fun jsonGetString(json: String, key: String): String? {
        val k = "\"$key\":"
        val i = json.indexOf(k)
        if (i < 0) return null
        val j = json.indexOf('"', i + k.length) + 1
        var k2 = j
        val sb = StringBuilder()
        while (k2 < json.length) {
            val c = json[k2]
            if (c == '\\' && k2 + 1 < json.length) {
                sb.append(json[k2 + 1]); k2 += 2
            } else {
                if (c == '"') break
                sb.append(c); k2++
            }
        }
        return sb.toString()
    }
}