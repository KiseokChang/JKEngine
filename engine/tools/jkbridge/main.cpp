// jkbridge — Phone web gateway for the Desktop Agent (spec:
// docs/superpowers/specs/2026-09-18-jkbridge-design.md).
//
// The phone's browser cannot reach the window-server named pipe, and the LLM
// engine spawns on this PC — so a small gateway runs here: HTTP serves the
// embedded web UI, WebSocket carries the frames, and every authenticated
// session gets one JKAgentClient (control-only, exactly what jkchat is) plus
// one JKLlmEngine. The relay is deliberately GENERIC (tool/approve/chat
// frames, not chat-only commands) so later tools (files/notes/settings) ride
// the same wire without bridge changes.
//
// Security model: LAN only + URL token. The token gates the WS handshake
// (one validation point — the static UI serving stays tokenless because the
// token is a tool-execution permission, not page visibility). Failed tokens
// are rate-limited per IP; frames are capped at 1 MiB; sessions at 4.
#include <agent/JKAgentClient.h>
#include <agent/JKAgentJson.h>
#include <agent/JKLlmEngine.h>
#include <JKCrashHandler.h>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// ---------------------------------------------------------------------------
// UTF-8 <-> UTF-16 + console output (plain console app — WriteConsoleW so
// hangul survives any codepage; CP949 lesson, docs/48).
// ---------------------------------------------------------------------------
static std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return std::wstring();
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(),
                                      static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()),
                        &w[0], n);
    return w;
}

static void OutW(const std::string& utf8) {
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(out, &mode)) {
        const std::wstring w = Utf8ToWide(utf8);
        DWORD wrote = 0;
        WriteConsoleW(out, w.c_str(), static_cast<DWORD>(w.size()), &wrote,
                      nullptr);
        WriteConsoleW(out, L"\n", 1, &wrote, nullptr);
    } else {
        // Redirected (probe `> log 2>&1` convention): WriteConsoleW fails on
        // file handles — plain UTF-8 bytes so the probe can grep them.
        DWORD wrote = 0;
        WriteFile(out, utf8.data(), static_cast<DWORD>(utf8.size()), &wrote,
                  nullptr);
        WriteFile(out, "\r\n", 2, &wrote, nullptr);
    }
}

// JSON string escape (UTF-8 bytes ≥0x80 pass through — raw UTF-8 is legal
// inside JSON strings, so hangul round-trips untouched).
static std::string JsonEsc(const std::string& s) {
    std::string o;
    o += '"';
    char b[8];
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') {
            o += '\\';
            o += static_cast<char>(c);
        } else if (c == '\n') {
            o += "\\n";
        } else if (c == '\r') {
            o += "\\r";
        } else if (c == '\t') {
            o += "\\t";
        } else if (c < 0x20) {
            std::snprintf(b, sizeof(b), "\\u%04x", c);
            o += b;
        } else {
            o += static_cast<char>(c);
        }
    }
    o += '"';
    return o;
}

static std::string ExeDirA() {
    char path[1024] = {};
    GetModuleFileNameA(nullptr, path, sizeof(path));
    std::string dir = path;
    const size_t slash = dir.find_last_of("\\/");
    if (slash != std::string::npos) dir = dir.substr(0, slash);
    return dir;
}

// ---------------------------------------------------------------------------
// Bridge config: state/jkbridge.json {token, port, bind} — first run
// generates the token (CSPRNG) and prints the bookmarkable URLs. bind is
// optional ("192.168.1.23") — docs/57 §9 백로그: INADDR_ANY는 PC가 붙은 모든
// 인터페이스(사내망 포함)에 노출되므로, 특정 인터페이스로 좁히고 싶을 때
// 지정한다. 미지정 = 기존 INADDR_ANY(토큰이 실질 게이트 — 기존 동작 유지).
// ---------------------------------------------------------------------------
static const int kDefaultPort = 8790;

static uint32_t RandU32() {
    unsigned char b[4] = {};
    BCryptGenRandom(nullptr, b, sizeof(b), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    return (static_cast<uint32_t>(b[0])) | (static_cast<uint32_t>(b[1]) << 8) |
           (static_cast<uint32_t>(b[2]) << 16) |
           (static_cast<uint32_t>(b[3]) << 24);
}

struct BridgeConfig {
    std::string token;  // 32-hex
    int port = kDefaultPort;
    std::string bindIp;  // 빈 문자열 = INADDR_ANY (기존 동작)
};

static std::string GenToken() {
    static const char* hex = "0123456789abcdef";
    std::string t;
    for (int i = 0; i < 32; i++) t += hex[RandU32() & 15];
    return t;
}

static BridgeConfig LoadBridgeConfig() {
    BridgeConfig cfg;
    std::FILE* f =
        std::fopen((ExeDirA() + "\\state\\jkbridge.json").c_str(), "rb");
    if (f) {
        char buf[1024] = {};
        const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
        std::fclose(f);
        buf[n] = '\0';
        jk::agent::AgentJson j(buf);
        std::string v;
        int port = 0;
        if (j.ok() && j.GetStr("token", v) && v.size() >= 16) {
            cfg.token = v;
            if (j.GetInt("port", port) && port > 0 && port < 65536) {
                cfg.port = port;
            }
            std::string bind;
            if (j.GetStr("bind", bind) && !bind.empty()) {
                // fail-closed: 오탈자 bind는 ANY로 폴백하면 의도(축소)가
                // 확장으로 반전된다 — 루프백으로 좁히고 경고 (폰 접속은
                // 끊기지만 콘솔에 원인이 보인다).
                if (inet_addr(bind.c_str()) == INADDR_NONE) {
                    OutW("[!] state\\jkbridge.json의 bind가 유효한 IPv4가 "
                         "아님(" + bind + ") — 루프백으로 좁힌다");
                    cfg.bindIp = "127.0.0.1";
                } else {
                    cfg.bindIp = bind;
                }
            }
            return cfg;
        }
    }
    // Fresh or corrupt: generate and persist (256KiB-class files — no cap
    // dance needed, this file is two fields).
    cfg.token = GenToken();
    const std::string json = "{\"token\":\"" + cfg.token +
                             "\",\"port\":" + std::to_string(cfg.port) + "}";
    f = std::fopen((ExeDirA() + "\\state\\jkbridge.json").c_str(), "wb");
    if (f) {
        std::fwrite(json.data(), 1, json.size(), f);
        std::fclose(f);
    } else {
        // A silently-failing save regenerates the token on every boot and
        // kills every phone bookmark (opus NIT-4).
        OutW("[!] state\\jkbridge.json 쓰기 실패 — 재부트마다 토큰이 재생성됨");
    }
    return cfg;
}

// ---------------------------------------------------------------------------
// SHA-1 + base64 (WebSocket accept key). Hand-rolled — the repo keeps HTTP
// libs out, and the handshake is the only consumer. Self-test at startup
// against RFC 6455's example vector; a wrong accept key bricks every phone
// handshake, so boot refuses to continue on mismatch.
// ---------------------------------------------------------------------------
static void Sha1(const uint8_t* data, size_t len, uint8_t out[20]) {
    uint32_t h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    const uint64_t bitLen = static_cast<uint64_t>(len) * 8;
    // Handshake inputs are short, but keep headroom (256) — the input is
    // caller-controlled and a hostile long key must not walk past the
    // padding loop. (First cut assumed 128 was enough "for short keys";
    // the opus review caught key 93+36 GUID bytes already overflowing.)
    uint8_t buf[256] = {};
    std::memcpy(buf, data, len);  // the message itself (self-test caught a
                                  // version that hashed zeros instead)
    size_t padded = len;
    buf[padded++] = 0x80;
    while (padded % 64 != 56) buf[padded++] = 0;
    for (int i = 0; i < 8; i++) {
        buf[padded++] = static_cast<uint8_t>(bitLen >> (56 - i * 8));
    }
    for (size_t off = 0; off < padded; off += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; i++) {
            w[i] = (static_cast<uint32_t>(buf[off + i * 4]) << 24) |
                   (static_cast<uint32_t>(buf[off + i * 4 + 1]) << 16) |
                   (static_cast<uint32_t>(buf[off + i * 4 + 2]) << 8) |
                   static_cast<uint32_t>(buf[off + i * 4 + 3]);
        }
        for (int i = 16; i < 80; i++) {
            const uint32_t a = w[i - 3], b = w[i - 8], c = w[i - 14],
                           d = w[i - 16];
            w[i] = ((a ^ b ^ c ^ d) << 1) | ((a ^ b ^ c ^ d) >> 31);
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 80; i++) {
            uint32_t f, k;
            if (i < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }
            const uint32_t t = ((a << 5) | (a >> 27)) + f + e + k + w[i];
            // FIPS 180-4 register chain: c takes the ROTATED b (rotl30),
            // b takes the old a. A first cut swapped these two lines and the
            // round-79 state collapsed into pure rotations of h1 (no mixing)
            // — the RFC-vector self-test caught it before any phone connect.
            e = d;
            d = c;
            c = (b << 30) | (b >> 2);
            b = a;
            a = t;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
    }
    for (int i = 0; i < 5; i++) {
        out[i * 4] = static_cast<uint8_t>(h[i] >> 24);
        out[i * 4 + 1] = static_cast<uint8_t>(h[i] >> 16);
        out[i * 4 + 2] = static_cast<uint8_t>(h[i] >> 8);
        out[i * 4 + 3] = static_cast<uint8_t>(h[i]);
    }
}

static std::string B64(const uint8_t* d, size_t n) {
    static const char* t =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string o;
    o.reserve((n + 2) / 3 * 4);
    for (size_t i = 0; i < n; i += 3) {
        const uint32_t v = (static_cast<uint32_t>(d[i]) << 16) |
                           (i + 1 < n ? static_cast<uint32_t>(d[i + 1]) << 8
                                      : 0) |
                           (i + 2 < n ? static_cast<uint32_t>(d[i + 2]) : 0);
        o += t[(v >> 18) & 63];
        o += t[(v >> 12) & 63];
        o += (i + 1 < n) ? t[(v >> 6) & 63] : '=';
        o += (i + 2 < n) ? t[v & 63] : '=';
    }
    return o;
}

// RFC 6455 §1.3 vector — the whole WS security rests on this arithmetic.
static bool Sha1SelfTest() {
    const std::string key = "dGhlIHNhbXBsZSBub25jZQ==";
    uint8_t digest[20];
    const std::string cat = key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    Sha1(reinterpret_cast<const uint8_t*>(cat.data()), cat.size(), digest);
    return B64(digest, 20) == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=";
}

// ---------------------------------------------------------------------------
// WebSocket framing.
// ---------------------------------------------------------------------------
static const size_t kMaxFrame = 1 * 1024 * 1024;

// Exact-size recv — TCP may fragment anywhere; every header read uses this.
static bool RecvAll(SOCKET s, void* buf, size_t n) {
    auto* p = static_cast<char*>(buf);
    size_t got = 0;
    while (got < n) {
        const int r = recv(s, p + got, static_cast<int>(n - got), 0);
        if (r <= 0) return false;
        got += static_cast<size_t>(r);
    }
    return true;
}

static bool WsSendFrame(SOCKET s, const std::string& text) {
    std::string frame;
    frame.reserve(text.size() + 10);
    frame += static_cast<char>(0x81);
    if (text.size() < 126) {
        frame += static_cast<char>(text.size());
    } else if (text.size() <= 0xFFFF) {
        frame += static_cast<char>(126);
        frame += static_cast<char>((text.size() >> 8) & 0xFF);
        frame += static_cast<char>(text.size() & 0xFF);
    } else {
        frame += static_cast<char>(127);
        const uint64_t l = text.size();
        for (int i = 7; i >= 0; i--) {
            frame += static_cast<char>((l >> (i * 8)) & 0xFF);
        }
    }
    frame += text;
    const char* p = frame.data();
    size_t left = frame.size();
    while (left > 0) {
        const int r = send(s, p, static_cast<int>(left), 0);
        if (r <= 0) return false;
        p += r;
        left -= static_cast<size_t>(r);
    }
    return true;
}

// ---------------------------------------------------------------------------
// Failed-token rate limit: 10 per 60 s per IP (stateless-ish — boot-lifetime
// map, pruned on every check; a LAN scanner is the only realistic load).
// ---------------------------------------------------------------------------
struct RateGate {
    std::mutex m;
    std::map<std::string, std::vector<int64_t>> hits;

    bool Allow(const std::string& ip) {
        std::lock_guard<std::mutex> lock(m);
        const int64_t now = static_cast<int64_t>(time(nullptr));
        std::vector<int64_t>& v = hits[ip];
        v.erase(std::remove_if(v.begin(), v.end(),
                               [now](int64_t t) { return now - t > 60; }),
                v.end());
        if (v.size() >= 10) return false;
        v.push_back(now);
        return true;
    }
};

static RateGate g_rate;

// ---------------------------------------------------------------------------
// Embedded web UI (Task 6). Single file, no deps, phone-first. Mirrors the
// jkchat transcript format and — load-bearing for review parity — the
// jkchat approval-strip QUEUE (a busy strip must not overwrite a parked
// earlier request; docs/53 §9 lesson) and the per-kind approval copy
// (MAJOR-1: a generic "close?" strip on any other kind is deceptive).
// ---------------------------------------------------------------------------
static const char kWebUi[] = R"JKUI(<!doctype html>
<html lang="ko"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<title>jkbridge — Agent Chat</title>
<style>
  :root { color-scheme: dark; }
  * { box-sizing: border-box; margin: 0; }
  body { background:#14161a; color:#e8e8ea; font-family:system-ui,'Malgun Gothic',sans-serif;
         height:100dvh; display:flex; flex-direction:column; overflow:hidden; }
  #hdr { padding:6px 10px; font-size:12px; color:#8a8f98; display:flex; justify-content:space-between;
         border-bottom:1px solid #26292f; }
  #log { flex:1; overflow-y:auto; padding:8px 10px; font-size:14px; line-height:1.5;
         word-break:break-word; white-space:pre-wrap; }
  #log .cmd { color:#9ecbff; font-weight:600; }
  #log .llm { color:#e8e8ea; }
  #log .sys { color:#8a8f98; font-size:12px; }
  #log .err { color:#ff8f8f; font-size:12px; }
  #log .ev  { color:#6f7680; font-size:11px; }
  #log .note{ color:#ffd28f; font-size:12px; }
  #appr { display:none; background:#2a2410; border-top:1px solid #4a4218;
          padding:8px 10px; font-size:13px; }
  #aptext { margin-bottom:6px; color:#ffe9a8; }
  #appr button { font-size:15px; padding:8px 22px; margin-right:10px; border-radius:6px;
                 border:1px solid #555; background:#1c1f24; color:#e8e8ea; }
  #appr button:disabled { opacity:.4; }
  #inrow { display:flex; gap:6px; padding:8px; padding-bottom:max(8px,env(safe-area-inset-bottom));
           border-top:1px solid #26292f; }
  #txt { flex:1; font-size:16px; padding:10px 12px; border-radius:8px; border:1px solid #33373d;
         background:#1b1e23; color:#e8e8ea; }
  #send { font-size:15px; padding:8px 18px; border-radius:8px; border:1px solid #3b6ea5;
          background:#274b6d; color:#fff; }
</style></head>
<body>
<div id="hdr"><span>jkbridge</span><span id="stat">연결 중…</span></div>
<div id="log"></div>
<div id="appr"><div id="aptext"></div>
  <div><button id="bAllow">허용</button><button id="bDeny">거부</button></div></div>
<div id="inrow"><input id="txt" autocomplete="off" placeholder="자연어 → LLM / 슬래시 커맨드">
  <button id="send">보내기</button></div>
<script>
const token = new URLSearchParams(location.search).get('token') || '';
const logEl = document.getElementById('log');
const apprEl = document.getElementById('appr');
const apText = document.getElementById('aptext');
const bAllow = document.getElementById('bAllow');
const bDeny = document.getElementById('bDeny');
const txt = document.getElementById('txt');
let ws = null, streamEl = null, streamIdx = -1, streamText = '';
// 진단 타임라인 (docs/57 §12): 서버가 모든 프레임에 ts(유닉스 초)를 싣는다 —
// 폰 대화 기록이 서버 리시트/파일 mtime과 직접 대조 가능(진실원은 서버 시각,
// 기기 시계 아님). 렌더된 모든 줄을 transcript에 적립 — /report 한 번으로
// 진단 대화 전체를 파일로 보낸다(진짜 폰에서 복사가 어려운 문제의 해법).
const transcript = [];
// LLM 마크다운 정리 (2026-09-20 사용자 보고 "답변에 ``` 섞여나와"): 모델이
// 코드펜스/사고 블록/굵게 마커를 원문으로 뱉어 폰(플레인 텍스트)에 아티팩트로
// 찍힌다. XSS 포스트(textContent 전용, innerHTML 금지)를 지키는 최소 정리 —
// 태그 변환 없이 마커만 제거한다. 스트리밍은 매 프레임 누적 원문을 통째로
// 정해 다시 그린다(펜스가 프레임 경계에 걸쳐도 안전).
function clean(s){
  s = String(s||'');
  s = s.replace(/([\s\S]*?)<\/think>/g, '$1');   // 닫힌 사고 블록 제거
  s = s.replace(/<think>[\s\S]*$/, '');          // 스트리밍 중 미닫힌 사고 블록
  s = s.replace(/^```[^\n]*$/gm, '');            // 펜스 마커 줄(언어 태그 포함)
  s = s.replace(/^#{1,6} /gm, '');               // 헤딩 # 마커
  s = s.replace(/\*\*([^*]*)\*\*/g, '$1');       // **굵게** → 텍스트만
  s = s.replace(/`([^`]*)`/g, '$1');             // 인라인 코드 마커
  s = s.replace(/\n{3,}/g, '\n\n');              // 펜스 제거 잔여 빈 줄 정리
  return s;
}
function hhmm(ts){ const d=new Date(ts*1000);
  return ('0'+d.getHours()).slice(-2)+':'+('0'+d.getMinutes()).slice(-2); }
// claude session continuity: the phone owns it (localStorage) — a reconnect
// resumes the engine, not the transcript (spec §5).
let claudeSession = localStorage.getItem('jkbridge_session') || '';
// approval-strip QUEUE (jkchat EnqueueApproval port): one strip, FIFO,
// re-armed by approval_resolved — a later ask must not overwrite a parked one.
let apprFront = null; const apprQueue = [];
let pendingUndo = null; // /close,/restore → save_layout pre_undo → act
// XSS posture: every server-sourced string reaches the DOM via textContent
// (add()/showFront()/route()/onEvent) — no innerHTML interpolation anywhere.
function cut(s,n){ s=String(s||''); return s.length>n ? s.slice(0,n) : s; }
// add: ts(유닉스 초, 서버 프레임에서 온 값) 있으면 그 시각, 없으면 기기 시각 —
// 모든 줄에 [HH:MM] 접두. client 생성 라인도 표기 대상이라 기본 now.
function add(text, cls, ts){ const t = ts || Date.now()/1000;
  const line = '['+hhmm(t)+'] '+text; transcript.push(line);
  const d=document.createElement('div'); d.className=cls||'';
  d.textContent=line; logEl.appendChild(d); logEl.scrollTop=logEl.scrollHeight; return d; }

function connect() {
  document.getElementById('stat').textContent='연결 중…';
  ws = new WebSocket((location.protocol==='https:'?'wss://':'ws://')+location.host+'/ws?token='+encodeURIComponent(token));
  ws.onopen = () => { document.getElementById('stat').textContent='연결됨';
    ws.send(JSON.stringify({type:'hello', resume_session:claudeSession})); };
  ws.onclose = () => { document.getElementById('stat').textContent='끊김 — 재접속…';
    streamEl = null; setTimeout(connect, 2000); };
  ws.onmessage = (m) => { let msg; try { msg = JSON.parse(m.data); } catch { return; }
    const j = (typeof msg.json === 'string') ? (()=>{try{return JSON.parse(msg.json)}catch{return null}})() : msg.json;
    route(msg, j); };
}
function route(msg, j) {
  if (msg.type === 'hello') {
    if (msg.pending_result) add('[복구] '+clean(msg.pending_result), 'note', msg.pending_ts);
    if (msg.busy) add('[LLM 실행 중 — 이전 턴 진행]', 'sys', msg.ts);
    return;
  }
  if (msg.type === 'stream') {
    if (!streamEl) { streamText = ''; streamEl = add('[LLM] ', 'llm', msg.ts);
      streamIdx = transcript.length - 1; }
    streamText += msg.text;
    const shown = clean(streamText);
    streamEl.textContent = '[LLM] ' + shown;
    transcript[streamIdx] = '['+hhmm(msg.ts || Date.now()/1000)+'] [LLM] '+shown;
    logEl.scrollTop = logEl.scrollHeight;
    return;
  }
  if (msg.type === 'chat_queued') { add('[대기열] 이전 턴이 끝나면 이어서 실행해요', 'sys', msg.ts); return; }
  if (msg.type === 'chat_queued_start') { add('[대기열] 대기 중이던 메시지 실행 시작', 'sys', msg.ts); return; }
  if (msg.type === 'chat_done') {
    if (streamEl) streamEl = null;
    if (msg.session_id) { claudeSession = msg.session_id; localStorage.setItem('jkbridge_session', msg.session_id); }
    if (msg.ok && !msg.streamed) add(clean(msg.result) || '(빈 응답)', 'llm', msg.ts);
    else if (!msg.ok) add('[!] LLM 응답 실패 — ' + (msg.result||''), 'err', msg.ts);
    else add('[LLM 완료]', 'sys', msg.ts);
    return;
  }
  if (msg.type === 'reply') { add('['+(msg.label||'?')+'] '+(j?JSON.stringify(j):'?'), 'sys', msg.ts); onReply(msg.label, j); return; }
  if (msg.type === 'event') { onEvent(msg.topic, j, msg.ts); return; }
  if (msg.type === 'error') { add('[!] '+(msg.text||''), 'err', msg.ts); return; }
}

// ---- approval strip (jkchat 문구 + 큐 순환 이식) ---------------------------
function approvalText(e) {
  const k = e.kind;
  if (k === 'trust_request')  return '[신뢰 요청] '+e.name+' ('+e.origin+') 해시 '+cut(e.fingerprint,15)+'… 승인할까요?';
  if (k === 'permission_set') return '[권한 변경] '+e.target_tool+' → '+e.decision+' 승인할까요?';
  if (k === 'capture_allow')  return '[캡처 허용] capture_window/region → '+e.decision+' 승인할까요?';
  if (k === 'trust_revoke')   return '[신뢰 해지] '+e.name+' ('+cut(e.fingerprint,15)+'…) 승인할까요?';
  if (k === 'files_access')   return '[파일 요청] '+e.tool+' → '+e.title+' 승인할까요?';
  // 앱 도구 허브 (스펙 2026-09-19-app-tool-hub §5 2단, 태스크 9 리뷰 M-4):
  // app_tool 승인이 close_window 재용 문구("창을 닫을까요?")로 나오는 기만
  // 픽스 — jkchat과 동일하게 대상 창을 정직하게 표기한다. target 있으면
  // "[<app> 창 #<windowId>] <tool> 실행할까요?", 부재(구버전 서버)는 name
  // 문구 하위호환. close_window(마지막 폴백) 문구는 그대로 유지.
  if (k === 'app_tool') {
    const t = e.target || {};
    if (t.windowId > 0 && t.app && t.tool)
      return '['+t.app+' 창 #'+t.windowId+'] '+t.tool+' 실행할까요?';
    return '[앱 도구] '+(e.name||'')+' 실행할까요?';
  }
  return '['+(e.title||'')+(' #'+(e.target_id||'?'))+'] 창을 닫을까요?';
}
function showFront() {
  if (!apprFront) { apprEl.style.display='none'; return; }
  apText.textContent = apprFront.text;
  apprEl.style.display = 'block';
  bAllow.disabled = bDeny.disabled = false;
}
function decide(d) {
  if (!apprFront) return;
  sendTool('approve', {request: apprFront.request, decision: d}, 'approve '+d);
  bAllow.disabled = bDeny.disabled = true; // one decision per request — re-arms on resolved
}
bAllow.onclick = () => decide('allow');
bDeny.onclick  = () => decide('deny');
function onEvent(topic, e, ts) {
  if (!e) { add('['+topic+']', 'ev', ts); return; }
  if (topic === 'agent.approval_request') {
    add('[승인 요청] ' + approvalText(e), 'note', ts);
    if (!apprFront) { apprFront = {request: e.request, text: approvalText(e)}; showFront(); }
    else { apprQueue.push({request: e.request, text: approvalText(e)});
           add('[대기] 승인 요청 #'+e.request+' — 현재 승인 처리 후 표시', 'sys', ts); }
    return;
  }
  if (topic === 'agent.approval_resolved') {
    add('[승인] request '+e.request+' → '+e.decision, 'sys', ts);
    if (apprFront && apprFront.request === e.request) {
      apprFront = apprQueue.shift() || null; showFront();
    } else { // another surface resolved a queued item
      const i = apprQueue.findIndex(a => a.request === e.request);
      if (i >= 0) apprQueue.splice(i, 1);
    }
    return;
  }
  if (topic === 'agent.notify') { add('[알림] '+((e.data||{}).title||'(무제)')+
      ((e.data||{}).body ? ' — '+e.data.body : ''), 'note', ts); return; }
  add('['+topic+'] '+(j?JSON.stringify(e):''), 'ev', ts);
}

// ---- slash commands (jkchat Submit 매핑 이식) -------------------------------
function sendTool(tool, args, label) {
  if (!ws || ws.readyState !== 1) { add('[!] 연결 안 됨', 'err'); return; }
  ws.send(JSON.stringify({type:'tool', tool, args, label}));
}
function onReply(label, j) {
  // /close,/restore 2단 체인: pre_undo 스냅샷 성공에만 파괴적 커맨드 발사.
  if (label === 'save pre_undo' && pendingUndo) {
    const p = pendingUndo; pendingUndo = null;
    if (j && j.ok === true) sendTool(p.tool, p.args, p.label);
    else add('[!] 스냅샷 실패 — 파괴적 커맨드 취소', 'err');
  }
}
function submit() {
  const line = txt.value.trim(); if (!line) return;
  txt.value = '';
  add('> '+line, 'cmd');
  if (line[0] !== '/') {
    if (!ws || ws.readyState !== 1) { add('[!] 연결 안 됨', 'err'); return; }
    ws.send(JSON.stringify({type:'chat', text:line})); return;
  }
  const sp = line.indexOf(' ');
  const cmd = sp<0 ? line.slice(1) : line.slice(1, sp);
  const arg = sp<0 ? '' : line.slice(sp+1);
  const need = () => add('사용법: /'+cmd+' <app|id|name>', 'sys');
  if (cmd==='help') {
    add('자연어 → LLM(claude 헤드리스) 위임 / 슬래시: 결정적 커맨드','sys');
    add('/list /launch <app> /close <id> /chat /notify /shot /triggers /trust','sys');
    add('/trigger <name> on|off /events /theme dark|light|classic /save <name> /restore <name>','sys');
    add('/undo /new /report — 대화 기록을 파일로 저장(PC 진단용)','sys');
  }
  else if (cmd==='new') { claudeSession=''; localStorage.removeItem('jkbridge_session'); add('새 LLM 세션','sys');
    if (ws && ws.readyState===1) ws.send(JSON.stringify({type:'hello', resume_session:''})); }
  else if (cmd==='list') sendTool('list_windows', {}, 'list');
  else if (cmd==='launch') { if(!arg){need();return;} sendTool('launch_app', {app:arg}, 'launch '+arg); }
  else if (cmd==='theme') { if(!['dark','light','classic'].includes(arg)){add('사용법: /theme dark|light|classic','sys');return;}
    sendTool('theme_set', {preset:arg}, 'theme '+arg); }
  else if (cmd==='close') { if(!arg){need();return;} pendingUndo={tool:'close_window',args:{id:Number(arg)},label:'close '+arg};
    sendTool('save_layout', {name:'pre_undo'}, 'save pre_undo'); }
  else if (cmd==='save') { if(!arg){need();return;} sendTool('save_layout', {name:arg}, 'save '+arg); }
  else if (cmd==='restore') { if(!arg){need();return;} pendingUndo={tool:'restore_layout',args:{name:arg},label:'restore '+arg};
    sendTool('save_layout', {name:'pre_undo'}, 'save pre_undo'); }
  else if (cmd==='undo') sendTool('restore_layout', {name:'pre_undo'}, 'undo');
  else if (cmd==='report') {
    // 대화 전체(타임스탬프 포함)를 서버로 — state\bridge_report_*.txt로
    // 기록되고 agent.notify가 뜬다. 최신 256KiB만(스트림 긴 턴 대비).
    if (!ws || ws.readyState !== 1) { add('[!] 연결 안 됨', 'err'); return; }
    let body = transcript.join('\n');
    if (body.length > 256*1024) body = '…(이하 생략 아님, 앞부분 절단)\n' + body.slice(body.length - 256*1024);
    ws.send(JSON.stringify({type:'report', text:body}));
  }
  else if (cmd==='notify') sendTool('open_notify', {}, 'notify');
  else if (cmd==='shot') sendTool('launch_app', {app:'shot'}, 'shot');
  else if (cmd==='triggers') sendTool('trigger_list', {}, 'triggers');
  else if (cmd==='events') sendTool('events_list', {}, 'events');
  else if (cmd==='trust') sendTool('trust_list', {}, 'trust');
  else if (cmd==='chat') sendTool('launch_chat', {}, 'chat');
  else if (cmd==='trigger') { const sp2=arg.indexOf(' '); if(sp2<0){add('사용법: /trigger <name> on|off','sys');return;}
    const n=arg.slice(0,sp2), mode=arg.slice(sp2+1);
    if(!n||n.includes('"')||(mode!=='on'&&mode!=='off')){add('사용법: /trigger <name> on|off','sys');return;}
    sendTool('trigger_toggle', {name:n, on:mode==='on'}, 'trigger '+n+' '+mode); }
  else add('알 수 없는 커맨드 — /help 참고', 'sys');
}
document.getElementById('send').onclick = submit;
txt.addEventListener('keydown', (e) => { if (e.key === 'Enter') submit(); });
if (!token) add('[!] 토큰 없음 — URL의 ?token= 필요', 'err');
connect();
</script></body></html>
)JKUI";

// ---------------------------------------------------------------------------
// Bridge session: one authenticated WS ↔ one JKAgentClient + one JKLlmEngine.
// Threads: the HTTP thread becomes the WS read loop (frame dispatcher); the
// pump thread mirrors jkchat's 400 ms ping pump (replies + events out).
// ---------------------------------------------------------------------------
struct BridgeSession {
    explicit BridgeSession(SOCKET sock)
        : sock_(sock),
          lastPong_(static_cast<int64_t>(time(nullptr))) {}

    // 대기열 (docs/59 유보 ③, 2026-09-20): 턴 실행 중 도착한 chat을 최대
    // kMaxPendingTurns개 적립 — 순서가 되면 OnLlmDone이 다음 턴을 시작한다.
    // 상한 초과분만 busy로 거부(무경합 플러드 방지 백로그 레슨 선례).
    static constexpr size_t kMaxPendingTurns = 3;
    std::mutex queueMtx_;
    std::vector<std::string> pendingTurns_;
    std::atomic<int64_t> lastReport_{0};  // /report 쿨다운 스탬프 (10초)

    // The dispatcher (Run) owns the lifecycle. Returns when the WS dies.
    void Run();

    bool SendText(const std::string& json) {
        if (!alive_.load()) return false;
        // 서버 스탬프 (docs/57 §12): 모든 프레임에 유닉스 초를 중앙 주입 —
        // 폰 대화 기록이 서버 리시트/파일 mtime과 직접 대조 가능하고, 재접속
        // 복구 라인에도 시각이 산다. 브라우저 시계는 기기마다 틀릴 수 있으니
        // 진실원은 서버가 된다. SendText 한 곳이면 미래 프레임도 자동 커버.
        std::string stamped;
        if (json.size() > 1) {
            stamped.reserve(json.size() + 16);
            stamped += "{\"ts\":";
            stamped += std::to_string(static_cast<long long>(time(nullptr)));
            stamped += ',';             // 쉼표 없이 이어붙이면 모든 프레임이
            stamped += json.substr(1);  // JSON 파산 (2026-09-20 라이브 실측 —
                                        // 폰 JSON.parse 전멸, "브리지가 대답을
                                        // 안 한다"로 보인 근본 원인)
        } else {
            stamped = json;
        }
        std::lock_guard<std::mutex> lock(wsMtx_);
        if (!WsSendFrame(sock_, stamped)) {
            Shutdown();
            return false;
        }
        return true;
    }

    // WS-level control frame under wsMtx_ — a naked send here would interleave
    // into another thread's frame (opus MINOR-4③).
    void SendPong(const std::string& payload) {
        if (!alive_.load()) return;
        std::lock_guard<std::mutex> lock(wsMtx_);
        std::string frame;
        frame.reserve(payload.size() + 10);
        frame += static_cast<char>(0x8A);  // pong, no mask (server frames)
        if (payload.size() < 126) {
            frame += static_cast<char>(payload.size());
        } else {
            return;  // RFC caps control payloads at 125 — refuse, don't emit
        }
        frame += payload;
        const int r = send(sock_, frame.data(),
                           static_cast<int>(frame.size()), 0);
        if (r <= 0) Shutdown();
    }

    void TouchPong() { lastPong_.store(static_cast<int64_t>(time(nullptr))); }

    // WS-level ping (empty, unmasked) — browsers answer at protocol level;
    // the pong keeps the dispatcher's recv fed and the stamp fresh.
    void SendWsPing() {
        if (!alive_.load()) return;
        std::lock_guard<std::mutex> lock(wsMtx_);
        const unsigned char ping[2] = {0x89, 0x00};
        const int r = send(sock_, reinterpret_cast<const char*>(ping), 2, 0);
        if (r <= 0) Shutdown();
    }

    bool PongStale() const {
        return static_cast<int64_t>(time(nullptr)) -
                   lastPong_.load() >
               45;
    }

    void Shutdown() {
        // shutdown() only here — closesocket in the destructor. A first cut
        // closed the socket immediately while other threads still held it;
        // the accept loop can then hand the same handle value to a new
        // session and stale sends write into its stream (opus MINOR-6).
        if (alive_.exchange(false)) {
            shutdown(sock_, SD_BOTH);
        }
    }

    ~BridgeSession() {
        if (alive_.exchange(false)) {
            shutdown(sock_, SD_BOTH);
        }
        if (sock_ != INVALID_SOCKET) {
            closesocket(sock_);
        }
    }

    bool Alive() const { return alive_.load(); }

    SOCKET sock_;
    std::mutex wsMtx_;       // serializes frame writes (3+ send() calls each)
    std::atomic<bool> alive_{true};
    std::atomic<int64_t> lastPong_;  // heartbeat: pump's WS pings / pongs back

    jk::agent::JKAgentClient agent_;
    std::mutex agentMtx_;    // pipe IO: dispatcher SendQuery vs pump Query
    // Approval-only SECOND control connection (final review Important 2):
    // the phone's tools/call relay and its approve both rode the session
    // connection — so an ask-gated app_tool (run_console_app / files_access
    // too) parked with THIS connection as requester, and the server's
    // self-approve gate (kind != close_window → self_approve, docs/31 §3)
    // made the phone's Allow tab dead-on-arrival. jkchat's cross-approve
    // precedent: route ONLY the approve query through a dedicated
    // Hello+subscribe=0 control connection so the server sees a different
    // requester. Never subscribed to events, never used by the pump.
    jk::agent::JKAgentClient approve_;
    std::mutex approveMtx_;
    std::mutex labelsMtx_;
    std::map<uint32_t, std::string> labels_;  // queryId → transcript label

    std::thread pump_;
    jk::agent::JKLlmEngine engine_;           // one turn at a time
    std::mutex resumeMtx_;                    // LLM worker ↔ dispatcher
    std::string resumeSession_;               // consumer-owned claude session
};

// Reads one frame. Continuation frames are refused (our protocol is
// strictly one-JSON-per-frame — a phone that fragments is misbehaving).
// Ping/pong are consumed (header, mask and payload) here — a first cut
// returned before consuming the mask, leaving stream desync — and pong
// refreshes the session's heartbeat stamp. Loop, not recursion: a ping
// flood recursed to stack death.
// Returns: 1 = got text payload, 0 = clean close, -1 = protocol error / I/O.
static int WsReadFrame(BridgeSession* s, std::string& out) {
    for (;;) {
        unsigned char hdr[2];
        if (!RecvAll(s->sock_, hdr, 2)) return -1;
        const unsigned char opcode = hdr[0] & 0x0F;
        const bool masked = (hdr[1] & 0x80) != 0;
        uint64_t len = hdr[1] & 0x7F;
        if (len == 126) {
            unsigned char ext[2] = {};
            if (!RecvAll(s->sock_, ext, 2)) return -1;
            len = (static_cast<uint64_t>(ext[0]) << 8) | ext[1];
        } else if (len == 127) {
            unsigned char ext[8] = {};
            if (!RecvAll(s->sock_, ext, 8)) return -1;
            len = 0;
            for (int i = 0; i < 8; i++) {
                len = (len << 8) | ext[i];
            }
        }
        if (opcode == 0x8) return 0;  // close
        if (opcode == 0x9 || opcode == 0xA) {
            // Control frame: consume mask + payload fully (RFC cap: 125).
            if (len > 125) return -1;
            unsigned char mask[4] = {};
            if (masked && !RecvAll(s->sock_, mask, 4)) return -1;
            std::string payload(static_cast<size_t>(len), '\0');
            if (len > 0 && !RecvAll(s->sock_, &payload[0],
                                    static_cast<size_t>(len))) {
                return -1;
            }
            if (masked) {
                for (size_t i = 0; i < payload.size(); i++) {
                    payload[i] = static_cast<char>(payload[i] ^ mask[i % 4]);
                }
            }
            if (opcode == 0xA) {
                s->TouchPong();  // heartbeat proof of life
            } else {
                s->SendPong(payload);  // ping → masked-echo pong
            }
            continue;
        }
        if (opcode != 0x1) return -1;  // binary / continuation — refused
        if (!masked) return -1;        // RFC: clients MUST mask
        if (len > kMaxFrame) return -1;
        unsigned char mask[4];
        if (!RecvAll(s->sock_, mask, 4)) return -1;
        out.resize(static_cast<size_t>(len));
        if (len > 0 && !RecvAll(s->sock_, &out[0], out.size())) return -1;
        for (size_t i = 0; i < out.size(); i++) {
            out[i] = static_cast<char>(out[i] ^ mask[i % 4]);
        }
        return 1;
    }
}

// Server frames are never masked. Sends must be serialized by the session's
// mutex (frame = up to 3 send() calls).
// claude session continuity across phone reconnects (spec §6): the last
// chat_done per claude session id, 5-minute window, 4 entries max — keyed by
// the id the phone will resume with.
struct DoneMemo {
    std::mutex m;
    std::map<std::string, std::pair<int64_t, std::string>> bySession;
    void Put(const std::string& sessionId, const std::string& result) {
        if (sessionId.empty()) return;
        std::lock_guard<std::mutex> lock(m);
        const int64_t now = static_cast<int64_t>(time(nullptr));
        bySession[sessionId] = {now, result};
        while (bySession.size() > 4) {
            auto oldest = bySession.begin();
            for (auto it = bySession.begin(); it != bySession.end(); ++it) {
                if (it->second.first < oldest->second.first) oldest = it;
            }
            bySession.erase(oldest);
        }
    }
    std::pair<int64_t, std::string> Get(const std::string& sessionId) {
        // 반환에 기록 시각을 포함한다 — hello의 pending_ts로 폰이 "[복구]"
        // 라인에 턴 완료 시각을 찍는다(재접속 후에도 타임라인 보존).
        if (sessionId.empty()) return {0, ""};
        std::lock_guard<std::mutex> lock(m);
        const auto it = bySession.find(sessionId);
        if (it == bySession.end()) return {0, ""};
        if (static_cast<int64_t>(time(nullptr)) - it->second.first > 300) {
            bySession.erase(it);
            return {0, ""};
        }
        return it->second;
    }
};
static DoneMemo g_doneMemo;

// LLM callbacks fire on the JKLlmEngine worker thread. The turn job carries
// a shared_ptr so a dying session cannot strand the turn: the session (and
// its engine member) lives until the done callback lands.
static void OnLlmDelta(const std::string& utf8, void* user) {
    auto* keep = static_cast<std::shared_ptr<BridgeSession>*>(user);
    (*keep)->SendText("{\"type\":\"stream\",\"text\":" +
                      JsonEsc(utf8) + "}");
}

static void OnLlmDone(jk::agent::LlmTurnResult&& r, void* user) {
    auto* keep = static_cast<std::shared_ptr<BridgeSession>*>(user);
    if (!r.sessionId.empty()) {
        std::lock_guard<std::mutex> lock((*keep)->resumeMtx_);
        (*keep)->resumeSession_ = r.sessionId;
    }
    std::string frame = "{\"type\":\"chat_done\",\"ok\":";
    frame += r.ok ? "1" : "0";
    frame += ",\"streamed\":";
    frame += r.streamed ? "1" : "0";
    frame += ",\"result\":" + JsonEsc(r.result);
    frame += ",\"session_id\":" + JsonEsc(r.sessionId) + "}";
    (*keep)->SendText(frame);
    // The reconnect memo: what a phone that dropped mid-turn should see.
    if (r.ok) {
        g_doneMemo.Put(r.sessionId, r.result);
    } else {
        g_doneMemo.Put(r.sessionId, "[!] LLM 실패 — " + r.result);
    }
    // 대기열 드레인 (docs/59 유보 ③): 다음 적립 턴을 즉시 시작한다 — busy_는
    // DoneFn 전에 해제돼 있다(engine Finish 관례)라 StartTurn은 성공. 같은
    // keep(세션 소유권)을 다음 턴의 DoneFn으로 넘기므로 여기서 delete하지
    // 않는다. 극히 드문 실패(재진입 경합)면 맨 앞에 되돌려 순서를 지킨다.
    std::string next;
    {
        std::lock_guard<std::mutex> lock((*keep)->queueMtx_);
        if (!(*keep)->pendingTurns_.empty()) {
            next = std::move((*keep)->pendingTurns_.front());
            (*keep)->pendingTurns_.erase((*keep)->pendingTurns_.begin());
        }
    }
    if (!next.empty()) {
        std::string resume;
        {
            std::lock_guard<std::mutex> lock((*keep)->resumeMtx_);
            resume = (*keep)->resumeSession_;
        }
        (*keep)->SendText("{\"type\":\"chat_queued_start\"}");
        if (!(*keep)->engine_.StartTurn(next, resume, OnLlmDelta,
                                        OnLlmDone, keep)) {
            std::lock_guard<std::mutex> lock((*keep)->queueMtx_);
            (*keep)->pendingTurns_.insert(
                (*keep)->pendingTurns_.begin(), next);
            delete keep;
        }
        return;
    }
    delete keep;
}

// The pump: a ping round-trip flushes parked replies (jkchat Pump pattern),
// then drain replies and events out over the WS. Blocking ping is a few ms
// — acceptable on the pump thread. EVERY agent touch sits inside agentMtx_ —
// a first cut left the reconnect path bare, racing the dispatcher's
// SendQuery on the unsynchronized JKAgentClient (opus MAJOR-2). The pump
// also heartbeats the phone at the WS level every ~5 s (browsers pong
// automatically): without it a suspended phone wedges the dispatcher's recv
// forever and burns a session slot (opus MAJOR-3).
static void PumpLoop(BridgeSession* s) {
    std::string json;
    int cycles = 0;
    while (s->Alive()) {
        bool connected = false;
        {
            std::lock_guard<std::mutex> lock(s->agentMtx_);
            if (!s->agent_.IsConnected()) {
                if (s->agent_.Connect()) {
                    s->agent_.SubscribeEvents(true);
                }
            }
            connected = s->agent_.IsConnected();
        }
        if (s->Alive() && !connected) {
            Sleep(400);
            continue;
        }
        bool dead = false;
        {
            std::lock_guard<std::mutex> lock(s->agentMtx_);
            if (++cycles % 12 == 0) {
                // WS heartbeat (~every 4.8 s). A phone that stops ponging
                // for 45 s is gone for good — free its slot.
                if (s->PongStale()) {
                    s->SendText(
                        "{\"type\":\"error\",\"text\":\"heartbeat timeout\"}");
                    s->Shutdown();
                    break;
                }
                s->SendWsPing();
            }
            std::string pong;
            s->agent_.Query("ping", "{}", pong);
            if (!s->agent_.IsConnected()) {
                dead = true;
            } else {
                // queryId → label mapping: dispatcher adds, pump consumes.
                std::lock_guard<std::mutex> labelLock(s->labelsMtx_);
                for (auto it = s->labels_.begin(); it != s->labels_.end();) {
                    if (s->agent_.PollReply(it->first, json)) {
                        if (!s->SendText("{\"type\":\"reply\",\"label\":" +
                                         JsonEsc(it->second) + ",\"json\":" +
                                         json + "}")) {
                            dead = true;
                            break;
                        }
                        it = s->labels_.erase(it);
                    } else {
                        ++it;
                    }
                }
                if (!dead) {
                    std::vector<jk::agent::AgentEvent> events;
                    s->agent_.PollEvents(events);
                    for (const auto& ev : events) {
                        if (!s->SendText("{\"type\":\"event\",\"topic\":" +
                                         JsonEsc(ev.topic) + ",\"json\":" +
                                         ev.json + "}")) {
                            dead = true;
                            break;
                        }
                    }
                }
            }
        }
        if (dead) {
            s->SendText(
                "{\"type\":\"error\",\"text\":\"server connection lost\"}");
            Sleep(400);
            continue;
        }
        Sleep(400);
    }
}

// The frame dispatcher — runs on the HTTP thread after the WS handshake.
static void SessionRun(std::shared_ptr<BridgeSession> s) {
    s->pump_ = std::thread(PumpLoop, s.get());
    std::string line;
    for (;;) {
        const int r = WsReadFrame(s.get(), line);
        if (r != 1) {
            s->Shutdown();
            break;
        }
        jk::agent::AgentJson f(line);
        std::string type;
        if (!f.ok() || !f.GetStr("type", type)) {
            s->SendText("{\"type\":\"error\",\"text\":\"bad frame\"}");
            continue;
        }
        if (type == "hello") {
            std::string resume;
            f.GetStr("resume_session", resume);
            std::string resumeAtHello;
            {
                // resumeSession_ is written by the LLM worker thread (opus
                // MINOR-1) — never read or written without the mutex.
                std::lock_guard<std::mutex> lock(s->resumeMtx_);
                if (!resume.empty()) s->resumeSession_ = resume;
                resumeAtHello = s->resumeSession_;
            }
            std::string frame =
                "{\"type\":\"hello\",\"ok\":1,\"busy\":" +
                std::string(s->engine_.Busy() ? "1" : "0");
            const auto memo = g_doneMemo.Get(resumeAtHello);
            if (!memo.second.empty()) {
                frame += ",\"pending_result\":" + JsonEsc(memo.second);
                frame += ",\"pending_ts\":" + std::to_string(memo.first);
            }
            frame += "}";
            s->SendText(frame);
        } else if (type == "chat") {
            std::string text;
            if (!f.GetStr("text", text) || text.empty()) {
                s->SendText("{\"type\":\"error\",\"text\":\"empty text\"}");
                continue;
            }
            std::string resumeAtChat;
            {
                std::lock_guard<std::mutex> lock(s->resumeMtx_);
                resumeAtChat = s->resumeSession_;
            }
            auto* keep = new std::shared_ptr<BridgeSession>(s);
            if (!s->engine_.StartTurn(text, resumeAtChat, OnLlmDelta,
                                      OnLlmDone, keep)) {
                // 대기열 (docs/59 유보 ③): 턴 실행 중 도착 메시지는 적립 —
                // 순서가 되면 OnLlmDone이 시작한다. 상한 초과분만 busy 거부.
                bool queued = false;
                {
                    std::lock_guard<std::mutex> lock(s->queueMtx_);
                    if (s->pendingTurns_.size() <
                        BridgeSession::kMaxPendingTurns) {
                        s->pendingTurns_.push_back(text);
                        queued = true;
                    }
                }
                if (queued) {
                    s->SendText("{\"type\":\"chat_queued\"}");
                } else {
                    delete keep;
                    s->SendText("{\"type\":\"error\",\"text\":\"busy\"}");
                }
            }
        } else if (type == "tool") {
            std::string tool, args, label;
            if (!f.GetStr("tool", tool) || tool.empty() ||
                tool.find('"') != std::string::npos) {
                s->SendText("{\"type\":\"error\",\"text\":\"bad tool\"}");
                continue;
            }
            f.GetStr("label", label);
            if (!f.GetRaw("args", args)) args = "{}";
            // Same request shape agentctl sends ({"tool","args"}).
            const uint32_t id = [&] {
                std::lock_guard<std::mutex> lock(s->agentMtx_);
                if (!s->agent_.IsConnected() &&
                    !s->agent_.Connect()) {
                    return 0u;
                }
                return s->agent_.SendQuery(tool, args);
            }();
            if (id == 0) {
                s->SendText(
                    "{\"type\":\"error\",\"text\":\"server not connected\"}");
                continue;
            }
            std::lock_guard<std::mutex> lock(s->labelsMtx_);
            if (s->labels_.size() >= 128) {
                // Bounded: a server that never replies must not grow the map
                // forever (each entry can carry a 1 MiB label). Crude
                // eviction is fine — a live phone has a handful of entries.
                s->labels_.clear();
            }
            s->labels_[id] = label.empty() ? tool : label;
        } else if (type == "approve") {
            int request = 0;
            std::string decision;
            if (!f.GetInt("request", request) || request <= 0 ||
                !f.GetStr("decision", decision) ||
                (decision != "allow" && decision != "deny")) {
                s->SendText(
                    "{\"type\":\"error\",\"text\":\"bad approve\"}");
                continue;
            }
            // Blocking Query on the approval connection is safe: the server
            // answers the APPROVER's approve query immediately in every path
            // (only the original requester's reply is parked/deferred) — so
            // this never waits on the ask itself. Failure surfaces as an
            // explicit error frame, same fail-closed shape as before.
            std::string reply;
            bool sent = false;
            {
                std::lock_guard<std::mutex> lock(s->approveMtx_);
                if (s->approve_.IsConnected() || s->approve_.Connect()) {
                    sent = s->approve_.Query(
                        "approve",
                        "{\"request\":" + std::to_string(request) +
                            ",\"decision\":\"" + decision + "\"}",
                        reply);
                }
            }
            if (!sent) {
                s->SendText("{\"type\":\"error\",\"text\":"
                            "\"approval connection failed\"}");
            } else {
                s->SendText("{\"type\":\"reply\",\"label\":\"approve\","
                            "\"json\":" + reply + "}");
            }
        } else if (type == "report") {
            // 폰 버그 리포트 (docs/57 §12): 대화 기록(타임스탬프 포함)을 폰이
            // 모아 보내면 state\bridge_report_<시각>.txt로 기록하고
            // agent.notify로 데스크탑에 알린다 — 진짜 폰에서 대화 복사가
            // 어려운 문제(사용자 피드백)의 직격 해법. PC Claude 세션은 파일을
            // 읽어 진단한다. 쿨다운 10초(플러드 레슨 — 사용자 행동이어도
            // 경계는 기계가 든다), 본문 상한 256KiB.
            std::string text;
            if (!f.GetStr("text", text) || text.empty() ||
                text.size() > 256 * 1024) {
                s->SendText(
                    "{\"type\":\"error\",\"text\":\"bad report\"}");
                continue;
            }
            const int64_t now = static_cast<int64_t>(time(nullptr));
            const int64_t prev = s->lastReport_.load();
            if (prev != 0 && now - prev < 10) {
                s->SendText(
                    "{\"type\":\"error\",\"text\":\"report_cooldown\"}");
                continue;
            }
            char stamp[32];
            std::tm tmb{};
            localtime_s(&tmb, &now);
            std::strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", &tmb);
            const std::string path = ExeDirA() + "\\state\\bridge_report_" +
                                     stamp + ".txt";
            std::FILE* fp = std::fopen(path.c_str(), "wb");
            bool wrote = false;
            if (fp) {
                wrote = std::fwrite(text.data(), 1, text.size(), fp) ==
                            text.size() &&
                        std::fclose(fp) == 0;
            }
            if (!wrote) {
                s->SendText(
                    "{\"type\":\"error\",\"text\":\"report write failed\"}");
                continue;
            }
            s->lastReport_.store(now);
            s->SendText("{\"type\":\"reply\",\"label\":\"report\","
                        "\"json\":{\"ok\":true,\"path\":" +
                        JsonEsc(path) + "}}");
            // notify는 결과와 무관하게 화재 — 실패는 조용히(리포트는 이미
            // 파일로 남아 다음 PC 세션이 찾을 수 있다).
            const uint32_t nid = [&] {
                std::lock_guard<std::mutex> lock(s->agentMtx_);
                if (!s->agent_.IsConnected() && !s->agent_.Connect()) {
                    return 0u;
                }
                return s->agent_.SendQuery(
                    "publish_event",
                    "{\"topic\":\"agent.notify\",\"data\":{\"title\":\"폰 "
                    "리포트 도착\",\"body\":\"" +
                        JsonEsc(stamp) + "\"}}");
            }();
            if (nid != 0) {
                std::lock_guard<std::mutex> lock(s->labelsMtx_);
                if (s->labels_.size() >= 128) s->labels_.clear();
                s->labels_[nid] = "notify report";
            }
        } else {
            s->SendText("{\"type\":\"error\",\"text\":\"unknown type\"}");
        }
    }
    s->pump_.join();
}

// ---------------------------------------------------------------------------
// HTTP + WS handshake.
// ---------------------------------------------------------------------------
static std::atomic<int> g_activeSessions{0};
static const int kMaxSessions = 4;

// Reads the request head (up to \r\n\r\n, 8 KiB cap). One recv per byte —
// slow (heads are a few hundred bytes, once per connection) but exact: a
// chunked read could swallow bytes past the terminator, and an unterminated
// 8 KiB head must be a failure, not a valid request (slowloris is killed by
// the socket read timeout, not here).
static bool ReadHttpHead(SOCKET s, std::string& head) {
    char c;
    while (head.size() < 8192) {
        const int r = recv(s, &c, 1, 0);
        if (r <= 0) return false;
        head += c;
        if (head.size() >= 4 && head.compare(head.size() - 4, 4, "\r\n\r\n") == 0) {
            return true;
        }
    }
    return false;
}

// Constant-shape token compare — the strings are 32 hex chars, so the length
// check leaks nothing (hygiene: early exit per byte is a LAN-negligible
// timing oracle, but it costs nothing to close).
static bool TokenEq(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    unsigned char d = 0;
    for (size_t i = 0; i < a.size(); i++) {
        d |= static_cast<unsigned char>(a[i] ^ b[i]);
    }
    return d == 0;
}

static std::string HeaderValue(const std::string& head, const char* name) {
    // Case-insensitive header scan ("Sec-WebSocket-Key" etc.).
    std::string lower = head;
    for (char& c : lower) c = static_cast<char>(tolower(c));
    std::string want = name;
    for (char& c : want) c = static_cast<char>(tolower(c));
    want += ':';
    const size_t at = lower.find(want);
    if (at == std::string::npos) return "";
    size_t v = at + want.size();
    while (v < head.size() && (head[v] == ' ' || head[v] == '\t')) v++;
    size_t e = v;
    while (e < head.size() && head[e] != '\r' && head[e] != '\n') e++;
    return head.substr(v, e - v);
}

static void SendAll(SOCKET s, const char* p, size_t n) {
    while (n > 0) {
        const int r = send(s, p, static_cast<int>(n), 0);
        if (r <= 0) return;
        p += r;
        n -= static_cast<size_t>(r);
    }
}

static void HttpReply(SOCKET s, int code, const char* body, size_t bodyLen,
                      const char* contentType) {
    const char* reason = code == 200 ? "OK" : code == 404 ? "Not Found"
                                  : code == 403 ? "Forbidden"
                                                : "Service Unavailable";
    char head[256];
    std::snprintf(head, sizeof(head),
                  "HTTP/1.1 %d %s\r\nContent-Length: %zu\r\n"
                  "Content-Type: %s\r\nConnection: close\r\n\r\n",
                  code, reason, bodyLen, contentType);
    SendAll(s, head, std::strlen(head));
    if (bodyLen) SendAll(s, body, bodyLen);
}

// The primary LAN IP for the printed URL (UDP-connect trick: no packets).
static std::string PrimaryIp() {
    SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s == INVALID_SOCKET) return "?";
    sockaddr_in dst{};
    dst.sin_family = AF_INET;
    dst.sin_port = htons(9);
    dst.sin_addr.s_addr = htonl(0x0AFFFFFE);  // 10.255.255.254 — never sent
    std::string ip = "?";
    if (connect(s, reinterpret_cast<sockaddr*>(&dst), sizeof(dst)) == 0) {
        sockaddr_in local{};
        int len = sizeof(local);
        if (getsockname(s, reinterpret_cast<sockaddr*>(&local), &len) == 0) {
            char buf[INET_ADDRSTRLEN] = {};
            if (inet_ntop(AF_INET, &local.sin_addr, buf, sizeof(buf))) {
                ip = buf;
            }
        }
    }
    closesocket(s);
    return ip;
}

// Token from the request target's query string.
static std::string QueryParam(const std::string& target, const char* name) {
    const size_t q = target.find('?');
    if (q == std::string::npos) return "";
    const std::string query = target.substr(q + 1);
    const std::string prefix = std::string(name) + "=";
    size_t at = 0;
    for (;;) {
        at = query.find(prefix, at);
        if (at == std::string::npos) return "";
        if (at == 0 || query[at - 1] == '&') break;
        at += 1;
    }
    const size_t v = at + prefix.size();
    size_t e = v;
    while (e < query.size() && query[e] != '&') e++;
    return query.substr(v, e - v);
}

static void HandleConn(SOCKET conn, const BridgeConfig& cfg) {
    // 30 s read/write timeouts on every connection: kills slowloris at the
    // HTTP stage, breaks a dead phone's blocked recv (slot leak — opus
    // MAJOR-3) and unblocks a wedged send. Healthy sessions stay fed by the
    // pump's WS heartbeat below.
    const DWORD timeoutMs = 30 * 1000;
    setsockopt(conn, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs));
    setsockopt(conn, SOL_SOCKET, SO_SNDTIMEO,
               reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs));
    std::string head;
    if (!ReadHttpHead(conn, head)) {
        closesocket(conn);
        return;
    }
    const size_t sp1 = head.find(' ');
    const size_t sp2 = head.find(' ', sp1 + 1);
    const std::string target = (sp1 == std::string::npos || sp2 == std::string::npos)
                                   ? ""
                                   : head.substr(sp1 + 1, sp2 - sp1 - 1);

    if (target.rfind("/ws", 0) == 0) {
        // ---- WebSocket upgrade, token-gated ------------------------------
        const std::string ip = [&] {
            sockaddr_in a{};
            int len = sizeof(a);
            if (getpeername(conn, reinterpret_cast<sockaddr*>(&a), &len) == 0) {
                char buf[INET_ADDRSTRLEN] = {};
                if (inet_ntop(AF_INET, &a.sin_addr, buf, sizeof(buf))) {
                    return std::string(buf);
                }
            }
            return std::string("?");
        }();
        if (!TokenEq(QueryParam(target, "token"), cfg.token)) {
            if (!g_rate.Allow(ip)) {
                HttpReply(conn, 403, "denied", 6, "text/plain");
            } else {
                HttpReply(conn, 401, "bad token", 9, "text/plain");
            }
            closesocket(conn);
            return;
        }
        const std::string key = HeaderValue(head, "Sec-WebSocket-Key");
        // RFC keys are 24 base64 chars; anything long is hostile. The cap
        // also keeps Sha1's padded input well inside its scratch buffer
        // (key 64 + 36 GUID bytes + padding ≤ 128 — buf is 256 for margin).
        if (HeaderValue(head, "Upgrade") != "websocket" || key.size() < 16 ||
            key.size() > 64) {
            HttpReply(conn, 400, "not websocket", 13, "text/plain");
            closesocket(conn);
            return;
        }
        uint8_t digest[20];
        const std::string accept =
            key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        Sha1(reinterpret_cast<const uint8_t*>(accept.data()), accept.size(),
             digest);
        std::string resp =
            "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n"
            "Connection: Upgrade\r\nSec-WebSocket-Accept: " +
            B64(digest, 20) + "\r\n\r\n";
        SendAll(conn, resp.data(), resp.size());

        // Session cap: authenticated phones only. fetch_add first (opus
        // MINOR-2 — load+++ raced two handshakes past the cap).
        if (g_activeSessions.fetch_add(1) >= kMaxSessions) {
            g_activeSessions.fetch_sub(1);
            WsSendFrame(conn,
                        "{\"type\":\"error\",\"text\":\"too many sessions\"}");
            closesocket(conn);
            return;
        }
        auto s = std::make_shared<BridgeSession>(conn);
        SessionRun(s);
        g_activeSessions--;
    }

    if (target == "/" || target.rfind("/?token=", 0) == 0) {
        HttpReply(conn, 200, kWebUi, std::strlen(kWebUi), "text/html; charset=utf-8");
    } else if (target == "/health") {
        HttpReply(conn, 200, "ok", 2, "text/plain");
    } else {
        HttpReply(conn, 404, "not found", 9, "text/plain");
    }
    closesocket(conn);
}

// ---------------------------------------------------------------------------
// QR encoder — console pair-up display (docs/57 §11). Byte mode, ECC level L,
// versions 1-6 (URL ≤ ~200 data bytes; the token URL is ~65 bytes, so V4 in
// practice). Version info blocks (V7+) are therefore never drawn.
// Constant provenance: segno's installed tables (ISO 18004) — the SHA-1
// lesson: every "well-known" value below was pulled from the reference
// library, not memory, and the whole encoder is matrix-diffed against segno
// plus decoded by cv2/pyzbar before shipping.
// ---------------------------------------------------------------------------
namespace qr {

struct BlockGroup { int nBlocks; int total; int data; };
// (blocks, total codewords, data codewords) per version, level L.
const BlockGroup kLevelL[6] = {
    {1, 26, 19},  {1, 44, 34},   {1, 70, 55},  {1, 100, 80},
    {1, 134, 108}, {2, 86, 68},
};
// Alignment pattern centers, index = version-1 (V1 has none; V6 = {6,34}).
const int kAlign[6][4] = {
    {0, 0, 0, 0}, {2, 6, 18, 0}, {2, 6, 22, 0},
    {2, 6, 26, 0}, {2, 6, 30, 0}, {3, 6, 22, 38},
};
// Final 15-bit format strings, level L, masks 0-7 (segno FORMAT_INFO[8..15]).
const unsigned kFormatL[8] = {30660, 29427, 32170, 30877,
                              26159, 25368, 27713, 26998};

// GF(256) with the QR primitive x^8+x^4+x^3+x^2+1 — tables computed, not
// transcribed (the only "constants" left to memory are the polynomial and
// the algorithm shape; everything numeric derives from them at runtime).
struct Gf {
    uint8_t exp[512], log[256];
    static const Gf& Table() {
        static const Gf g;
        return g;
    }
    static uint8_t Mul(uint8_t a, uint8_t b) {
        if (!a || !b) return 0;
        const Gf& g = Table();
        return g.exp[g.log[a] + g.log[b]];
    }

  private:
    Gf() {
        unsigned x = 1;
        for (int i = 0; i < 255; ++i) {
            log[x] = static_cast<uint8_t>(i);
            exp[i] = static_cast<uint8_t>(x);
            x <<= 1;
            if (x & 0x100) x ^= 0x11D;
        }
        for (int i = 255; i < 512; ++i) exp[i] = exp[i - 255];
        log[0] = 0;
    }
};

struct Field {
    int size = 0;
    std::vector<uint8_t> dark, func;
    uint8_t& Dark(int r, int c) { return dark[static_cast<size_t>(r) * size + c]; }
    uint8_t& Func(int r, int c) { return func[static_cast<size_t>(r) * size + c]; }
};

// Data bitstream: byte mode + 8-bit count (V1-9) + bytes + terminator +
// pad codewords 0xEC/0x11 alternating, exactly filling `dataCw`.
static void BuildBitstream(const std::string& url, int dataCw,
                           std::vector<uint8_t>* bits) {
    bits->clear();
    bits->reserve(static_cast<size_t>(dataCw) * 8);
    const auto push = [&](unsigned val, int n) {
        for (int i = n - 1; i >= 0; --i)
            bits->push_back(static_cast<uint8_t>((val >> i) & 1));
    };
    push(0b0100, 4);  // byte mode
    push(static_cast<unsigned>(url.size()), 8);
    for (unsigned char c : url) push(c, 8);
    const int capBits = dataCw * 8;
    const int term = std::min(4, capBits - static_cast<int>(bits->size()));
    if (term > 0) push(0, term);
    bits->resize((bits->size() + 7) / 8 * 8, 0);
    unsigned pad = 0xEC;
    while (static_cast<int>(bits->size()) < capBits) {
        push(pad, 8);
        pad = (pad == 0xEC) ? 0x11 : 0xEC;
    }
}

// Reed-Solomon remainder of one data block (monic generator, synthetic
// division over GF(256)).
static void EccBytes(const std::vector<uint8_t>& block, int ecc,
                     std::vector<uint8_t>* out) {
    std::vector<uint8_t> gen{1};  // coefficient j of x^(ecc-j), gen[0]=1
    for (int i = 0; i < ecc; ++i) {
        std::vector<uint8_t> ng(gen.size() + 1, 0);
        for (size_t j = 0; j < gen.size(); ++j) {
            ng[j + 1] ^= gen[j];
            ng[j] ^= Gf::Mul(gen[j], Gf::Table().exp[i]);
        }
        gen = ng;
    }
    std::vector<uint8_t> rem(static_cast<size_t>(ecc), 0);
    for (uint8_t d : block) {
        const uint8_t f = static_cast<uint8_t>(d ^ rem[0]);
        std::memmove(rem.data(), rem.data() + 1, static_cast<size_t>(ecc) - 1);
        rem[ecc - 1] = 0;
        if (f) {
            // gen is built low-order first (gen[0] = x^0 coeff); rem[j] holds
            // the x^(ecc-1-j) coefficient, so the multiplier is the mirrored
            // index — using gen[j+1] here silently scrambles the ECC.
            for (int j = 0; j < ecc; ++j)
                rem[j] ^= Gf::Mul(gen[ecc - 1 - j], f);
        }
    }
    *out = rem;
}

// Data + ECC codewords, interleaved (V1-V6 L always splits into equal-size
// blocks, so plain round-robin is the full ISO 18004 interleave here).
static void BuildCodewords(const std::string& url, int version,
                           std::vector<uint8_t>* out) {
    const BlockGroup g = kLevelL[version - 1];
    const int ecc = g.total - g.data;
    std::vector<uint8_t> bits;
    BuildBitstream(url, g.data * g.nBlocks, &bits);
    // bit vector -> data codewords per block
    std::vector<std::vector<uint8_t>> blocks(g.nBlocks);
    size_t bit = 0;
    for (int b = 0; b < g.nBlocks; ++b) {
        blocks[b].reserve(static_cast<size_t>(g.data));
        for (int i = 0; i < g.data; ++i, bit += 8) {
            unsigned v = 0;
            for (int k = 0; k < 8; ++k) v = (v << 1) | bits[bit + k];
            blocks[b].push_back(static_cast<uint8_t>(v));
        }
    }
    out->clear();
    for (int i = 0; i < g.data; ++i)
        for (int b = 0; b < g.nBlocks; ++b) out->push_back(blocks[b][i]);
    for (int b = 0; b < g.nBlocks; ++b) {
        std::vector<uint8_t> e;
        EccBytes(blocks[b], ecc, &e);
        for (uint8_t v : e) out->push_back(v);
    }
}

static void DrawPatterns(Field* f, int version) {
    const int n = f->size;
    const auto mark = [&](int r, int c, int d) {
        f->Dark(r, c) = static_cast<uint8_t>(d);
        f->Func(r, c) = 1;
    };
    // finder patterns + separators
    const int fp[3][2] = {{0, 0}, {0, n - 7}, {n - 7, 0}};
    for (const auto& p : fp) {
        for (int r = -1; r <= 7; ++r) {
            for (int c = -1; c <= 7; ++c) {
                const int rr = p[0] + r, cc = p[1] + c;
                if (rr < 0 || cc < 0 || rr >= n || cc >= n) continue;
                int d = 0;  // separators (the -1..7 ring)
                if (r >= 0 && r <= 6 && c >= 0 && c <= 6) {
                    d = (r == 0 || r == 6 || c == 0 || c == 6 ||
                         (r >= 2 && r <= 4 && c >= 2 && c <= 4))
                            ? 1
                            : 0;
                }
                mark(rr, cc, d);
            }
        }
    }
    // timing patterns (skip cells already claimed by finders)
    for (int i = 8; i < n - 8; ++i) {
        if (!f->Func(6, i)) mark(6, i, (i & 1) ? 0 : 1);
        if (!f->Func(i, 6)) mark(i, 6, (i & 1) ? 0 : 1);
    }
    // alignment patterns (skip finder overlaps)
    const int* ap = kAlign[version - 1];
    const int cnt = ap[0];
    for (int a = 0; a < cnt; ++a) {
        for (int b = 0; b < cnt; ++b) {
            const int r = ap[1 + a], c = ap[1 + b];
            if ((r == 6 && c == 6) || (r == 6 && c == n - 7) ||
                (r == n - 7 && c == 6))
                continue;
            for (int dr = -2; dr <= 2; ++dr) {
                for (int dc = -2; dc <= 2; ++dc) {
                    const int d = (dr == -2 || dr == 2 || dc == -2 ||
                                   dc == 2 || (dr == 0 && dc == 0))
                                      ? 1
                                      : 0;
                    mark(r + dr, c + dc, d);
                }
            }
        }
    }
    // reserve format-info cells + dark module
    for (int i = 0; i <= 8; ++i) {
        if (!f->Func(i, 8)) mark(i, 8, 0);
        if (!f->Func(8, i)) mark(8, i, 0);
    }
    for (int i = 0; i < 8; ++i) {
        mark(8, n - 1 - i, 0);
        mark(n - 1 - i, 8, 0);
    }
    mark(n - 8, 8, 1);
}

static unsigned MaskBit(int mask, int r, int c) {
    // Dark where the condition holds (ISO 18004 §7.8.2 — the "== 0" forms).
    switch (mask) {
        case 0: return static_cast<unsigned>((r + c) % 2 == 0);
        case 1: return static_cast<unsigned>(r % 2 == 0);
        case 2: return static_cast<unsigned>(c % 3 == 0);
        case 3: return static_cast<unsigned>((r + c) % 3 == 0);
        case 4: return static_cast<unsigned>(((r / 2) + (c / 3)) % 2 == 0);
        case 5: return static_cast<unsigned>(
                    ((r * c) % 2) + ((r * c) % 3) == 0);
        case 6: return static_cast<unsigned>(
                    (((r * c) % 2) + ((r * c) % 3)) % 2 == 0);
        default: return static_cast<unsigned>(
                     (((r + c) % 2) + ((r * c) % 3)) % 2 == 0);
    }
}

static int Penalty(const Field& f) {
    const int n = f.size;
    int score = 0;
    const auto darkAt = [&](int r, int c) {
        return f.dark[static_cast<size_t>(r) * n + c] != 0;
    };
    for (int axis = 0; axis < 2; ++axis) {  // N1: runs ≥ 5, rows then cols
        for (int a = 0; a < n; ++a) {
            int run = 1;
            for (int b = 1; b <= n; ++b) {
                const bool cur = b < n && (axis ? darkAt(b, a) : darkAt(a, b));
                const bool prev = (axis ? darkAt(b - 1, a) : darkAt(a, b - 1));
                if (b < n && cur == prev) {
                    run++;
                } else {
                    if (run >= 5) score += 3 + (run - 5);
                    run = 1;
                }
            }
        }
    }
    for (int r = 0; r + 1 < n; ++r) {  // N2: 2x2 same-color blocks
        for (int c = 0; c + 1 < n; ++c) {
            const bool d = darkAt(r, c);
            if (d == darkAt(r, c + 1) && d == darkAt(r + 1, c) &&
                d == darkAt(r + 1, c + 1))
                score += 3;
        }
    }
    const unsigned p1 = 0x05D, p2 = 0x05D >> 1;  // 10111010000 / 00001011101
    for (int axis = 0; axis < 2; ++axis) {  // N3: finder-like sequences
        for (int a = 0; a < n; ++a) {
            unsigned win1 = 0, win2 = 0;
            for (int b = 0; b < n; ++b) {
                const unsigned bit =
                    axis ? (darkAt(b, a) ? 1u : 0u) : (darkAt(a, b) ? 1u : 0u);
                win1 = ((win1 << 1) | bit) & 0x7FF;
                win2 = ((win2 >> 1) | (bit << 10)) & 0x7FF;
                if (b >= 10 && (win1 == p1 || win2 == p2)) score += 40;
            }
        }
    }
    int darkCount = 0;  // N4: dark proportion
    for (uint8_t d : f.dark) darkCount += d ? 1 : 0;
    const int pct = darkCount * 100 / (n * n);
    score += std::abs(pct - 50) / 5 * 10;
    return score;
}

// Writes data bits + chosen-mask format info into a fresh matrix.
static void Compose(int version, int mask, const std::vector<uint8_t>& bits,
                    Field* f) {
    const int n = f->size;
    int bit = 0;
    for (int right = n - 1; right > 0; right -= 2) {
        const int r2 = (right <= 6) ? right - 1 : right;
        for (int vert = 0; vert < n; ++vert) {
            for (int z = 0; z < 2; ++z) {
                const int j = r2 - z;
                bool up = ((r2 & 2) == 0);
                if (j < 6) up = !up;
                const int i = up ? (n - 1 - vert) : vert;
                if (f->Func(i, j)) continue;
                f->Dark(i, j) = static_cast<uint8_t>(
                    (bit < static_cast<int>(bits.size()) ? bits[bit] : 0) ^
                    MaskBit(mask, i, j));
                bit++;
            }
        }
    }
    const unsigned fmt = kFormatL[mask];
    int voff = 0, hoff = 0;
    for (int i = 0; i < 8; ++i) {
        const unsigned vbit = (fmt >> i) & 1;
        const unsigned hbit = (fmt >> (14 - i)) & 1;
        if (i == 6) { voff = 1; hoff = 1; }
        f->Dark(i + voff, 8) = static_cast<uint8_t>(vbit);
        f->Dark(8, i + hoff) = static_cast<uint8_t>(hbit);
        f->Dark(8, n - 1 - i) = static_cast<uint8_t>(vbit);
        f->Dark(n - 1 - i, 8) = static_cast<uint8_t>(hbit);
    }
    f->Dark(n - 8, 8) = 1;  // dark module
}

// Encodes `url` — returns the version, fills `matrix` (dark=1, size 17+4v).
static int Encode(const std::string& url, std::vector<uint8_t>* matrix,
                  int* sizeOut, int* maskOut) {
    int version = 0;
    // Data budget must count the full segment overhead (mode nibble + 8-bit
    // count for V1-9 + terminator up to 4 bits) — comparing the raw byte
    // count against the codeword capacity silently truncates boundary URLs.
    for (int v = 1; v <= 6; ++v) {
        const int capBits = kLevelL[v - 1].data * kLevelL[v - 1].nBlocks * 8;
        // (terminator may be truncated to the remaining capacity — ISO
        // 18004 §7.4.9 — so the plain needBits <= capBits check suffices)
        const int needBits = 4 + 8 + static_cast<int>(url.size()) * 8;
        if (needBits <= capBits) {
            version = v;
            break;
        }
    }
    if (!version) return 0;
    // Placement consumes the FULL interleaved codeword sequence (data + ECC).
    // Feeding it the data bitstream alone leaves the ECC region as zero
    // padding — an undecodable symbol.
    std::vector<uint8_t> cw;
    BuildCodewords(url, version, &cw);
    std::vector<uint8_t> bits;
    bits.reserve(cw.size() * 8);
    for (uint8_t b : cw)
        for (int k = 7; k >= 0; --k)
            bits.push_back(static_cast<uint8_t>((b >> k) & 1));
    Field base;
    base.size = 17 + 4 * version;
    base.dark.assign(static_cast<size_t>(base.size) * base.size, 0);
    base.func.assign(static_cast<size_t>(base.size) * base.size, 0);
    DrawPatterns(&base, version);
    int bestMask = 0, bestScore = -1;
    Field work;
    for (int mask = 0; mask < 8; ++mask) {
        work = base;
        Compose(version, mask, bits, &work);
        const int score = Penalty(work);
        if (bestScore < 0 || score < bestScore) {
            bestScore = score;
            bestMask = mask;
        }
    }
    work = base;
    Compose(version, bestMask, bits, &work);
    if (sizeOut) *sizeOut = work.size;
    if (maskOut) *maskOut = bestMask;
    *matrix = work.dark;
    return version;
}

// Console rendering: swap the console attribute to white-background/black-
// foreground and print — "██" then renders DARK modules on a light field
// (true dark-on-light, what phone cameras prefer). Two QR rows merge per
// text row via the half-block glyphs (▀/▄) so a V4 symbol fits ~25 lines.
// Redirected output (probe convention) skips the QR — --qr-debug covers it.
static void PrintConsole(const std::vector<uint8_t>& m, int n, bool force) {
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    const bool console = GetConsoleMode(out, &mode) != 0;
    if (!console && !force) return;
    CONSOLE_SCREEN_BUFFER_INFO info{};
    const WORD saved =
        GetConsoleScreenBufferInfo(out, &info)
            ? info.wAttributes
            : static_cast<WORD>(FOREGROUND_RED | FOREGROUND_GREEN |
                                FOREGROUND_BLUE);
    if (console) {
        SetConsoleTextAttribute(
            out, BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE);
    }
    const auto rowLine = [&](int top, int bottom) {
        std::string line(8, ' ');
        for (int c = 0; c < n; ++c) {
            const int a = m[static_cast<size_t>(top) * n + c];
            const int b = m[static_cast<size_t>(bottom) * n + c];
            // UTF-8 glyphs: ██ dark/dark, ▀▀ upper dark, ▄▄ lower dark.
            // Half-block cells double the vertical resolution so a V4
            // symbol fits ~25 console lines.
            if (a && b) line += "\xe2\x96\x88\xe2\x96\x88";
            else if (a) line += "\xe2\x96\x80\xe2\x96\x80";
            else if (b) line += "\xe2\x96\x84\xe2\x96\x84";
            else line += "  ";
        }
        line += std::string(8, ' ');
        OutW(line);
    };
    OutW(std::string(2 * n + 16, ' '));
    for (int r = 0; r < n; r += 2) {
        if (r + 1 < n) {
            rowLine(r, r + 1);
        } else {  // odd height: last row renders as (dark, light)
            std::string line(8, ' ');
            for (int c = 0; c < n; ++c) {
                const int a = m[static_cast<size_t>(r) * n + c];
                line += a ? "\xe2\x96\x80\xe2\x96\x80" : "  ";
            }
            line += std::string(8, ' ');
            OutW(line);
        }
    }
    OutW(std::string(2 * n + 16, ' '));
    if (console) SetConsoleTextAttribute(out, saved);
}

// Debug dump: version/mask header + matrix rows of '0'/'1' — python diffs
// this against segno and decodes it via cv2/pyzbar (verification gate).
static void DumpDebug(const std::string& url, const std::vector<uint8_t>& m,
                      int n, int version, int mask) {
    char head[64];
    std::snprintf(head, sizeof(head), "version=%d mask=%d size=%d", version,
                  mask, n);
    OutW(head);
    // codeword hex dump — python diffs the bitstream separately from layout
    {
        std::vector<uint8_t> cw;
        BuildCodewords(url, version, &cw);
        std::string hex;
        char b[8];
        for (uint8_t v : cw) {
            std::snprintf(b, sizeof(b), "%02x", v);
            hex += b;
        }
        OutW("codewords=" + hex);
    }
    std::string row;
    for (int r = 0; r < n; ++r) {
        row.clear();
        for (int c = 0; c < n; ++c)
            row += m[static_cast<size_t>(r) * n + c] ? '1' : '0';
        OutW(row);
    }
}

}  // namespace qr

int main(int argc, char** argv) {
    // 크래시 증거 보존(docs/57 §13): 서버 무음 사망(2026-09-20)을 브리지도
    // 따라가지 않게. OutW는 Win32 콘솔 핸들 직접 출력이라 로그 미러 대상이
    // 아님 — 미니덤프가 브리지 죽음의 진실원.
    jk::InstallCrashHandler("state/logs", "bridge");
    // --qr-debug-func: function-cell map dump (python verifier diff input).
    if (argc >= 2 && std::strcmp(argv[1], "--qr-debug-func") == 0) {
        qr::Field base;
        base.size = 33;
        base.dark.assign(static_cast<size_t>(base.size) * base.size, 0);
        base.func.assign(static_cast<size_t>(base.size) * base.size, 0);
        qr::DrawPatterns(&base, 4);
        for (int r = 0; r < base.size; ++r) {
            std::string row;
            for (int c = 0; c < base.size; ++c)
                row += base.Func(r, c) ? '1' : '0';
            OutW(row);
        }
        return 0;
    }
    // --qr-print <url>: console QR glyphs even when redirected (the probe
    // reassembles the matrix from the half-block glyphs and decodes it).
    if (argc >= 3 && std::strcmp(argv[1], "--qr-print") == 0) {
        std::vector<uint8_t> m;
        int n = 0, mask = 0;
        const int v = qr::Encode(argv[2], &m, &n, &mask);
        if (!v) {
            OutW("encode-failed");
            return 1;
        }
        qr::PrintConsole(m, n, true);
        return 0;
    }
    // --qr-debug <url>: matrix dump for the python verifier, no server.
    if (argc >= 3 && std::strcmp(argv[1], "--qr-debug") == 0) {
        std::vector<uint8_t> m;
        int n = 0, mask = 0;
        const int v = qr::Encode(argv[2], &m, &n, &mask);
        if (!v) {
            OutW("encode-failed");
            return 1;
        }
        qr::DumpDebug(argv[2], m, n, v, mask);
        return 0;
    }
    if (!Sha1SelfTest()) {
        OutW("[!] SHA-1 self-test FAIL — 중단");
        return 1;
    }
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        OutW("[!] WSAStartup failed");
        return 1;
    }
    const BridgeConfig cfg = LoadBridgeConfig();

    SOCKET listener = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    // bind 미지정 = INADDR_ANY(전 인터페이스 — LAN + loopback, 토큰이 게이트;
    // docs/57 §9 기존 동작). 지정 시 그 인터페이스만 — 사내망 노출 봉쇄.
    addr.sin_addr.s_addr = cfg.bindIp.empty()
                               ? INADDR_ANY
                               : inet_addr(cfg.bindIp.c_str());
    addr.sin_port = htons(static_cast<u_short>(cfg.port));
    BOOL reuse = TRUE;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<char*>(&reuse), sizeof(reuse));
    if (bind(listener, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) !=
            0 ||
        listen(listener, 8) != 0) {
        OutW("[!] bind/listen failed — 포트 " + std::to_string(cfg.port));
        return 1;
    }

    OutW("jkbridge — phone web gateway");
    if (!cfg.bindIp.empty()) {
        OutW("  bind: " + cfg.bindIp + " (전 인터페이스가 아니라 이 인터페이스만)");
    }
    // bind 지정 시 URL도 그 IP 기준 — PrimaryIp() 자동탐지는 ANY일 때만.
    const std::string urlHost = cfg.bindIp.empty() ? PrimaryIp() : cfg.bindIp;
    const std::string url = "http://" + urlHost + ":" +
                            std::to_string(cfg.port) + "/?token=" + cfg.token;
    OutW("  URL: " + url);
    OutW("  (같은 Wi-Fi의 폰 브라우저에서 위 URL 열기 — 토큰은 state\\jkbridge.json)");
    OutW("  폰 카메라로 아래 QR을 스캔해도 접속됩니다:");
    {
        std::vector<uint8_t> m;
        int n = 0, mask = 0;
        if (qr::Encode(url, &m, &n, &mask)) qr::PrintConsole(m, n, false);
    }
    OutW("  Ctrl+C 종료. 세션 상한 " + std::to_string(kMaxSessions) + ", 프레임 상한 1MiB.");

    for (;;) {
        const SOCKET conn = accept(listener, nullptr, nullptr);
        if (conn == INVALID_SOCKET) continue;
        std::thread(HandleConn, conn, cfg).detach();
    }
}