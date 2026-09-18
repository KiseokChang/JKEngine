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

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
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
// Bridge config: state/jkbridge.json {token, port} — first run generates the
// token (CSPRNG) and prints the bookmarkable URLs.
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
    // Short keys only (handshake: <64 bytes) — one-block scratch is plenty.
    uint8_t buf[128] = {};
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

// Reads one text frame. Continuation frames are refused (our protocol is
// strictly one-JSON-per-frame — a phone that fragments is misbehaving).
// Returns: 1 = got payload, 0 = clean close, -1 = protocol error / I/O fail.
static int WsReadFrame(SOCKET s, std::string& out) {
    unsigned char hdr[2];
    size_t got = 0;
    while (got < 2) {
        const int r = recv(s, reinterpret_cast<char*>(hdr) + got,
                           static_cast<int>(2 - got), 0);
        if (r <= 0) return -1;
        got += static_cast<size_t>(r);
    }
    const unsigned char opcode = hdr[0] & 0x0F;
    const bool masked = (hdr[1] & 0x80) != 0;
    uint64_t len = hdr[1] & 0x7F;
    if (len == 126) {
        unsigned char ext[2] = {};
        if (recv(s, reinterpret_cast<char*>(ext), 2, 0) != 2) return -1;
        len = (static_cast<uint64_t>(ext[0]) << 8) | ext[1];
    } else if (len == 127) {
        unsigned char ext[8] = {};
        if (recv(s, reinterpret_cast<char*>(ext), 8, 0) != 8) return -1;
        len = 0;
        for (int i = 0; i < 8; i++) {
            len = (len << 8) | ext[i];
        }
    }
    if (opcode == 0x8) return 0;  // close
    if (opcode == 0xA) return WsReadFrame(s, out);  // pong — skip
    if (opcode == 0x9) {  // ping → pong (masked-echo of our own 0x8A)
        const unsigned char pong[2] = {0x8A, 0x00};
        send(s, reinterpret_cast<const char*>(pong), 2, 0);
        return WsReadFrame(s, out);
    }
    if (opcode != 0x1) return -1;  // binary / continuation — refused
    if (!masked) return -1;        // RFC: clients MUST mask
    if (len > kMaxFrame) return -1;
    unsigned char mask[4];
    if (recv(s, reinterpret_cast<char*>(mask), 4, 0) != 4) return -1;
    out.resize(static_cast<size_t>(len));
    size_t off = 0;
    while (off < out.size()) {
        const int r = recv(s, &out[off],
                           static_cast<int>(out.size() - off), 0);
        if (r <= 0) return -1;
        off += static_cast<size_t>(r);
    }
    for (size_t i = 0; i < out.size(); i++) out[i] ^= mask[i % 4];
    return 1;
}

// Server frames are never masked. Sends must be serialized by the session's
// mutex (frame = up to 3 send() calls).
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
let ws = null, streamEl = null;
// claude session continuity: the phone owns it (localStorage) — a reconnect
// resumes the engine, not the transcript (spec §5).
let claudeSession = localStorage.getItem('jkbridge_session') || '';
// approval-strip QUEUE (jkchat EnqueueApproval port): one strip, FIFO,
// re-armed by approval_resolved — a later ask must not overwrite a parked one.
let apprFront = null; const apprQueue = [];
let pendingUndo = null; // /close,/restore → save_layout pre_undo → act
function esc(s){ return String(s).replace(/[&<>"]/g, c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c])); }
function cut(s,n){ s=String(s||''); return s.length>n ? s.slice(0,n) : s; }
function add(text, cls){ const d=document.createElement('div'); d.className=cls||'';
  d.textContent=text; logEl.appendChild(d); logEl.scrollTop=logEl.scrollHeight; return d; }

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
    if (msg.pending_result) add('[복구] '+msg.pending_result, 'note');
    if (msg.busy) add('[LLM 실행 중 — 이전 턴 진행]', 'sys');
    return;
  }
  if (msg.type === 'stream') {
    if (!streamEl) streamEl = add('[LLM] ', 'llm');
    streamEl.textContent += msg.text;
    logEl.scrollTop = logEl.scrollHeight;
    return;
  }
  if (msg.type === 'chat_done') {
    if (streamEl) streamEl = null;
    if (msg.session_id) { claudeSession = msg.session_id; localStorage.setItem('jkbridge_session', msg.session_id); }
    if (msg.ok && !msg.streamed) add(msg.result || '(빈 응답)', 'llm');
    else if (!msg.ok) add('[!] LLM 응답 실패 — ' + (msg.result||''), 'err');
    else add('[LLM 완료]', 'sys');
    return;
  }
  if (msg.type === 'reply') { add('['+(msg.label||'?')+'] '+(j?JSON.stringify(j):'?'), 'sys'); onReply(msg.label, j); return; }
  if (msg.type === 'event') { onEvent(msg.topic, j); return; }
  if (msg.type === 'error') { add('[!] '+(msg.text||''), 'err'); return; }
}

// ---- approval strip (jkchat 문구 + 큐 순환 이식) ---------------------------
function approvalText(e) {
  const k = e.kind;
  if (k === 'trust_request')  return '[신뢰 요청] '+e.name+' ('+e.origin+') 해시 '+cut(e.fingerprint,15)+'… 승인할까요?';
  if (k === 'permission_set') return '[권한 변경] '+e.target_tool+' → '+e.decision+' 승인할까요?';
  if (k === 'capture_allow')  return '[캡처 허용] capture_window/region → '+e.decision+' 승인할까요?';
  if (k === 'trust_revoke')   return '[신뢰 해지] '+e.name+' ('+cut(e.fingerprint,15)+'…) 승인할까요?';
  if (k === 'files_access')   return '[파일 요청] '+e.tool+' → '+e.title+' 승인할까요?';
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
function onEvent(topic, e) {
  if (!e) { add('['+topic+']', 'ev'); return; }
  if (topic === 'agent.approval_request') {
    add('[승인 요청] ' + approvalText(e), 'note');
    if (!apprFront) { apprFront = {request: e.request, text: approvalText(e)}; showFront(); }
    else { apprQueue.push({request: e.request, text: approvalText(e)});
           add('[대기] 승인 요청 #'+e.request+' — 현재 승인 처리 후 표시', 'sys'); }
    return;
  }
  if (topic === 'agent.approval_resolved') {
    add('[승인] request '+e.request+' → '+e.decision, 'sys');
    if (apprFront && apprFront.request === e.request) {
      apprFront = apprQueue.shift() || null; showFront();
    } else { // another surface resolved a queued item
      const i = apprQueue.findIndex(a => a.request === e.request);
      if (i >= 0) apprQueue.splice(i, 1);
    }
    return;
  }
  if (topic === 'agent.notify') { add('[알림] '+((e.data||{}).title||'(무제)')+
      ((e.data||{}).body ? ' — '+e.data.body : ''), 'note'); return; }
  add('['+topic+'] '+(j?JSON.stringify(e):''), 'ev');
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
    add('/undo /new', 'sys');
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
    explicit BridgeSession(SOCKET sock) : sock_(sock) {}

    // The dispatcher (Run) owns the lifecycle. Returns when the WS dies.
    void Run();

    bool SendText(const std::string& json) {
        if (!alive_.load()) return false;
        std::lock_guard<std::mutex> lock(wsMtx_);
        if (!WsSendFrame(sock_, json)) {
            Shutdown();
            return false;
        }
        return true;
    }

    void Shutdown() {
        if (alive_.exchange(false)) {
            shutdown(sock_, SD_BOTH);
            closesocket(sock_);
        }
    }

    bool Alive() const { return alive_.load(); }

    SOCKET sock_;
    std::mutex wsMtx_;       // serializes frame writes (3+ send() calls each)
    std::atomic<bool> alive_{true};

    jk::agent::JKAgentClient agent_;
    std::mutex agentMtx_;    // pipe IO: dispatcher SendQuery vs pump Query
    std::mutex labelsMtx_;
    std::map<uint32_t, std::string> labels_;  // queryId → transcript label

    std::thread pump_;
    jk::agent::JKLlmEngine engine_;           // one turn at a time
    std::string resumeSession_;               // consumer-owned claude session
};

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
    std::string Get(const std::string& sessionId) {
        if (sessionId.empty()) return "";
        std::lock_guard<std::mutex> lock(m);
        const auto it = bySession.find(sessionId);
        if (it == bySession.end()) return "";
        if (static_cast<int64_t>(time(nullptr)) - it->second.first > 300) {
            bySession.erase(it);
            return "";
        }
        return it->second.second;
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
    if (!r.sessionId.empty()) (*keep)->resumeSession_ = r.sessionId;
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
    delete keep;
}

// The pump: a ping round-trip flushes parked replies (jkchat Pump pattern),
// then drain replies and events out over the WS. Blocking ping is a few ms
// — acceptable on the pump thread.
static void PumpLoop(BridgeSession* s) {
    std::string json;
    while (s->Alive()) {
        if (!s->agent_.IsConnected()) {
            if (s->agent_.Connect()) {
                s->agent_.SubscribeEvents(true);
            } else {
                Sleep(400);
                continue;
            }
        }
        {
            std::lock_guard<std::mutex> lock(s->agentMtx_);
            std::string pong;
            s->agent_.Query("ping", "{}", pong);
            if (!s->agent_.IsConnected()) {
                s->SendText("{\"type\":\"error\",\"text\":\"server connection lost\"}");
                Sleep(400);
                continue;
            }
            // queryId → label mapping: dispatcher adds, pump consumes.
            std::lock_guard<std::mutex> labelLock(s->labelsMtx_);
            for (auto it = s->labels_.begin(); it != s->labels_.end();) {
                if (s->agent_.PollReply(it->first, json)) {
                    s->SendText("{\"type\":\"reply\",\"label\":" +
                                JsonEsc(it->second) + ",\"json\":" + json + "}");
                    it = s->labels_.erase(it);
                } else {
                    ++it;
                }
            }
            std::vector<jk::agent::AgentEvent> events;
            s->agent_.PollEvents(events);
            for (const auto& ev : events) {
                s->SendText("{\"type\":\"event\",\"topic\":" + JsonEsc(ev.topic) +
                            ",\"json\":" + ev.json + "}");
            }
        }
        Sleep(400);
    }
}

// The frame dispatcher — runs on the HTTP thread after the WS handshake.
static void SessionRun(std::shared_ptr<BridgeSession> s) {
    s->pump_ = std::thread(PumpLoop, s.get());
    std::string line;
    for (;;) {
        const int r = WsReadFrame(s->sock_, line);
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
            if (!resume.empty()) s->resumeSession_ = resume;
            std::string frame =
                "{\"type\":\"hello\",\"ok\":1,\"busy\":" +
                std::string(s->engine_.Busy() ? "1" : "0");
            const std::string pending = g_doneMemo.Get(s->resumeSession_);
            if (!pending.empty()) {
                frame += ",\"pending_result\":" + JsonEsc(pending);
            }
            frame += "}";
            s->SendText(frame);
        } else if (type == "chat") {
            std::string text;
            if (!f.GetStr("text", text) || text.empty()) {
                s->SendText("{\"type\":\"error\",\"text\":\"empty text\"}");
                continue;
            }
            auto* keep = new std::shared_ptr<BridgeSession>(s);
            if (!s->engine_.StartTurn(text, s->resumeSession_, OnLlmDelta,
                                      OnLlmDone, keep)) {
                delete keep;
                s->SendText("{\"type\":\"error\",\"text\":\"busy\"}");
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
            std::lock_guard<std::mutex> lock(s->agentMtx_);
            const uint32_t aid = s->agent_.SendQuery(
                "approve",
                "{\"request\":" + std::to_string(request) +
                    ",\"decision\":\"" + decision + "\"}");
            // The pump only forwards labelled replies — without this the
            // phone never learns whether the approval actually landed.
            if (aid != 0) {
                std::lock_guard<std::mutex> l2(s->labelsMtx_);
                s->labels_[aid] = "approve";
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

// Reads the request head (up to \r\n\r\n, 8 KiB cap).
static bool ReadHttpHead(SOCKET s, std::string& head) {
    char chunk[1024];
    while (head.find("\r\n\r\n") == std::string::npos && head.size() < 8192) {
        const int r = recv(s, chunk, sizeof(chunk), 0);
        if (r <= 0) return false;
        head.append(chunk, static_cast<size_t>(r));
    }
    return true;
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
        if (QueryParam(target, "token") != cfg.token) {
            if (!g_rate.Allow(ip)) {
                HttpReply(conn, 403, "denied", 6, "text/plain");
            } else {
                HttpReply(conn, 401, "bad token", 9, "text/plain");
            }
            closesocket(conn);
            return;
        }
        const std::string key = HeaderValue(head, "Sec-WebSocket-Key");
        // Upper bound = Sha1's 128-byte scratch (len ≤ 120 fits with padding);
        // RFC keys are 24 chars — anything long is hostile anyway.
        if (HeaderValue(head, "Upgrade") != "websocket" || key.size() < 16 ||
            key.size() > 120) {
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

        // Session cap: authenticated phones only.
        if (g_activeSessions.load() >= kMaxSessions) {
            WsSendFrame(conn,
                        "{\"type\":\"error\",\"text\":\"too many sessions\"}");
            closesocket(conn);
            return;
        }
        g_activeSessions++;
        auto s = std::make_shared<BridgeSession>(conn);
        SessionRun(s);
        g_activeSessions--;
        return;
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

int main() {
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
    addr.sin_addr.s_addr = INADDR_ANY;  // LAN + loopback — token is the gate
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
    OutW("  URL: http://" + PrimaryIp() + ":" + std::to_string(cfg.port) +
         "/?token=" + cfg.token);
    OutW("  (같은 Wi-Fi의 폰 브라우저에서 위 URL 열기 — 토큰은 state\\jkbridge.json)");
    OutW("  Ctrl+C 종료. 세션 상한 " + std::to_string(kMaxSessions) + ", 프레임 상한 1MiB.");

    for (;;) {
        const SOCKET conn = accept(listener, nullptr, nullptr);
        if (conn == INVALID_SOCKET) continue;
        std::thread(HandleConn, conn, cfg).detach();
    }
}