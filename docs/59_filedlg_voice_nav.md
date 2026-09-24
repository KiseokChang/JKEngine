# 59. filedlg 음성 내비게이션 as-built — 2026-09-19

스펙: `docs/superpowers/specs/2026-09-19-filedlg-voice-nav-design.md` / 플랜:
`docs/superpowers/plans/2026-09-19-filedlg-voice-nav.md` / 원장(리뷰·룰링
전문): `.superpowers/sdd/2026-09-19-filedlg-voice-nav/progress.md`.

요구(사용자 2026-09-19): "열기 대화상자에서 파일 리스트 위/아래, pgdn/pgup,
상위 폴더, 선택이 말로 되어야 리모트 구동" — 폰(jkbridge)에서 file_open이
띄운 다이얼로그를 에이전트가 대신 조작한다. 앱 도구 허브(docs/58) 위에 얹는
두 번째 소비자이며, 사용자 지정 캐치 **"앱과 모달 다이얼로그는 차이가 있다"**를
프로토콜에 구조적으로 반영(modal 플래그 + 슬롯 소유 재검증)했다.

커밋 스팬: `7d0617c`(스펙) → `4c778e7`(플랜) → `fdec7e4`+`3d8f7e5`(서버
모달+가드) → `f684c5f`+`2a28b1a`(서버 wait:event+이벤트) → `988eb5e`+
`d11eca1`(filedlg 도구 3종) → `0405e5e`(브로커) → `99d19b8`+`0e3b328`+
`359555d`(프로브) → 본 커밋(docs/59) → 최종리뷰 FIX REQUIRED 픽스
`42b1ea5`(서버 Important-1/2+Minor-1) → `36dda88`(프로브 c9d) → 본 갱신
(docs/59 정정).

**검증 상태(정직 기록)**: 코드는 확정(태스크별 리뷰 FIX_REQUIRED→픽스→
재심사 FIXES_VERIFIED)이나 **e2e 프로브 ×2 ALL PASS는 미실행** — 라이브
사용자 데스크탑(파이프 상수라 공존 불가)+jkdesktop.exe 스테일(파일락) 2중
차단(§5). 실측 PASS는 jkagentd selftest와 프로브 fail-fast 가드 발화 실증,
PS5.1 ReadLineAsync 폴링 재실측뿐이다.

## 0. 스펙 결정 1-7의 코드 반영 (소스 근거)

| 결정 | 코드 반영 | 근거 |
|---|---|---|
| 1 도구 주체=filedlg 앱 자체 | `SendAgentToolRegister("filedlg", tools, true)` — 허브 경로 그대로, 브리지/브로커 제네릭 릴레이로 폰 노출(수정 0) | `ClientFileDialogApp.cpp:568` |
| 2 등록 시점=params 수락 직후 | 등록은 `PumpReplies`의 file_dialog_params 성공 경로(title 적용 직후) 말미 — params 실패(`no_pending_dialog`) 경로는 등록 코드 미도달 → 수동 기동은 애초 미등록(고아 가드 겸용) | `ClientFileDialogApp.cpp:568` 위치, 프로브 c10 전제(`JKWindowServer.cpp:4826` no_pending_dialog) |
| 3 modal 와이어 플래그 | 등록 JSON 루트 `"modal":true`(bool) → 서버 `AppToolManifest.modal`(기본 false) → `list_app_tools` 행에 false도 명시 | `JKClientSurface.cpp:511-517`(top-level, modal일 때만 부가 — 기존 발신자 바이트 무변경), `JKWindowServer.cpp:5042-5043`(`GetInt("modal")` — AgentJson에 bool 리더가 없어 bool=true가 JS_ToInt64로 1 수용되는 기존 파서 계약), `JKWindowServer.cpp:2674-2675` |
| 4 스테일 가드=서버가 슬롯 소유 판정 | `PendingFileDialog.dialogConnId`(params 처리 시점 기록, requesterConnId와 역할 분리) != 매니페스트 connId → 중계 전 `tool_gone` | 기록 `JKWindowServer.cpp:4842`, 가드(즉시 중계 경로) `:2738` |
| 5 file_open wait:event | `args.wait` 3분류(`""|"reply"|"event"`, 그 외 bad_args) → waitAsync 슬롯은 즉시 `{"ok":true,"parked":true}`, 해소/만료는 `file.open_result` 이벤트 단일 배달 | 파싱+bad_args `JKWindowServer.cpp:4745` 부근, 대입 `:4804`, 즉답 `:4808`, 해소 이벤트 `:4870-4891`, 만료 `:1595-1610` |
| 6 예약 접두 "file." | kReservedTopicPrefixes 편입 + publish_event 게이트 살리기(데드코드 수술 — c9ba8bf부터 무효였던 선행 결함) | `JKWindowServer.cpp:2568`, 게이트 `:3667-3680`, events_list 카탈로그 `:4087` |
| 7 choose와 navigate.select 역할 분리 | 공용 `ResolveOnOk`(빈 대상=nothing_selected/폴더=descended/파일=resolved+Finish) — choose는 이름 지정 즉해소(IEquals), select는 OnOk 동등 | `ClientFileDialogApp.cpp:672`(ResolveOnOk), `:771`(navigate select), `:834`(choose), `:820`(IEquals) |

결정 5가 설계 노트가 놓친 갭의 해소다: file_open의 파킹 응답은 해소 때까지
안 오므로(최대 600s) 브로커 QueryRaw가 블록 — 그 사이 에이전트가 filedlg
도구를 부를 수 없어 음성 흐름 자체가 성립하지 않았다. wait:event 주입(§7)이
이 갭을 끊는다.

### 0.1 리뷰에서 나온 반영 (결정에 없는 as-built 추가)

- **resolve 재중계 경로 가드**(3d8f7e5, Task 1 Important-1): ask 파킹 중 슬롯이
  재사용된 뒤 승인되면 `HandleApprovalResolve`의 재조회가 가드 없이 고아
  다이얼로그로 중계하는 우회 경로 — 즉시 중계 경로(`:2738`)와 동일 조건식을
  `JKWindowServer.cpp:4673-4674`에도 삽입. 중계 경로 전수 조사로 제3 경로
  없음 확인(파킹 기록 시점은 가드 통과 후라 안전).
- **만료 이중 전달 제거**(2a28b1a, Task 2 Important-2): waitAsync 만료 시
  신규 이벤트 뒤에 기존 dialog_timeout AgentReply가 무조건 실행돼 이미
  parked-ack 응답된 queryId에 2통째 reply — `eventModeExpired` 플래그로
  timeout reply 스킵(`JKWindowServer.cpp:1595-1626`). 단일 배달 불변식 회복.
- **down 미선택(-1)→0 착지**(d11eca1, Task 3 Minor-1): 이동 기준을
  `clamp(selectedIdx_+delta, 0, count-1)`로 — 초기 컷의 "0 기준 재계산"은
  미선택에서 첫 down이 entry 0을 스킵하는 무공개 편차였음(보고 누락 시정
  포함). **list "dir" 중복 키 제거**(동 커밋 Minor-2) — CommonFields() 단일
  출처로 통일.

## 1. Task 1 — 서버 modal 플래그 + 슬롯 소유자 재검증 (`fdec7e4` + `3d8f7e5`)

- `PendingFileDialog`에 `dialogConnId = 0`(`JKWindowServer.h:272`),
  `AppToolManifest`에 `modal = false`(`:297-298`). 주석에 requester(호출자
  상관)와 dialog(파라미터를 준 연결 — 가드 진실원)의 역할 구분 명기.
- 가드는 `HandleAgentQuery` clientsMutex_ 보유 경로(list_app_tools와 동일
  블록) — 신규 락 0. 슬롯 소멸 3지점(만료 스캔/해소 소진/요청자 연결 회수)
  전부 `PendingFileDialog{}`로 dialogConnId까지 0 초기화, 슬롯 재사용은
  `dialog_busy` 게이트 덕에 전면 소멸 후에만 가능. **최종리뷰 정정+픽스
  (42b1ea5, Important-1)**: 초기 구현은 케이스 ③(재사용 후 옛 다이얼로그)이
  **실측 미성립**이었다 — 가드가 `cands.size()==1` 분기 안에 있어 고아가
  후보에 끼면 ambiguous 즉답이 먼저 채택되고, 정상 슬롯 소유 다이얼로그의
  호출까지 고아가 살아 있는 한 봉쇄됐다(다이얼로그는 자기 만료가 없음).
  픽스 = app_tool 중계 후보 수집 직후 모달 후보를 슬롯 소유로 선별 제거
  (`m->modal && dialogConnId != m->connId`) — 제거 후 비면 tool_gone 즉답
  (기존 가드 동일 페이로드), 애초 후보 부재/windowId 불일치로 비는 경로는
  unknown_app_tool 현행 유지. 픽스 후 케이스 ①수동 기동 ②만료/해소 후 고아
  ③재사용 후 옛 다이얼로그 성립 — 정적 실측+프로브 c9d 체크 신설
  (36dda88), **e2e 실행은 §5.3 미실행 상태 유지**. windowId 직행 경로와
  resolve 재중계 경로(`:4673` 부근)는 connId 고정 조회라 무수정 확인.
- `list_app_tools` 행: `,"modal":` + true/false — **false도 명시**(스펙 §4,
  소비자 파싱 단순화).
- 리뷰 Important-1 픽스가 3d8f7e5(§0.1 첫 항목). NIT 수용(기록만): modal
  강제 파싱("2"/1.9→true — 인증된 등록 경로라 무해), disconnect-미정리
  매니페스트의 응답이 unknown_app_tool→tool_gone 변화(소비자 무구분).

## 2. Task 2 — 서버 file_open wait 모드 + file.open_result (`f684c5f` + `2a28b1a`)

- `PendingFileDialog.waitAsync` — wait 파싱은 filter/start/title과 동일
  블록, 검증 체인 선두에 `wait ∉ {"", "reply", "event"}` → bad_args
  (256자 상한 검사보다 앞 — 최저비용 모드 검증).
- 즉답은 HandleAgentQuery 꼬리의 기존 `if (replied)` 전송 경로 재사용 —
  신규 전송 코드 0. 해소 경로는 슬롯 reset 전에 waitAsync 캡처(리셋 후엔
  판독 불가) → waitAsync면 요청자 WriteAgentJson 완전 생략 + 이벤트 1회:
  ok+path / ok(빈 path) / 취소 `{"ok":false}`(**error 멤버 없음** — 사용자의
  정당한 행동) / 만료 `{"ok":false,"error":"expired"}`. 비waitAsync 경로는
  재들여쓰기만, 와이어 바이트 동일.
- 만료 이벤트는 슬라이스 슬롯 대조가 성공한 분기에서만 — 대조 실패(이미
  소진) 시엔 waitAsync 판독 불가+수신처 부재라 발행 안 함.
- **최종리뷰 픽스(42b1ea5) 2건**:
  - **해소 경로도 dialogConnId 진실원 적용(Important-2)**: file_open_result의
    senderMatched는 요청자 에코 일치만 보므로, 슬롯 재사용(고아 다이얼로그+
    동일 요청자의 재호출 — 브로커 연결은 영속이라 connId 동일) 조합에서
    고아의 결과가 "새" 슬롯을 대신 해소하는 교차배달이 가능했다.
    `senderMatched && (dialogConnId == 0 || client.Id() == dialogConnId)`
    로 발신 연결 소유까지 검증 — ClientFileDialogApp.h "an orphan dialog
    must resolve nothing" 계약의 집행. 수동 기동(requesterConnId 0,
    무슬롯)은 기존 no-op 불변 유지.
  - **요청자 연결 회수 경로의 이벤트 마감(Minor-1)**: 요청자가 다이얼로그
    도중 끊기면(폰 브리지 재접속 등) 슬롯을 회수만 하고 발행이 없어
    waitAsync 요청이 무통지로 사라졌다 — 회수 후엔 만료 스캔의 슬롯 대조가
    requestId 불일치로 영구 실패해 expired 이벤트도 timeout reply도 없었다.
    회수 시점에 waitAsync면 `file.open_result
    {"ok":false,"error":"requester_gone"}` 방송으로 waitAsync 슬롯의 모든
    종료 경로(해소/만료/회수)를 이벤트로 마감. **재접속 비복구 한계 명시**:
    이 방송은 푸시 시점 구독자에게만 가므로 끊긴 연결·재접속한 세션은 이
    통지를 수취할 수 없고 슬롯은 소진돼 복구되지 않는다 — 해소/만료와 같은
    자리의 최후 통지이지 재접속 복구 기능이 아니다.
- `kReservedTopicPrefixes`에 `"file."` — HandleToolRegister의
  namespace_conflict 검사가 같은 표를 공유하므로 등록 경로도 동시 강제.
- **부수 수술**: publish_event 예약 접두 게이트가 `reserved` 계산을 topic
  파싱 전(빈 문자열)에 돌려 항상 false — docs/54 NIT-3 게이트가 c9ba8bf부터
  사실상 no-op이었다(프로브가 reserved_topic을 단언 안 해서 생존). 픽스=
  파싱 후 재평가 람다 `topicReserved`(`:3667-3680`). 결정 6은 이 게이트
  위에 서 있었으므로 같이 무력 상태였던 셈.
- 편차 수용: 브리프 문구대로면 명시 `"reply"`도 bad_args였으나 스펙 §6이
  reply를 유효 모드로 명시 — 스펙 우위로 3값 수용.

## 3. Task 3 — filedlg 도구 3종 (`988eb5e` + `d11eca1`)

- 등록(결정 1/2/3): 도구명 navigate/list/choose + description +
  inputSchema 원문, `toolsRegistered_` 1회 가드. modal 시그니처는
  `SendAgentToolRegister(app, tools, bool modal = false)` 확장 —
  `JKClientSurface.cpp:511-517`이 `if (modal)`일 때만 루트에 `"modal":true`
  를 붙여 기존 vplayer 발신 바이트 무변경, 서버 루트 GetInt 위치와 일치
  실측(quickjs `JS_ToInt64Free`가 JS bool→1).
- **navigate**(`ClientFileDialogApp.cpp:740-771` 부근): index/key 배타 —
  둘 다 있으면 index 승리(명시 인자 우선), 모두 없으면 bad_args. index
  범위 밖 → `bad_index`+`count`. up/down ±1, pgup/pgdn ±페이지 크기 —
  페이지 크기는 코어 Run 스윕이 NewFrame 밖이므로 **클리퍼 루프가 갱신하는
  `visibleRows_` 캐시**(`:268`, `:750-753`, 최소 1). 이동 시 SetFileName
  미러(레거시 OnSelect 계약)+`scrollToSelection_` 플래그 → 다음 프레임
  클리퍼 루프에서 `SetScrollHereY(0.5f)` 1회(`:285-287`). 스키마 밖 key는
  bad_args 방어선(스펙 미정의).
- **list**: limit 기본 50/최대 200 클램프, 비양수 → 1(빈 페이지 방지 —
  스펙 상한만 명시한 것에 대한 구현 판단). offset 음수/범위 밖은 클램프가
  아니라 빈 `entries`+요청 offset 에코("그 위치엔 없다"). error_ 비어있지
  않으면 `"error"` 필드 부가.
- **choose**: name 있으면 IEquals 대소문자 무시 탐색(`:820`) → 미발견
  `no_such_entry`(상태 불변). name 없으면 현재 fileBuf/선택지로 ResolveOnOk.
- **해소 순서 보장**(스펙 §3.3): `ResolveOnOk`이 out 응답을 **선세팅 후
  Finish** — 코어가 훅 반환 직후 `SendAgentToolResult`를 먼저 보내고
  `!running_` 체크가 그 다음(JKClientApplication.cpp :286→:287→:292)이므로
  훅 안의 RequestQuit도 결과 전송 보장. OnOk()를 직접 부르지 않은 이유:
  Finish는 응답 형태를 모르고 하강/해소 판정이 응답 조립에 필요.
- ImGui 호출 0(페이지 캐시), 신규 락 0, 블로킹 I/O 0, 외부 의존 0.
  filedlg는 런처 .jkx가 아니라 서버가 `filedlg:<json>`로 직접 스폰하는
  shell-adjacent DLL(CMakeLists :545-547) — jkdesktop 파일락과 무관하게
  `jkapp_filedlg.dll`만 재빌드면 되고 .jkx 재포장 불필요.

## 4. Task 4 — 브로커 file_open 재조립 (`0405e5e`)

- **브리프 전제 정정(구현자 실측)**: file_open은 "분기 부재로 `{}` 폴백"이
  아니라 **3사이트 전부 부재**였다 — `kCoreToolsListJson` 정적부 행 부재
  (tools/list 미노출)+`IsKnownTool`/`LoadPermissions` kNames 부재(tools/call
  이 재조립 체인에 닿기도 전에 `ResolveAppTool` 역매칭 실패로 즉시
  unknown_tool). 즉 **MCP file_open 경로 자체가 처음 열린 것** — 부수 픽스라
  기각하기엔 효과가 크다. 3곳 모두 등록(`main.cpp:80-100-143`).
- 재조립: `BuildFileOpenArgs`(`main.cpp:562-563`) — `{"wait":"event"`로
  시작해 filter/start/title을 전달. **wait은 MCP 스키마에 노출하지 않는다**
  (주입 전용 — 노출하면 "스키마에 있으면 쓰인다"는 오해). tools/list의
  file_open description에 parked 즉답+read_events 폴링 문구.
- 동적 합성(filedlg_navigate/list/choose)은 기존 제네릭 경로 — 수정 0.
  폰(jkbridge) 제네릭 릴레이도 수정 0. 유일한 상호작용: 어떤 앱이
  (app="file", tool="open")을 등록하면 코어 충돌 가드가 `file_open` 합성명을
  스킵 — 코어 승리 규칙의 기존 의도(filedlg는 앱명 filedlg라 실충돌 없음).
- **셀프테스트 결함 1건 실측/픽스**: 초기안이 tools/call file_open 라이브
  질의로 not_connected를 기대했는데 이 머신에 라이브 서버가 살아 있어
  Connect가 성공 → **진짜 파킹 쿼리가 나가 셀프테스트가 120s 블록**. 서버
  질의를 안 하는 정적 검증(IsKnownTool 직접 호출+BuildFileOpenArgs 기대
  JSON 동등 비교, `main.cpp:875-899`)으로 교체. 교훈: 셀프테스트에 서버
  질의를 태우지 마라. (실수로 나간 질의 1건의 잔여 filedlg 다이얼로그
  PID 15628은 600s 만료 회수 대상 — 사용자 조치 불요.)

## 5. Task 5 — 프로브 probe_filedlg_voice (`99d19b8` + `0e3b328` + `359555d`)

### 5.1 설계 (스펙 §9 12체크 대응)

PS5.1 ASCII 전용, `> log 2>&1`, PID-유니크 permissions 백업+스테일 잔여
가드(전용 네임스페이스 `perm_pre_filedlgvoice_*`)+모든 fail-fast/finally에서
바이트 동일 복원. fixture `tmp/pfdv_<PID>/start/{a.txt,b.txt,c.txt,sub\}` →
entries `[.., sub, a.txt, b.txt, c.txt]`(total 5) — 정렬 계약까지 단정.

| §9 | 체크 | 검증 내용 |
|---|---|---|
| 1 | c1/c1b | file_open(reply 기본) → `--filedlg` 스폰 + 2초간 qid 401 응답 부재 = reply 모드가 정말 파킹 |
| 2 | c2 5종 | `list_app_tools` filedlg 3행 + `"modal":true` + title 에코 + inputSchema 원문 |
| 3 | c3 | list 필드(total/entries/dir/selected/file)+정렬 단정 |
| 4 | c4 | navigate index → selected 반영 + fileBuf 미러 |
| 5 | c5a-e | -1에서 down→0 착지(d11eca1 경계 수학)/up@0 클램프/마지막 down 클램프/bad_args/bad_index+count |
| 6 | c6 | key:parent 상승 + choose(dir) descended:true 하강 |
| 7 | c7 | choose(파일) → 앱 resolved **그리고** 파킹 qid 401 type-18 reply 수신 + 다이얼로그 소멸+카탈로그 0행 |
| 8 | c8a/c8b/c8c | wait:event 즉시 parked ack(<5s)+다이얼로그 B 스폰/choose 해소+file.open_result 이벤트+qid 402 이중전달 부재단정(1.1s 드레인)/**c8c=브로커 stdio 세션 e2e** |
| 9 | c9a/b/c | c9b=스펙 §5 직접 검증: 요청자 연결 Dispose → 슬롯 회수 → 다이얼로그 생존 상태 app_tool → `tool_gone`(중계 전 차단). c9c=kill → 매니페스트 정리 → unknown_app_tool |
| 10 | c10 | 수 manual `--filedlg "{}"` → params 실패로 미등록(결정 2의 부작용=가드 검증) |
| 11 | c11/c11b | publish_event `file.open_result` → reserved_topic 거부 + events_list 카탈로그 행 |
| 12 | c12a-e | jkagentd selftest(전체행 단정)/jkdesktop test 생존/probe_app_tools ×2/**c12d probe_files/c12e probe_agent_chat 중첩 편입** |

안전 설계: 스폰 전 인벤토리 출력+외래 fail-fast(파이프 상수 `JKWindowServerPipe`
라 재정의 불가 — 제2 서버 기동=인스턴스 갈림)/행동 이중 가드(이미지가 없어도
파이프에 응답하는 리스너 거부)/PID 스코프 킬($myPids+finally StartTime 필터
스윕 — 스폰 전 존재 프로세스는 실행 중 신규 기동분도 절대 안 죽임)/
Test-ForeignFree(c12c/d/e 각 중첩 직전 인벤토리 재검사 — 중첩 프로브가 이미지
전량 킬 패턴이라 실행 중간 창의 외래 간접 킬 봉쇄). PS5.1: [theme] 행 필터,
ProcessStartInfo 원시 Arguments 조립, `PathRx`(NavigateTo의 lexically_normal이
슬래시를 백슬래시로 돌려줌 — `[/\\]` 양쪽 수용), Read-Frame qid/ok 노출 확장
(probe_app_tools 것은 헤더 절단이라 파킹 reply 상관 불가).

### 5.2 검토 이력 — 3라운드

1. **`99d19b8` → 리뷰 FIX_REQUIRED(task-5-review.md)**: 11체크+가드 4종+
   PS5.1 대응은 소스 대조 전부 정합, 정직성 양호(차단 실측 기술). 결함:
   **C1 Critical — c8c가 브로커 stdout에서 file.open_result 대기 → 영구
   교착**(jkagentd stdout은 요청 1행당 응답 1행이 전부, 이벤트는 큐→
   read_events로만 배출; **보고서가 "stdout 비스트리밍"을 실측해놓고 코드는
   반대로 씀** — 그리고 read_events를 choose 후에만 호출해도 구독 선점
   없이는 영구 놓침: 브로커는 기동 시 subscribe=0, 서버는 푸시 시점 구독자에게만
   방송)+I1(probe_files/agent_chat 누락 미기재)+I2(c12c 중첩의 실행 중간
   외래 간접 킬)+M1(selftest 부분일치 "10 failures" 오탐)+M2(이중전달
   부재단정 부재)+M3(스테일 가드 jkagentd/jkapp_filedlg 미커버).
2. **픽스 `0e3b328`(+150/−37) → 재심사 FIXES_VERIFIED(task-5-rereview.md)**:
   C1 = c8c 재작성(id 1 init/2 file_open/3 read_events 구독 선점/4 choose/
   5 read_events 응답 행 단정 — id 유일·순차, notifications는 무id라
   요청:응답 1:1 유지)+`Read-BrokerLine`(ReadLineAsync 50ms 폴링+30s
   deadline, 무응답 프로세스 실발화 TIMEOUT-FIRED ms=3026 — 재심사가 본
   머신 ms=3004로 재실측). I1 = c12d/e 중첩 편입(양립 불가 전제 없음 소스
   확인). I2 = Test-ForeignFree 신설. M2 = c8b-no-reply-double-delivery
   (type 18+qid 402 부재단정). M3 = 스테일 가드 3쌍 — **jkdesktop.exe↔
   libjkserver.a, jkagentd.exe↔libjkcore.a, jkapp_filedlg.dll↔
   libjkclient.a**(`libjkagentd.a`는 빌드 트리에 존재하지 않음 — 없는 이름
   가정 금지; CMake 근거 jkagentd→jkcore, jkapp_filedlg→jkclient).
   신규: N-1 가드가 jkchat/jkapp_vplayer 미점검, N-2 주석 부정확.
3. **마이크로 픽스 `359555d`(2줄 규모, 컨트롤러 diff 직접 검증)**:
   Test-ForeignFree+인벤토리에 jkchat/jkapp_vplayer 추가, 함수/Read-BrokerLine
   주석 정정 — 로직 변경 0, PARSE-OK. **코드 확정 — 잔여는 e2e ×2뿐.**

### 5.3 e2e 미실행 상태 (정직 기록)

**12체크 e2e는 미실행.** "ALL PASS" 서술은 없다 — 아래가 현재 상태의 전부다.

- **차단 1 — 라이브 사용자 데스크탑**: 프로브 시작 시점 인벤토리 실측 —
  jkdesktop 4(서버 24080/taskbar 30052/vplayer 15708/잔여 filedlg 15628)+
  jkbridge 29824, 전부 Responding. 파이프 이름이 컴파일 타임 상수라
  (`JKWireEndpoints.h`) 유니크 파이프로 공존 불가, 태스크 제약(스포 안 한
  프로세스 킬 금지)과 결합하면 fail-fast가 유일한 정직한 경로.
- **차단 2 — 스테일 jkdesktop.exe**: mtime 17:32:36 < libjkserver.a 18:37 →
  Task 1/2 서버 코드가 미링크(바이너리 grep 실측: `file.open_result` 0건·
  `,"modal":` 0건). 원인 = 풀빌드의 jkdesktop.exe 재링크가 라이브 데스크탑
  파일락(ld Permission denied)으로 전부 실패. 재시도도 동일하게 실패 실측.
- **자기 가드(재실행 시 전제를 프로브가 스스로 재검증)**: ①setup-foreign-instance
  (스폰 전 인벤토리) ②setup-unknown-server(이미지 없어도 파이프 응답 리스너
  거부) ③setup-stale-binary(3쌍 mtime 대비 — 위 사건의 재발 방어) ④
  missing-artifacts+Test-ForeignFree(중첩 직전 외래 재출현 시 skip+FAIL).
- **가드 발화 실증은 실측**: 프로브 1회 실실행 → `setup-foreign-instance`
  FAIL, exit 1, 스폰 0, 라이브 5 PID 생존, permissions.json 바이트 동일
  (7키 allow 세트, mtime 프리 프로브 값 유지). PARSE-OK(PS AST 파서).
- **실측 PASS인 것**: jkagentd `--selftest` → `selftest: 0 failures`, exit 0
  (c12a와 동일 검증을 세션에서 직접 수행). PS5.1 ReadLineAsync 폴링 비봉쇄
  (TIMEOUT-FIRED 실측 2회).
- **미실측인 것**: 스펙 §9 12체크 실행, probe_app_tools ×2/probe_files/
  probe_agent_chat(c12c/d/e로 편입됨 — 실행 대기), c8a/c8b/c9b 타이밍
  플레이크 관찰. 정적 대조로는 12체크 전부가 Task 1-4 as-built 와이어
  (응답 봉투/파킹 reply 헤더/이벤트 페이로드/카탈로그 행 형식)와 대응 확인.

**재실행 절차** (사용자 조정 필요 — 데스크탑 종료 동의):

1. 라이브 데스크탑/브리지 세션 종료(눈확인 대기 세션일 수 있음 — 사용자 확인).
2. `cmake --build build --target jkdesktop` — libjkserver.a가 새로(스테일 가드 통과용).
3. `powershell -NoProfile -ExecutionPolicy Bypass -File engine/tools/probes/probe_filedlg_voice.ps1 > engine/tools/probes/probe_filedlg_voice_run1.log 2>&1` — ×2 연속 ALL PASS 요구.
4. 종료 후 permissions.json 바이트 동일(7키) 확인.

## 6. 오류 표 (스펙 §8 vs as-built 대조)

9행 전부 구현됐고 정적 대조(소스 판독)로 스펙 문구와 정합 — 단 **전부
런타임 미실측**(§5.3). 프로브 체크 열은 실행 대기인 대응 체크다.

| 상황 | 스펙 §8 응답 | as-built | 근거/체크 |
|---|---|---|---|
| navigate key/index 모두 없음 | `{"ok":true,"error":"bad_args"}` | 일치 — err 람다가 앱 실패도 ok:true 봉투로 | `ClientFileDialogApp.cpp` navigate 진입 / c5d |
| navigate index 범위 밖 | `{"ok":true,"error":"bad_index","count":N}` | 일치 | c5e |
| select/choose 대상 없음 | `{"ok":true,"error":"nothing_selected"}` | 일치(ResolveOnOk 공용) | `:672` / c5 계열·c7 |
| choose name 부재 | `{"ok":true,"error":"no_such_entry"}` | 일치(IEquals, 상태 불변) | `:820` / c6 |
| choose/select 폴더 | `{"ok":true,"descended":true,"dir":...}` | 일치 | c6 choose-dir |
| choose/select 파일 해소 | `{"ok":true,"resolved":true,"path":...}` | 일치 — out 선세팅 후 Finish(:286→:287 순서 근거) | c7/c8b |
| 모달 가드 실패(고아/미소유) | 서버 `tool_gone`(중계 전 차단) | 일치 — 즉시 중계+resolve 재중계 양쪽 가드 | `:2738`/`:4673` / c9b |
| file_open wait 알 수 없는 값 | `{"ok":false,"error":"bad_args"}` | 일치 — 단 `""`/`"reply"`/`"event"` 3값은 유효(스펙 §6 우위 편차 수용) | `:4745` 부근 |
| filedlg 미기동/사망 | 기존 `unknown_app_tool`/`tool_gone` | docs/58 §9 표면 그대로(연결 정리=unknown_app_tool, 중계 대기 중=tool_gone) | c9c/c10 |

as-built 추가 표면(스펙 표에 없음): 빈 리스트 navigate → `empty_list`,
list limit 비양수 → 1 클램프, 스키마 밖 key → `bad_args`(방어선), 예약 토픽
발행 → `reserved_topic`(c11).

## 7. 레슨

1. **구독 선점이 없으면 이벤트는 영원히 안 온다** — 서버는 푸시 시점
   구독자에게만 방송하고 브로커는 기동 시 subscribe=0으로 선언하므로, 이벤트
   수신 전 read_events 1회(구독+드레인)가 필수. choose 후에만 호출하는
   코드는 논리상 완전해 보여도 영원히 못 받는다(원 리뷰 C1의 2차 결함).
2. **브로커 stdout은 요청 1행당 응답 1행이 전부** — 이벤트는 내부 큐에만
   쌓이고 read_events로만 배출된다. "이벤트를 stdout에서 기다리는" 코드는
   영구 교착이며, 타임아웃 가드가 읽기 뒤에 있으면 무방비 — 타임아웃은
   읽기 자체에 내장돼야 한다(Read-BrokerLine 패턴).
3. **소스 실측 문구와 코드의 불일치는 진짜 결함이다** — 보고서가 "stdout
   비스트리밍 실측"이라 써놓고 코드는 반대로 썼다. 실측 기록은 코드 리뷰에서
   코드와 대조돼야 증거가 되고, 불일치 자체가 Critical로 판명했다.
4. **스테일 바이너리 판정은 exe mtime vs 짝 lib*.a mtime** — jkagentd.exe의
   짝은 libjkcore.a, jkapp_filedlg.dll의 짝은 libjkclient.a(libjkagentd.a는
   존재하지 않음 — 없는 이름 가정 금지). 풀빌드가 라이브 프로세스 파일락으로
   실패해도 일부 타깃만 빌드되면 불일치가 조용히 생긴다. 이 가드는 이번에
   실제 발화 조건이 성립한 상태로 발견됐다.
5. **중첩 회귀 프로브는 이미지 전량 킬을 한다** — 프로브가 다른 프로브를
   중첩 실행하면 그 프로브의 `Get-Process <이미지> | Stop-Process`가 수 분
   실행 중간에 뜬 외래 세션을 간접 킬한다. 중첩 직전마다 인벤토리 재검사+
   외래 재출현 시 skip+FAIL(Test-ForeignFree)이 최소 방어.
6. **PS5.1 ReadLineAsync는 동기 우회 없이 폴링 가능** — ReadLineAsync 50ms
   폴링+deadline이 무응답 프로세스에서 실발화(TIMEOUT-FIRED, 2회 재실측)로
   검증됐다. 단순 ReadLine 대기 루프는 타임아웃 가드가 루프 뒤에 있으면
   교착을 못 잡는다.

## 8. 후속 / 범위 밖

- **e2e ×2 ALL PASS** — §5.3 재실행 절차. 유일한 미완료 항목.
- **Minor-2(선행 결함 — 본 브랜치 회귀 아님, 후속 이월)**:
  `file_dialog_params` 게이트 none/allow(docs/48 이후 존재한 표면)로 제3
  에이전트 연결이 슬롯 활성 중 params+`requesterConnId`를 선점(다이얼로그
  굶김)할 수 있고, 얻은 id로 `file_open_result` 에코를 위조해 상관 검증을
  통과할 수 있다. 저비용 봉쇄안 = 호출자가 창 연결/windowId 보유 or
  `client.Id()==0` 기록 전 거부(본 브랜치가 만든 dialogConnId로 가능) —
  Important-2의 소유 검증이 1차 방어선이 되므로 긴급도는 낮음. 후속 스펙
  항목으로 이월(픽스 강제 아님).
- 스펙 §3.1 응답 예시의 `"selected":{name,isDir}` 객체 표기 vs 구현의
  `selected:i` 인덱스+`selectedName/selectedIsDir` 병행 — 구현이 양쪽 소비자
  모두 수용하며 스펙 문구 정정은 Task 6 문서로 이월돼 본 문서로 기록
  (스펙 §3.1 예시는 초안 유실로 판단).
- 스펙 §10 범위 밖 그대로: 필터 콤보 조작 도구/절대 경로 직접 해소/브로커
  tools/list의 modal 노출/다중 동시 다이얼로그(1-in-flight 유지).
- deferred minors: modal 강제 파싱(비문자열 진리값 — 인증된 등록 경로라
  무해)/unknown_app_tool→tool_gone 응답 변화의 소비자 무구분/list limit
  비양수 클램프(스펙 미정의 구현 판단)/navigate 스키마 밖 key bad_args 등 —
  원장 `.superpowers/sdd/2026-09-19-filedlg-voice-nav/progress.md` 참조.
## 9. e2e 라운드 — 파이프 최종쓰기 유실 근본 픽스 + 실측 (2026-09-20)

§5.3 e2e 재실행 중 폭발한 실전 결함 2건(폰 음성 내비 "MCP 불안정" 오인의
진짜 근원) + 프로브 검증 버그 일괄 픽스. 커밋 `7fd7b82`(서버) + `b53a85d`
(filedlg+프로브).

### 9.1 서버 — 연결 소멸 시 미독 메시지 큐 유실 (7fd7b82)

- **증상**: choose 직후 `tool_gone`+`dialog_busy`(600s 만료까지), ~1/7
  확률. 다이얼로그는 file_open_result/AgentToolResult를 전부 성공 쓰기
  (trace ret=1)하고 정상 종료.
- **오인 3연쇄**: ①클라 측 `FlushFileBuffers` 추가→불충분(ret=1에도 err=109
  지속) ②"커널이 버퍼 데이터 폐기" 가설→trace 실험으로 기각 ③진짜 인과:
  메인 루프가 `ProcessPendingMessages → Composite → Cleanup` 순서라 read
  스레드가 **Composite 도중** 마지막 메시지를 큐잉+disconnect 마킹하면
  cleanup이 **읽지 않은 큐째로** 클라이언트를 소멸. trace 빌드(느림)에선
  성공하고 cheap 빌드에선 ~1/7 파산한 타이밍 민감성이 정확히 이 창 크기.
- **픽스**: CleanupDisconnectedClients가 disconnected인데 큐가 남은
  클라이언트는 소멸 1이터레이션 유보(disconnected_==true면 read 루프 종료로
  큐 불변 → 다음 루프 ProcessPendingMessages가 드레인한 뒤 소멸). 클라 측
  FlushFileBuffers는 커널 인계 벨트로 유지(단독 픽스 아님).
- **레슨**: `FlushFileBuffers` 성공은 피어 앱의 소비를 보증하지 않는다.
  "데이터 유실"은 커널이 아니라 **앱 큐 소멸 경쟁**일 수 있다 — 실패율이
  빌드 속도(trace 유무)에 반비례하면 락/순서 경쟁부터 의심.

### 9.2 filedlg — params 타이틀 미전파 + 등록 게이트 불작동 (b53a85d)

- params title이 `JKWindow::SetTitle`(로컬 전용)로만 적용 → 서버 크롬과
  카탈로그가 메타 타이틀 고정. `SendWindowTitle` 전파 추가(docs/33,
  ClientNotifyApp 선례) — 전파가 등록보다 먼저 큐잉되므로 카탈로그도 갱신.
- 도구 등록 게이트 `if (!reply.ok) continue;`는 **한 번도 작동한 적 없음** —
  HandleAgentQuery가 응답 ok 바이트를 태초(220230e)부터 하드코딩 1로 보내
  no_pending_dialog도 ok=1로 도착. 스펙 §0 불변식(도구 가시성==슬롯 소유)
  이 깨져 수기 기동도 도구 3행 등록. 게이트를 params JSON의
  `requesterConnId`로 판정하도록 수정.
- **레슨**: 와이어 ok 바이트는 "응답 도달" 의미다 — 논리 ok는 JSON 필드.
  "항상 참인 것으로 쓰인 게이트"는 불변식이 처음 성립했는지 기생 커밋
  mtime으로라도 검증한다.

### 9.3 프로브 픽스 + 실측

- PathRx/인라인 경로 regex가 와이어 이중 백슬래시(`I:\progwork`)를 못 맞춤
  — `{1,2}` 수용(c2/c3/c4/c6/c7/c8b 일괄). c2 타이틀 에코는 카탈로그가 아닌
  `list_windows`로 검증(카탈로그 title=등록 시점 창 제목). c9c 후 connT
  요청자 폐기(c9d 파킹 슬롯이 c10에 params를 오염 주입 차단).
- **실측**: `probe_filedlg_voice` 60체크 **×2 연속 ALL PASS**(회귀 중첩
  c12a-e 포함). `pfdv_diag2` 12/12 해소. 중첩 probe_app_tools run1에서
  1회 플레이크(rows=0, vplayer 등록 레이스 — 재실행 통과, 기존 결함 아님).

## 10. 폰 플로우 재발 진단 — jkagentd tools/list 접합 파산 (2026-09-20)

§9 종료 뒤 사용자 폰 실사에서 동일 증상 재보고("MCP 연결이 자주 끊기네요",
"[!] busy", 수동 선택 무반응). 증거 3중 소스로 박제:

- **폰 세션 claude CLI 트랜스크립트**(`projects/I--progwork-JKENGINE/
  f2541842-*.jsonl`): 성공 호출 6건(list_windows×2 → launch_app →
  list_windows → file_open parked → read_events 빈) 뒤 MCP 호출이
  전면 "No such tool available. The MCP server 'jkdesktop' is still
  connecting"로 전락. 부수 실측: 폰 LLM이 Bash 도구로 jkdesktop 프로세스
  2인분 관측 → kill/`--server` 재기동 에스컬레이션(다중 기동 레슨 재현),
  `filedlg_open` 등 존재하지 않는 도구명 환각.
- **claude CLI 측 MCP 로그**(`AppData/Local/claude-cli-nodejs/Cache/
  I--progwork-JKENGINE/mcp-logs-jkdesktop/*.jsonl`)가 결정타:
  `Ignoring non-JSON line on stdout: JSON Parse error: Expected ']'` +
  `Failed to fetch tools: MCP error -32001: Request timed out` — tools/list
  응답이 비-JSON이라 CLI가 폐기 → 30s 타임아웃 → **jkagentd 강제종료·재스폰
  반복**(4분간 6회 스폰). "MCP 연결 불안정" 전부가 이 루프.
- **근원 1줄**: `ComposeToolsListJson` 동적부 조립이
  `if (dyn.empty()) dyn = ","` — 첫 행 앞에만 쉼표를 넣어 2행째부터
  `}{` 접합. 카탈로그가 비어 있으면(동적 0행) 정적부라 정상 — vplayer
  등 앱 창이 살아 도구를 등록하는 순간 tools/list 전체가 파산. 셀프테스트는
  서버 부재 환경이라 동적 경로가 한 번도 검증된 적 없었음(커버리지 갭).
- **픽스**: 합성 본체를 카탈로그 응답 문자열을 받는
  `ComposeToolsListJsonFromReply`로 추출 + 행 경계 쉼표 전면 수정
  (`if (dyn.empty()) dyn=","; else dyn += ",`;`). 셀프테스트에 스크립트
  카탈로그 3행 주입 검증 추가(코어 28 + 동적 3 = 31행, AgentJson 파싱).
- **검증**: selftest 0 failures. 라이브 서버(vplayer 창 id 6 생존, 카탈로그
  6행)에 tools/list → 유효 JSON 34행(28 코어 + 6 동적) + tools/call 정상.
- **유보(권고)**: ①폰 LLM의 Bash/파일 도구 제한(jkbridge 스폰 인자에
  `--disallowedTools "Bash"`) — 서버 kill·재기동 에스컬레이션 봉쇄. 단
  진단용 Bash까지 막히는 트레이드오프라 사용자 판정 필요. ②서버 단일
  인스턴스 가드(명명 뮤텍스) — 다중 기동 시 클라이언트 인스턴스 갈림의
  근본 봉쇄. ③jkbridge busy 게이트 큐잉 전환 — UX 수준.
- **레슨**: ①동적 합성 경로는 "정적 경로만 지나는 테스트"로는 무증거 —
  스크립트 주입형 합성 검증이 정답. ②"MCP 불안정" 같은 상위 증상은 자체
  프로세스가 아니라 **소비자(CLI)의 로그**에서 확정한다 — CLI가 남기는
  stdout 파싱 에러 한 줄이 3시간 추측을 대체. ③폰 LLM의 에스컬레이션
  (프로세스 kill/재기동)은 진짜 결함을 가리는 소음이자 새 결함의 원인 —
  에이전트에게 OS 프로세스 권한을 주지 않는 것이 안정성 그 자체.

## 11.1 가드를 Init 전으로 — 거부 인스턴스 선행 작업 소각 (2026-09-20, 4435aeb)

§11 as-built의 "위치" 갱신. `StartAcceptor`는 `Init`(앱 스캔+아이콘 디코드
전부) 뒤에 불리므로, 거부 인스턴스가 installed app 26줄+런처 아이콘 로드를
전부 수행한 뒤에야 기각됐다(사용자 붙여넣기 로그 실측). 픽스:
`TryAcquireSingleInstanceGuard`로 가드 블록 분리 → main.cpp/jkwinserver_main.cpp
양 경로가 **Init 전**에 취득, `StartAcceptor`는 미취득 시에만 취득(직접 호출자
방어선). 실측: 제2 인스턴스가 테마 1줄+가드 메시지 1줄만 출력하고 즉시
exit 1. probe_workshop ×2 + probe_app_tools ×2 ALL PASS(재빌드 회귀).

- **부수 픽스(잠복 결함 폭로)**: ClientScriptApp.h의 `#include <windows.h>`
  (docs/60 워크숍)가 main.cpp의 `JKDC::TextOut` 사용부를 wingdi
  `#define TextOut TextOutA` 매크로로 오염 — main.cpp.obj가 스테일로
  살아 있어 워크숍 빌드에선 무증상, main.cpp 재컴파일과 함께 발현.
  fileapi.h로 교체도 실패(wancode 레거시 BOOL/CreateDirectoryA 충돌) →
  **GetFileAttributesExA 수기 선언**(`_WINBASE_` 센티넬 — windows.h 선행
  TU는 SDK 선언 사용, 후행 TU는 수기 선언)로 양 TU 모두 해소.
- **레슨**: ①가드류는 첫머리에서 봉쇄 — 무거운 초기화 뒤의 기각은 기각이
  아니라 낭비 ②스테일 obj는 잠복 결함을 은폐한다 — 헤더에 windows 계열
  인클루드를 넣으면 그 헤더를 쓰는 전 TU를 상기시킬 것(main.cpp 포함)
  ③레거시 typedef(wancode BOOL)와 SDK 헤더가 같은 TU에 공존하면
  windows.h/fileapi.h 모두 안전하지 않다 — 필요한 OS 함수만 수기 선언.

## 11. 서버 단일 인스턴스 가드 — 명명 뮤텍스 + 파이프 벨트 (2026-09-20)

§10 유보 ② 채택(사용자 "2") 구현. 두 jkdesktop 서버가
`PIPE_UNLIMITED_INSTANCES` 파이프에 각자 인스턴스를 만들면 클라이언트
접속이 인스턴스 간 갈려 window_not_found 오판이 나는 것을 근본 봉쇄.

- **설계(2층)**: ①세션 로컬 명명 뮤텍스 `Local\jkdesktop-server-<파이프
  basename>` — `CreateMutexA` 초기소유, `ERROR_ALREADY_EXISTS`면 기각.
  커널이 프로세스 사망 시 뮤텍스를 회수하므로 크래시-재시작이 무료.
  ②뮤텍스 다음 `WaitNamedPipeA`(50ms) 벨트 — 가드 없는 구형 바이너리가
  이미 파이프에 살아 있는 경우 성공·`ERROR_PIPE_BUSY`면 기각,
  `ERROR_FILE_NOT_FOUND`면 진행. 기각 경로는 뮤텍스 release/close 후
  acceptor 미가동.
- **위치**: `JKWindowServer::StartAcceptor` 선두(bool 화 — 기각 시 false,
  호출부 main.cpp/jkwinserver_main.cpp는 exit 1). TU 관례 유지
  (windows.h 회피 — kernel32 수기 `extern "C" __declspec(dllimport)`
  선언 + `kErrorAlreadyExists=183`/`kErrorPipeBusy=231` constexpr,
  헤더 멤버는 `void*`로 spawnedClients_ 선례).
- **검증(양브랜치 실측)**: ①벨트 — 라이브 데스크탑 가동 중
  `jkwinserver.exe` 기동 → `a live pipe instance is already serving
  '\.\pipe\JKWindowServerPipe' (single-instance guard) — close it first`
  + exit 1, 라이브 서버 무교란. ②뮤텍스 — Init 없는 임시 하네스로
  커스텀 파이프 2인스턴스 → 2번째
  `another window server already holds the single-instance guard` 기각,
  GUARD-OK. 하네스·CMake 타깃은 검증 후 전부 제거(원복 확인).
- **부작용 2건 처리**: ①실행 중 exe 재링크 — 이전처럼 삭제는 불가라
  `jkdesktop.exe` → `jkdesktop.exe.running` 리네임 후 링크(구 프로세스는
  리네임 이미지 계속 사용; 구 프로세스 종료 후 `.running` 삭제 가능).
  ②하네스 StartAcceptor가 태스크바 클라이언트를 자동 스폰해 **사용자
  라이브 서버에 침투**(surfaceId=20, shell register DENIED로 권한은
  정상 봉쇄) → close_window id 20 + 프로세스 자연 종료로 청소.
- **레슨**: ①StartAcceptor는 Init와 무관하게 클라이언트 호스트를 스폰한다
  — dll이 인접해 있으면 하네스도 스폰하므로 서버 클래스만 손대는 테스트
  하네스는 커스텀 파이프+스폰 차단까지 고려. ②실행 중 이미지 교체는
  리네임 링크가 표준 절차(삭제 불가·리네임 가능). ③shell register
  DENIED가 실전에서 침투자 권한을 정확히 봉쇄했다 — 가드 이중 방어.

### 11.1 접 처리(takeover) — 기각에서 인수로 (2026-09-24, 28494e2)

사용자 보고 "'닫으라'고만 하는데 눈에도 안 보인다 — 명령을 알려주던가
아니면 직접 처리 하던가"(라이브 스택 서버가 숨김 기동이라 닫을 대상이
안 보였고, 라이브 서버는 `jkwinserver.exe`인데 사용자가 기동하는 건
`jkdesktop.exe --server`라 이미지명도 달라 매번 탐정 놀이가 됐다).

- **takeover 기본 ON** — 가드 충돌 시 보유자 `jkwinserver.exe`를
  TerminateProcess로 직접 종료하고 뮤텍스 재취득(최대 ~3s 재시도 루프).
  파이프 벨트 충돌(가드 이전 구 바이너리 보유자)도 같은 접 처리 후
  `ERROR_FILE_NOT_FOUND`(파이프 소멸)까지 대기. **단일 인스턴스 원칙은
  유지** — 죽이고 나서 취득하므로 갈림은 생기지 않고, 고아 태스크바는
  자가 종료(407af96)하므로 인수 후 일시 중복만 있다. 첫 실측: 3대 연쇄
  기동에서 매번 `takeover — killed holder jkwinserver.exe PID …` +
  신규 태스바 스폰 + ping ok.
- **jkdesktop.exe는 겨냥 금지** — 태스크바 클라·단일 프로세스 앱과
  이미지명이 같아 명령행(Toolhelp로 읽을 수 없음) 없이는 구분 불가.
  그 보유자일 때만 PID 후보 + `taskkill` 명령 힌트를 stderr로 출력.
- **부수 결함(첫 실측이 잡음)**: 후보 스캔이 거부 인스턴스 자기 자신을
  후보에 포함 — `GetCurrentProcessId()` 제외로 픽스.
- **레슨**: ①Toolhelp 스냅샷도 수기 선언 관례 동일(`PROCESSENTRY32W`는
  `ULONG_PTR` 멤버 8바이트 정렬이라 기본 정렬 — `FindFileDataA`의
  pack(4)과 반대). ②거부 UX의 정답은 "닫으라"가 아니라 "대신 정리해
  주거나 정확한 대상을 말해 주는 것" — 프로세스 이름만으로 정체를 알 수
  없는 환경(숨김 기동)에서는 특히.
- **후속**: 서버 인수 시 jkbridge는 기존 서버 접속을 잃는다 — 재기동
  필요(토큰 불변이라 폰 QR 재스캔 불요). 브리지 자동 재접속은 백로그.

## 12. 2차 파산 — inputSchema.type 정규화 누락 (2026-09-20)

§10 픽스 후 폰 재시도에서 재발("MCP 연결이 또 끊겼어요", send_keys 도구
없음 전락). MCP 로그가 즉시 결정타 — 이번엔 **JSON은 유효**했으나 CLI가
스키마 검증(zod)에서 거부:

```
tools/list failed ([{"code":"invalid_value","values":["object"],
 "path":["tools",29,"inputSchema","type"],"message":"Invalid input"}, ...
 "path":["tools",33,...]])
→ Failed to fetch tools → Terminating MCP server process tree → 재스폰
```

- **근원**: vplayer 카탈로그가 `play_pause`/`get_status`(동적 인덱스 29·33)
  의 inputSchema로 빈 객체 `{}`를 내보냄. MCP SDK는
  `inputSchema.type=="object"`를 필수 검증 — `{}`는 JSON으로 유효해도
  스키마 계약 위반. 조립부의 누락 폴백(`schema="{}"`)도 같은 값이라 같은
  파산. §10 검증이 "온전한 JSON 34행"만 봤던 게 갭 — **JSON 유효성 ≠
  소비자 스키마 계약**.
- **픽스**: `ComposeToolsListJsonFromReply`에 스키마 정규화 3단 — ①빈
  객체 `{}`(공백 제거 비교) → 완결형 `{"type":"object","properties":{}}`
  교체 ②type 누락 → 첫 `{` 뒤 `"type":"object",` 주입(properties 보존)
  ③type이 object 아님 → 완결형 교체(도구 생존 우선, 스키마만 상실).
  정규화는 `AgentJson s` 파싱 **전**에 수행(교체 후 파싱 — 처음엔 파싱
  뒤에 두어 s가 원본을 보고 중복 주입 `{"type":"object","type":"object"...}`
  가 됐다, selftest가 잡음). 서버 부재 기본 폴백도 완결형으로.
- **검증**: selftest에 t2(`{}` 스키마) 행의 정규화 검증 추가 — 0 failures
  (중복 주입·후행 쉼표 `{"type":"object",}` 두 픽스 라운드의 회귀 전부
  포착). 라이브 MCP 핸드셰이크 실측: initialize+tools/list → 34행 전부
  `inputSchema.type=="object"`, schema-bad 0.
- **레슨**: ①검증은 소비자의 계약 수준에서 — "JSON 파싱 성공"은 MCP
  SDK의 zod 스키마 검증을 통과하지 못하면 무의미. 상위 증상(MCP 연결
  끊김)이 재발하면 로그의 에러 **형태**가 바뀌었는지 먼저 본다(1차=비-JSON
  파싱 에러, 2차=zod invalid_value — 같은 증상, 다른 결함). ②문자열 주입
  정규화는 빈 컨테이너 특례가 먼저(`{`+주입은 후행 쉼표 파산). ③교체형
  정규화는 파싱 전에 — 정규화 결과를 파서가 봐야 type 이중 주입이
  안 생긴다.

## 13. 3차 파산 — filedlg 슬롯 리바인드 (2026-09-20)

§11·§12 픽스 뒤 폰 재시도에서 도구 목록·filedlg_open은 성공, 그러나
**턴 2의 `filedlg_navigate`가 tool_gone** — 다이얼로그가 화면에 떠 있는
데도 내비게이션 전면 봉쇄. 트랜스크립트가 ground truth:

- `file_open` → `{"ok":true,"parked":true}`(wait:event 정상 ack) →
  다이얼로그 스폰·창 등록까지 성공. 이 단계까지는 전부 정상.
- **근원**: jkagentd는 CLI가 턴마다 재스폰하므로 **요청자 연결이 턴마다
  죽는다**. 턴 종료 시 서버의 요청자 회수기(final-review MINOR-2)가
  filedlg 슬롯을 전량 비운다(`pendingFileDialog_ = {}`) → 생존한
  다이얼로그의 매니페스트는 모달 가드(스펙 §5)에서
  `dialogConnId != m->connId` 판정 → 영구 tool_gone. §5의 "고아 가드"는
  데스크탑 세션(요청자가 계속 살아 있음)에선 옳았지만 폰 플로우에선
  **요청자 교체가 정상 수명주기**라 가드가 곧 기능 파괴. 기존 프로브
  c9b가 tool_gone을 설계 동작으로 검증하고 있었음(설계·검증이 같은
  전제 — 폰 턴 모델이 이 전제를 깬 것).
- **픽스(슬롯 리바인드 — 소유 승계)**: app_tool 중계에 사전 단계 추가 —
  슬롯이 "완전히" 비어 있고(회수/만료) 목표 모달 후보의 연결이 생존해
  있으면 제어 채널 호출자를 새 요청자로 리바인드(paramsTaken=true —
  원 다이얼로그가 이미 소진, waitAsync=true — 해소는 이벤트 방송).
  슬롯에 소유 다이얼로그가 기록돼 있으면 기존 tool_gone 유지(진짜 모달
  경합). 연쇄 픽스 2건: ①해소 상관 검증 — 다이얼로그는 **원 요청자 id**를
  에코하므로 요청자 교체 후 에코가 영구 불일치 → 발신 연결 ==
  슬롯 dialogConnId(파라미터를 준 다이얼로그 본인 — 요청자 에코보다
  강한 진실원)도 수락 ②waitAsync 해소 이벤트가 파킹 승인 항목 매칭에
  묶여 있어 리바인드 슬롯(항목 없음)은 해소돼도 무통지 → 이벤트 방송을
  승인 항목과 독립으로 호이스트(부수로 "해소됐는데 구독자가 못 받는"
  기존 무통지 구멍도 마감).
- **검증**: 빌드+링크 OK. probe_filedlg_voice c9b 기대를 리바인드 계약으로
  갱신(dispose → 새 control-only 호출이 리바인드하고 중계 정상 응답 —
  라이브 데스크탑 점유 중이라 프로브 실행은 데스크탑 종료 후 첫 세션).
  서버 측 픽스라 **데스크탑 재기동 필요** — 현행 서버(27756)는 이전
  바이너리.
- **레슨**: ①요청자 수명 모델이 다른 소비자(데스크탑 영구 세션 vs 폰
  턴당 리스폰)를 하나의 슬롯 계약에 몰아넣으면 반드시 어긋난다 — 슬롯
  소유는 "요청자"가 아니라 "소유 다이얼로그"를 진실원으로 하고 요청자는
  승계 가능하게. ②설계 당시 프로브가 tool_gone을 PASS로 검증했어도
  전제(요청자 불사)가 틀리면 검증은 결함을 확증한 것 — e2e 검증은
  소비자의 실제 수명 모델 위에서. ③트랜스크립트의 "승인 팝업이 안 뜬다"
  같은 LLM 해석은 틀리기 쉽다 — parked:true는 wait:event의 정상 ack였다.

## 14. 유보 ③① 소각 — bridge busy 큐잉 + LLM kill-deny (2026-09-20)

사용자 "쭉쭉 재량껏"으로 §10 유보 잔여 2건 처리.

- **③ busy 큐잉**: 기존 `[!] busy` 즉거부를 **대기열 전환**으로. BridgeSession에
  대기열(상한 3 — 플러드 백로그 레슨 선례)을 두고, 턴 실행 중 도착 chat은
  `chat_queued`로 적립. OnLlmDone이 다음 턴을 즉시 시작(`chat_queued_start`
  통지) — busy_는 DoneFn **전에** 해제된다는 엔진 Finish 관례(주석 명시)가
  성립 조건. 같은 keep(세소유권 shared_ptr)을 다음 턴의 DoneFn으로 넘기므로
  드레인 경로에서 delete하지 않는다. 재진입 경합(극히 드문 StartTurn 실패)은
  맨 앞 되돌림으로 순서 보존. jkchat은 무수정(엔진 아닌 브리지 측 큐).
- **① kill-deny**: 전면 Bash 차단(§10 원안)은 진단 능력 상실로 미채택 —
  **프로세스 kill 계열만 deny**하는 `--settings`를 BuildEngineCmd에 고정
  주입. 실측 2건: ①직접 claude 경로 ②실제 브리지 스폰 경로
  (cmd.exe /c → ollama launch → claude) — **--dangerously-skip-permissions
  하에서도 deny 규칙은 강제된다**(permission_denials로 차단). 인용은
  prompt의 `\"` 이스케이프와 동일 CRT 규칙 — 1차 구현이 원문 따옴표를
  써서 argv 재파싱이 JSON을 쪼갤 뻔한 것을 검토 중 발견·픽스. 잔여 우회
  (powershell 래퍼 등 접두 외 경로)는 미커버 — 재발 시 전면 차단으로 상향.
- **검증**: 빌드 클린 + jkagentd selftest 0 failures. smoke_llm_stream /
  probe_jkbridge는 실행 중 데스크탑·브리지를 이미지 단위로 죽이므로
  다음 데스크탑 종료창에 회귀(서버·브리지 재기동 후 효과 발동).
- **레슨**: ①deny 규칙은 skipPermissions에서도 강제된다("권한 우회가 deny를
  우회한다"는 추측은 무증상 실측으로 반증) ②명령행 임베드 JSON은
  반드시 인용 이스케이프 — prompt 선례와 같은 CRT 규칙 ③이미지 단위
  Stop-Process를 하는 프로브는 실행 중 라이브 프로세스와 공존 불가 —
  회귀는 프로세스 정지창에 예약.

## 15. 회귀 창 소각 + 프로브 타이밍 플레이크 4종 (2026-09-20, a755ff9)

눈확인(노트·files 창) 완료 후 데스크탑 정지창에 §14 회귀 3종 실행 — 전부
2연속 GREEN: probe_jkbridge 28체크 ×2, smoke_llm_stream 3/3, 
probe_filedlg_voice 60체크 ×2(중첩 probe_app_tools ×2·probe_files·
probe_agent_chat·highlight 포함), probe_agent_chat 단독 ×2.

프로브 4종의 하네스 결함(제품 결함 아님 — 전부 타이밍 플레이크):

- **probe_jkbridge resume-memo FAIL ×3**: 직전 세션 캡 단계가 소켓 5개를
  닫고 2초 대기인데 드레인이 늦으면 다음 접속이 "too many sessions" 캡
  에러 프레임을 받는다 — hello 자체가 안 옴. 재시도 접속 루프로 해소.
  DBG 1줄만 넣어도 통과하던 미스터리는 캡 프레임 수신이 우연히 늦은 것.
- **probe_filedlg_voice foreign 오탐 ×2종**: ①Test-ForeignFree가 **자기
  서버의 태스크바 자식**(`--client taskbar`)을 foreign으로 잡아 중첩
  회귀 전량 SKIPPED — Add-TreePids가 셋업에 1회뿐인데 태스크바 스폰이
  server-up 감지보다 늦으면 myPids에 못 들어간다. 체크 직전 트리
  재수집으로 해소. ②c12b 테스트 인스턴스를 부모만 Stop-Process하면
  고아가 남지만(실측: test 모드는 자식 없음 — 방어만) 트리 kill로 정리.
  foreign 진단에 ppid/cmdline 출력 추가 — 이 출력이 범인 특정의 결정타.
- **probe_agent_chat minesweeper**: 창 생성 대기 고정 2초 부족(빈
  windows[]) → 15회 폴링.
- **probe_files 서버 기동**: 4초 고정 슬립 — 파이프가 늦으면 첫 2체크가
  CreateFileA(2)로 사망 → ping 폴링. 중첩 로그도 유니크 파일명(run2의
  ALL PASS가 run1 실패 로그를 덮어 판독 불가였던 것).
- **레슨**: ①프로브 FAIL 재현 불명이면 먼저 "어떤 프레임을 받았나"를
  출력해 확인 — resume-memo는 캡 에러 프레임이었다 ②foreign 판정은
  ppid/cmdline 없이는 단서가 안 된다 ③중첩 프로브의 고정 슬립은 전부
  readiness 폴링으로 — 파이프 존재는 서버 응답으로만 확증 ④회귀 프로브
  2연속 원칙은 플레이크 폭로 장치로 기능한다(1회 PASS는 반칙).

## 15.1 프로브 state 누출 — 라이브 토큰 사망 (2026-09-20, 6138e42)

재기동한 브리지 콘솔에 토큰이 `probetoken0123456789abcdef`로 찍혔다 —
프로브 토큰이 라이브 `state/jkbridge.json`을 대체해 **폰의 저장 URL이
401로 사망**. probe_jkbridge는 백업·복구 코드가 이미 있었지만 복구가
파일 끝(클린 완료 경로)에만 존재 — 본문 중간 크래시(probes 디렉의
bash.exe.stackdump 실존)가 복구를 건너뛰면 누출된다. 이후 실행들은
누출된 파일을 "원본"으로 백업해 누출을 영구화(백업이 오염을 상속).
픽스=cleanup+restore 전체를 finally로 — 모든 탈출 경로 보장. 유저 토큰은
복구 불가(백업 없음) → 32hex 신규 발급, 폰 QR 재스캔 필요.

- **레슨**: ①프로브의 state 복구는 "코드 끝"이 아니라 **finally** —
  클린 경로 복구는 복구가 아니다 ②누출된 state를 백업하는 후속 실행은
  오염을 상속해 영구화한다 — 백업 시점에 이미 오염돼 있을 가능성을
  의심 ③라이브 소비자(브리지) 재기동 시 토큰·포트 같은 "유저가 저장한
  연결 수단"을 콘솔 출력으로 즉시 확인 — 프로브 창 직후 재기동은 특히.

## 16. 레저 소액 소각 4건 (2026-09-20, 무커밋→커밋)

docs/56 §2b + docs/57 §9 + docs/54 §10 백로그 소각. 회귀 프로브는
사용자 선택으로 후속(데스크탑 정지창 필요 — 살아있는 데스크탑과 파이프
충돌). 빌드 전부 GREEN(jkwinserver/jkbridge).

1. **파킹 플러드 상한 (docs/56 §2b)** — pendingApprovals_ 요청자(connection
   id)별 미해결 승인 8개 상한. 초과 파킹 시도는 파킹하지 않고
   `approval_overflow` 즉시 응답. 파킹 사이트 10곳 전부 가드
   (close_window/trust_request/permission_set/trust_revoke/run_console_app/
   capture_allow/files_access 3곳/app_tool/file_open) — 가드가 사이트별로
   흩어져 있어 한 곳이라 빠뜨리면 그 종류만 뚫린다(형태 3종: break형
   if-gate 6곳, if/else형 else-if 3곳, file_open 1곳).
2. **jkbridge 인터페이스 바인딩 (docs/57 §9)** — state/jkbridge.json 옵션
   `"bind":"<IPv4>"`. 미지정 = 기존 INADDR_ANY(토큰 게이트). 오탈자 IP는
   fail-closed 루프백+경고 — **ANY 폴백은 "축소하려는 의도"를 "확장"으로
   반전시킨다** (URL 표시도 bind 지정 시 그 IP 기준).
3. **레이아웃 경로 (docs/54 §10)** — 코드 변경 불요. 저장/열거 전부
   cf0d828(2026-09-10)부터 `state\` 기준 — docs 표기가 스테일. **레슨:
   백로그 문서도 코드와 대조 확인 후 소각 — "exe-dir 열거"는 이미 10일 전
   해소된 내용이었다.**
4. **receipt 프룬 스트리밍화 (docs/54 §10)** — 전체 파일+kept 문자열 메모리
   적재 → 64KiB 청크 행 스캔 + 행 단위 `.new` 복사. 행당 메모리 = 최대 행
   길이. **픽스 구조: `.new`가 완성될 때까지 원본 무손상(쓰기 실패 시
   remove(.new)만) — 구 구현은 rename 후 쓰기라 쓰기 실패에 복원 댄스가
   필요했는데, 순서를 바꾸면 복원 경로 자체가 사라진다.** M5 rename 댄스
   (브로커 append 창 공유 위반 → 중단) 규약 유지, .bak 1세대 보존 유지.

- **레슨**: ①플러드 가드는 게이트 결과 응답이 아니라 **파킹 직전**에 —
  승인 이벤트 방송 후에 걸면 스트립 도배는 이미 일어났다 ②제한 설정의
  실패 모드는 항상 "의도의 반전"을 의심 — bind 오탈자의 자연 폴백(ANY)은
  요청의 반대다 ③구 rename 후 쓰기 → 쓰기 후 rename으로 순서만 바꿔도
  복원 경로가 필요 없어진다 (실패 가능 지점을 원본 파괴 전으로 모은다).

## 17. 빌드 temp C:→I: 이전 — "MinGW가 I:(exFAT)에 못 쓴다" 스테일 제약 소각 (2026-09-20)

C: 97% 압박 — temp_jkdesktop(1.5G)·temp_jkdesktop_wt(1.3G)·
temp_jkdesktop_agentmgr(420M)·temp_jkdesktop_build·temp_jkproto_sdl2_jkwindow
삭제(약 3.3G 회수). build_with_temp.sh의 TEMP를 /i/temp_jkdesktop으로.

근거 실측: ①본 세션 내내 engine/build(I:)에서 직접 `cmake --build` 성공
②I: temp에서 configure+ninja 풀빌드 [157/157] 성공 — **exFAT 쓰기 제약은
재현 불가(스테일)**. docs/10의 "재현되면 빌드 트리만 C:로 분리"는
컨틴전시 플랜으로 유지. temp_jkwin_verify(2.2M)는 후속 요청으로
I:\temp_jkwin_verify로 이동+참조 갱신(tools 검증 스크립트 5종+docs/15) —
C: 잔여 temp(temp_jkwin_verify+C:\temp) 전부 소각.

- **레슨**: ①빌드 temp는 전량 I:로 — C:에 소스 사본을 두지 않는다(사본은
  반드시 스테일이 된다 — docs/57 §8과 같은 뿌리) ②"못 쓴다"류 드라이브
  제약은 주기적으로 재실측 — 환경(msys2/드라이브)이 바뀌면 제약은 유령이
  된다 ③ninja ld 일시 실패(exit 67) 재확인 — 1차 실패→재실행 회복, 파이프
  `tail`이 실패 지점을 잘라 재현에 2번 들었다(파일 리다이렉트 레슨 재확인).

## 16.1 회귀 + 신규 프로브 (2026-09-20, 정지창)

§16 픽스 회귀 2연속 전부 PASS: probe_settings(프룬 스트리밍 경로)×2 ALL
PASS, probe_files×2, probe_agent_trust×2, probe_app_tools×2, jkdesktop
test 0 failure. **probe_approval_overflow 신설** — raw 파이프 하네스
(probe_app_tools 계열, Hello→구독→같은 연결에서 AgentQuery 9회)로
trust_request 8회 파킹+9번째 approval_overflow+승인 이벤트 8회 실측
×2 PASS. 진단 노트: 첫 판정 "9개 전부 즉시 응답"은 파킹 기계 결함이 아니라
프로브 지문 65자(64hex+1) bad_fingerprint — 응답이 전부 에러였다.

- 참고: 진행 중 permissions.json이 사라져 있는 것을 발견(어제 만든 전면
  allow 런타임 파일, 백업 없음) — 사용자 확인 후 전면 allow로 복원
  (close_window/trust_request/run_console_app/trust_revoke/permission_set/
  files_list/files_read/files_audit/file_open). 어젯밤 mgr_t1류 검증
  프로브의 Remove-Item 패턴이 유력 원인 — **프로브가 유저 런타임 파일을
  소유·삭제할 때 백업+콘솔 고지 필수**.

## 18. vplayer open 실패가 에이전트 표면에서 증발 (2026-09-20)

폰 브리지 제보: "vplayer가 영상을 안 열어요" — `open` 앱 도구가
`accepted:true`를 돌려주고 `get_status`는 `opened=false`만 반복
(`openFailed=false`, `error=""`), vplayer 재시작 후에도 동일. LLM은
원인을 알 방법이 없어 수동 Ctrl+O 안내로 끝났다.

**원인 1 — 파일이 손상됐다(제품 결함 아님).** `I:\@keep\SAME-234ch.mp4`
(4.77GB)와 `SAME-220\hhd800.com@SAME-220.mp4` (4.2GB) 모두 순정
ffprobe(FFmpeg 8.1.1 풀빌드)도 "moov atom not found"로 거부. 전체
파일 스캔 결과 'moov' fourcc가 어디에도 없고, mdat 뒤에 moov **몸통**
(trak/tkhd/stbl/stsz/co64/udta)은 있지만 **헤더(size+'moov'+mvhd)가
소실**되고 그 자리에 영상 데이터 ~2.4MB가 덮여 있음 — 다운로드 조립
불량형 손상(두 파일 mtime이 세션 직전 13:26/13:37, 이후 크기 불변).
vplayer의 판정(`open: Invalid data found when processing input`)은
정확했다. 4GiB 경계/32비트 오프셋 가설은 ffprobe 재현으로 기각.

**원인 2 — 진짜 결함: 실패가 get_status에서 증발.** BuildUi는
`st.openFailed` 스냅을 openError_로 옮기고 즉시 ClosePlayer하므로
(1프레임 내) 실패 후 `player_`가 null → get_status가 `Snap{}` 기본값
(전부 false, error "")을 반환. 폰 LLM은 "수락됐는데 아무 일도 없음"만
보고 영원히 폴링했다. **교훈: 비동기 오픈의 실패 상태가 UI 소비 경로에서
파괴되면, 에이전트 표면은 성공/실패 이외 제3의 상태(침묵)를 보게 된다 —
실패 기록은 표면이 아니라 소유자가 보존해야 한다.**

**픽스** — `ClientVPlayerApp::OnAgentToolCall` get_status: `player_`
부재 시 openError_가 마지막 open의 유일한 생존 기록이므로 이를 그대로
보고(`openFailed`: openError_ 비어있지 않으면 true, `error`:
EscapeJson(openError_)). 코어 해체 설계는 유지(리소스 회수 정상)하고
표면만 복원 — 최소 변경. **프로브**: probe_app_tools에 c3b 신설
(손상 픽스처 tmp/vpt1_trunc_tail.mp4 → get_status openFailed+error
실측) ×2 ALL PASS(63체크). 라이브 재검증: 같은 파일이
`{"openFailed":true,"error":"open: Invalid data found when processing
input"}` 반환 확인.

- 참고: 진단 중 MCP `app_tool`의 중첩 인자 파라미터명을 잘못 줘
  bad_args가 2회 발생 — vplayer의 bad_args 응답은 이렇게도 도달성
  확인에 쓰였다(에러가 앱까지 살아서 돌아오면 중계 경로는 정상).
