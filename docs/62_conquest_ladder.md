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

수용 편차(최종리뷰 기재): 스펙 원안의 wheel `{x?,y?}`는 잘라냈다 — 실시간 휠
경로도 좌표를 실지 않는다(JKWindowServer.cpp:1527-1533, dx/dy만). 스펙의 `target`/`vk`/`scan`은
구현에서 `id`/`key`(SDL keycode)로 개명해 list_windows의 id와 SDL keycode
계약(list_windows의 기존 도구들과 동일)에 정렬했다.

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
- probe_conquest_minesweeper.ps1(런 시 **19체크** — setup-server-up + cycle1 6 +
  **cycle1 집계** + 이벤트 3 + recover-gone + cycle2 6 + cycle2-after-recover;
  이전 표기 18은 집계 Check "cycle1" 누락 착시): 동일 정적검증 완료. 단
  recover-gone이 FAIL하면 하드 게이트가 cycle2 블록을 스킵 — 실제로 런되는
  체크는 12뿐이다(§7-7의 아이러니: 계수 표기는 실제 런 분기를 따라야 한다).
- probe_conquest_tetris.ps1(런그 2 — 템플릿 복제, **19체크 구조 동일**, drive만
  click→key로 교체: `{"op":"key","key":1073741904}` = SDLK_LEFT, 인자 형식은
  probe_send_input.ps1 key-ok 선례 그대로, 소스 실측 = JKAppModule_tetris.cpp:8
  스폰명 "tetris"/타이틀 "Tetris" + TetrisApp.cpp TetrisGrid::RespondMessage의
  SDLK_LEFT/RIGHT/UP/DOWN/SPACE `changed`→Invalidate 갱신): 동일 정적검증
  완료(ASCII 무BOM + ninja GREEN 유지 + 템플릿 diff로 시나리오 라인 외 무편집
  확인). 유의: verify-hash는 게임 중력 타이머도 화면을 다시 그리므로 drive+렌더
  종합 증거이고, 키 고유 게이트는 drive-key 도구 승인 응답이다.
- **회귀 스윕 배치 대기**: probe_app_tools ×2 + `jkdesktop test` 종료코드 0 +
  probe_agent_e2e ×1(minesweeper 스폰 경로 무손상).

### 배치 런 절차 (파워셰일 가용 세션에서)

**프로브 대상 분리 — 배치는 두 서버를 건드린다**(스크립트 하드코딩 기준):

| 프로브 | 테스트 대상 서버 | permissions.json | teardown |
|---|---|---|---|
| probe_send_input / probe_send_input_ask / probe_conquest_minesweeper / probe_conquest_tetris | **워크트리** build(스크립트 `$build` 하드코딩) | Copy-Item 백업+finally 복원(검증됨) | jkwinserver/jkdesktop/jkagentd/jkbridge 전면 정지 |
| probe_app_tools | **메인 트리** build(`$exe` 하드코딩) | TEMP per-PID 백업+stale 가드+finally 복원 | jkdesktop/jkapp_vplayer/jkbridge/jkchat 정지 |
| probe_agent_e2e | **메인 트리** build(`$exe`/`$agnt` 하드코딩 — 사용자 라이브 데스크톱이 쓰는 그 빌드) | **백업 없이** `{"close_window":"allow"}`로 덮어쓰고 끝에 Remove-Item으로 **삭제**(docs/59 §16.1 재발 패턴) | jkdesktop 정지 |

따라서 "각 프로브의 finally가 permissions.json을 복원한다"는 **워크트리 4종 +
probe_app_tools에만 참**이다 — probe_agent_e2e는 수동 백업/복원 의무가 있다(아래
0·4단). **단계 순서는 강제다 — (0) 백업 → (1) 워크트리 프로브 → (2) 메인 트리
회귀 순서를 바꾸지 마라**: (1)의 프로브 teardown이 jkwinserver를 정지해야 (2)의
메인 트리 프로브가 자기 서버를 스폰할 수 있다. 라이브 jkwinserver가 파이프를
계속 소유한 채 (2)를 먼저 돌리면 메인 트리 프로브의 호출이 전부 라이브(테스트
대상 아닌) 인스턴스에 착지한다(§7-4 실측의 배치판 변형). 프로브는 전부
`> log 2>&1` 파일 리다이렉트로 구동(파이프 grep은 버퍼링
행걸 오판 — 기존 레슨), 정복 계약 프로브는 ×2(2연통 원칙):

```powershell
cd I:\progwork\JKENGINE\.claude\worktrees\conquest-ladder\engine\tools\probes
# (0) 메인 트리 permissions.json 수동 백업 — probe_agent_e2e가 백업 없이 삭제한다
Copy-Item I:\progwork\JKENGINE\engine\build\permissions.json I:\progwork\JKENGINE\engine\build\permissions.json.bak_b62 -Force
Write-Output "NOTICE: main-tree permissions.json backed up to permissions.json.bak_b62 (probe_agent_e2e deletes it — restore after the batch)"
# 이 배치가 중간에 비정상 종료하면 4단의 NOTICE를 믿지 말고 즉시 수동 복원한다 —
# probe_agent_e2e가 백업 없이 삭제·재작성하는 파일이라 크래시된 4단은 복원을
# 보장하지 않는다(최종리뷰 Minor 7):
#   Copy-Item I:\progwork\JKENGINE\engine\build\permissions.json.bak_b62 I:\progwork\JKENGINE\engine\build\permissions.json -Force
# (1) 워크트리 프로브 — 정복 계약 ×2
powershell -ExecutionPolicy Bypass -File probe_send_input_ask.ps1       > probe_send_input_ask.log 2>&1
powershell -ExecutionPolicy Bypass -File probe_send_input_ask.ps1       > probe_send_input_ask_run2.log 2>&1
powershell -ExecutionPolicy Bypass -File probe_conquest_minesweeper.ps1 > probe_conquest_minesweeper.log 2>&1
powershell -ExecutionPolicy Bypass -File probe_conquest_minesweeper.ps1 > probe_conquest_minesweeper_run2.log 2>&1
powershell -ExecutionPolicy Bypass -File probe_conquest_tetris.ps1      > probe_conquest_tetris.log 2>&1
powershell -ExecutionPolicy Bypass -File probe_conquest_tetris.ps1      > probe_conquest_tetris_run2.log 2>&1
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

**런그 3 — probe_conquest_vplayer.ps1 (트랙 A 시제, 2026-09-22, 23체크 ×2 CONQUEST
PASS)**: 템플릿 골격(사이클/aggregate `$script:cycleOk`/recover 하드 게이트) 원문
유지, drive만 send_input → **app_tool**(앱 자기 도구)로 교체 — open(비동기라
진실원은 get_status.opened 폴링)→play_pause(paused:true)→seek(pos<2.0).
verify-hash는 유휴 창 해시 → 재생 창 해시 변화(재생이 픽셀을 움직였다). 차이 2건:
①drive가 도구 릴레이라 **permissions.json 무편집**(app_tool 기본 게이트 allow,
kPermMatrix 행 — 사용자 런타임 파일 그대로 통과) ②teardown에
jkapp_vplayer 전용 이미지명 추가(레슨 42 — jkdesktop 공유명이 아니라 안전).

**런그 4 — probe_conquest_terminal.ps1 (트랙 B, 2026-09-23, 25체크 ×2 CONQUEST
PASS)**: terminal(800×500, pty) — drive가 send_input **type+key**(SDLK_RETURN=13)
조합: `echo <마커>` 타이핑+엔터 → pty가 셸을 실행시킨다. verify가 2중:
①**terminal.output 이벤트**(앱 자기 토픽, data.text에 마커) ②캡처 해시 변화.
recover는 템플릿 동일(pid 강제 종료 → recover-gone 하드 게이트 → app.crashed →
cycle2). 이 런그가 **정복 사다리 최초의 실제 제품 결함**을 잡았다:

- **c9ba8bf reserved-topic 회귀(docs/54 NIT-3 픽스의 부작용, 2026-09-18~23)** —
  kReservedTopicPrefixes의 규칙은 "카탈로그 **server** 행 접두"인데 `terminal.`
  이 포함됐고, 유일한 terminal.* 토픽인 terminal.output의 카탈로그 행은
  `source:"app"`(터미널 앱이 publish_event로 직접 발행 — M2b)이다. 접두 일괄
  봉쇄가 정당 발행자를 `reserved_topic`으로 죽였고, 피해는 4건:
  (a) 터미널 앱의 terminal.output 발행 전멸(앱이 응답 무시 fire-and-forget이라
  5일간 무음) (b) jktriggers desktop.notify(agent.notify 발행) 사망 — 트리거
  액션 전체가 조용히 무력화 (c) probe_agent_triggers 3체크 FAIL (d)
  probe_agent_triggerctl on-resumes FAIL. **진단 경로가 교훈 그 자체**: 프로브의
  이벤트 드레인 실패 → agent-events CLI stdout 완전 버퍼링(probe_agent_trust
  레슨 1 재확인) → fired 카운터 0(publish 미도달 확정) → 앱 SendAgentQuery
  계측(send=1) → **앱 측 reply 프린트가 결판**(`reserved_topic` 회신).
  **픽스**: publish 게이트 topicReserved에 source:"app" 카탈로그 행 2개의
  **정확-토픽 면제**(terminal.output, agent.notify) — 접두 예약 자체는 유지
  (window./audio. 특권 서버 소비자 스푸핑 봉쇄가 존재 이유), 공유 표
  (HandleToolRegister namespace 검사)는 건드리지 않는다. 스푸핑 노출은 트리거
  오타동/가짜 알림뿐 — 스크립트 신뢰 등급에서 허용. 회귀 실측: conquest
  terminal 25×2 + probe_agent_triggers **7/7 완전 회복** + triggerctl 5체크
  PASS + probe_agent_notify ×2 PASS.
- 부수 하네스 픽스: probe_agent_triggers/probe_agent_e2e가 permissions.json을
  **백업 없이 덮고 Remove-Item으로 삭제**하는 §16.1 동형 결함(런그 4 진단 중
  유저 런타임 파일 실제 소실 피해) → 백업+NOTICE+복원 패턴 적용(두 프로브 다시
  실측 PASS+복원 확인). conquest 프로브의 RMW도 파일 부재 시 조용히 실패하지
  않게 존재 가드+DIAG.

**런그 5 — probe_conquest_taskmgr.ps1 (트랙 B, 2026-09-23, 27체크 ×2 CONQUEST
PASS)**: taskmgr(ImGui 앱, 900×620) — drive는 2단 클릭: ①Minesweeper 행(1행,
표면 y≈57) 싱글 클릭으로 선택 → ②Activate 버튼(표면 ≈38,509) 클릭.
verify는 **list_windows focused 플래그 플립**(Minesweeper true/Task Manager
false — 상태 기반 강한 게이트)+window.focused fired 카운터. **캡처 해시는 이
앱에서 검증기가 될 수 없다** — taskmgr는 CPU%/ImPlot 히스토리를 상시 리드로우해
무드라이브에도 해시가 바뀐다(실측 HASH-IDLE-CHANGED: True). 이 런그가 두 번째
**실제 제품 결함**을 잡았다:

- **send_input 클릭이 ImGui 앱에서 무력(런그 5 신규 결함)** — ExecuteSendInputOp
  클릭 경로가 MouseDown+MouseUp만 보냈는데, ImGui 백엔드는 **MouseMove에서만
  MousePos를 갱신한다**(imgui_impl_jkwindow.cpp:219 — MouseDown은 좌표를 안
  실음). 이동 없는 합성 클릭은 마지막 MousePos에 착지해 taskmgr에서 클릭이
  전면 무반응. 런그 1 minesweeper가 통과한 것은 JKDC 커스텀 렌더링이라 이벤트
  좌표를 직독하기 때문 — ImGui 앱(taskmgr·launcher 등)은 전부 해당.
  **픽스**: 클릭 경로에 선행 MouseMove 주입(JKWindowServer.cpp:1645, 실시간
  경로와 동일한 좌표 변환식 재사용). 회귀 실측: conquest_minesweeper ×2 +
  probe_send_input ALL PASS(런그 1 무손상).
- **부수 진단 결판 2건**: ①행 매핑 — taskmgr 표는 PID 값 정렬이 아니라
  **스폰(삽입) 순서**로 행을 나열한다(실측: 큰 pid가 1행인 케이스 관측). pid
  대소로 행을 추정하면 자기 행을 골라 Activate가 서버 셀프 가드
  (surfaceId==client.Id(), :2012)로 무음 무시된다 — 프로브는 minesweeper를
  먼저 스폰해 행 1을 고정한다. ②좌표 기준 — list_windows rect == 클라 표면
  (크롬은 그 밖에 합성), send_input 논리 좌표 변환식과 일치: 표면 (cx,cy) =
  논리 (win.x+cx, win.y+cy), **크롬 오프셋 없음**(Input dump 헤더 클릭으로
  실증). observe-focus-baseline은 등장 폴링이 즉시 break해 포커스 인계가 늦게
  확정되는 레이스가 있어 settle 재폴링으로 픽스(1차 런 4 FAIL의 전부).

**런그 6 — probe_conquest_workshop.ps1 (트랙 A+B 복합, 2026-09-23, 26체크 ×2
CONQUEST PASS)**: 스크립트 앱 단. **트랙 판정(스펙 §3.2의 재량)**: scriptdemo
자체(SCRI 내장, 시계 스크립트)는 도구·이벤트 표면이 0이고 1Hz 자체 리드로우라
정복 계약의 drive/verify가 도구 표면에서 성립하지 않는다 — 대상을 같은
jkapp_script 모듈의 정복 가능 형태인 **workshop**(get_script/set_script 등록,
docs/60)으로 판정. scriptdemo는 관찰 기록으로 남는다(런그 5의 "ImGui 해시
무효"와 동형: 시계가 해시 검증기를 무효화).

- **드라이브는 사다리 최초 복합 트랙**: (A) app_tool set_script → SyncReload
  (동일 연결 in-place, id 불변 — ClientScriptApp.h:182) → get_script 왕복+
  리로드 후 캡처 해시로 검증 → (B) send_input 클릭으로 **재작성된 스크립트의**
  카운터 버튼을 때려 해시 변화로 검증. 재작성 스크립트를 무시계(정적)로 쓰는
  것이 핵심 설계 — 정적 표면에서만 해시가 유효 검증기가 된다(verify-static-hash
  체크가 그 전제를 매 런 증명).
- **부수 진단 결판 3건**: ①agentctl+공백 = argv 분열(레슨 26의 app_tool 확장) —
  set_script 페이로드가 `var x = 1;`만 돼도 bad_request(공백 없는 `varx=1;`는
  통과, MCP 채널은 공백 있어도 정상). probe_workshop의 소형 페이로드가 이를
  가려왔다. **공백 가능성 있는 app_tool 페이로드는 MCP 채널 의무**. ②스크립트
  위젯은 선언 좌표 그대로 렌더되지 않는다 — x20,y60 버튼의 실측 표면 중심은
  (97,128)(패널 내부 오프셋). 캡처 기준 좌표로 때려야 한다. ③verify A의
  get_script 왕복 인자는 반드시 `{}` 리터럴 — 빈 문자열은 `"args":}`로 죽는다.
- **사용자 파일 보호 확장**: state/scripts/myapp.js는 워크숍의 살아있는 진실원
  (폰 세션 콘텐츠) — permissions.json 동형의 백업+finally 복원+콘솔 고지를
  스크립트 파일에도 적용. probe_workshop 회귀(ALL PASS, c7 진실원 복원 체크
  포함)로 복원 이중 확인.

**런그 7 — probe_conquest_browser.ps1 (트랙 B, 2026-09-23, 32체크 ×2 CONQUEST
PASS)**: browser(CEF 오프스크린, 960×640). drive는 3단 조합: URL 바 클릭(표면
≈300,54) → **type**(file:// URL) → **key RETURN(13)** — InputText 커밋 경로. verify는
2중: ①캡처 해시(다크 홈 페이지 → 프로브 소유 밝은 페이지 state/conq7_page.html,
프로브가 생성·finally 삭제) ②**교차 사이클 해시 결정성** — h0·h1이 사이클1/2와
캘리브레이션 3런에서 전부 동일 값 재현(CC8070C9…/AA103F7A…). 진단 결판:

- **about:blank 불가** — Navigate()가 `://` 없는 문자열에 https:// 접두를 붙여
  `https://about:blank`가 되고 네트워크 오류 페이지로 랜드(LAN 의존 플레이크).
  검증 페이지는 file:// 로컬로.
- **클릭 후 셋틀 800ms 필수** — 클릭 400ms 후 타이핑하면 텍스트가 안 찍힌다
  (첫 캘리브레이션 실패의 전부). InputText 활성화 레이스.
- **exact h2==h0 금지** — Home 클릭 라운드트립 후 표면은 홈이지만 URL 바 버퍼에
  home URL이 남아(Home이 urlBuf_를 쓴다) 포커스/버퍼 렌더가 fresh h0와 다르다.
  라운드트립 판정은 "h2≠h1 + 정적 해시"로 하고, exact-match 판정은 cycle2의
  재드라이브(h1c2==h1c1)로 승격 — 이 형태가 캐럿/버퍼 상태 오염에 면역.
- 전제 확인: 앱이 PreProcessMessage에서 **모든 이벤트를 ImGui 백엔드에 먼저**
  흘려보내므로(WantTextInput 게이트는 CEF 전달만 제어) URL 바 포커스 상태의
  type+key가 InputText로 착지한다. Ctrl+A 선택 후 재타이핑 경로는 불안정
  (버퍼 잔재) — 프로브는 사이클2 재스폰으로 동일 드라이브를 반복해 대체.

**런그 8 — probe_conquest_taskbar.ps1 (최종 관문, 2026-09-23, 27체크 ×2 CONQUEST
PASS)**: taskbar/데스크톱 셸 단. **판정(스펙 §3.2 재량 — 트랙 B 불가의 구조적 근거
확인 후)**: 셸은 send_input 대상 배제 설계(IsShell() → bad_target, 2026-09-21
사용자 승인)이고 list_windows도 셸을 제외한다(:2963) — 표준 트랙 B의
launch→observe→drive가 도구 표면에서 성립하지 않는다. 셸의 정당한 조작 채널은
**셸 프로토콜 그 자체**(docs/28 — WindowList 스냅샷 푸시)이므로 drive를
창 수명주기 도구(launch_app/pid kill)로 대체하고 셸 자신의 픽셀로 검증하는
**셸 프로토콜 프록시 드라이브**로 정복. 캡처_window는 컴포지터 레이어 직접
조회라 셸 배제가 없어(:822, 레이어 조회만) 셸 표면 캡처가 가능 — observe의
유일한 창구. 진단 결판:

- **taskbar는 해시 유효 앱** — ClientTaskbarApp는 WindowListChanged/SizeChanged
  때만 다시 그린다(시계·자체 리드로우 없음, 런그 5 taskmgr와 반대). 정적 해시가
  검증기로 성립: h0(버튼 0) → launch_app tetris → h1(버튼 1) → kill →
  **h2==h0 exact**(버튼 폭이 kButtonMaxWidth=180 캡이라 1·2개 창 레이아웃 동일).
- **셸 레이어 id 발견** — list_windows에 없으므로 capture_window의 레이어
  직접 조회로 id 1..N을 스캔해 1280×40 단층을 찾는다. **스캔 상한은 프로브
  수명 동안 계속 커야 한다**: agentctl/MCP 호출마다 control-only 클라가 같은
  카운터에서 id를 소모한다(실측 diag_tb8e — ~35 호출 뒤 재스폰 taskbar가 id 25).
  1..16 고정 스캔은 cycle2에서 영원히 못 찾는다(1차 런 FAIL의 원인 — 제품
  결함 아님).
- **셸 사망 후 재등록** — kill해도 서버는 셸 없이 생존(실측), `launch_app
  {"app":"taskbar"}` → `--client taskbar` 스폰 → first-wins ShellRegister로
  셸 역할 재획득(실측 stderr "shell register accepted"). recover-gone 하드
  게이트는 list_windows 대신 **capture_window → window_not_found 폴링**.
- **설계 배제 검증이 drive 2단** — send_input on shell → `bad_target`(게이트
  ask라면 승인 오류로 가려지므로 도달하려면 allow RMW 필요), list_windows에
  "Taskbar" 부재 — 배제 설계 자체를 매 런 증명.
- cycle2 결정성: 재등록 셸의 h0·재드라이브 h1이 cycle1과 전부 exact 일치
  (정적 렌더 — 테마/지오메트리 불변).

**배치 공식런 완료 (2026-09-22, main 머지 후 — 프로브를 main 빌드로 재지정, 99e2928)**:
사다리가 main에 머지됐으므로 공식런도 main 빌드로 수행이 옳아 4종 프로브의 `$build`
하드코딩을 `engine\build`로 변경(영구 — worktree 경로 소각). 절차 전순 준수:
step0 permissions.json 백업 → (1) send_input_ask ×2(7체크 ALL PASS — 문서 표기 11은
착시, 실제 체크 행 7)/conquest_minesweeper ×2(19체크 CONQUEST PASS)/conquest_tetris ×2
(19체크 CONQUEST PASS) → (2) app_tools ×2(64체크 ALL PASS)+agent_e2e ×1(PASS, restore/
receipts/final 전부) → (3) `jkdesktop test` exit 0 → (4) permissions.json 복원(9키
전면 allow 동일) → 라이브 복원(메인 빌드 경로 — jkwinserver+jkbridge 기동, pong+8899
LISTENING+taskbar 클라 1개 확인). 런그 1(minesweeper)+런그 2(tetris) **코드 정복
판정 완료** — 잔여 판정 = LLM 실전 세션(§6)만.

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
| **minesweeper** | **코드 정복 완료+공식런 GREEN(2026-09-22 배치, §4)** — send_input 도구 + 정복 프로브 템플릿 확립. 잔여 판정 = LLM 실전 세션(§6) |
| tetris | **코드 정복 완료+공식런 GREEN(2026-09-22 배치, 19체크 ×2)** — probe_conquest_tetris.ps1(템플릿 복제+키 드라이브) |
| scriptdemo | **판정 완료(2026-09-23, 런그 6)** — 도구·이벤트 표면 0+시계 자체 리드로우로 verify 불능. 대상을 workshop으로 판정(아래 행), scriptdemo는 관찰 기록 |
| workshop | **코드 정복 완료+공식런 GREEN(2026-09-23, 런그 6 — 26체크 ×2)** — 사다리 최초 복합 트랙(A app_tool set_script + B send_input 클릭). **agentctl+공백 argv 분열(app_tool 판)을 잡은 단** |
| taskmgr | **코드 정복 완료+공식런 GREEN(2026-09-23, 런그 5 — 27체크 ×2)** — 첫 ImGui 앱 정복. drive=행 선택+Activate 버튼 2단 클릭, verify=focused 플래그 플립. **send_input 클릭 MouseMove 결함을 잡은 단** |
| terminal | **코드 정복 완료+공식런 GREEN(2026-09-23, 런그 4 — 25체크 ×2)** — type+key 드라이브, verify=앱 자기 토픽 이벤트+캡처 해시 2중. **정복 사다리가 첫 실제 제품 결함(reserved-topic 회귀)을 잡은 단** |
| vplayer | **코드 정복 완료+공식런 GREEN(2026-09-22, 런그 3 — 트랙 A 시제)** — drive가 send_input이 아니라 앱 자기 도구(app_tool open/play_pause/seek/get_status)로 갈아타는 첫 케이스. 잔여 판정 = LLM 실전 세션 |
| browser | **코드 정복 완료+공식런 GREEN(2026-09-23, 런그 7 — 32체크 ×2)** — 첫 CEF 앱 정복. drive=URL 바 클릭+type+RETURN 3단, verify=해시+교차 사이클 결정성 |
| taskbar | **코드 정복 완료+공식런 GREEN(2026-09-23, 런그 8 — 27체크 ×2)** — 최종 관문. **셸 프로토콜 프록시 드라이브**(창 수명주기 도구로 drive, 셸 표면 캡처로 verify) + 설계 배제(bad_target/list_windows 제외) 매 런 검증. send_input 표준 트랙 B는 셸 배제 설계상 부적합 판정. **사다리 8단 전부 코드 정복 완료** |

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
