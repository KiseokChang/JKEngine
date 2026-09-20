# docs/62 — 앱 정복 사다리 M1 as-built: send_input + minesweeper 코드 정복 (2026-09-21)

스펙: `docs/superpowers/specs/2026-09-21-conquest-ladder-design.md` (원안 docs/29 §6.6,
토대 docs/58 앱 도구 허브). M1 = 사다리 첫 단(minesweeper)을 **코드로 정복**하는 것:
트랙 B 조작 수단 `send_input` 서버 도구 신설 + 정복 계약 프로브 템플릿 확립.
LLM 실전 세션(스펙 §2 (b))은 사용자 수행 항목이라 §6에 절차로 기록한다.

구현 커밋: 10db9dd(send_input 도구) → d817d7f(캡처 오버레이 배제) →
c3d52ff(ask 파킹+승인 재실행) → c245c2f(정복 프로브) → c712f92(프로브 recover 하드 게이트).

## 1. 왜 이것인가

앱 정복 계약(launch→observe→drive→verify→recover)의 재료는 이미 있었다:
launch_app/launch/recover는 docs/58, observe는 list_windows/capture_window,
recover는 app.crashed 이벤트. 없던 것은 **drive**였다 — MCP 앱 도구가 등록되지
않은 앱(minesweeper 포함)은 조작 수단이 전무했고, 기존 프로브들은 그 틈을 OS
SendInput 직접 호출로 메꾸고 있었다. 스펙 합의 3(프로브 = 자동화의 초기 코드)에
따라 이 틈을 **에이전트 도구 표면**으로 닫는 것이 M1이다: send_input이 생기면
프로브의 drive가 도구 호출이 되고, 그 시퀀스가 jktriggers로 1:1 이식된다.

## 2. send_input as-built (스펙 §3.1)

**도구 표면**: jkagentd MCP 카탈로그 등록(engine/tools/jkagentd/main.cpp:83,
required `id`+`op`) → 서버 HandleAgentQuery 도구 분기(JKWindowServer.cpp ~3559).
서버 전용 도구라 브리지/CLI의 generic relay를 그대로 탄다(브리지 무수정).

인자(`BuildSendInputOp`, JKWindowServer.cpp:1666 — 오류키 bad_op/bad_target/
bad_key/bad_text/bad_action):

| 인자 | 타입 | 기본 | 의미 |
|---|---|---|---|
| `id` | int | 필수 | 대상 window id (list_windows의 id, 논리 데스크톱 기준) |
| `op` | enum | 필수 | `click` \| `key` \| `type` \| `wheel` |
| `x`,`y` | int | 0 | click: **논리 데스크톱 좌표** — 서버가 표면 좌표로 변환 |
| `button` | int | 1 | click: 마우스 버튼 |
| `clicks` | int | 1 | click: 클릭 수 |
| `key` | int | 0 | key: SDL keycode (0이면 bad_key) |
| `mods` | int | 0 | key: SDL mod |
| `action` | enum | `tap` | key: `tap`\|`down`\|`up` |
| `text` | string | — | type: UTF-8 (63B 단위, UTF-8 후속 바이트 경계에서 분할 발송) |
| `dx`,`dy` | int | 0 | wheel: 델타 |

실행(`ExecuteSendInputOp`, :1589): 대상 연결을 clients_에서 직접 순회해 찾고
(window_not_found), 기존 `InputEventPayload` 경로로 주입 — 와이어 신규 0.
클릭은 MouseDown+MouseUp 쌍, key는 action에 따라 Down/Up, type은 63B 분할 Char,
wheel은 MouseWheel. 좌표 변환은 실시간 경로의 표면 변환식
`((mx/outputScale) - X()) / layerScale`에서 논리 입력이므로 물리 전곱이 소거된
`(논리 - client->X()) / layerScale`이 된다.

**대상 배제(최종리뷰 Important)**: 셸(`IsShell()`)과 캡처 오버레이
(`Title()==kCaptureOverlayTitle`)는 `bad_target` 거부 — 캡처 오버레이는 셸과
별개 실존 연결이라 러버밴드 캡처 중 합성 click/key가 오염시킨다(선례 쌍검사
560·1054·5888). 크롬 닫기는 close_window 도구가 담당.

**게이트 — 기본 ask**: kPermMatrix `{"send_input","server","ask"}` 행(:2139,
2026-09-21 사용자 승인). permissions.json 파일 부재/키 부재 기본도 ask(:5698),
파일값 "ask"는 Allow로 열화하지 않는다(:5685, files/app_tool 편입 선례).
자동화 편의는 런타임 permissions.json에서 `"send_input":"allow"`로 조정.

**ask 경로(승인 파킹)**: 승인 가용성 검사(AgentEventSubscriber 부재 →
`approval_unavailable`) + 파킹 상한(`approval_overflow`)을 통과한 뒤
PendingApproval(kind="send_input", name=조작명, targetId=창 id, 60s 만료)에
**args 원문 JSON을 `sendArgs`로 통째 보관**하고 `agent.approval_request` 이벤트를
발행, 도구 응답은 파킹(승인 해소 시 회신).

**승인 시점 원 요청 재실행**(:5079, files_access 파킹-재실행 선례): resolve 시점에
게이트를 재검사하고(파킹 대기 중 permissions.json이 deny로 바뀌면
`permission_denied` — 최신 게이트 강제), 파킹해 둔 원문을 `BuildSendInputOp`로
재조립해 `ExecuteSendInputOp`를 다시 태운다. 도구 경로와 승인 재실행 경로가 같은
`SendInputOp` 구조체를 소비하므로 두 경로가 갈라질 여지가 없다.

**응답**: 성공 `{"ok":true,"sent":true}`, 실패
`{"ok":false,"error":"<키>"}` — 도구 오류키 외에 permission_denied /
approval_unavailable / approval_overflow.

## 3. 정복 프로브 템플릿 (스펙 §3.2)

**`engine/tools/probes/probe_conquest_minesweeper.ps1`가 템플릿이다.** 다음 단
(예: tetris)은 이 파일을 복제해 앱 이름과 시나리오만 교체한다 — 공통 골격은
건드리지 않는다.

- **5단계 골격 = `Invoke-ConquestCycle`**: (1) launch — `launch_app` MCP 호출 +
  `list_windows` 등장 폴링(하드 게이트) → (2) observe — 창 필드(id/title/pid/
  geometry) 비퇴화 + 타이틀 매치 + `capture_window` 해시 → (3) drive —
  `send_input` click 보드 중앙(**논리 좌표** — list_windows rect가 논리 기준) →
  (4) verify — 캡처 해시 변화(클릭이 픽셀을 움직였다) + 같은 id로 생존.
  recover는 메인 흐름: pid 강제 종료 → **recover-gone 하드 게이트**(죽은 pid가
  list_windows에서 사라지는 폴링 — 게이트 없으면 cycle2가 낡은 창을 무음 매치) →
  app.crashed 이벤트(소프트) + events_list fired 카운터(소프트) → 재스폰 → 전
  사이클 재실행(cycle2).
- **원칙 — drive/verify는 도구 표면 전용. OS SendInput 직접 호출 금지**(스펙
  §3.2). 이 프로브가 쓰는 것은 launch_app / list_windows / send_input /
  capture_window / events_list / agent-events 6개뿐이다. 이 시퀀스가 jktriggers로
  이식 가능한 자동화 씨앗이다.
- **복제 원본**: Check/Stop-ProbeProcs/Invoke-Mcp/Invoke-Agentctl/Capture-Hash =
  probe_send_input.ps1, Get-MineWindow/Watch-Events = probe_agent_maximize.ps1,
  Invoke-Ctl = probe_agent_events.ps1, 승인 request-id 취득(raw 파이프+frame
  decode) = probe_approve_self.ps1 관례.
- **관례**: PS5.1 ASCII 무BOM, `> log 2>&1` 파일 리다이렉트, permissions.json은
  Add-Member RMW+finally 복원(백업+콘솔 고지 — docs/59 §16.1 레슨), 이벤트 잡은
  트리거 호출 전 구독(레슨 28), 클라 종료는 pid 한정(레슨 42).

## 4. 검증 결과

**실측(공식 런)**:

- **probe_send_input.ps1 — 11체크 ×2 ALL PASS**(2026-09-21 00:35,
  `engine/tools/probes/probe_send_input.log` / `probe_send_input_run2.log`):
  setup-server-up / launch-window / capture-before / click-ok /
  click-changes-pixels / bad-id / bad-op / key-ok / bad-key / wheel-nosuch /
  wheel-ok. (task-1 보고의 "12 CHECK"는 `ALL PASS` 행을 계수에 넣은 착시 —
  실제 체크 행은 11이다.)
- 빌드: 워크트리 engine/build ninja 전체 빌드 GREEN(작업 1~3 전부).

**작성+정적검증 완료, 공식런 배치 대기**(이 세션의 샌드박스가 파워셰일 실행을
전면 거부 — 하네스 제약, 결함 아님):

- probe_send_input_ask.ps1(11체크 — 파킹/승인/재실행/클릭 착지/deny/무주입):
  ASCII 무BOM·ninja GREEN·와이어 포맷 선례 대조 완료.
- probe_conquest_minesweeper.ps1(런 시 18체크 — setup-server-up + cycle1 6 +
  이벤트 3 + recover-gone + cycle2 6 + cycle2-after-recover): 동일 정적검증
  완료.
- **회귀 스윕 배치 대기**: probe_app_tools ×2 + `jkdesktop test` 종료코드 0 +
  probe_agent_e2e ×1(minesweeper 스폰 경로 무손상).

### 배치 런 절차 (파워셰일 가용 세션에서)

**프로브 대상 분리 — 배치는 두 서버를 건드린다**(스크립트 하드코딩 기준):

| 프로브 | 테스트 대상 서버 | permissions.json | teardown |
|---|---|---|---|
| probe_send_input / probe_send_input_ask / probe_conquest_minesweeper | **워크트리** build(스크립트 `$build` 하드코딩) | Copy-Item 백업+finally 복원(검증됨) | jkwinserver/jkdesktop/jkagentd/jkbridge 전면 정지 |
| probe_app_tools | **메인 트리** build(`$exe` 하드코딩) | TEMP per-PID 백업+stale 가드+finally 복원 | jkdesktop/jkapp_vplayer/jkbridge/jkchat 정지 |
| probe_agent_e2e | **메인 트리** build(`$exe`/`$agnt` 하드코딩 — 사용자 라이브 데스크톱이 쓰는 그 빌드) | **백업 없이** `{"close_window":"allow"}`로 덮어쓰고 끝에 Remove-Item으로 **삭제**(docs/59 §16.1 재발 패턴) | jkdesktop 정지 |

따라서 "각 프로브의 finally가 permissions.json을 복원한다"는 **워크트리 3종 + 
probe_app_tools에만 참**이다 — probe_agent_e2e는 수동 백업/복원 의무가 있다(아래
0·4단). 프로브는 전부 `> log 2>&1` 파일 리다이렉트로 구동(파이프 grep은 버퍼링
행걸 오판 — 기존 레슨), 정복 계약 프로브는 ×2(2연통 원칙):

```powershell
cd I:\progwork\JKENGINE\.claude\worktrees\conquest-ladder\engine\tools\probes
# (0) 메인 트리 permissions.json 수동 백업 — probe_agent_e2e가 백업 없이 삭제한다
Copy-Item I:\progwork\JKENGINE\engine\build\permissions.json I:\progwork\JKENGINE\engine\build\permissions.json.bak_b62 -Force
Write-Output "NOTICE: main-tree permissions.json backed up to permissions.json.bak_b62 (probe_agent_e2e deletes it — restore after the batch)"
# (1) 워크트리 프로브 — 정복 계약 ×2
powershell -ExecutionPolicy Bypass -File probe_send_input_ask.ps1       > probe_send_input_ask.log 2>&1
powershell -ExecutionPolicy Bypass -File probe_send_input_ask.ps1       > probe_send_input_ask_run2.log 2>&1
powershell -ExecutionPolicy Bypass -File probe_conquest_minesweeper.ps1 > probe_conquest_minesweeper.log 2>&1
powershell -ExecutionPolicy Bypass -File probe_conquest_minesweeper.ps1 > probe_conquest_minesweeper_run2.log 2>&1
# (2) 메인 트리 회귀
powershell -ExecutionPolicy Bypass -File probe_app_tools.ps1            > probe_app_tools_b62.log 2>&1
powershell -ExecutionPolicy Bypass -File probe_app_tools.ps1            > probe_app_tools_b62_run2.log 2>&1
powershell -ExecutionPolicy Bypass -File probe_agent_e2e.ps1            > probe_agent_e2e_b62.log 2>&1
# (3) self-test: 종료코드 0 기대
I:\progwork\JKENGINE\.claude\worktrees\conquest-ladder\engine\build\jkdesktop.exe test > jkdesktop_test_b62.log 2>&1
echo "exit=$LASTEXITCODE"
# (4) 메인 트리 permissions.json 수동 복원
Copy-Item I:\progwork\JKENGINE\engine\build\permissions.json.bak_b62 I:\progwork\JKENGINE\engine\build\permissions.json -Force
Remove-Item I:\progwork\JKENGINE\engine\build\permissions.json.bak_b62 -Force
Write-Output "NOTICE: main-tree permissions.json restored"
```

판정: 각 로그에서 `FAIL` 0 + `ALL PASS`(정복 프로브는 `CONQUEST PASS`) 확인.
probe_agent_e2e만 PASS/FAIL을 exit 코드로 보고한다.

**라이브 서버 복원 의무 — 반드시 메인 빌드 경로에서**: 배치가 남기는 결손은
teardown이 전부 정지시킨 jkwinserver / jkdesktop(--client taskbar) / jkbridge다.
복원을 워크트리 build로 하면 안 된다 — state 디렉토리는 exe 경로를 추종하고
(SettingsKvPath가 GetModuleFileNameA의 `dir\state`, JKWindowServer.cpp:2214),
워크트리 서버는 `worktrees\conquest-ladder\engine\build\state`(빈 셸)로 조용히
갈아타 사용자의 permissions/브리지 토큰/notes가 소실처럼 보인다. jkagentd는 복원
목록에서 제외 — CLI가 턴마다 재스폰한다.

```powershell
# 복원 = 라이브 환경(메인 빌드). 프로브가 테스트한 워크트리 빌드와 분리.
Start-Process -FilePath I:\progwork\JKENGINE\engine\build\jkwinserver.exe -WorkingDirectory I:\progwork\JKENGINE\engine\build
Start-Process -FilePath I:\progwork\JKENGINE\engine\build\jkdesktop.exe -ArgumentList "--client","taskbar" -WorkingDirectory I:\progwork\JKENGINE\engine\build
Start-Process -FilePath I:\progwork\JKENGINE\engine\build\jkbridge.exe -WorkingDirectory I:\progwork\JKENGINE\engine\build
```

복원 뒤 jkdesktop 프로세스가 1개인지 확인 — 2개 관측은 좀비 의심(기존 레슨).

## 5. 사다리 현황

| 단 | 상태 |
|---|---|
| **minesweeper** | **코드 정복 완료** — send_input 도구 + 정복 프로브 템플릿 확립. 프로브 공식런+회귀 스윕은 배치 대기(§4). 남은 판정 = 배치 런 + LLM 실전 세션(§6) |
| tetris | 대기 — probe_conquest_minesweeper.ps1 복제+시나리오 교체 |
| scriptdemo | 대기 |
| taskmgr | 대기 |
| terminal | 대기 |
| vplayer | 대기 — MCP 도구 6개 기보유, 트랙 A(도구 릴레이) 시제 케이스 |
| browser | 대기 |
| taskbar | 대기 — 최종 관문(셸 자체 정복) |

## 6. LLM 실전 체크리스트 (minesweeper)

```
1. 폰/채팅에서: "지뢰찾기 켜 줘" → launch_app (MCP)
2. "한 칸 열어 줘" → send_input click (승인 스트립에서 허용)
3. 결과 확인: 창 상태 응답 or 스크린샷
```

2번의 승인은 기본 ask 게이트를 걷는다 — 승인 스트립(GUI)에서 허용하면 파킹된
원 요청이 재실행되어 클릭이 착지한다(§2). 런타임 permissions.json을 allow로
바꾸면 승인 없이 통과한다. 앱마다 이 형식으로 § 한 줄씩 누적한다.

## 7. 레슨 (실측 것만)

1. **승인 재실행은 args 원문을 파킹한다** — 개별 필드로 분해 보관하면 승인 시점
   재조립이 파싱 규칙 변경을 흡수하지 못한다. `sendArgs`(args 원문 JSON)를
   PendingApproval에 보관하고 승인 시점에 `BuildSendInputOp`→`ExecuteSendInputOp`
   재파싱(files_access의 파킹-재실행 + app_tool의 원문-보관 선례 결합). 도구 경로와
   승인 경로가 같은 `SendInputOp` 구조체를 소비해 갈라짐이 구조적으로 없다.
2. **캡처 오버레이는 셸과 별개 실존 연결** — 대상 배제는 `IsShell() ||
   Title()==kCaptureOverlayTitle` 쌍검사 선례(560·1054·5888). 셸만 막으면
   러버밴드 캡처 중 합성 입력이 오버레이로 새고, 오버레이만 막으면 셸 크롬으로
   샌다.
3. **PS 함수가 PASS/FAIL을 출력하면 반환값에 출력이 접착된다** — 컬렉션의
   `[bool]` 캐스팅은 항상 true라 사이클이 절대 FAIL하지 못한다. 사이클 결과는
   `$script:cycleOk`로 운반(실측: 정복 프로브 리뷰 1라운드).
4. **프로브가 서버를 재스폰해도 낡은 jkwinserver가 파이프를 소유하고 있으면 모든
   MCP 호출이 옛 서버에 떨어진다** — 실측: probe run 1의 send_input 체크 전부
   FAIL. 그래서 Stop-ProbeProcs는 전면 teardown(jkdesktop/jkwinserver/jkagentd/
   jkbridge)이고, 복원 의무는 docs에 절차로 남는다(§4).
5. **승인 파킹형 도구는 승인 가용성 검사+파킹 상한을 전부 통과해야 한다** —
   subscriber 부재 → approval_unavailable, 상한 초과 → approval_overflow.
   run_console_app/files_access와 같은 공용 계약이다.
6. **논리 좌표 입력의 변환은 물리 전곱이 소거된다** — 실시간 경로의 표면 변환식
   `((mx/outputScale) - X()) / layerScale`에서 send_input은 논리 좌표를 받으므로
   `(논리 - client->X()) / layerScale`. outputScale 인자를 그대로 옮기면 이중
   스케일이 된다.
7. **프로브 체크 계수는 로그의 `ALL PASS` 행을 제외하고 센다** — `grep -c
   "PASS\|FAIL"`은 종결 행까지 세어 12로 과계수했다(task-1 보고 착시, 실체 11).
   "체크 N" 표기는 PASS 행 기준으로 통일한다.
8. **하네스 제약은 결함이 아니라 상태다** — 파워셰일 실행 봉쇄 세션에서 공식런을
   억지로 대체하지 않고, 정적검증(ASCII/BOM·ninja·와이어 대조)만 수행하고 배치
   런 절차를 문서로 남겼다(§4). 문서가 절차를 갖고 있으면 이관이 손실 없다.
