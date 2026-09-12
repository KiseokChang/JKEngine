# 트리거 Rate Limiter 구현 계획

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 신뢰된 스크립트의 폭주(셀프루프/타이머 스팸/발행 폭탄)를 3곳의 rate cap으로 차단 — jktriggers 핸들러 예산, 라이브 타이머 상한, 서버 publish_event 연결 상한. 부수로 docs/37 후속 minor 3건 경질화.

**Architecture:** jktriggers는 TriggerReg.source(컨테이너 키)별 고정 윈도우 예산(60회/60초)을 DispatchEvent의 JS_Call 직전에 확인·소비하고, 초과 첫 발화에 agent.notify 1회 + 로그 1회. 타이머는 전역 64개 상한. 서버는 publish_event를 연결별 고정 윈도우(30 이벤트/10초)로 게이트해 초과분을 `dropped:1` 회신과 함께 드롭.

**Tech Stack:** C++20, QuickJS(quickjs-ng), MinGW UCRT64, CMake(engine/build)

**Spec:** docs/superpowers/specs/2026-09-13-trigger-rate-limit-design.md

## Global Constraints

- cap 초과 = **스킵 + 조용히 계속** — 절대 크래시/펌프 정지 없음. 다른 트리거 발화는 항상 정상.
- 통지/로그는 **윈도우당 1회** (폭주 중 알림 폭주 금지 — 이 기능의 목적이 그것 방지).
- jktriggers rate 로직은 `NowMs()` 주입 가능한 코어(`RateAllowAt(source, nowMs)`)로 — selftest가 실제 시계 없이 경계를 검증.
- 서버 publish_event 초과 회신은 `{"ok":true,"dropped":1}` — 발신자가 재시도 루프를 돌지 않게 ok 유지.
- trust 관련 기존 불변 유지: fail-closed, packer user-레코드 보존, 지문 검증 순서(missing_name → bad_name → bad_fingerprint → bad_origin) — name 상한은 **bad_fingerprint 검사 앞**에 끼워 넣는다.
- 회귀 전부 유지: triggers 7/7, trust 7/7, events 5/5, chat 7/7, mcp 5/5, e2e 7/7, palette 4/4.
- 커밋 관례: `feat(trust)`/`fix(...)` 스타일 + `Co-Authored-By: Claude Code <noreply@anthropic.com>`.

---

### Task 1: jktriggers — 핸들러 예산 + 타이머 상한 + ts int64

**Files:**
- Modify: `engine/tools/jktriggers/main.cpp` (TriggerReg 근처 globals, DispatchEvent, JsSetTimeout/JsSetInterval, TrustRecord/LoadTrustRecords)

**Interfaces:**
- Consumes: `Publish(topic, data)` 기존 헬퍼, `HostLog`, `NowMs`, `TriggerReg.source`
- Produces: `RateAllowAt(source, nowMs)` (selftest가 소비), 상수 `kRateLimitCount=60` `kRateLimitWindowMs=60000` `kMaxTimers=64`

- [ ] **Step 1: 예산 구조 + 코어 함수 추가** (g_enabled 선언 근처)

```cpp
// Rate limiter (docs/38 spec §2): per-source fixed-window budget. A trusted
// script that runs away (publish→on self-loop) is throttled instead of
// flooding the bus. Convenience guard — not a security boundary (spec §1).
struct SourceBudget {
    uint64_t windowStartMs = 0;
    int count = 0;
    bool notified = false;   // one notify + one log per window
};
constexpr int kRateLimitCount = 60;
constexpr uint64_t kRateLimitWindowMs = 60000;
constexpr size_t kMaxTimers = 64;
std::map<std::string, SourceBudget> g_budget;
```

```cpp
// Core with injected clock (selftest). Returns true when this invocation may
// proceed; consumes 1 budget unit either way once the filter has passed.
bool RateAllowAt(const std::string& source, uint64_t nowMs) {
    SourceBudget& b = g_budget[source];
    if (nowMs - b.windowStartMs >= kRateLimitWindowMs) {
        b.windowStartMs = nowMs;
        b.count = 0;
        b.notified = false;
    }
    if (b.count >= kRateLimitCount) {
        if (!b.notified) {
            b.notified = true;
            HostLog("[triggers] rate limit: " + source + " (" +
                    std::to_string(kRateLimitCount) + " fires/" +
                    std::to_string(kRateLimitWindowMs / 1000) + "s)");
            Publish("agent.notify",
                    "{\"title\":\"트리거 발화 제한\",\"body\":\"" +
                        JsonEscapeStr(source) + " — 이벤트 발화 상한 도달\"}");
        }
        return false;
    }
    ++b.count;
    return true;
}
bool RateAllow(const std::string& source) { return RateAllowAt(source, NowMs()); }
```

- [ ] **Step 2: DispatchEvent 게이트** — `for (auto& t : g_triggers)` 루프 안, filter 통과 후 `JS_Call` 직전:

```cpp
if (!RateAllow(t.source)) continue;   // docs/38: skip quietly (notify once)
```

- [ ] **Step 3: 타이머 상한** — JsSetTimeout/JsSetInterval의 push 직전 각각:

```cpp
if (g_timers.size() >= kMaxTimers) {
    HostLog("[triggers] timer cap reached (" +
            std::to_string(kMaxTimers) + ") — timer dropped");
    return JS_UNDEFINED;
}
```

- [ ] **Step 4: ts int64** — `TrustRecord.ts`를 `long long ts = 0;`로. LoadTrustRecords의 ts 읽기: AgentJson에 int64 배열 접근자가 있으면 사용, 없으면 `GetArrStr`로 읽어 `strtoll` 폴백(구현자가 include/JKAgentJson.h 실제 접근자 확인해 선택). `std::to_string(long long)`은 서식 불변 — SaveTrustRecords·서버 trust_list 표시도 동일 처리(server는 Task 2).
- [ ] **Step 5: selftest 추가** — RateAllowAt 경계: 60회 전부 true → 61회 false → now+60s에서 리셋(재허용 + notified=false) → 재소진 재거부. ts 라운드트립 1900000000. (`--selftest`에 케이스 추가, 케이스 수 갱신된 assert 총계 확인)
- [ ] **Step 6: 빌드 + selftest**: `jktriggers.exe --selftest` exit 0
- [ ] **Step 7: Commit** `feat(triggers): per-source rate limit + timer cap + ts int64 (docs/38 Task 1)`

⚠️ 회귀 게이트: 서버 publish_event cap(30/10s)이 이 시점엔 없다 — 기존 triggers 프로브의 발화량(≤ 수 회)에 영향 없음을 Task 3에서 실측. 이 커밋 이후 `jkdesktop test`는 불필요(jkdesktop 미변경).

### Task 2: 서버 — publish_event 연결 cap + name 상한 + trust_list 파손 구분

**Files:**
- Modify: `engine/src/server/JKWindowServer.cpp` (publish_event 브랜치 ~1621, trust_request 브랜치 ~1440s, trust_list ~1875, 헤더 include/server/JKWindowServer.h 멤버)

**Interfaces:**
- Consumes: 기존 trust_request 거부 모양(`bad_name`은 bad_fingerprint와 동일 도형), clientsMutex_ 핫패스 전제(레슨 35 — 새 맵은 이미 락 잡힌 핫패스에서만 접근)
- Produces: publish_event 초과 `dropped` 회신 — Task 3 프로브가 실측

- [ ] **Step 1: 멤버 추가** (JKWindowServer.h, pendingApprovals_ 근처):

```cpp
// publish_event connection rate budget (docs/38 spec §4) — fixed window.
struct PublishBudget { uint64_t windowStartMs = 0; int count = 0; bool logged = false; };
std::map<uint64_t, PublishBudget> publishBudgets_;
```

- [ ] **Step 2: publish_event 브랜치 게이트** — 검증 통과 후 `PushAgentEventJson(ev)` 앞:

```cpp
// docs/38: connection-level cap — 30 events / 10s. Over-cap events are
// dropped but answered ok so senders don't turn into retry bombs.
constexpr int kPublishCap = 30;
constexpr uint64_t kPublishWindowMs = 10000;
const uint64_t nowMs = static_cast<uint64_t>(
    std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
PublishBudget& b = publishBudgets_[<requesterConnId>];
if (nowMs - b.windowStartMs >= kPublishWindowMs) {
    b.windowStartMs = nowMs; b.count = 0; b.logged = false;
}
if (++b.count > kPublishCap) {
    if (!b.logged) { b.logged = true; Log("[server] publish_event rate-capped (conn %llu)"); }
    reply = "{\"ok\":true,\"dropped\":1}";
    // — already-replied branch: this branch must skip PushAgentEventJson and
    //   the normal ok reply (mirror the branch's replied-flag pattern).
} else {
    PushAgentEventJson(ev);
    reply = "{\"ok\":true}";
}
```

`<requesterConnId>`: HandleAgentQuery가 승인 파킹에 쓰는 연결 id와 동일 변수 — 구현자가 확인해 사용. steady_clock vs 시스템 클럭 혼용 금지(이 브랜치 내부 일관).
- [ ] **Step 3: name 상한** — trust_request 브랜치, missing_name 검사 뒤 bad_fingerprint 앞:

```cpp
if (name.size() > 96) { reply = "{\"ok\":false,\"error\":\"bad_name\"}"; /* skip parking */ }
```

- [ ] **Step 4: trust_list 파손 구분** — fopen 실패 또는 `json.ok()/GetArraySize` 실패 시 `reply = "{\"ok\":false,\"error\":\"trust_store_unreadable\"}"`. 기록 0개의 정상 파일은 기존대로 ok:true. (`GetArrInt("ts")`도 Task 1과 동일 int64 처리 — 표시 전용)
- [ ] **Step 5: 빌드 + `jkdesktop.exe test` 0 실패** + `jkagentd --selftest` 0 (trust_list 도형 소비자 실측: palette /trust·jkchat /trust는 비ok 회신을 원문 표시하는지 한 번 눈으로 확인 — 변경 불필요가 스펙 전제)
- [ ] **Step 6: Commit** `feat(server): publish_event conn cap + trust_request name cap + trust_list unreadable (docs/38 Task 2)`

### Task 3: probe_agent_ratelimit.ps1 + 회귀

**Files:**
- Create: `engine/tools/probes/probe_agent_ratelimit.ps1`
- Modify: `engine/tools/triggers/rate_probe/{manifest.txt, main.js}` (테스트 번들 소스)

**Interfaces:**
- Consumes: probe_agent_trust.ps1의 기동/정리 관례(포트, process kill, trust.json 보존), BOM 필수 레슨 50, agentctl 공백 제한(레슨 26/32)

- [ ] **Step 1: 테스트 번들** — `rate_probe` (dev .js로도 드롭 가능하나 trust 게이트가 프롬프트 파킹시키므로 **패키지 경로**로: `--pack` 후 부팅 — probe_agent_trust의 자가치유 재팩 관례 재사용). `main.js`:

```js
on("ratelimit.ping", null, function (e) {
  var n = (e.data && e.data.n) || 0;
  desktop.publish("ratelimit.ping", { n: n + 1 });
});
on("ratelimit.other", null, function (e) { desktop.log("other fired"); });
on("ratelimit.timers", null, function (e) {
  for (var i = 0; i < 70; i++) setTimeout(function () {}, 1000);
});
```

셀프루프: ping 발화 → publish ping → 다음 펌프에서 재발화 → cap 60에서 정지.
- [ ] **Step 2: 프로브 체크 (7개)** — 각 PASS/FAIL + 실패 exit 1, cleanup(trust.json pack 보존 관례):
  1. 셀프루프 발화 ≤ 61 (호스트 stdout 로그의 publish 카운트 or 다른 관측 지표 — desktop.log 마커로 실측 권장: 핸들러에 `desktop.log("ping:"+n)` 추가해 로그 행 카운트)
  2. `[triggers] rate limit:` 로그 정확 1회
  3. `트리거 발화 제한` 알림 채팅 트랜스크립트에 정확 1회 (probe_agent_triggers의 트랜스크립트 판독 관례 — 레슨 33: 프로세스 죽이기 전 판독)
  4. 격리 — `ratelimit.other` 마커는 cap 도달 이후에도 여전히 발화 (다른 source 예산)
  5. 타이머 상한 — `ratelimit.timers` 발화 후 호스트 로그에 `timer cap` → `timer dropped` 7회(70-64)
  6. 서버 cap — 40회 publish_event(agentctl, 토픽 ratelimit.server, 공백 없는 data) → 마지막 10개 회신에 `"dropped":1`
  7. 회귀 — probe_agent_triggers.ps1 7/7 + probe_agent_trust.ps1 7/7 (별도 실행)
- [ ] **Step 3: 전체 회귀** — mcp 5/5, e2e 7/7, palette 4/4, chat 7/7, events 5/5
- [ ] **Step 4: Commit** `test(triggers): probe_agent_ratelimit — self-loop cap, isolation, server drop`

### Task 4: 문서화 — docs/38 + docs/32 §8 + 메모리

**Files:**
- Create: `docs/38_trigger_rate_limit.md` (docs/37 관례: 상태/섹션 요약/테스트/제한 — 스펙 내용 압축)
- Modify: `docs/32_desktop_agent_triggers.md` §8 "무한 루프 트리거…rate limiter" 줄 → `구현됨 (2026-09-13, docs/38)` 표기
- Modify: `C:\Users\kisoc\.claude\projects\I--progwork-JKENGINE\memory\MEMORY.md` roadmap 라인 + `jkengine_roadmap.md` (완료 문단 추가 — 커밋 대상 아님)

- [ ] **Step 1: docs/38 작성** (스펙 압축: §1 3지점 표, §2 예산 동작, §3 타이머, §4 서버 cap, §5 경질화 3건, §6 프로브 7체크, §7 제한)
- [ ] **Step 2: docs/32 §8 갱신**
- [ ] **Step 3: 메모리 갱신** (MEMORY.md 라인 교체 + jkengine_roadmap.md 문단 추가 — git 커밋 제외)
- [ ] **Step 4: Commit** `docs(38): trigger rate limiter — per-source budget + timer cap + publish conn cap`

## Task 의존성

Task 1 → Task 2 (독립 파일이나 회귀 기준선 정리를 위해 순차) → Task 3 (전부 의존) → Task 4 (Task 3 통과 후).