# Settings Hub Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 설정 허브 — jkapp_settings 5섹션 GUI + settings_read/settings_set 도구 2종 + audio.master 코어 펌프(모든 앱 일괄 음소거/볼륨) + receipt 보존기간.

**Architecture:** agentmgr의 검증된 도구-소비 패턴 위에 신규 서버 도구 2종(settings_read/settings_set, B 진화 씨앗 봉투)을 얹는다. 클라 이벤트는 JKClientApplication 코어의 단일 펌프(OnAgentEvent 훅)로 이관하고 vplayer의 자체 펌프는 그 훅으로 옮긴다(이중 드레인 경쟁 봉쇄 — vpt12가 회귀 게이트).

**Tech Stack:** C++17 / MinGW ucrt64 + ninja, SDL2, imgui 1.92, jkcore/jkclient, AgentJson(2레벨 파서 — bool 접근자 없음, ts=초, ok="0"/"1" 문자열).

**Spec:** `docs/superpowers/specs/2026-09-18-settings-hub-design.md` (이 플랜은 스펙과 함께 읽는다)

## Global Constraints

- 파서 계약(docs/38/53): AgentJson은 **bool 접근자 없음** — 서버가 bool을 쓸 땐 int 0/1로 직렬화, ts는 epoch 초(서버 내부 저장은 ms).
- 서버 도구 추가는 **jkagentd 4곳** 모두: 도구 목록 JSON(:63 부근) / IsKnownTool 배열(:77) / LoadPermissions 배열(:109) / MCP args-rebuild 분기(:484 부근의 selftest는 회귀).
- kPermMatrix 행 형태: `{"도구명","게이트","기본값"}` (JKWindowServer.cpp:1715).
- 스테일 빌드 금지(레슨 37): dll/jkx mtime > 소스 mtime. 앱 DLL 신규 추가 시 해당 타겟 1회 빌드 필수(레슨 57).
- 빌드: `cd engine/build && PATH=/c/msys64/ucrt64/bin:$PATH cmake --build . --target <T>`; 실행 중 jkdesktop.exe가 dll을 잠그므로 **빌드 전 `taskkill //F //IM jkdesktop.exe`** (git-bash는 `//F //IM` 더블슬래시).
- 프로브: PS5.1 ASCII-only, 출력은 `> log 2>&1` 파일 리다이렉트(grep 파이프 버퍼링 행걸 오판 방지 — 2026-09-17 레슨), jkdesktop 테스트 모드 인자는 `test`(dash 없음).
- 사내 전용 repo — 외부 공개 금지.
- docs는 산출물: 모든 결정+직감을 스펙/플랜/docs에 기록.

---

### Task 1: 서버 — settings.json KV + settings_read/settings_set 도구

**Files:**
- Modify: `engine/src/server/JKWindowServer.cpp` (kPermMatrix :1715 부근, 도구 분기 :2377 이후 아무 곳 — theme_set 뒤)
- Modify: `engine/src/server/JKWindowServer.h` (멤버 3건)

**Interfaces:**
- Consumes: `StateDir()`, `JsonEsc()`, `WritePermissionsEntry(permTool, decision)`(JKWindowServer.cpp:1762 — capture_allow 재사용), `PushAgentEventJson(const std::string&)`, `jk::agent::AgentJson`
- Produces: 도구 `settings_read` → `{"ok":true,"settings":[...],"receipts":{...}}`, 도구 `settings_set` → `{"ok":true,"applied":{...}}|bad_key|bad_value|write_failed`; 멤버 `audioMasterMute_`, `audioMasterVolume_`, `receiptRetentionDays_`

- [ ] **Step 1: 헤더 멤버 추가 (JKWindowServer.h)**

기존 상태 멤버(preMaxRects_/preFsRects_ 등이 선언된 private 섹션)에 3건 추가:

```cpp
    // 설정 허브 KV (스펙 2026-09-18-settings-hub §2.2): state/settings.json의
    // 런타임 미러 — 부팅 로드, settings_set 쓰기.
    bool audioMasterMute_ = false;
    int audioMasterVolume_ = 80;      // 0..100
    int receiptRetentionDays_ = 0;    // 0 = 무기한(현재 관행)
```

- [ ] **Step 2: KV 읽기/쓰기 헬퍼 + 도구 2종 (JKWindowServer.cpp)**

`WritePermissionsEntry` 정의 바로 아래에 헬퍼 추가:

```cpp
// 설정 허브 KV (스펙 §2.2): state/settings.json — {audio_master_mute:0/1,
// audio_master_volume:int, receipt_retention_days:int}. 작은 파일 — 실패 시
// write_failed 반환(조용한 소실 금지, docs/52 리뷰 MINOR 규약).
static std::string SettingsKvPath() {
    return StateDir() + "\\settings.json";
}
static void LoadSettingsKv(bool& mute, int& volume, int& retention) {
    std::FILE* f = std::fopen(SettingsKvPath().c_str(), "rb");
    if (!f) return;
    char buf[2048] = {};
    const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
    std::fclose(f);
    jk::agent::AgentJson json(buf);
    if (!json.ok()) return;
    int v = 0;
    if (json.GetObjInt("audio", "mute", v)) mute = (v == 1);
    if (json.GetObjInt("audio", "volume", v) && v >= 0 && v <= 100) volume = v;
    if (json.GetObjInt("retention", "days", v) && v >= 1) retention = v;
}
static bool WriteSettingsKv(bool mute, int volume, int retention) {
    char out[256];
    std::snprintf(out, sizeof(out),
        "{\"audio\":{\"mute\":%d,\"volume\":%d},\"retention\":{\"days\":%d}}",
        mute ? 1 : 0, volume, retention);
    std::FILE* f = std::fopen(SettingsKvPath().c_str(), "wb");
    if (!f) return false;
    std::fwrite(out, 1, std::strlen(out), f);
    std::fclose(f);
    return true;
}
```

도구 분기 — `theme_set` 브랜치 뒤에 추가. kPermMatrix에도 2행 추가:

```cpp
    {"settings_read", "none", "allow"},
    {"settings_set", "none", "allow"},
```

도구 본문(분기는 `theme_set` 브랜치 뒤에 배치):

```cpp
    } else if (tool == "settings_read") {
        // 스펙 §2.2: 현재 설정 수집 — B 진화 씨앗 봉투(key/kind/value).
        // bool은 int 0/1로 직렬화(파서 계약), ts는 epoch 초.
        std::string out = "{\"ok\":true,\"settings\":[";
        char item[512];
        // theme.current: theme.json의 preset 필드 — 파일은 exe 옆
        // (jk::theme::DefaultThemePath(), JKThemeConfig.cpp:43 — StateDir 아님!).
        {
            std::string preset = "dark";
            if (std::FILE* f = std::fopen(jk::theme::DefaultThemePath().c_str(), "rb")) {
                char buf[4096] = {};
                std::fread(buf, 1, sizeof(buf) - 1, f);
                std::fclose(f);
                jk::agent::AgentJson json(std::string(buf));
                std::string p;
                if (json.ok() && json.GetStr("preset", p) && !p.empty()) preset = p;
            }
            std::snprintf(item, sizeof(item), "{\"key\":\"theme.current\","
                "\"kind\":\"string\",\"value\":\"%s\"}", JsonEsc(preset).c_str());
            out += item;
        }
        // triggers: state/triggers.json 플래그(트리거 토글의 진실원).
        {
            std::FILE* f = std::fopen((StateDir() + "\\triggers.json").c_str(), "rb");
            if (f) {
                char buf[8192] = {};
                const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
                std::fclose(f);
                jk::agent::AgentJson json(buf);
                int cnt = 0;
                if (json.ok() && json.GetArraySize("triggers", cnt)) {
                    for (int i = 0; i < cnt && i < 64; ++i) {
                        std::string nm; int en = 1;
                        if (!json.GetArrStr("triggers", i, "name", nm)) continue;
                        json.GetArrInt("triggers", i, "enabled", en);
                        std::snprintf(item, sizeof(item),
                            ",{\"key\":\"trigger.%s\",\"kind\":\"bool\","
                            "\"value\":%d}", JsonEsc(nm).c_str(), en ? 1 : 0);
                        out += item;
                    }
                }
            }
        }
        // idle_minutes: state/idle_minutes 파일(trig_idle이 읽는다).
        {
            int idle = 30;
            if (std::FILE* f = std::fopen((StateDir() + "\\idle_minutes").c_str(), "rb")) {
                char buf[32] = {}; std::fread(buf, 1, sizeof(buf) - 1, f); std::fclose(f);
                const int v = std::atoi(buf);
                if (v >= 0) idle = v;
            }
            std::snprintf(item, sizeof(item), ",{\"key\":\"idle_minutes\","
                "\"kind\":\"int\",\"value\":%d}", idle);
            out += item;
        }
        std::snprintf(item, sizeof(item),
            ",{\"key\":\"receipt_retention_days\",\"kind\":\"int\",\"value\":%d}"
            ",{\"key\":\"audio_master_mute\",\"kind\":\"bool\",\"value\":%d}"
            ",{\"key\":\"audio_master_volume\",\"kind\":\"int\",\"value\":%d}",
            receiptRetentionDays_, audioMasterMute_ ? 1 : 0, audioMasterVolume_);
        out += item;
        // layouts: state/layout_*.json 열거(이름+창수는 건너뛰고 이름만).
        {
            const std::string dir = StateDir();
            WIN32_FIND_DATAA fd;
            HANDLE h = FindFirstFileA((dir + "\\layout_*.json").c_str(), &fd);
            while (h != INVALID_HANDLE_VALUE) {
                std::string name = fd.cFileName;
                name = name.substr(7, name.size() - 12); // layout_<x>.json
                std::snprintf(item, sizeof(item), ",{\"key\":\"layout.%s\","
                    "\"kind\":\"string\",\"value\":\"%s\"}",
                    JsonEsc(name).c_str(), JsonEsc(name).c_str());
                out += item;
                if (!FindNextFileA(h, &fd)) { FindClose(h); break; }
            }
        }
        // receipts 통계: 총 행수 + 최근 ts(초). 작은 파일 전체 읽기 허용.
        {
            long long rows = 0, lastTs = 0;
            if (std::FILE* f = std::fopen((StateDir() + "\\receipts.jsonl").c_str(), "rb")) {
                std::fseek(f, 0, SEEK_END); const long size = std::ftell(f);
                std::fseek(f, 0, SEEK_SET);
                std::vector<char> buf(static_cast<size_t>(size = (size > 262144 ? 262144 : size)) + 1);
                const size_t n = std::fread(buf.data(), 1, buf.size() - 1, f);
                std::fclose(f); buf[n] = '\0';
                for (const char* p = buf.data(); p && *p; ) {
                    const char* nl = std::strchr(p, '\n');
                    const size_t len = nl ? (size_t)(nl - p) : std::strlen(p);
                    const size_t tp = std::string(p, len).find("\"ts\":");
                    if (len > 2) ++rows;
                    if (tp != std::string::npos)
                        lastTs = std::atoll(p + tp + 5);
                    if (!nl) break; p = nl + 1;
                }
            }
            std::snprintf(item, sizeof(item),
                "],\"receipts\":{\"rows\":%lld,\"last_ts\":%lld}",
                rows, lastTs / 1000);
            out += item;
        }
        // settings 배열 닫기는 위 snprintf의 `]`가 담당 — 마지막 '}'만.
        out += "}";
        reply = out;
```

  **주의 (구현 시)**: receipts 통계 앞에서 `settings` 배열을 닫는 중괄호 위치가
  꼬이지 않게 — `layouts` 뒤에서 `settings` 배열을 닫고(`"}`), 그 다음
  `receipts` 오브젝트를 붙인다. 위 코드의 snprintf 문자열에서 `"` 시작부를
  `\"},\"receipts\":...` 로 정확히 맞출 것. 컴파일+agentctl 실측으로 봉투 검증.

`settings_set` 브랜치 (바로 아래에 배치):

```cpp
    } else if (tool == "settings_set") {
        // 스펙 §2.2: 화이트리스트 KV. capture_allow 키만 Ask(§2.2 —
        // 에이전트 경로의 권한 우회 봉쇄, 키별 게이트). 응답에 적용 후 값
        // (fullscreen 도구의 단일 신뢰원 규약과 동일).
        std::string key, valStr;
        int valInt = 0;
        const bool hasInt = req.GetObjInt("args", "value", valInt);
        req.GetObjStr("args", "key", key);
        if (key.empty()) { reply = "{\"ok\":false,\"error\":\"bad_key\"}"; }
        else if (key == "idle_minutes") {
            if (!hasInt || valInt < 0) {
                reply = "{\"ok\":false,\"error\":\"bad_value\"}";
            } else {
                std::FILE* f = std::fopen((StateDir() + "\\idle_minutes").c_str(), "wb");
                if (!f) reply = "{\"ok\":false,\"error\":\"write_failed\"}";
                else {
                    std::fprintf(f, "%d", valInt); std::fclose(f);
                    reply = "{\"ok\":true,\"applied\":{\"idle_minutes\":" +
                            std::to_string(valInt) + "}}";
                }
            }
        } else if (key == "receipt_retention_days") {
            if (!hasInt || valInt < 1) {
                reply = "{\"ok\":false,\"error\":\"bad_value\"}";
            } else if (!WriteSettingsKv(audioMasterMute_, audioMasterVolume_, valInt)) {
                reply = "{\"ok\":false,\"error\":\"write_failed\"}";
            } else {
                receiptRetentionDays_ = valInt;
                reply = "{\"ok\":true,\"applied\":{\"receipt_retention_days\":" +
                        std::to_string(valInt) + "}}";
            }
        } else if (key == "audio_master_mute" || key == "audio_master_volume") {
            if (key == "audio_master_mute" && hasInt && (valInt == 0 || valInt == 1)) {
                audioMasterMute_ = (valInt == 1);
            } else if (key == "audio_master_volume" && hasInt &&
                       valInt >= 0 && valInt <= 100) {
                audioMasterVolume_ = valInt;
                audioMasterMute_ = false;   // 볼륨 조작은 음소거 해제 의미
            } else {
                reply = "{\"ok\":false,\"error\":\"bad_value\"}";
                goto settings_set_done;
            }
            if (!WriteSettingsKv(audioMasterMute_, audioMasterVolume_,
                                 receiptRetentionDays_)) {
                reply = "{\"ok\":false,\"error\":\"write_failed\"}";
            } else {
                char ev[160];
                std::snprintf(ev, sizeof(ev),
                    "{\"topic\":\"audio.master\",\"data\":{\"mute\":%d,"
                    "\"volume\":%d},\"ts\":%lld}",
                    audioMasterMute_ ? 1 : 0, audioMasterVolume_,
                    (long long)std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count());
                PushAgentEventJson(ev);
                reply = "{\"ok\":true,\"applied\":{\"audio_master_mute\":" +
                        std::string(audioMasterMute_ ? "1" : "0") +
                        ",\"audio_master_volume\":" +
                        std::to_string(audioMasterVolume_) + "}}";
            }
        } else if (key == "capture_allow") {
            // §2.2: 키별 Ask — 승인 큐로 파킹된다. value 1=allow, 0=ask.
            if (!hasInt || (valInt != 0 && valInt != 1)) {
                reply = "{\"ok\":false,\"error\":\"bad_value\"}";
            } else if (AgentToolAllowed("settings_set") != AgentDecision::Allow) {
                reply = "{\"ok\":false,\"error\":\"ask_required\"}";
            } else {
                const std::string d = valInt ? "allow" : "ask";
                const std::string e1 = WritePermissionsEntry("capture_window", d);
                const std::string e2 =
                    e1.empty() ? WritePermissionsEntry("capture_region", d) : e1;
                reply = e2.empty()
                    ? "{\"ok\":true,\"applied\":{\"capture_allow\":" +
                      std::to_string(valInt) + "}}"
                    : "{\"ok\":false,\"error\":\"write_failed\"}";
            }
        } else {
            reply = "{\"ok\":false,\"error\":\"bad_key\"}";
        }
        settings_set_done:;
    }
```

  **주의 (구현 시)**: C++ goto/label 대신 조기 반환 구조로 정리할 것 — 위
  의사의 `goto settings_set_done`은 각 분기를 else-if 체인으로 정리한 뒤
  남기지 않는다. Ask 게이트: **AgentToolAllowed("settings_set")가 Allow일
  때만** capture_allow를 통과시킨다 — permission_set의 ask-고정 계약과 달리
  settings_set 행 자체는 allow이므로, Ask 도구 경로(승인 파이프라인을 거친
  재호출)에서만 도달한다.

- [ ] **Step 3: 부팅 로드 — 서버 초기화**

`StateDir()`이 처음 쓰이는 서버 초기화 지점(생성자 or Run 진입 — 기존
trust.json/permissions.json 로드 위치 옆)에:

```cpp
    LoadSettingsKv(audioMasterMute_, audioMasterVolume_, receiptRetentionDays_);
```

- [ ] **Step 4: receipt 정리 — retention 적용 시 즉시 정리**

`receipt_retention_days` 분기의 적용 직후에 호출할 헬퍼(WriteSettingsKv 아래):

```cpp
// receipts.jsonl 보존기간 정리 (스펙 §2.5): ts(epoch ms)가 경계 이전인 행
// 삭제 + 전체 리라이트. .bak 1세대 보존(bookmarks/trust 관례). 실패 시
// .bak 복원 후 false — 부분 상태 방지.
static bool PruneReceipts(int retentionDays) {
    const std::string path = StateDir() + "\\receipts.jsonl";
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return true;   // 파일 없음 = 정리할 것도 없음 (정상)
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    std::vector<char> buf(static_cast<size_t>(size) + 1);
    const size_t n = std::fread(buf.data(), 1, size, f);
    std::fclose(f);
    buf[n] = '\0';
    const long long cutoff =
        (long long)std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count() -
        (long long)retentionDays * 86400 * 1000;
    std::string kept, bak;
    size_t pos = 0;
    while (pos < n) {
        const char* begin = buf.data() + pos;
        const char* nl = static_cast<const char*>(std::memchr(begin, '\n', n - pos));
        const size_t len = nl ? (size_t)(nl - begin) : (n - pos);
        const std::string line(begin, len);
        if (len > 0) {
            const size_t tp = line.find("\"ts\":");
            const long long ts = tp == std::string::npos
                ? 0 : std::atoll(line.c_str() + tp + 5);
            if (ts >= cutoff) kept += line + "\n";
        }
        pos += len + (nl ? 1 : 0);
    }
    // .bak 1세대: 기존 .bak는 덮어쓴다(1세대 규약).
    std::remove((path + ".bak").c_str());
    std::rename(path.c_str(), (path + ".bak").c_str());
    std::FILE* w = std::fopen(path.c_str(), "wb");
    if (!w) {
        std::rename((path + ".bak").c_str(), path.c_str());  // 복원
        return false;
    }
    std::fwrite(kept.data(), 1, kept.size(), w);
    std::fclose(w);
    return true;
}
```

  settings_set의 receipt_retention_days 분기에서 `PruneReceipts(valInt)`를
  KV 저장 성공 후 호출 — 실패 시 reply write_failed(KV는 이미 적용됐지만
  다음 set에서 재정리되므로 수용; 응답은 정직).

- [ ] **Step 5: 빌드 + 수동 agentctl 실측**

```bash
taskkill //F //IM jkdesktop.exe 2>/dev/null
cd engine/build && PATH=/c/msys64/ucrt64/bin:$PATH cmake --build . --target jkserver jkdesktop
./jkdesktop.exe --server & sleep 4
./jkdesktop.exe agentctl '{"tool":"settings_read","args":{}}'
./jkdesktop.exe agentctl '{"tool":"settings_set","args":{"key":"audio_master_volume","value":60}}'
./jkdesktop.exe agentctl '{"tool":"settings_read","args":{}}'   # volume 60 반영 확인
./jkdesktop.exe agentctl '{"tool":"settings_set","args":{"key":"nope","value":1}}'  # bad_key
```

- [ ] **Step 6: Commit**

```bash
git add engine/src/server/JKWindowServer.cpp engine/src/server/JKWindowServer.h
git commit -m "feat(server): settings_read/settings_set tools + settings.json KV + receipt prune (spec 2026-09-18-settings-hub)"
```

---

### Task 2: 코어 이벤트 펌프 — OnAgentEvent 훅 + audio.master 수용

**Files:**
- Modify: `engine/include/client/JKClientApplication.h`
- Modify: `engine/src/client/JKClientApplication.cpp` (Init + Run)

**Interfaces:**
- Consumes: `JKClientSurface::SendAgentEventSubscribe(bool)`(include/client/JKClientSurface.h:112), `DrainAgentEvents(std::vector<std::string>&)`(:116), `JKSoundManager::GetInstance().SetMasterVolume(float 0..1)`(src/JKSoundManager.cpp:219)
- Produces: `virtual void OnAgentEvent(const std::string&)` (보호 가상, 기본 no-op) — Task 3의 vplayer가 오버라이드

- [ ] **Step 1: 헤더에 훅 추가**

`JKClientApplication.h` public 섹션(App의 Init/Run 선언 근처):

```cpp
    // 설정 허브(스펙 2026-09-18-settings-hub §2.3): 코어가 에이전트 이벤트의
    // 유일 소비자(단일 펌프 — 이중 드레인 경쟁 봉쇄)이고, 앱은 이 훅으로
    // 받는다. 기본 구현은 무시(이벤트 소실 무해).
    virtual void OnAgentEvent(const std::string& /*eventJson*/) {}
```

- [ ] **Step 2: Init에서 구독 + Run에서 드레인 (JKClientApplication.cpp)**

Init()의 `surface_->Connect()` 성공 직후:

```cpp
    // 코어 레벨 에이전트 이벤트 구독(스펙 §2.3): audio.master 등 서버 상태
    // 변경을 모든 클라가 받는다. 자체 구독 앱(vplayer 등)과 멱등 공존.
    surface_->SendAgentEventSubscribe(true);
```

Run() 루프 — DrainInputChannel() 뒤(`t2` 측정 전 배치, 스톨 워치독 페이즈
왜곡 방지를 위해 idle 앞):

```cpp
        // 코어 에이전트 이벤트 펌프(스펙 §2.3): 유일 소비자. audio.master는
        // 코어가 직접 JKSoundManager 마스터 게인에 적용하고, 나머지는 앱 훅으로.
        {
            std::vector<std::string> events;
            if (surface_->DrainAgentEvents(events) > 0) {
                for (const std::string& js : events) {
                    if (js.find("\"topic\":\"audio.master\"") != std::string::npos) {
                        jk::agent::AgentJson body(js);
                        int mute = 0, vol = 80;
                        body.GetObjInt("data", "mute", mute);
                        body.GetObjInt("data", "volume", vol);
                        JKSoundManager::GetInstance().SetMasterVolume(
                            mute ? 0.f : std::min(1.f, vol / 100.f));
                    }
                    OnAgentEvent(js);
                }
            }
        }
```

- [ ] **Step 3: 빌드 + selftest**

```bash
taskkill //F //IM jkdesktop.exe 2>/dev/null
cd engine/build && PATH=/c/msys64/ucrt64/bin:$PATH cmake --build . --target jkdesktop
bash engine/run_selftest.sh   # 0 failure
```

- [ ] **Step 4: Commit**

```bash
git add engine/include/client/JKClientApplication.h engine/src/client/JKClientApplication.cpp
git commit -m "feat(client): core-level agent event pump + OnAgentEvent hook + audio.master"
```

---

### Task 3: vplayer 펌프 이관 (vpt12 회귀 게이트)

**Files:**
- Modify: `engine/src/apps/ClientVPlayerApp.cpp`
- Modify: `engine/src/apps/ClientVPlayerApp.h`

**Interfaces:**
- Consumes: Task 2의 `OnAgentEvent` 훅
- Produces: vplayer의 풀스크린 미러가 코어 펌프로 이동 — 동작 불변(vpt12 18체크)

- [ ] **Step 1: OnAgentEvent 오버라이드로 미러 이관**

ClientVPlayerApp.h 클래스 선언에:

```cpp
    void OnAgentEvent(const std::string& eventJson) override;
```

ClientVPlayerApp.cpp — 기존 `PumpAgentEvents()`(≈:1838) 본문을 그대로
`OnAgentEvent(const std::string& js)` 시그니처로 이식(단, `DrainAgentEvents`
루프 제거 — 인자로 받은 단일 이벤트만 처리):

```cpp
void ClientVPlayerApp::OnAgentEvent(const std::string& js) {
    const bool fsOn = js.find("\"topic\":\"window.fullscreen\"") != std::string::npos;
    const bool fsOff = js.find("\"topic\":\"window.fullscreen_exit\"") != std::string::npos;
    if (!fsOn && !fsOff) return;
    jk::client::JKClientSurface* surface = Surface();
    if (!surface) return;
    char needle[32];
    std::snprintf(needle, sizeof(needle), "\"id\":%u,", surface->SurfaceId());
    if (js.find(needle) == std::string::npos) return;
    if (fsOn != fullscreenUi_) {
        fullscreenUi_ = fsOn;
        osdShown_ = false;
        osdAlpha_ = 0.f;
        const auto now = std::chrono::steady_clock::now();
        osdLastActivity_ = now;
        lastOsdTick_ = now;
    }
}
```

- [ ] **Step 2: 자체 구독/펌프 제거**

- RenderOverlay의 `surface->SendAgentEventSubscribe(true);` 행 삭제(코어가
  구독 — 중복 무해하나 제거해 펌프 소유가 명시된다).
- 프레임 루프의 `PumpAgentEvents();` 호출 삭제. **`PumpAgentReplies()`는
  유지** — 응답은 이벤트와 별개 큐(PollAgentReply)라 경쟁 없음.
- `PumpAgentEvents()` 메서드+선언 삭제.

- [ ] **Step 3: 빌드 + vpt12 공식런**

```bash
taskkill //F //IM jkdesktop.exe 2>/dev/null
cd engine/build && PATH=/c/msys64/ucrt64/bin:$PATH cmake --build . --target jkapp_vplayer jkx_packages
taskkill //F //IM jkdesktop.exe 2>/dev/null; sleep 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools/probes/vpt12_fullscreen.ps1 > build/vpt12_run1.log 2>&1
grep -E '^(FAIL|RESULT)' build/vpt12_run1.log   # RESULT: ALL PASS 필수
powershell -NoProfile -ExecutionPolicy Bypass -File tools/probes/vpt12_fullscreen.ps1 > build/vpt12_run2.log 2>&1
grep -E '^(FAIL|RESULT)' build/vpt12_run2.log   # 2연속 ALL PASS
```

- [ ] **Step 4: Commit**

```bash
git add engine/src/apps/ClientVPlayerApp.cpp engine/src/apps/ClientVPlayerApp.h
git commit -m "refactor(vplayer): migrate fullscreen event mirror to core OnAgentEvent pump (single drain)"
```

---

### Task 4: jkagentd 등록 (4곳)

**Files:**
- Modify: `engine/tools/jkagentd/main.cpp` (:63 도구 목록, :77 IsKnownTool, :109 LoadPermissions, :484 selftest 회귀는 자동)

**Interfaces:**
- Consumes: Task 1의 도구 2종
- Produces: MCP 에이전트가 settings_read/settings_set 사용 가능

- [ ] **Step 1: 도구 목록 JSON에 2행 추가**(:63 agent_permissions 행 옆 —
  같은 형식):

```cpp
"{\"name\":\"settings_read\",\"description\":\"Read desktop settings (theme, triggers, idle threshold, receipt retention, audio master, layouts)\",\"inputSchema\":{\"type\":\"object\",\"properties\":{}},"
"{\"name\":\"settings_set\",\"description\":\"Set a whitelisted desktop setting: idle_minutes, receipt_retention_days, audio_master_mute, audio_master_volume, capture_allow\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"key\":{\"type\":\"string\"},\"value\":{}}},"
```

  (실제 형식은 주변 행과 동일하게 — 배열 요소 1개당 1행.)

- [ ] **Step 2: IsKnownTool / LoadPermissions 배열에 `"settings_read", "settings_set"` 추가**(:77과 :109 두 곳 — permission_set/trust_revoke 등이 있는 리스트 끝).

- [ ] **Step 3: MCP args-rebuild 분기 확인** — window_fullscreen과 달리
  settings_read는 인자 없음, settings_set은 key/value 전달이므로 **특별 분기
  불필요**(기본 passthrough). :484 부근 selftest가 도구 등록을 회귀하므로
  selftest 재실행으로 검증:

```bash
./build/jkagentd.exe --selftest   # 0 failures
```

- [ ] **Step 4: Commit**

```bash
git add engine/tools/jkagentd/main.cpp
git commit -m "feat(jkagentd): register settings_read/settings_set (4 sites)"
```

---

### Task 5: jkapp_settings GUI + jkx + 팔레트

**Files:**
- Create: `engine/src/apps/ClientSettingsApp.cpp`, `engine/src/apps/ClientSettingsApp.h`, `engine/src/apps/JKAppModule_settings.cpp`
- Modify: `engine/CMakeLists.txt` (target + jkx custom command), `engine/src/apps/ClientPaletteApp.cpp`(:224 부근)

**Interfaces:**
- Consumes: Task 1 도구 2종, 기존 `theme_set`/`trigger_toggle`/`trigger_list`/`save_layout`/`restore_layout`/`agent_permissions`/`launch_app`
- Produces: 앱 `settings`(런처 셀 자동 — .jkx 스캔), 팔레트 `/settings`

- [ ] **Step 1: 모듈 셰이믈** (`JKAppModule_settings.cpp` — JKAppModule_agentmgr.cpp 동일 구조):

```cpp
// Settings hub app module (specs/2026-09-18-settings-hub). All C++ stays in
// the DLL; the host only sees the C ABI from JKAppModule.h.
#include <apps/JKAppModule.h>
#include <apps/ClientSettingsApp.h>

JKAPP_EXPORT const jk::JKAppMeta* jk_app_meta() {
    static const jk::JKAppMeta meta{ "settings", "Settings", 900, 620 };
    return &meta;
}

JKAPP_EXPORT int jk_app_run_client(const char* pipeName) {
    const jk::JKAppMeta* meta = jk_app_meta();
    jk::ClientSettingsApp app;
    if (!app.Init(meta->title, meta->width, meta->height, pipeName)) {
        return 1;
    }
    return app.Run();
}
```

- [ ] **Step 2: GUI 본문** (`ClientSettingsApp.cpp`). 구조 = ClientAgentMgrApp 패턴
  (Init: surface 구독 + `SendTool("settings_read","{}")`로 초기 수집 →
  PollAgentReply로 응답 파싱 → BuildUi). 핵심 상태와 UI:

```cpp
// ClientSettingsApp.h
#pragma once
#include <client/JKClientApplication.h>
#include <agent/JKAgentClient.h>
#include <string>
#include <vector>
#include <map>

class ClientSettingsApp : public jk::JKClientApplication {
public:
    void OnInit() override;
    void OnIdle() override;
protected:
    void BuildUi();
    void SendTool(const std::string& tool, const std::string& argsJson);
    bool PollReply(std::string& toolOut, std::string& jsonOut);
private:
    void ApplyReply(const std::string& tool, const std::string& json);
    // settings_read 결과: key -> {kind, value}
    std::map<std::string, std::pair<std::string, std::string>> kv_;
    std::vector<std::pair<std::string, int>> triggers_;   // name, enabled
    int receiptRows_ = 0; long long receiptLastTs_ = 0;
    unsigned pendingQueryId_ = 0; std::string pendingTool_;
    std::string statusLine_;
};
```

  BuildUi의 5섹션(스펙 §2.4 표 순서대로):
  1. **테마**: `ImGui::Text("현재: %s", kv_["theme.current"])` + 3버튼
     dark/light/classic → `SendTool("theme_set", "{\"preset\":\"dark\"}")`
     등. 응답 preset을 kv_에 즉시 반영.
  2. **트리거**: triggers_ 행마다 Checkbox →
     `SendTool("trigger_toggle", "{\"name\":\"<n>\",\"on\":<0|1>}")` + idle
     임계 `InputInt`+적용 버튼 → `settings_set(idle_minutes)`.
  3. **데이터**: receipts 건수/최근 ts 표시(settings_read의 receipts) + 보존기간
     콤보(7/30/90/365 — 현재값 없으면 "무기한" 라벨) →
     `settings_set(receipt_retention_days)` + 레이아웃 목록(읽기 전용) +
     이름 InputText + 저장/복원 버튼(`save_layout`/`restore_layout`).
  4. **권한**: agent_permissions 응답의 행 요약(Tool/Gate/기본)을 텍스트 목록
     + "에이전트 관리자 열기" 버튼 → `SendTool("launch_app", "{\"app\":\"agentmgr\"}")`.
  5. **장치**: 음소거 Checkbox + 볼륨 SliderInt(0-100, 커밋-on-release —
     seekingUi_ 패턴) → `settings_set(audio_master_mute/volume)` + 캡처
     스위치(체크박스 → `settings_set(capture_allow)` — **Ask 경로: 응답이
     ask_required/승인 대기면 상태 줄에 "jkchat에서 승인" 표시**) + 비활성
     행 "마이크/카메라 — 4단계 예정"(BeginDisabled).
  - 응답 파싱: AgentJson(reply) — settings_read는
    `GetArraySize("settings",n)` + `GetArrStr("settings",i,"key")` +
    `GetArrStr(...,"value")` + `GetObjInt("receipts","rows",..)`.
    applied 응답은 kv_ 갱신 + statusLine_ = "적용됨: <key>".
  - 톤: agentmgr과 동일 — 어두운 배경, 섹션 헤더 `ImGui::SeparatorText`.

- [ ] **Step 3: CMakeLists.txt** — jkapp_agentmgr 블록(:468) 아래:

```cmake
# Settings hub module (specs/2026-09-18-settings-hub): 5-section GUI over
# theme/triggers/data/permissions/devices. A launcher .jkx (no icon — packer
# treats icons as optional; launcher falls back to flat drawing).
add_library(jkapp_settings SHARED
    src/apps/JKAppModule_settings.cpp
    src/apps/ClientSettingsApp.cpp
)
target_compile_definitions(jkapp_settings PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_settings PRIVATE jkclient imgui)
set_target_properties(jkapp_settings PROPERTIES PREFIX "")
```

jkx custom command(agentmgr.jkx 블록 :831 아래):

```cmake
add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/apps/settings.jkx"
    COMMAND "$<TARGET_FILE:jkdesktop>" jkx-pack settings
    DEPENDS jkdesktop jkapp_settings
    COMMENT "Repacking apps/settings.jkx"
    VERBATIM)
```

  + `jkx_packages` DEPENDS 목록에 `apps/settings.jkx` 추가(**2곳 등록 레슨 —
  OUTPUT과 DEPENDS 둘 다**).

- [ ] **Step 4: 팔레트 /settings** (`ClientPaletteApp.cpp:224` — /agentmgr 옆):

```cpp
    } else if (cmd == "settings") {
        SendTool("launch_app", "{\"app\":\"settings\"}");
```

  도움말 행(:224 위 `AppendLog("  /agentmgr")` 옆)에 `AppendLog("  /settings");` 추가.

- [ ] **Step 5: 빌드 + jkx 리팩 + 수동 실측**

```bash
taskkill //F //IM jkdesktop.exe 2>/dev/null
cd engine/build && PATH=/c/msys64/ucrt64/bin:$PATH cmake --build . --target jkapp_settings jkx_packages
./jkdesktop.exe --server & sleep 4
./jkdesktop.exe agentctl '{"tool":"launch_app","args":{"app":"settings"}}'   # ok
# 눈확인: 창이 뜨고 5섹션 렌더, 테마 버튼/트리거 체크박스/볼륨 슬라이더 동작
taskkill //F //IM jkdesktop.exe 2>/dev/null
```

- [ ] **Step 6: Commit**

```bash
git add engine/src/apps/ClientSettingsApp.cpp engine/src/apps/ClientSettingsApp.h engine/src/apps/JKAppModule_settings.cpp engine/CMakeLists.txt engine/src/apps/ClientPaletteApp.cpp
git commit -m "feat(settings): jkapp_settings 5-section GUI + jkx + palette /settings"
```

---

### Task 6: probe_settings + 회귀 + docs + opus 최종리뷰

**Files:**
- Create: `engine/tools/probes/probe_settings.ps1`
- Modify: `docs/32_desktop_agent_triggers.md`(필요 시), 신규 `docs/54_settings_hub.md`

**Interfaces:**
- Consumes: Task 1-5 전부

- [ ] **Step 1: probe_settings.ps1** (PS5.1 ASCII, vpt12 하니스 레시피 —
  콘솔 SW_MINIMIZE + 서버 TOPMOST + agentctl JSON 이스케이프
  `$json.Replace('"', [string][char]92 + '"')`). 체크 목록(15):

```
1.  setup: server up (ping ok)
2.  setup: settings app spawned (list_windows "Settings")
3.  settings_read ok + 키 존재 (theme.current / idle_minutes / audio_master_volume)
4.  settings_read receipts 필드 (rows int)
5.  theme_set dark → settings_read value 반영
6.  theme_set light → settings_read value 반영 → dark 복원
7.  trigger_toggle trig_idle off → settings_read trigger.trig_idle 0
8.  trigger_toggle trig_idle on 복원 → 1
9.  settings_set idle_minutes 5 → state/idle_minutes 파일 "5"
10. settings_set idle_minutes 30 복원
11. settings_set receipt_retention_days 1 → receipts.jsonl 오래된 행 감소 + .bak 생성
12. settings_set receipt_retention_days 0 → bad_value (무기한은 설정 불가 — 현재값이 무기한)
13. settings_set audio_master_mute 1 → agent-events 캡처에 audio.master 이벤트 + settings_read mute 1
14. settings_set capture_allow 0 (agentctl) → ask_required (Ask 게이트 실측)
15. settings_set bad_key → bad_key / settings_set audio_master_volume 101 → bad_value
```

  이벤트 캡처는 vpt12 패턴: `Start-Process jkdesktop.exe agent-events 4 -RedirectStandardOutput` 후 파싱.

- [ ] **Step 2: 공식런 2연속 ALL PASS**

```bash
taskkill //F //IM jkdesktop.exe 2>/dev/null; sleep 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools/probes/probe_settings.ps1 > build/probe_settings_run1.log 2>&1
grep -E '^(FAIL|RESULT)' build/probe_settings_run1.log
# 2회 반복 — 2연속 ALL PASS 필수
```

- [ ] **Step 3: 회귀 전수**

```bash
bash engine/run_selftest.sh                                  # 0 failure
./build/jkagentd.exe --selftest                              # 0 failures
powershell tools/probes/probe_agentmgr.ps1                   # ALL PASS
powershell tools/probes/vpt12_fullscreen.ps1                 # ALL PASS (펌프 리팩 게이트)
powershell tools/probes/vpt11_reverse.ps1                    # ALL PASS
powershell tools/probes/probe_agent_palette.ps1              # ALL PASS (/settings 포함)
```

- [ ] **Step 4: docs/54 as-built + docs/32 §7 표 갱신 + 메모리**

- docs/54: 스펙 대비 결정 사항(as-built), 실측, 레슨.
- docs/32 §7 후보 표(docs/superpowers/specs/2026-09-09-desktop-agent-platform-design.md
  §7 표)의 설정 허브 행에 구현 마크.
- 메모리: MEMORY.md + roadmap "다음 세션 후보" 갱신.

- [ ] **Step 5: opus 최종리뷰** — 전체 diff + probe_settings 실측 전달, 판정
  반영(상임 관례), 커밋:

```bash
git add engine/tools/probes/probe_settings.ps1 docs/54_settings_hub.md
git commit -m "test(settings): probe_settings + docs/54 as-built (spec 2026-09-18-settings-hub)"
```