# agentmgr/SDK 잔여 레저 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** docs/53 §9 NON-BLOCKING 잔여 + approve self-approve 게이트 + docs/51 C 후보 잔여(--attach, 콘솔 앱→.jkx 승격)를 전부 소각한다.

**Architecture:** 서버(JKWindowServer) 승인 파이프라인 소폭 경화 2건, jkchat Win32 승인 스트립 큐화, jkctl CLI 3종 확장(--attach/promote/install .jkx). 서버 도구·와이어 프로토콜 변경 없음.

**Tech Stack:** C++ (MinGW/ucrt64, CMake), Win32 (jkchat), PowerShell 5.1 probes, JKJkxFile 컨테이너(JKX1).

**Spec:**
- `docs/superpowers/specs/2026-09-16-agent-manager-design.md` §7 (approve self-approve)
- `docs/superpowers/specs/2026-09-16-p4-sdk-contract-design.md` §4 (--attach 예제)
- as-built: `docs/53_desktop_agent_mgr.md` §9, `docs/51_p4_sdk_contract.md` "C 후보"

## Global Constraints

- 빌드: `cd I:/progwork/JKENGINE/engine/build && PATH=/c/msys64/ucrt64/bin:$PATH cmake --build . --target <T>` — **레슨 57**: 앱 DLL 신규 분은 아니지만 agentmgr.jkx 재팩 필요(레슨 18): DLL 빌드 후 반드시 `--target jkx_packages` 재팩.
- 빌드 후 **mtime 게이트**(레슨 37): 산출 exe/dll mtime > 수정한 소스 mtime 확인. 링크 실패(grep 필터로 가려짐) 방지 — 빌드 출력을 grep으로 줄이지 않는다.
- 프로브 규약: PS5.1, ASCII-only + **BOM**(lesson 50 — Write tool 생성 파일은 BOM 필요), Heisenberg 클린바이너리 공식런(lesson 61), `Receive-Job` 결과는 `-join` 후 판정(docs/53 §8).
- **jkdesktop 테스트 인자는 `test`** (dash 없음 — docs/53 §8).
- 코드 스타일: 주석은 한국어 설계 사유 위주(기존 코드 관례), 함수 앞 규약 주석.
- 최종리뷰: 전체 커밋 후 opus 리뷰(사용자 상임 관례).
- 실행 완료 후 roadmap/killer-app 메모리의 "실행 지시" 항목과 동기화.

---

### Task 1: approve self-approve 게이트 (서버)

**Files:**
- Modify: `engine/src/server/JKWindowServer.cpp` (approve 브랜치, ~line 2871)
- Test: `engine/tools/probes/probe_approve_self.ps1` (신규)

**Interfaces:**
- Consumes: `HandleAgentQuery(JKClientConnection& client, ...)` 파라미터 `client.Id()` (호출자 연결 id), `pendingApprovals_`의 `PendingApproval.requesterId`.
- Produces: approve 도구의 새 즉답 에러 `{"ok":false,"error":"self_approve"}` — permission_set/trust_revoke kind에 한해 요청자=호출자일 때. 파킹은 건드리지 않음(재시도/타임아웃 유지).

**설계 룰링 (스펙 §7 권장의 축소 적용 — 컨트롤러 결정, 스펙 ledger 기록):**
- 검사를 **모든 kind에 적용하면 jkchat의 설계된 UX가 깨진다**: ask 모드에서 채팅 자신의 `/close`는 파킹되고 채팅 자신의 스트립이 해소한다(docs/31 §3 — non-blocking 채팅의 존재 이유). 요청자=호출자인 legit 승인은 close_window뿐이다.
- 따라서 **permission_set/trust_revoke에만** self-approve를 봉쇄한다. 이 두 kind의 파킹 요청자(agentmgr 앱/jkagentd)가 스스로 approve를 보낼 legit 경로는 오늘도 없다 — 위반만 막고 기존 흐름은 무손상.
- 잔여 위험(기록): 콘솔 앱이 jkctl agent 2회로 park+approve하는 경로는 여전히 열려 있다(다른 연결이므로). docs/38 룰링과 동일한 위협 모델 — 같은 머신 신뢰 스크립트는 자기 예산을 회피할 수 있다. 스펙 §7에 잔여로 명시.

- [x] **Step 1: 서버 approve 브랜치에 게이트 추가**

`engine/src/server/JKWindowServer.cpp` approve 브랜치(2871행 부근). 루프 선두의 requestId 매치 직후에 삽입하고, 꼬리의 `if (!resolved)`가 self_approve 응답을 덮어쓰지 않게 플래그를 추가한다:

```cpp
    } else if (tool == "approve") {
        // M2 chat: resolve one pending approval. The parked query's reply
        // goes to the ORIGINAL requester; the approver gets the ack below.
        int request = 0;
        std::string decision;
        req.GetObjInt("args", "request", request);
        req.GetObjStr("args", "decision", decision);
        const bool allow = (decision == "allow");
        bool resolved = false;
        bool selfApprove = false;  // 스펙 §7 후속: self-approve 봉쇄
        for (auto it = pendingApprovals_.begin();
             it != pendingApprovals_.end(); ++it) {
            if (it->requestId != static_cast<uint32_t>(request)) continue;
            // 승인 파이프라인의 2단 우회 봉쇄(스펙 §7 리뷰 후속): 파킹을
            // 자기 연결에서 approve하면 승인 없는 허가가 된다. close_window는
            // 예외 — ask 모드에서 채팅 자신의 /close를 자기 스트립으로 해소하는
            // 것은 docs/31 §3의 설계된 UX다. permission_set/trust_revoke에는
            // legit 요청자-자체승인 경로가 없다.
            if (it->requesterId == client.Id() &&
                (it->kind == "permission_set" || it->kind == "trust_revoke")) {
                selfApprove = true;
                break;
            }
            resolved = true;
            // ... (기존 본문 그대로 — close_window/run_console_app/permission_set/
            //  trust_revoke 해소 + approval_resolved 브로드캐스트 + erase)
        }
        if (selfApprove)
            reply = "{\"ok\":false,\"error\":\"self_approve\"}";
        else if (!resolved) reply = "{\"ok\":false,\"error\":\"unknown_request\"}";
```

주의: 기존 `if (!resolved) reply = ...unknown_request...` 한 줄을 위의 3분기로 교체한다. 파킹 엔트리는 erase하지 않은 채 break — 다른 표면(채팅)의 승인은 여전히 가능하다.

- [x] **Step 2: 빌드 + mtime 게이트**

```bash
cd /i/progwork/JKENGINE/engine/build && PATH=/c/msys64/ucrt64/bin:$PATH cmake --build . --target jkdesktop 2>&1 | tail -5
```
Expected: 링크 성공. `ls -l jkdesktop.exe` mtime이 소스 mtime보다 최신이면 통과.

- [x] **Step 3: probe_approve_self.ps1 작성 (raw 파이프 클라이언트 — 같은 연결에서 park+approve)**

`engine/tools/probes/probe_approve_self.ps1` (ASCII-only, BOM 포함 — Write tool 사용). 핵심: agentctl은 쿼리 1건당 프로세스 1개라 "같은 연결" 시험을 못 한다 — 와이어(JKX1 아님, JKPipe)를 PS5.1로 직접 드라이브한다.

```powershell
# probe_approve_self: approve self-approve gate (docs/53 spec section 7).
# One raw-pipe connection parks permission_set (fixed-ask, no permissions
# seeding needed) and then approves its OWN parked request -> expect
# self_approve. A second approve from agentctl (different connection) must
# still resolve it (regression). ASCII-only PS5.1 + BOM (lesson 50).
$ErrorActionPreference = "Continue"
$script:fail = 0
function Check([string]$name, [bool]$ok, [string]$detail) {
    if ($ok) { Write-Host "ok: $name" } else { $script:fail++; Write-Host "FAIL: $name ($detail)" }
}
$exe = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
$root = Split-Path $exe
$permFile = Join-Path $root "permissions.json"

# guard: this probe's approved RMW creates permissions.json - it must own it.
if (Test-Path $permFile) { Write-Host "ABORT - permissions.json exists"; exit 1 }

function Invoke-Agentctl([string]$json) {
    $escaped = $json -replace '"', '\"'
    return (& $exe agentctl $escaped) -join "`n"
}

Get-Process jkdesktop,jktriggers -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $root -WindowStyle Hidden
Start-Sleep -Seconds 3

# --- raw pipe client (JKX wire: 12B header magic/type/len + payload) -------
$pipe = New-Object System.IO.Pipes.NamedPipeClientStream(".", "JKWindowServerPipe",
    [System.IO.Pipes.PipeDirection]::InOut)
$pipe.Connect(5000)
function ReadExact([System.IO.Pipes.NamedPipeClientStream]$s, [int]$n) {
    $buf = New-Object byte[] $n; $off = 0
    while ($off -lt $n) {
        $r = $s.Read($buf, $off, $n - $off)
        if ($r -le 0) { throw "pipe closed" }
        $off += $r
    }
    return $buf
}
function SendMsg([System.IO.Pipes.NamedPipeClientStream]$s, [int]$type, [byte[]]$payload) {
    $hdr = New-Object byte[] 12
    [BitConverter]::GetBytes([uint32]0x4A4B0001).CopyTo($hdr, 0)
    [BitConverter]::GetBytes([uint32]$type).CopyTo($hdr, 4)
    [BitConverter]::GetBytes([uint32]$payload.Length).CopyTo($hdr, 8)
    $s.Write($hdr, 0, 12)
    if ($payload.Length -gt 0) { $s.Write($payload, 0, $payload.Length) }
    $s.Flush()
}
# Hello (type 1): protocolVersion=2 + pid. AgentEventSubscribe (type 19): 4B.
$hello = New-Object byte[] 8
[BitConverter]::GetBytes([uint32]2).CopyTo($hello, 0)
[BitConverter]::GetBytes([uint32]$PID).CopyTo($hello, 4)
SendMsg $pipe 1 $hello
$sub = New-Object byte[] 4
[BitConverter]::GetBytes([uint32]1).CopyTo($sub, 0)   # subscribe = 1
SendMsg $pipe 19 $sub
function SendQuery([System.IO.Pipes.NamedPipeClientStream]$s, [uint32]$qid, [string]$json) {
    $body = [Text.Encoding]::UTF8.GetBytes($json)
    $payload = New-Object byte[] (8 + $body.Length)
    [BitConverter]::GetBytes([uint32]$qid).CopyTo($payload, 0)
    [BitConverter]::GetBytes([uint32]$body.Length).CopyTo($payload, 4)
    [Array]::Copy($body, 0, $payload, 8, $body.Length)
    SendMsg $s 17 $payload
}
function ReadFrame([System.IO.Pipes.NamedPipeClientStream]$s) {
    $hdr = ReadExact $s 12
    return @{
        type = [BitConverter]::ToUInt32($hdr, 4)
        len  = [BitConverter]::ToUInt32($hdr, 8)
    }
}
function ReadPayload([System.IO.Pipes.NamedPipeClientStream]$s, [int]$len) {
    if ($len -le 0) { return "" }
    return [Text.Encoding]::UTF8.GetString((ReadExact $s $len))
}

# --- 1. park permission_set (fixed-ask -> parks unconditionally) -----------
SendQuery $pipe 1 '{"tool":"permission_set","args":{"tool":"close_window","decision":"allow"}}'
# read frames until the approval_request push arrives (type 20)
$reqId = 0
foreach ($i in 1..15) {
    $f = ReadFrame $pipe
    if ($f.type -eq 20) {
        $ev = ReadPayload $pipe $f.len
        if ($ev -match '"request":(\d+)' -and $ev -match '"kind":"permission_set"') {
            $reqId = [int]$Matches[1]; break
        }
    } elseif ($f.type -eq 18) { ReadPayload $pipe $f.len | Out-Null }
    else { ReadPayload $pipe $f.len | Out-Null }
}
Check "1-parked" ($reqId -gt 0) ("request=" + $reqId)

# --- 2. approve from the SAME connection -> self_approve -------------------
SendQuery $pipe 2 ('{"tool":"approve","args":{"request":' + $reqId + ',"decision":"allow"}}')
$gotSelf = $false
foreach ($i in 1..10) {
    $f = ReadFrame $pipe
    $json = ReadPayload $pipe $f.len
    if ($f.type -eq 18) {
        $qid = 0
        # reply payload header = queryId(4)+ok(4)+jsonLen(4) then json
        $hdr2 = ReadExact $pipe 12
        $qid = [BitConverter]::ToUInt32($hdr2, 0)
        $jlen = [BitConverter]::ToUInt32($hdr2, 8)
        $reply = ReadPayload $pipe $jlen
        if ($qid -eq 2) { $gotSelf = ($reply -match 'self_approve'); break }
    }
}
Check "2-self-approve-rejected" $gotSelf "expect self_approve"

# --- 3. approve from agentctl (different connection) -> resolves -----------
$ap = Invoke-Agentctl ('{"tool":"approve","args":{"request":' + $reqId + ',"decision":"allow"}}')
Check "3-cross-approve" ($ap -match 'approved\\?":true') $ap
# the parked query's reply must land on OUR connection (queryId 1)
$gotReply = $false
foreach ($i in 1..10) {
    $f = ReadFrame $pipe
    $hdr2 = ReadExact $pipe 12   # AgentReplyHeader queryId/ok/jsonLen
    $qid = [BitConverter]::ToUInt32($hdr2, 0)
    $jlen = [BitConverter]::ToUInt32($hdr2, 8)
    $reply = ReadPayload $pipe $jlen
    if ($qid -eq 1) { $gotSelf = ($reply -match '"written":true'); break }
}
Check "4-parked-reply" $gotSelf "parked permission_set resolved by cross-approve"
$fraw = Get-Content $permFile -Raw -ErrorAction SilentlyContinue
Check "5-file" ($fraw -match '"close_window":"allow"') "RMW wrote the file"

$pipe.Close()
# cleanup: the probe created permissions.json via the approved RMW
Remove-Item $permFile -Force -ErrorAction SilentlyContinue
Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
if ($script:fail -eq 0) { Write-Host "RESULT: ALL PASS" } else { Write-Host ("RESULT: {0} FAILURE(S)" -f $script:fail); exit 1 }
```

주의(실행자): 위에서 `$gotSelf` 변수를 2/3 검사에서 재사용했다 — 3번 검사는 별도 변수 `$gotParked`로 이름을 바꿔 쓸 것(복붙 실수 방지). 프레임 읽기 순서는 실제 와이어와 일치해야 한다: **Reply = 12B WireHeader(type=18) → payload: AgentReplyHeader 12B(queryId, ok, jsonLen) → json**. Event = 12B 헤더(type=20) → AgentEventHeader 4B(jsonLen) → json — 즉 **event의 jsonLen은 payload 앞 4B**다. 위 `ReadFrame`은 와이어 헤더의 length만 읽으므로, event의 실제 json은 `ReadPayload $pipe $f.len`에서 와이어 헤더 length 그대로 읽으면 된다(서버가 WireHeader.length에 jsonLen을 쓰는지 WriteAgentJson 구현에서 확인하고, 다르면 보정할 것 — **1차 가지치기로 서버 측 WriteAgentJson/ReadAgentJson 구현(src/ipc/JKWireProtocol.cpp)을 읽고 프레임 규약을 확인한 뒤 프로브를 맞출 것**).

- [x] **Step 4: 공식런**

```bash
cd /i/progwork/JKENGINE/engine/tools/probes && powershell -ExecutionPolicy Bypass -File probe_approve_self.ps1
```
Expected: 5체크 ALL PASS. permission_set은 fixed-ask라 permissions.json 시딩 불필요. 실패 시 서버 로그(stdout)로 승인 파이프라인 동작 확인.

- [x] **Step 5: Commit**

```bash
cd /i/progwork/JKENGINE && git add engine/src/server/JKWindowServer.cpp engine/tools/probes/probe_approve_self.ps1 && git commit -m "feat(server): approve self-approve gate for permission_set/trust_revoke (spec 7)
```

---

### Task 2: 서버 state 파일 전체 읽기 (4KB/64KB 상한 해소)

**Files:**
- Modify: `engine/src/server/JKWindowServer.cpp` — `WritePermissionsEntry` (~1664), `RevokeTrustRecord` (~1737)
- Test: `engine/tools/probes/probe_agentmgr.ps1` (체크 추가)

**Interfaces:**
- Consumes: 없음(정적 함수 내부 변경).
- Produces: 동작만 변경 — 반환 계약 불변(빈 문자열=성공, "write_failed"/"not_found"/"trust_store_unreadable").

- [x] **Step 1: WritePermissionsEntry 전체 읽기**

1674-1688행의 4096 스택 버프를 전체 읽기로 교체:

```cpp
    std::map<std::string, std::string> values;
    if (std::FILE* f = std::fopen(path.c_str(), "rb")) {
        // 전체 읽기 (docs/53 §9 잔여): 4KB 스택 버프는 파일 뒤쪽의 알려진
        // 도구 행을 잘라내 RMW가 기본값으로 되돌렸다. 256KiB 상한 = 이상
        // 파일 메모리 가드 — 초과 시 잘린 JSON이 파싱 실패하면 전재기록이
        // 기본값으로 복원한다(자기 치유, 알려진 키만 기록되는 RMW 원래 의미).
        std::fseek(f, 0, SEEK_END);
        const long sz = std::ftell(f);
        std::fseek(f, 0, SEEK_SET);
        if (sz > 0) {
            const size_t cap =
                std::min<size_t>(static_cast<size_t>(sz), 256 * 1024);
            std::vector<char> buf(cap + 1, '\0');
            const size_t n = std::fread(buf.data(), 1, cap, f);
            buf[n] = '\0';
            jk::agent::AgentJson json(buf.data());
            std::string v;
            if (json.ok()) {
                for (const AgentPermRow& r : kPermMatrix) {
                    if (json.GetStr(r.tool, v) &&
                        (v == "allow" || v == "ask" || v == "deny")) {
                        values[r.tool] = v;
                    }
                }
            }
        }
        std::fclose(f);
    }
```

- [x] **Step 2: RevokeTrustRecord 전체 읽기**

1746-1752행의 64KB 버프를 전체 읽기로 교체:

```cpp
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return "trust_store_unreadable";
    // 전체 읽기 (docs/53 §9 잔여): 64KB 캡은 장기 설치의 스토어에서 뒤쪽
    // 레코드를 not_found로 미끄러뜨린다. 8MiB 상한 = 이상 파일 가드;
    // 초과분의 레코드는 여전히 not_found(정직한 오류 — 잘림 조용 통과 아님).
    std::fseek(f, 0, SEEK_END);
    const long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    const size_t cap = sz > 0
        ? std::min<size_t>(static_cast<size_t>(sz), 8u * 1024 * 1024) : 0;
    std::vector<char> buf(cap + 1, '\0');
    const size_t n = std::fread(buf.data(), 1, cap, f);
    std::fclose(f);
    buf[n] = '\0';
    const std::string text(buf.data());
```

(이하 TrustRecordText/콤마 수술/.bak/쓰기 경계는 무변경.)

- [x] **Step 3: 빌드 + mtime 게이트** (`--target jkdesktop`)

- [x] **Step 4: probe_agentmgr.ps1에 2체크 추가**

`3d-largefile-preserves`(permissions 4KB 픽스) — 체크 3 뒤에 삽입. 시나리오: >4KB 파일에서 4KB 너머의 `trust_request:"ask"`가 RMW 후에도 보존되는가(구코드는 4KB 절단 → 파싱 실패 → 전재기록이 기본값 deny로 되돌렸다). 판정 대상은 RMW 시의 **타깃 이외 키 보존**이므로 RMW가 실제 일어나는 **allow** 결정으로 승인한다:

```powershell
# --- 3d. >4KB permissions.json: known-key rows beyond the 4KB read cap
# survive the RMW (docs/53 section 9 leftover). filler puts trust_request
# past the 4KB mark; permission_set on ANOTHER tool must preserve it.
$big = '{"' + ("x" * 4080) + 'filler":"' + ("a" * 64) + '","close_window":"allow","trust_request":"ask"}'
Write-NoBom $permFile $big
$job3 = Start-Job -ScriptBlock {
    param($e)
    $x = '{"tool":"permission_set","args":{"tool":"publish_event","decision":"deny"}}' -replace '"', '\"'
    & $e agentctl $x
} -ArgumentList $exe
Start-Sleep -Seconds 3
Invoke-Agentctl '{"tool":"approve","args":{"request":3,"decision":"allow"}}' | Out-Null
Start-Sleep -Seconds 2
$reply3 = (Receive-Job $job3 -Wait) -join "`n"
Check "3d-a-reply" ($reply3 -match '"written":true') $reply3
$f3 = Get-Content $permFile -Raw
Check "3d-b-preserved" ($f3 -match '"trust_request":"ask"' -and $f3 -match '"close_window":"allow"' -and $f3 -match '"publish_event":"deny"') $f3
```

주의: request id는 서버에서 누적 시퀀스라 — **실행자는 probe_agentmgr의 기존 request id 흐름(체크 3=1, 체크 4=2 사용)을 먼저 확인하고 새 체크의 id(위 예: 3)를 누적 순서에 맞게 배정할 것.** 승인은 allow로 박는다 — RMW는 allow 시에만 일어난다(deny는 파일 불변). 타깃을 `publish_event`(approve 루프에서 부수효과 없는 도구)로 쓴다 — `run_console_app`을 타깃으로 하면 allow 승인 시 실제 스폰이 일어난다(승인 루프의 `allow && kind == "run_console_app"` 분기, JKWindowServer.cpp:2893). 3d-b는 3점 판정: `trust_request:"ask"` 보존(4KB 너머 — 픽스의 본 대상) + `close_window:"allow"` 보존 + 타깃 `publish_event:"deny"` 기록.

**request id 재시도 가드**: 체크 3/4가 id 1/2를 썼으므로 3d는 3을 쓴다. 만약 approve 응답이 `unknown_request`면 id를 +1해 1회 재시도하는 래퍼를 Check 앞에 두는 것을 권장(체크 4/5 사이 삽입 위치에 따라 id가 밀릴 수 있음).

`5c-largestore-revoke`(trust 64KB 픽스) — 체크 5 뒤에 삽입. 1000행(~140KB > 64KB) 스토어에서 꼬리 레코드 해지:

```powershell
# --- 5c. >64KB trust store: a record past the old 64KB read cap revokes.
# (probe owns trust.json here - teardown restores the pre-state copy)
$tailFp = "sha256:" + ("ee" * 32)
$sb = New-Object System.Text.StringBuilder
[void]$sb.Append('{"records":[')
for ($i = 0; $i -lt 999; $i++) {
    if ($i -gt 0) { [void]$sb.Append(',') }
    [void]$sb.Append(('{"fingerprint":"sha256:' + ("f0" * 32) + '","name":"filler' + $i + '","source":"dev","ts":1700000000000}'))
}
[void]$sb.Append(',')
[void]$sb.Append(('{"fingerprint":"' + $tailFp + '","name":"mgrtail","source":"dev","ts":1700000000000}'))
[void]$sb.Append(']}')
Write-NoBom $trust $sb.ToString()
$rv = Invoke-Agentctl ('{"tool":"trust_revoke","args":{"fingerprint":"' + $tailFp + '"}}')
Check "5c-large-revoke" ($rv -match 'restart_needed') $rv
$tAfter = Get-Content $trust -Raw
Check "5d-large-removed" ($tAfter -notmatch $tailFp) "tail record must be gone"
Check "5e-large-intact" ($tAfter -match 'filler0' -and $tAfter -match 'filler998') "other records must survive"
```

(filler 지문 999개는 동일 해시("f0"*32)다 — 중복 지문 레코드가 스토어에 쌓여도 TrustRecordText는 첫 매치만 찾으므로 무방하나, **서버 신뢰 로더가 중복 지문을 허용하는지** 실측이 필요하면 filler마다 `("f0"*31) + hex2자` 유니크로 바꿀 것. 안전하게 유니크로 쓰는 것을 권장: `"sha256:f0" + ("0" + $i) ...` 형태로 64자 유니크 hex 구성.)

**실행 시정 (as-run)**: 위 5c 스니펫의 직접 `Invoke-Agentctl` 호출은 실패한다 —
trust_revoke는 기본 Ask 게이트라 직접 호출은 파킹→60s approval_timeout이고
`restart_needed`는 영원히 오지 않는다(플랜이 게이트를 놓쳤다). 시정: 체크 4와
같은 승인 E2E(파킹 job + approve **request 4**; 3d가 request 3을 썼으므로)로
배치 — trust_revoke 분기의 존재 확인 읽기(~2070)와 RevokeTrustRecord 두
전체-읽기 경로를 모두 검증한다. 또한 3d는 "체크 3 뒤"가 아니라 **체크 4 뒤**에
삽입했다(플랜 예시의 request id 3과 일치시키기 위해; 체크 3=1, 4=2 사용).
그리고 3d의 본 판별 키는 `close_window:"allow"`(기본값 deny와 다름)다 —
`trust_request:"ask"`는 기본값과 같아 단독으로는 픽스를 판별하지 못한다
(3점 판정의 보조 축). 공식런 21/21 ALL PASS.

- [x] **Step 5: 공식런** — `powershell -ExecutionPolicy Bypass -File probe_agentmgr.ps1` → 기존 16체크 + 신규 2~5체크 ALL PASS. (환경 전제는 docs/53 §7: build/apps/triggers 설치 + trust.json 레코드 존재.)
- [x] **Step 6: Commit**

```bash
git add engine/src/server/JKWindowServer.cpp engine/tools/probes/probe_agentmgr.ps1
git commit -m "fix(server): full-file reads in permissions RMW + trust revoke (docs/53 s9 caps)"
```

---

### Task 3: agentmgr ImGui Begin/End 짝 보장

**Files:**
- Modify: `engine/src/apps/ClientAgentMgrApp.cpp` (~301)

**Interfaces:** 없음 — imgui.h:431 계약("Always call a matching End() for each Begin() call, regardless of its return value") 준수. 도달 불가 경로지만 docs/53 §9 명시 잔여.

- [x] **Step 1: End 짝 보장**

```cpp
    if (!ImGui::Begin("agentmgr", nullptr,
                      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove)) {
        ImGui::End();  // Begin 계약: 반환값 무관 항상 짝 (imgui.h:431)
        return;
    }
    ImGui::End();
```

- [x] **Step 2: 빌드** — `--target jkapp_agentmgr` 후 `--target jkx_packages` (레슨 18/57: agentmgr.jkx 재팩 필수). dll/jkx mtime 게이트 확인.
- [x] **Step 3: spawn 스모크** — 서버 기동 → agentctl launch_app agentmgr → list_windows에 "Agent Manager". (probe_agentmgr의 체크 1이 커버 — Task 2 공식런이 이를 수행하므로 여기선 수동 스모크 불필요, Task 2 뒤에 배치해도 무방하나 독립 커밋.)
- [x] **Step 4: Commit**

```bash
git add engine/src/apps/ClientAgentMgrApp.cpp
git commit -m "fix(agentmgr): ImGui Begin/End pairing on clipped path (docs/53 s9)"
```

---

### Task 4: jkchat 승인 스트립 큐 (다중 승인)

**Files:**
- Modify: `engine/tools/jkchat/main.cpp` (~155-264, ~592-604)

**Interfaces:**
- Consumes: 기존 `ShowApproval`/`ShowTrustApproval`/`ShowPermissionApproval`/`ShowTrustRevokeApproval` 서명(그대로 유지 — 내부만 큐로), `g_approvalRequest`, `HideApproval()`, `Decision()`.
- Produces: 승인 요청이 스트립 점유 중이면 큐 대기 → resolved 시 순차 표시. UI 레이아웃 무변경.

- [x] **Step 1: 큐 도입 + Show* 리팩**

`g_approvalRequest` 옆(158행 부근)에 추가:

```cpp
// 승인 스트립 큐 (docs/53 §9 잔여 — 단일 슬롯은 후발 요청이 선행 요청을
// 덮어썼고, 파킹된 선행 요청은 시간초과까지 보이지 않았다). 스트립 1개를
// 큐로 순환: 점유 중엔 대기, resolved되면 다음 요청이 스트립에 오른다.
struct ApprovalUi {
    uint32_t request;
    std::wstring text;
};
static std::vector<ApprovalUi> g_approvalQueue;

static void ShowApprovalText(const std::wstring& text) {
    SetWindowTextW(g_hPrompt, text.c_str());
    ShowWindow(g_hPrompt, SW_SHOWNORMAL);
    ShowWindow(g_hAllow, SW_SHOWNORMAL);
    ShowWindow(g_hDeny, SW_SHOWNORMAL);
    EnableWindow(g_hAllow, TRUE);
    EnableWindow(g_hDeny, TRUE);
}

// approval_request 수신 경유점: 스트립이 비었으면 즉시 표시, 아니면 대기 큐.
static void EnqueueApproval(uint32_t request, const std::wstring& text) {
    if (g_approvalRequest == 0) {
        g_approvalRequest = request;
        ShowApprovalText(text);
    } else {
        g_approvalQueue.push_back({request, text});
        Log(L"[대기] 승인 요청 #" + std::to_wstring(request) +
            L" — 현재 승인 처리 후 표시");
    }
}
```

기존 4개 Show*는 본문 끝의 `g_approvalRequest = request; ... ShowWindow x3` 블록을 다음 형태로 교체(문구 생성은 그대로):

```cpp
static void ShowApproval(const std::string& title, uint32_t targetId,
                         uint32_t request) {
    wchar_t buf[512];
    _snwprintf_s(buf, _TRUNCATE, L"[%s #%u] 창을 닫을까요?", Utf8ToWide(title).c_str(),
                 targetId);
    EnqueueApproval(request, buf);
}
```
(ShowTrustApproval/ShowPermissionApproval/ShowTrustRevokeApproval 동일 패턴 — 각자의 문구 유지, `EnqueueApproval(request, buf)` 호출로 끝맺음.)

- [x] **Step 2: approval_resolved 핸들러 큐 처리**

```cpp
    } else if (ev.topic == "agent.approval_resolved") {
        jk::agent::AgentJson e(ev.json);
        std::string decision;
        int request = 0;
        e.GetInt("request", request);
        e.GetStr("decision", decision);
        if (g_approvalRequest == static_cast<uint32_t>(request)) {
            // 프론트 해소 — 큐의 다음 요청이 스트립에 오르거나 숨김.
            if (!g_approvalQueue.empty()) {
                ApprovalUi next = g_approvalQueue.front();
                g_approvalQueue.erase(g_approvalQueue.begin());
                g_approvalRequest = next.request;
                ShowApprovalText(next.text);
            } else {
                HideApproval();
            }
        } else {
            // 다른 표면(다른 채팅창)이 해소한 대기 항목 — 큐에서 제거.
            for (auto it = g_approvalQueue.begin(); it != g_approvalQueue.end(); ++it) {
                if (it->request == static_cast<uint32_t>(request)) {
                    g_approvalQueue.erase(it);
                    break;
                }
            }
        }
        Log("[승인] request " + std::to_string(request) + " → " + decision);
    }
```

주의: `HideApproval()`이 버튼 Enable 상태를 건드리지 않으므로(현행 그대로), 프론트 교체 시 `ShowApprovalText`가 EnableWindow(TRUE)를 해준다. `Decision()`의 비활성화(레이스 방지)는 유지 — approval_timeout도 approval_resolved(decision=timeout)로 온다(서버 1448-1455행 실측 확인)이므로 타임아웃된 프론트도 자동 순환한다.
- [x] **Step 3: 빌드** — `--target jkchat` + mtime 게이트.
- [x] **Step 4: 회귀** — probe_agent_chat.ps1 (스트립 와이어 경로 — chat exe 스폰만 검증) PASS 확인.
- [x] **Step 5: Commit**

```bash
git add engine/tools/jkchat/main.cpp
git commit -m "feat(chat): approval strip queue for concurrent approvals (docs/53 s9)"
```

---

### Task 5: jkctl ask --attach

**Files:**
- Modify: `engine/tools/jkctl/main.cpp` (Ask 함수 ~104-133, wmain 디스패치 ~543)
- Test: `engine/tools/probes/probe_jkctl_init.ps1` (에러 경로 체크 추가)

**Interfaces:**
- Produces: `jkctl ask "<q>" --attach <path>` (반복 가능). 파일 본문을 프롬프트에 텍스트 블록으로 첨부. 에러: 읽기 실패/바이너리/과대 → exit 2.
- 실엔진 응답 검증은 네트워크 의존이라 커밋 프로브에서 제외 — 컨트롤러가 실엔진 스모크 1회 수행.

- [x] **Step 1: Ask 재구성**

기존 `int Ask(const char* question)`을 구조체 버전으로 교체:

```cpp
// ask — 로컬 LLM 원컷: 응답이 jkctl의 stdout으로 통과한다(동기 원컷, §4).
// --attach는 파일 본문을 프롬프트에 텍스트 블록으로 첨부한다(docs/51 C 후보
// 잔여 — 스펙 §4 예제). 텍스트 전용: NUL 포함은 거부(경로를 프롬프트에
// 넣는 구안 유지). 16KiB 절단(UTF-8 경계 보정). CP949 파일은 ACP 경유
// 재인코딩(docs/48 레슨). CreateProcessW cmdLine 32767 한계 — 최종 길이
// 검사로 조용한 잘림을 막는다(publish_event의 이스케이프 후 크기 검사 선례).
struct AskRequest {
    std::string question;
    std::vector<std::string> attaches;
};

int Ask(const AskRequest& req) {
    if (req.question.empty()) {
        std::fprintf(stderr, "ask: empty question\n");
        return 2;
    }
    std::string prompt = req.question;
    for (const std::string& p : req.attaches) {
        std::ifstream in(p, std::ios::binary);
        if (!in) {
            std::fprintf(stderr, "ask: cannot read attachment: %s\n", p.c_str());
            return 2;
        }
        std::string raw((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());
        if (raw.size() > 16 * 1024) {
            // UTF-8 연속 바이트(0x80-0xBF)에서 물러나 잘라낸다 — 절단이
            // 멀티바이트 문자 중간에 끊기면 모델 입력에 FFFD 파손이 온다.
            size_t cut = 16 * 1024;
            while (cut > 0 && (static_cast<unsigned char>(raw[cut]) & 0xC0) == 0x80)
                --cut;
            raw.resize(cut);
        }
        if (raw.find('\0') != std::string::npos) {
            std::fprintf(stderr, "ask: binary attachment not supported: %s "
                                 "(pass the path in the prompt instead)\n",
                         p.c_str());
            return 2;
        }
        // UTF-8 검증 실패 = ANSI/CP949 텍스트일 확률 — ACP 경유 정규화.
        {
            const int wlen = MultiByteToWideChar(
                CP_UTF8, MB_ERR_INVALID_CHARS, raw.c_str(),
                static_cast<int>(raw.size()), nullptr, 0);
            if (wlen == 0 && !raw.empty()) {
                const int wlenA = MultiByteToWideChar(
                    CP_ACP, 0, raw.c_str(), static_cast<int>(raw.size()),
                    nullptr, 0);
                std::wstring w(wlenA > 0 ? wlenA : 0, L'\0');
                if (wlenA > 0)
                    MultiByteToWideChar(CP_ACP, 0, raw.c_str(),
                                        static_cast<int>(raw.size()), &w[0],
                                        wlenA);
                const int u8len = WideCharToMultiByte(
                    CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
                std::string u8(u8len > 0 ? u8len : 0, '\0');
                if (u8len > 0)
                    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &u8[0],
                                        u8len, nullptr, nullptr);
                raw = u8;
            }
        }
        std::string base = p;
        const size_t slash = base.find_last_of("\\/");
        if (slash != std::string::npos) base = base.substr(slash + 1);
        prompt += "\n\n--- attached file: " + base + " ---\n" + raw +
                  "\n--- end of " + base + " ---";
    }
    std::string esc;
    for (const char c : prompt) {
        if (c == '"') esc += "\\\"";
        else esc += c;
    }
    const std::string cmdA = "ollama launch claude --model \"" + LoadModel() +
                             "\" -- -p \"" + esc + "\"";
    const std::wstring cmd = Utf8ToWide(cmdA);
    // CreateProcessW cmdLine 상한 32767 wchar — 초과 시 CreateProcess 실패
    // 원인을 알기 어렵다. 30000 여유로 미리 거부.
    if (cmd.size() > 30000) {
        std::fprintf(stderr,
                     "ask: prompt too large (%zu chars) — reduce attachments\n",
                     cmd.size());
        return 2;
    }

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessW(nullptr, cmd.empty() ? nullptr : cmd.data(), nullptr,
                        nullptr, TRUE, 0, nullptr, nullptr, &si, &pi)) {
        std::fprintf(stderr, "jkctl: LLM launch failed (err=%lu) — ollama/claude CLI 확인\n",
                     GetLastError());
        return 1;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return (code == 0) ? 0 : 1;
}
```

- [x] **Step 2: wmain ask 디스패치 + usage 갱신**

```cpp
    if (sub == "ask") {
        AskRequest req;
        for (int i = 2; i < argc; ++i) {
            char a8[1024] = {};
            WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, a8, sizeof(a8) - 1,
                                nullptr, nullptr);
            if (std::strcmp(a8, "--attach") == 0) {
                if (i + 1 >= argc) {
                    std::fprintf(stderr, "ask: --attach needs a path\n");
                    return 2;
                }
                char p8[1024] = {};
                WideCharToMultiByte(CP_UTF8, 0, argv[++i], -1, p8,
                                    sizeof(p8) - 1, nullptr, nullptr);
                req.attaches.push_back(p8);
            } else if (req.question.empty()) {
                req.question = a8;
            } else {
                std::fprintf(stderr, "usage: jkctl ask \"<question>\" [--attach <file>]...\n");
                return 2;
            }
        }
        if (req.question.empty()) {
            std::fprintf(stderr, "usage: jkctl ask \"<question>\" [--attach <file>]...\n");
            return 2;
        }
        return Ask(req);
    }
```
헤더 usage 문자열(543행 부근)의 ask 행도 `ask "<question>" [--attach <file>]`로 갱신. (기존 `Ask(a2)` 호출 제거.)
- [x] **Step 3: 빌드** — `--target jkctl` + mtime 게이트.
- [x] **Step 4: probe_jkctl_init.ps1 에러 경로 체크 추가** (체크 11~13으로):

```powershell
    # 11: attach to a missing file is a clean error (exit 2), not a silent ask
    $null = & $jkctl ask "q" --attach (Join-Path $work "no_such_file.txt") 2>&1
    Check "ask attach missing file rejected" ($LASTEXITCODE -eq 2) "exit=$LASTEXITCODE"

    # 12: binary attachment rejected (NUL byte)
    $binP = Join-Path $work "bin_att.bin"
    [IO.File]::WriteAllBytes($binP, (New-Object byte[] 16))
    $null = & $jkctl ask "q" --attach $binP 2>&1
    Check "ask attach binary rejected" ($LASTEXITCODE -eq 2) "exit=$LASTEXITCODE"

    # 13: valid text attachment passes arg handling far enough to try the
    # LLM launch path (exit != 2 — real engine/network is out of probe scope;
    # 0 or 1 both mean the prompt made it into the process launch).
    $txtP = Join-Path $work "att.txt"
    [IO.File]::WriteAllText($txtP, "hello", (New-Object System.Text.UTF8Encoding($false)))
    $null = & $jkctl ask "echo test" --attach $txtP 2>&1
    Check "ask attach text reaches LLM launch" ($LASTEXITCODE -ne 2) "exit=$LASTEXITCODE (0/1 ok — network dependent)"
```
(주의: 체크 13은 ollama가 설치된 머신에서만 실행되며 네트워크 실패 시 exit 1 — `$LASTEXITCODE -ne 2`만 판정한다. 서버 불필요.)
- [x] **Step 5: 공식런** probe_jkctl_init.ps1 ALL PASS. (단, 신규 체크는 서버 불요 — 기존 프로브는 서버 ping 가드만 있음, 확인.)
- [x] **Step 6: 실엔진 스모크(컨트롤러 실행, 커밋 아님)** — `jkctl ask "첨부 요약" --attach engine/README.md` → 한글/파일 반영 응답 1회. 실패해도 CLI 계약(위 체크)은 유효 — 결과를 리포트에 기록.
- [ ] **Step 7: Commit**

```bash
git add engine/tools/jkctl/main.cpp engine/tools/probes/probe_jkctl_init.ps1
git commit -m "feat(jkctl): ask --attach text embedding (docs/51 C leftover)"
```

---

### Task 6: jkctl promote (콘솔 앱 → .jkx) + install .jkx

**Files:**
- Modify: `engine/tools/jkctl/main.cpp` (신규 `Promote`, `InstallJkx`, install 디스패치)
- Test: `engine/tools/probes/probe_jkctl_init.ps1` (체크 14~17)

**Interfaces:**
- Consumes: `jk::JKJkxFile::Write/Entries/FindEntry/ReadEntry` (jkctl은 이미 jkcore 링크 — CMakeLists:633), `InstallFromDir`, `ManifestString`, `ValidAppName`, `Sha256Hex`, `TrustPreRecord`, zip 설치의 스테이징/클린업 관용구.
- Produces: `jkctl promote <folder>` → `<name>.jkx` (JKX1 v1 컨테이너). `jkctl install <path>.jkx` → TOC 언팩 → InstallFromDir. 서버/런처 무변경 — 승격 .jkx는 배포 산출물일 뿐(MODL 부재로 서버 스캔이 런처에 안 올리고, jkctl이 apps\에 .jkx를 두지 않으므로 충돌 없음).

- [x] **Step 1: 헤더 include + Promote 구현**

`main.cpp` 상단: `#include <JKJkxFile.h>` 추가. Pack 뒤에:

```cpp
// promote — docs/51 C 후보 잔여 "콘솔 앱 → .jkx 승격(매니페스트→컨테이너
// 변환)": 콘솔 앱 폴더를 JKX1 컨테이너로 포장한다. pack(<name>.zip)과 같은
// 배포 역할이 컨테이너 매직 버전 — install이 언팩해 설치한다. 서버 스캔
// (apps/*.jkx)은 MODL 엔트리 기반이라 이 파일을 런처로 올리지 않는다.
int Promote(const std::string& folder) {
    std::error_code ec;
    if (!std::filesystem::is_directory(folder, ec)) {
        std::fprintf(stderr, "promote: not a directory: %s\n", folder.c_str());
        return 2;
    }
    std::ifstream in(folder + "\\manifest.json", std::ios::binary);
    if (!in) {
        std::fprintf(stderr, "promote: not a console app (missing %s\\manifest.json)\n",
                     folder.c_str());
        return 2;
    }
    std::string body((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
    const std::string name = ManifestString(body, "name");
    if (!ValidAppName(name)) {
        std::fprintf(stderr, "promote: bad name '%s' in manifest ([A-Za-z0-9_-] 1..64)\n",
                     name.c_str());
        return 2;
    }

    // 파일 수집 — 매니페스트가 TOC 머리에 오도록 먼저 넣는다. in 스트림은
    // 이미 소비됐으므로(manifest 본문 = body 변수) 여기서 다시 쓰지 않는다.
    std::vector<std::pair<std::string, std::vector<uint8_t>>> entries;
    std::vector<uint8_t> mbytes(body.begin(), body.end());
    entries.emplace_back("manifest.json", std::move(mbytes));
    for (auto it = std::filesystem::recursive_directory_iterator(
             folder, std::filesystem::directory_options::skip_permission_denied, ec);
         it != std::filesystem::recursive_directory_iterator();
         it.increment(ec)) {
        if (ec) break;
        if (!it->is_regular_file(ec)) continue;
        std::string rel = std::filesystem::relative(it->path(), folder, ec).string();
        if (ec || rel.empty()) continue;
        std::replace(rel.begin(), rel.end(), '\\', '/');
        if (rel == "manifest.json") continue;  // 이미 머리에 있음
        // TOC name 필드는 60B — 초과 항목은 조용히 잘려 컨테이너가 깨진다.
        if (rel.size() > 59) {
            std::fprintf(stderr, "promote: entry name too long (max 59): %s\n",
                         rel.c_str());
            return 2;
        }
        std::ifstream f(it->path().string(), std::ios::binary);
        std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)),
                                   std::istreambuf_iterator<char>());
        entries.emplace_back(rel, std::move(bytes));
    }
    if (ec) {
        std::fprintf(stderr, "promote: walk failed (%s)\n", ec.message().c_str());
        return 2;
    }
    const std::string out = name + ".jkx";
    if (!jk::JKJkxFile::Write(out, entries)) {
        std::fprintf(stderr, "promote: cannot write %s\n", out.c_str());
        return 2;
    }
    std::printf("promoted %s (%zu entry(ies))\n"
                "install with: jkctl install %s\n",
                out.c_str(), entries.size(), out.c_str());
    return 0;
}
```

(실행자 주의: 위 스니펫의 `mbytes`/`mraw` 자리 표시는 실수 여지가 있다 — **"manifest.json" 엔트리의 바이트는 이미 읽은 `body` 변수에서 만든다**: `std::vector<uint8_t> mbytes(body.begin(), body.end()); entries.emplace_back("manifest.json", std::move(mbytes));` — `in` 스트림은 소진됐음을 상기. 그리고 Pack과 달리 `entries.emplace_back("manifest.json", ...)` 이름은 정확히 `manifest.json`이어야 install의 `InstallFromDir` 검증을 통과한다.)

- [x] **Step 2: InstallJkx + 디스패치**

```cpp
// install(.jkx) — 승격 컨테이너 언팩 설치. JKJkxFile::Open이 version/codec
// 검증을 대신한다(미지원 버전/코덱 거부 — Open 계약). 엔트리 경로는 zip과
// 동일 unsafe-path 가드(절대/드라이브/'..', '\'도 구분자 — zip-slip 리뷰
// MAJOR 동일 규약). 스테이징 후 InstallFromDir(공용 꼬리: 검증+trust 선기록).
int InstallJkx(const std::string& path) {
    jk::JKJkxFile f;
    if (!f.Open(path)) {
        std::fprintf(stderr, "install: not a valid .jkx container: %s\n",
                     path.c_str());
        return 2;
    }
    std::string base = std::filesystem::path(path).filename().string();
    if (base.size() >= 4 && base.substr(base.size() - 4) == ".jkx")
        base.resize(base.size() - 4);
    const std::string tmp = ExeDirA() + "tmp\\install_" + base;
    std::error_code ec;
    std::filesystem::remove_all(tmp, ec);
    std::filesystem::create_directories(tmp, ec);
    if (ec) {
        std::fprintf(stderr, "install: cannot stage %s (%s)\n", tmp.c_str(),
                     ec.message().c_str());
        return 2;
    }
    int rc = 0;
    for (const jk::JKJkxFile::Entry& e : f.Entries()) {
        const std::string nm = e.name;
        // zip 설치와 동일 규칙 — 컨테이너는 자체 도구가 만들지만 방어는 공짜다.
        bool unsafe = nm.empty() || nm[0] == '/' || nm[0] == '\\' ||
                      (nm.size() >= 2 && nm[1] == ':');
        for (size_t p = 0; !unsafe && p + 1 < nm.size(); ++p) {
            if (nm[p] == '.' && nm[p + 1] == '.' &&
                (p == 0 || nm[p - 1] == '/' || nm[p - 1] == '\\') &&
                (p + 2 >= nm.size() || nm[p + 2] == '/' || nm[p + 2] == '\\'))
                unsafe = true;
        }
        if (unsafe) {
            std::fprintf(stderr, "install: unsafe container entry: %s\n",
                         nm.c_str());
            rc = 2;
            break;
        }
        std::vector<uint8_t> bytes;
        if (!f.ReadEntry(f.FindEntry(nullptr, nm), bytes)) {
            std::fprintf(stderr, "install: extract failed: %s\n", nm.c_str());
            rc = 2;
            break;
        }
        std::string outPath = tmp + "\\" + nm;
        std::replace(outPath.begin(), outPath.end(), '/', '\\');
        std::filesystem::create_directories(
            std::filesystem::path(outPath).parent_path(), ec);
        std::ofstream out(outPath, std::ios::binary);
        if (!out || ec) {
            std::fprintf(stderr, "install: cannot write: %s\n", outPath.c_str());
            rc = 2;
            break;
        }
        out.write(reinterpret_cast<const char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
        if (!out) {
            std::fprintf(stderr, "install: write failed: %s\n", outPath.c_str());
            rc = 2;
            break;
        }
    }
    if (rc != 0) {
        std::filesystem::remove_all(tmp, ec);
        return rc;
    }
    const int irc = InstallFromDir(tmp, true, path);
    if (irc != 0) {
        std::error_code cec;
        std::filesystem::remove_all(tmp, cec);
    }
    return irc;
}
```

wmain 디스패치(Install 내부):

```cpp
int Install(const std::string& path) {
    std::error_code ec;
    if (std::filesystem::is_directory(path, ec))
        return InstallFromDir(path, false, path);
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".jkx")
        return InstallJkx(path);
    // 확장자 없는 컨테이너도 매직으로 인식 (JKX1 스니프)
    if (path.size() >= 4) {
        std::ifstream in(path, std::ios::binary);
        char m4[4] = {};
        if (in.read(m4, 4) && std::memcmp(m4, "JKX1", 4) == 0)
            return InstallJkx(path);
    }
    if (path.size() < 4 || path.substr(path.size() - 4) != ".zip") { ... 기존 에러 ... }
    ... 기존 zip 경로 ...
}
```
wmain usage에 `promote "<folder>"` 행 추가 + 디스패치 `if (sub == "promote") return Promote(a2);`.
- [x] **Step 3: 빌드** — `--target jkctl` + mtime 게이트.
- [x] **Step 4: probe_jkctl_init.ps1 체크 14~17** (체크 13 뒤, zip 설치가 이미 apps\<name>을 차지 — **Remove-Item 후 재설치 패턴**(체크 8-10과 동일)을 따른다):

```powershell
    # 14: promote wraps the folder into <name>.jkx (JKX1 magic)
    Remove-Item -Recurse -Force (Join-Path $build ("apps\" + $appName)) -ErrorAction SilentlyContinue
    $jkxPath = Join-Path $work ($appName + ".jkx")
    $null = & $jkctl promote $appDir 2>&1
    Check "promote exit ok" ($LASTEXITCODE -eq 0) "exit=$LASTEXITCODE"
    Check "promote wrote <name>.jkx" (Test-Path $jkxPath) $jkxPath
    if (Test-Path $jkxPath) {
        $m4 = New-Object byte[] 4
        $fs = [IO.File]::OpenRead($jkxPath)
        $null = $fs.Read($m4, 0, 4); $fs.Close()
        Check "jkx has JKX1 magic" ([Text.Encoding]::ASCII.GetString($m4) -eq "JKX1") ""
    }
    # 15: install from the .jkx lands the tree (staging + manifest validation
    #     + trust pre-record ride the shared InstallFromDir tail)
    $null = & $jkctl install $jkxPath 2>&1
    Check "jkx install exit ok" ($LASTEXITCODE -eq 0) "exit=$LASTEXITCODE"
    Check "jkx install landed in build apps" ((Test-Path (Join-Path $build ("apps\" + $appName + "\manifest.json")))) "installed from .jkx"
    # 16: trust pre-record fires for the container path too (same cmd fp)
    $trustTxt2 = ""
    if (Test-Path $trustP) { $trustTxt2 = Get-Content $trustP -Raw -Encoding UTF8 }
    $wantFp2 = & $fpOf "main.cmd"
    Check "jkx trust pre-record" ($trustTxt2 -match [regex]::Escape($wantFp2)) ""
    # 17: desktop .jkx (manifest.txt/MODL) is honestly rejected by install
    #     (missing manifest.json in staged tree)
    # (실 데스크탑 .jkx는 build\apps\에 존재 — 읽기 전용 스니프 설치 시도는
    #  설치 부수효과가 있으므로 하지 않고, 대신 매니페스트 없는 폴더로 검증)
    $emptyDir = Join-Path $work "emptyapp"
    New-Item -ItemType Directory -Force -Path $emptyDir | Out-Null
    $null = & $jkctl promote $emptyDir 2>&1
    Check "promote rejects manifest-less dir" ($LASTEXITCODE -eq 2) "exit=$LASTEXITCODE"
```
(`$jkxPath = Join-Path $work ($appName + ".jkx")`를 상단 변수 블록에 추가. teardown의 apps\<name> 제거가 그대로 커버.)
- [x] **Step 5: 공식런** probe_jkctl_init.ps1 ALL PASS.
- [x] **Step 6: Commit**

```bash
git add engine/tools/jkctl/main.cpp engine/tools/probes/probe_jkctl_init.ps1
git commit -m "feat(jkctl): console app to .jkx promotion + install from container (docs/51 C leftover)"
```

---

### Task 7: 전체 회귀 + 문서/스펙 ledger + 메모리 동기화

**Files:**
- Modify: `docs/53_desktop_agent_mgr.md` §9, `docs/51_p4_sdk_contract.md` C 후보 절, `docs/superpowers/specs/2026-09-16-agent-manager-design.md` §7, `docs/superpowers/specs/2026-09-16-p4-sdk-contract-design.md` §4
- Modify: `C:\Users\kisoc\.claude\projects\I--progwork-JKENGINE\memory\killer_app_absorption.md` (실행 지시 갱신)

- [x] **Step 1: 전체 회귀 공식런** — 순서대로 전부 PASS:

```bash
cd /i/progwork/JKENGINE/engine/build && PATH=/c/msys64/ucrt64/bin:$PATH ./jkdesktop.exe test   # jkdesktop test 0 (인자 'test' — dash 없음, docs/53 §8)
PATH=/c/msys64/ucrt64/bin:$PATH ./jkagentd.exe --selftest                      # 0 failures
cd ../tools/probes
powershell -ExecutionPolicy Bypass -File probe_approve_self.ps1                # 신규
powershell -ExecutionPolicy Bypass -File probe_agentmgr.ps1                    # 확장판 ALL PASS
powershell -ExecutionPolicy Bypass -File probe_agent_chat.ps1                  # approve 회귀
powershell -ExecutionPolicy Bypass -File probe_agent_trust.ps1
powershell -ExecutionPolicy Bypass -File probe_jkctl_init.ps1                  # 확장판 ALL PASS
```

- [x] **Step 2: docs/53 §9 갱신** — NON-BLOCKING 4항목 상태 표기: End 짝(픽스)/4KB·64KB 캡(픽스+프로브 3d·5c-5e)/단일 스트립(큐, docs/31 self-close 흐름 보존)/approve self-approve(루링: permission_set/trust_revoke만 봉쇄, close_window 예외 사유, jkctl 2회 잔여 명시).
- [x] **Step 3: docs/51 C 후복 절** — `--attach`(텍스트 첨부 구현, 16KiB/CP949/30000 가드)와 승격 도구(`promote`/`install .jkx`)를 완료 표기 + probe_jkctl_init 체크 수 갱신.
- [x] **Step 4: 스펙 ledger** — agent-manager 스펙 §7 표의 approve 행에 룰링 기입; p4-sdk 스펙 §4에 --attach as-built 표기.
- [x] **Step 5: 메모리 동기화** — `killer_app_absorption.md`의 "★다음 세션 실행 지시" 갱신(잔여 레저 소각, 눈확인 항목 유지, 새 잔여 = jkctl 2회 self-approve 잔여만) + `MEMORY.md` 훅 라인 갱신.
- [x] **Step 6: 최종리뷰(opus) 준비** — 전체 커밋 diff + 신규 probe 결과를 리뷰어에게 전달(사용자 상임 관례). 리뷰 픽스가 나오면 반영 커밋.