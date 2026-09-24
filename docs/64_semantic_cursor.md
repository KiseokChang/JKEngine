# docs/64 — 의미 커서(semantic cursor) v1 as-built (2026-09-22)

스펙: docs/superpowers/specs/2026-09-22-semantic-cursor-design.md
플랜/SDD: docs/superpowers/plans/2026-09-22-semantic-cursor.md (6태스크, 원장
`.superpowers/sdd/2026-09-22-semantic-cursor/progress.md`)

**핵심 한 문장**(스펙): 앱은 "내 공간의 지도(선언)"와 "행위의 의미(act 콜백)"만 제공하고,
커서·이동·승인·하이라이트는 플랫폼이 소유한다. 첫 소비자 = 지뢰찾기(자유 커서).

커밋 사슬(BASE abde207 → 3cd68c3, 총 11커밋): de4f5ef+575a0e1+b626349(서버 선언 파싱+
move/read), e7a8ff3+bc69ded(act 릴레이+배너), bc765b7+146af23(하이라이트 2계층),
fe3e693+5df153d(지뢰찾기 클라), 7e859cb+3cd68c3(probe_semantic_cursor 신설).

## 1. 구현 요약 — 계약 5단

| 단 | 내용 | 구현 위치 |
|---|---|---|
| ① 선언 블록 | 앱이 `AgentToolRegister`(와이어 type 22)에 `cursor` 블록을 실어 보고 — cell-grid/coordSpace client/origin/cellW/H/rows/cols/cursorOwner/act.kinds/act.gate. 실패(`bad_cursor`/`cursor_owner_unsupported`/`cursor_name_conflict`/control-only `cursor_window_required`)는 등록 전체 거부(fail-closed — 반쪽 선언 혼종 봉쇄) | `JKWindowServer::HandleToolRegister` (src/server/JKWindowServer.cpp:5993), 클라 발신 `JKClientSurface::SendAgentToolRegister` 4번 인자 `cursorJson` (src/client/JKClientSurface.cpp:536, :545) |
| ② 합성 도구 | 선언 존재 시 매니페스트에 `move`/`read`(무승인 — window_move 분류)와 앱 자체 act 부재 시 `act`(기본 ask)를 합성 등록 → list_app_tools/tools/list 자동 흐름. 앱 자체 act엔 선언 kinds enum을 병합 주입(합성·own 양경로) | 합성 등록 JKWindowServer.cpp:6190 근처, `InjectKindsEnum` :5873 |
| ③ act 릴레이 | `<app>.act`를 릴레이 체인 앞에서 사전검증(kind enum/인자/격자 → bad_args/bad_grid 즉답, 유효=원문 패스스루) → 파킹(셀 rect 고정) → 승인 배너 `<app>.<kind> at (r,c)` → 승인 시점 `CursorActStale` 재검증(선언 소실/격자 밖/rect 불일치/kind enum 이탈=bad_grid 거부) 후 원 요청 릴레이 | 인터셉트 :3140, 파킹 레코드 :3340(헤더 :349-355), 배너명 :3324, 재검증 `CursorActStale` :6711(호출 :5473-5496) |
| ④ 하이라이트 2계층 | 커서 셀(지속, 파랑 3중 스트로크 — 매 프레임 최신 선언+cursorState로 산출) + 승인 대상 셀(파킹 시 고정 rect, 호박 링) — 기존 승인 창 링/배너 격침, 앱 표면 무접촉 | `DrawSemanticCursorCells` :6987(계층2 :7049), `DrawRing3` :6844, 오버레이 훅 :253 |
| ⑤ 지뢰찾기 클라 | 순수 게임 전이(`MineSweeperGame::Act`/`SnapshotLines`)+렌더 산식 실측 선언(`GetBoardGeometry`→`CursorDeclJson`)+스폰 직후 등록+act/snapshot 훅 | src/apps/MineSweeperApp.cpp:384-(:402 Act, :464 SnapshotLines, :554 GetBoardGeometry), ClientMineSweeperApp.cpp:151/:174/:225/:228 |

플랫폼 도구 2종은 `JKSemanticCursor` 순수 상태 머신(include/server/JKSemanticCursor.h:28,
cpp 72줄 — 뷰/서버 의존 0, TerminalHangulInput 선례)이 소유: `Reset/MoveTo/MoveRelative/
RunSteps` → `Result{row,col,error}`(:42-:51). 상대·스텝은 격자 경계 클램프, 스텝은 **첫 경계에서
중단+도달 칸 에코**(src/server/JKSemanticCursor.cpp:66-67), 델타 상한 ±2²⁰(:11)/스텝 상한 32(:10).

## 2. as-built 규약 확정값 (소비자 계약)

- **deny 토큰 `"denied"`** — 3계층 키(`app_tool.<app>.<tool>` > `app_tool.<app>` > `app_tool`)
  명시 deny만 존중, 무키=allow(스펙 §3 무조건 allow와 운용 관례의 절충 — 룰링).
- **에코 = 사전 위치** — move 실패(bad_args/bad_grid)도 현재(사전) 위치 row/col 동실.
- **read 실패 = `ok:false` + 커서 필드 유지 + `"error"`** (`snapshot_failed`/전송 에러 키) —
  부분 성공 위장 금지 룰링. snapshot 미선언 앱은 `unknown_app_tool` 즉답(10s 타임아웃 봉쇄),
  빈 결과는 `"snapshot":"unknown"`.
- **act 에코** — 성공/실패 공통 `{kind,row,col,opened,status}`(+`error?`); 앱 실패도 ok:true+에러
  필드(filedlg 레슨 f 선례). 파킹 이벤트는 배너 name+request+target{app,tool,windowId,title} 전체
  방송(폰 브리지가 소비 — jkbridge/jkchat은 `e.name` 선호 표기).
- **커서 초기/리셋 (0,0) 좌상단** — 리셋 전이도 (0,0). act 후 커서 유지, **승인된 reset 성공 해소 시에만
  서버가 (0,0)으로 리셋**(아래 편차 ④). 커서 소멸 = 창 닫힘(매니페스트 erase 편승).
- **snapshot 직렬화(지뢰찾기)** — `lines` 9줄 순수 보드(`#`/`F`/`?`/`0`-`8`/패배·승리 후 미마크
  지뢰 `*`)+`status` 별도 JSON 필드(편차 ③). 패배 후 지뢰 공개는 렌더와 직렬화가 같은 진실원
  (마크는 지뢰 공개보다 우선 — 깃발 `F`/물음표 `?` 유지).
- **음수 델타 합법** — 스텝/상대 음수는 정상 이동(경계 클램프), 사전검증은 `|delta| > 1<<20`만
  bad_args(부호비교, abs(INT_MIN) UB 회피). 스텝 원소 축 생략 = 0(무이동, 관대 파싱).
- **선언 게이트** — origin 0..100000, cell 1..4096, rows≤1024, kinds≤32 토큰 검증, 커서 선언 4KiB 캡
  (`bad_cursor`; 2KiB는 act 스키마 enum 병합 캡 — §3.11), control-only 거부.

## 3. 스펙/브리프와의 편차 (전부 문서화된 결정)

1. **헤더 경로** — 브리프 `engine/src/server/JKSemanticCursor.h` → 실제
   `engine/include/server/`. `include`만 인클루드 경로라 `<server/...>`가 성립해야 하고
   헤더-인-src 선례 부재(TerminalHangulInput.h는 `engine/include/apps/`).
2. **read 실패 `ok:false`** — 허브의 `ok:true`+error 관례 이탈. "읽기 실패를 성공으로
   위장 금지" 룰링 적용(리뷰 승인).
3. **snapshot status-as-field** — 브리프 "첫/마지막 줄이 status를 가진다" → `status`를 별도
   JSON 필드로, `lines`는 정확 9줄 순수 보드. 줄 수 단정(9×9)이 프로브/폰 파싱에 더 안전.
4. **뷰 리셋 분해** — 에이전트 reset이 `MineSweeperGame::NewGame`(상태 머신)만 거쳐 뷰 리셋
   (타이머/게임오버 래치/라벨)을 우회하는 결함 → `Impl::NewGame`을 게임 리셋+`ResetViewState()`
   로 분해하고 `Impl::Act` reset 성공 경로에 배선. `game.Act`가 유일 전이 경로라는 설계 유지
   (네이티브/에이전트 동일 뷰 상태).
5. **reset 커서 리셋 = 서버 측** — 스펙 §4 "게임 리셋=정의 전이(좌상단)"의 앱 측 갭(앱→서버 커서
   힌트 통로 부재)을 `InflightAppTool.resetCursorOnOk`(헤더 :455)로 해소: 파킹 해소+즉시 allow
   양경로에서 설정, `HandleToolResult` ok 회송 시 `cursorState.Reset(rows,cols)`(:6224). 룰링:
   플랫폼 소유 커서의 리셋은 서버 측이 정답. 앱 에러(bad_state)엔 리셋 없음.
6. **난이도 재등록** — 정적 선언의 스테일 시나리오 중 난이도 축 해소: `SetCursorDeclChangedCb`
   +`RegisterAgentTools` 분리 → SetDifficulty가 `SendAgentToolRegister` 재전송(서버 upsert,
   upsert의 커서 (0,0) 리셋 = 새 격자 좌상단 = 정의 전이). 창 리사이즈 경로는 백로그.
7. **승인 시점 kind 재검증(semKind)** — 파킹 시점 검증만으론 "동일 기하 재선언이 어휘만 바꾼
   경우"(`["flag"]`→`["reveal"]`)의 파킹 act가 통과 → `CursorActStale`에 `semKind ∈ 최신
   act.kinds` 검사 추가(거부 토큰은 단일 bad_grid 유지).
8. **셀 링 클램프** — 파서는 origin/cell/rows를 각각 검증하지만 origin+rows×cellW/H가 표면 안에
   맞는지는 검증하지 않음(표면보다 큰 격자 선언=합법) → 부분 교차 셀의 링 픽셀이 창 밖으로 새는
   결함 → 양계층 모두 물리 셀 rect를 창 rect로 `SDL_IntersectRect` 클램프(계층2는 파킹 표시
   기간의 유일 방어선).
9. **`DrawRing3` 헬퍼** — 3중 링 루프 3벌 복붙을 공용 헬퍼로 추출(동작 불변 — 색 지정 순서·
   스트로크 결과 동일).
10. **커서 셀 색 = 파랑 (0,120,212) 고정** — JKTheme kDefault/kLight `selectionBg` 값, 테마 무관
    고정(승인 호박 (230,140,40)의 "식별색 고정, 테마 무관" 규약 승계 — 프로브 판정 안정).
    테마 실시간 연동은 백로그.
11. **앱 자체 act에 kinds enum 병합** — 스펙 §2는 "스키마도 앱 몫"이었으나 리뷰에서 own-act
    경로도 선언 enum 주입으로 확정(카탈로그 일관 — LLM이 합성/own 중 어느 act를 보든 같은
    enum). 2KiB 캡 초과/병합 불가 시 merge skip(등록 불파).
12. **음수 델타 룰링 반전** — 라운드 1 지시문("negative→bad_args")은 컨트롤러 오류로 판정:
    음수는 합법 이동, 과대 크기만 사전 거부(§2 규약 확정값 참조).

## 3b. 스펙 준수로 유지된 것 (기록용)

- 신규 와이어 메시지 0(jkagentd 무접촉), 폰/브리지는 generic 릴레이로 무수정 노출.
- kPermMatrix 행 불요(:2123 — 앱 도구 허브 3단 키와 동일 분류, window_move 선례).
- 파킹 파이프라인 형태 불변(거부=approval_resolved 방송+erase로 이중 실행 봉쇄).
- 배너명 인젝션 안전(ValidAppToolToken+JsonEsc), 승인 배너 밴드/창 링은 기존 정책 유지.

## 4. 검증 실측 (2026-09-22 최종 회귀 — 라이브 스택 정지 후 ×2 연속)

| 프로브 | 체크 | r1 | r2 |
|---|---|---|---|
| probe_semantic_cursor.ps1 (공식 — 선언/move/read/act 파킹 승인/bad_state/boom/reset/사전거부) | 59 | ALL PASS | ALL PASS |
| probe_app_tools.ps1 (앱 도구 허브 회귀 — 미선언 앱 불변) | 64 | ALL PASS | ALL PASS |
| probe_workshop.ps1 (워크숍 회귀) | 17 | ALL PASS | ALL PASS |
| probe_agent_events.ps1 (이벤트 카탈로그+focused 디바운스) | 8 | 8/8 PASS | 8/8 PASS |
| jkdesktop test (지뢰찾기 논리층 16건 포함 셀프테스트) | — | AppSelfTest: 0 failure(s) | (1회) |

- 누적 ad-hoc 라운드 실측(태스크 1-5): semc1 43체크, semc2 35, semc3 40(실물 픽셀 —
  SDL_RenderReadPixels 밴드 덤프), semc4 33(실물 지뢰찾기 e2e), semc4_geom(선언 기하 픽셀
  444 vs 대조 0), probe_app_tools_highlight 13, probe_window_geom 30, probe_jkbridge 33,
  probe_approval_overflow — 전부 ×2 ALL PASS 이력.
- 환경 주의(제품 결함 아님): probe_app_tools의 내부 highlight 중첩 프로브는 자기 서버를 가시
  기동하므로 라이브 jkwinserver 생존 시 단일 인스턴스 가드(docs/59 §11)로 c15만 FAIL —
  회귀 스윕 전 라이브 서버 정지가 전제.
- 라이브 반영: 재빌드 불요(HEAD 3cd68c3 = Task 5가 ninja 최신 확인한 바이너리) — jkwinserver+
  클라(서버가 taskbar 클라 자기 스폰)+jkbridge 재기동 —
  ping ok / 8899 LISTENING / HTTP 200 / 토큰 불변(폰 QR 재스캔 불요).

## 5. 백로그 (수집 — 구현 중 기각/이월 전체)

- **테마 연동 커서 색** — 고정 파랑(편차 10)의 근거(프로브 판정 안정+식별색 고정 규약)가
  현재 결정의 근거. 원하면 `jk::theme` selectionBg 실시간 참조로.
- **배너 밴드가 row-0 셀 링 상단을 덮음** — 기존 창 링 정책과 일관(미측정 사례로 기록).
- **approval_resolved error 필드** — bad_grid 거부(재검증 실패) 시 해소 이벤트가
  `decision:"allow"`만 실음(요청자 AgentReply엔 bad_grid 이미 실림). 승인자가
  "승인했지만 실행 거부"를 이벤트만으로 구분하려면 error 필드 필요.
- **난이도 변경 스테일 잔여** — 창 리사이즈 경로의 격자 기하 변화는 재선언 없음(난이도 버튼
  축만 해소, 편차 6). CursorActStale이 fail-closed 거부하므로 안전은 유지.
- **재등록 스킵 엣지** — 난이도 재등록이 CursorDeclJson 빈값(레이아웃 미완)으로 건너뛰면 구
  매니페스트 잔존 — 동기 레이아웃 재계산으로 희박(저위).
- **모달 복구 e2e 갭** — 게임오버 모달(gameOverShown) 래치 복구는 코드 검사만 유지
  (semc4가 폭발을 유발하지 않음 — e2e 결정론 불가, 논리층+직렬화 핀으로 대체).
- **semc1 잔여(공식 프로브엔 반영됨)** — Read-Reply qid 미매치/개행/probe-owned 서버 정지
  부재 등 애드혹 파일 내구성 잔여(비차단).
- **probe_conquest_minesweeper** — 정복 사다리 지뢰찾기 프로브를 향후 회귀 스윕 후보로.
- **같은 셀 2계층 겹침 미측정** — 파랑 위 호박 링. 실측은 같은 창의 다른 셀 병존까지(색 채널
  거리 논거는 설계 근거).
- **GetObjInt64 승격** — 커서 좌표 GetInt 축소 캐스트(±32768 가드 우회) — docs/57 §13.3-1 합류.
- **표시 페이드** — 스펙 §4 "표시만 페이드 가능"의 미래 항목(v1은 지속 표시).
- **cursorOwner app (테트리스류) 제2 소비자** — native-cursor 분기 미구현(v1은
  `cursor_owner_unsupported` 거부로 fail-closed). 스펙 §7 절제선: 소비자 2개 통과 후
  추상화 고정.
- **P4 SDK(.jkx manifest) 승격** — 위 절제선 통과 후.
- **스크립트 앱 declareCursor** — [소각 2026-09-24, docs/60 §13] 스펙 §6 재선언이
  워크숍 스크립트 바인딩 `declareCursor(decl)`로 실현(스펙 가칭 `agent.declareCursor()`
  대신 전역 함수 — 스크립트 호스트의 기존 바인딩 관용을 따랐다). "말로 만든 앱이 커서
  조작을 즉시 획득" — move/read 합성+act 중계는 서버 v1 그대로, 앱 쪽은 스크립트
  전역 `onAgentAct(kind,row,col)` 콜백 1함수.

## 6. 레슨 (구현 세션 실측)

1. **문자열 인지는 진입점에도** — 수기 브레이스 스캐너(InjectKindsEnum)가 matchBrace만
   string-aware면 진입 find가 오염: 키 검사가 따옴표 소비에 선행돼야 하고, 진입 로케이터도
   문자열 리터럴을 스킵해야 한다(라운드 2 MAJOR — "kind" 키 스캔 데드코드로 enum 병합 전무).
   raw 바이트에서 카운트/판정해야 하는 이유도 동일(파서는 last-wins로 중복 키를 조용히 흡수).
2. **"프로브 기대치 오탈"과 제품 결함의 변별** — 서버 로그 덤프(카탈로그 원문)로 병합 정상을
   확인하고 프로브 파서를 고쳤다(라운드 2 run1 3 FAIL 전부 프로브 측). FAIL 재현 불명은 받은
   프레임 출력 먼저(레슨 재확인).
3. **하니스가 거치지 않는 층은 하니스 검증 불가** — 논리층(모달 래치·패배 전 지뢰 위치)은
   e2e 결정론 구동 불가 → `NewGameWithMines` 결정론 헤너스+직렬화 핀으로 대체하고 한계를
   문서화(레슨 30/31 계열 재확인).
4. **단일 인스턴스 가드는 회귀 스위트 전제** — probe_app_tools 내부 highlight 중첩 프로브가
   자기 서버를 가시 기동하므로 라이브 서버 생존 시 c15만 FAIL한다(서버 정지 후 ×2 ALL PASS).
   애드혹 프로브(semc1류)가 teardown에서 자기 probe-owned 서버를 안 죽이면 뒤이은 정식
   기동이 가드 거부 즉사 — ping/8899가 잔존 서버에 응답한 오판 방지(잔존 프로세스 확인이
   복원 전제).
5. **qid 매칭** — 파이프 Read-Reply는 캡처 qid와 일치하는 프레임만 수령해야 한다(이전 질의의
   지연 응답이 다음 판독기를 오염 — 오탈방향 플레이크).
6. **수용판정은 결과가 아닌 입력으로** — act 사전검증(kind enum/격자)은 앱 도달 전 서버에서
   끝낸다(파킹해도 rect를 고정할 수 없는 요청은 파킹하지 않는다 — 릴레이 원문 패스스루의
   안전 조건).
7. **승인 재실행형은 승인 시점 재검증**(files_access 선례 재적용) — 파킹 시점 선언은 당시
   기준일 뿐. 기하 보존 재선언은 통과(선언 불변과 동치), 어긋나면 거부.
8. **식별색 고정 규약의 승계** — 승인 호박 선례대로 커서 파랑도 테마 무관 고정이 프로브
   판정 안정과 사용자 식별 모두에 이득(테마 연동은 백로그로 미룬 의사결정).

## 7. 사용자 눈확인 대기

폰 실전(라이브 스택 가동 중, 토큰 불변 — QR 재스캔 불요):

> **"지뢰찾기 켜 줘" → "지금 어디?" → "가운데 열어 줘" → "거기 깃발"**

확인 포인트: 파란 커서 셀 표시/이동, 승인 배너 `minesweeper.reveal at (r,c)` 문구+호박 셀 링,
read 스냅샷 기반 LLM의 칸 지칭, 깃발 act의 셀 하이라이트.

## 8. 폰 실전 1판 실측(2026-09-22 07:34-07:36) — 전 흐름 통과+픽스 2건

폰 시나리오 실측: 기동("지뢰찾기 띄워줘" → 창 스폰) → read(9줄 보드+커서 (0,0) 응답) →
reveal (0,0)(배너 `minesweeper.reveal at (0,0)` → 승인 → opened:19 에코) → flag (2,1)
(배너 표기+사용자 거부 → deny 응답) → reveal (1,2)(지뢰 → lost). 승인 배너 셀 표기,
파킹→승인→판 변화, 거부, 폭발+status lost 전부 계약대로 동작.

실측에서 나온 제품 결함/개선 2건(즉시 픽스 — 커밋 bb04beb/891430f):

1. **게임오버 모달 잔존** — 다이얼로그가 뜬 상태에서 폰 reset act → 판은 리셋되지만
   "Game Over" 모달이 화면에 남음(T4 뷰 리셋 픽스 5df153d가 래치/타이머만 커버).
   픽스: `ResetViewState`에서 열려 있는 박스를 프로그래매틱 해제(`SetOnResult(nullptr)`
   +`RequestClose()`+모달 복원 — 사용자 Ok 경로는 이미 RequestClose된 박스를 만나
   건너뛴다: 실행 중 std::function 파괴 방지).
2. **act 후 상태 재동기 습관** — 이전 턴 act 응답(opened:N 숫자뿐)만 믿고 다음 행동을
   read 없이 실행 → 맹목 개방. 대화 기억(`--resume`)과 별개 — 프리앰블에 "상태를 바꾸는
   도구를 썼다면 다음 행동 전에 상태 읽기" 1문장 추가(kLlmTurnPreamble).

관찰(픽스 불요): 거부 응답의 원시 토큰 `denied_by_user`가 LLM 답변에 그대로 노출
(JKWindowServer.cpp:5373 와이어 토큰 — LLM 문구 다듬기는 백로그). 폰 LLM 세션은
`--resume`으로 턴 간 대화 이력 유지(폰 localStorage 세션 id, `/new`로 리셋, 재접속 복구).

## 9. 그려지는 격자와 오버레이 박스 불일치(2026-09-22 픽스, 5fca652)

사용자 보고: "그려지는 그리드와 오버레이의 박스가 일치하지 않아요. 실제 그려지는 그리드가
클라이언트 영역을 벗어날 때가 많아요."

### 근원 — 3중 지오메트리의 실체

픽셀 분석으로는 3개 지오메트리 상태가 한 텍스처에 겹쳐 보여 오판 연쇄(격자 높이 340
오버플로 가설 등). 임시 계측(msgeom.log — declare/paint/winrect 라인)으로 클라 춤춤을
직접 관측해 확정:

- 등록 시점 선언 origin (16,75) — 격자 rect {12,70,296,298} (정확)
- 첫 레이아웃 패스 뒤 페인트 origin (16,57) — 격자 rect {10,10,296,334} (툴바 침범+클라
  영역 하단 초과)
- 난이도 B 클릭 후 pitch 24 재페인트 (ResizeMineWindow kCellSize=24 리사이즈 경로)

### 진범 — JKControl::PerformLayout의 blanket 자식 재귀

`JKControl::PerformLayout` 끝이 모든 자식에게 무조건 `child->PerformLayout(전체
클라이언트)`를 재귀했다. JKWindow::OnRectChanged는 도크 인식 3패스(edge-docked 먼저
remaining 축소 → DOCK_FILL은 잔여 영역 → DOCK_NONE)로 배치하는데, 그 직후 blanket
재귀가 DOCK_FILL 자식(지뢰찾기 격자)에게 **윈도우 전체 클라이언트 {0,0,316,354}를 다시
뿌려** 도크 배치를 덮어썼다 — 격자가 툴바 밑으로 미끄러지고 마진이 사라짐.
OnRectChanged 주석(:77-81)이 경고하던 바로 그 이중 적용 트랩.

### 픽스 (5fca652)

1. **LayoutChildren 가상화** — `JKControl::LayoutChildren()` 신설(기존 blanket 재귀
   이관), `JKWindow`는 **no-op 오버라이드**(자식 배치는 OnRectChanged 도크 패스 전담).
   PerformLayout 꼬리는 `SetRect(desired); LayoutChildren();`
2. **격자 기하 라이브 재선언** — MineGrid에 `SetOnGeometryChanged` 훅+`OnRectChanged`
   오버라이드(소유자는 dirty 플래그만 세움 — 레이아웃 도중 선언 발신 봉쇄),
   MineGameWindow::Impl::OnTimer에서 `CheckCursorDecl()`: `GetBoardGeometry` 실측을
   마지막 선언 캐시와 대조, 변화 시 `cursorDeclChangedCb()` 재발신. 서버 주도 리사이즈
   등 `SetDifficulty` 밖 경로도 100ms 안에 추종(MINOR-4 재선언의 일반화).
3. **클라 act 스키마 enum에 chord 누락 픽스** — ClientMineSweeperApp 하드코딩 enum에
   chord 추가(§6 픽스에서 서버/게임은 chord 지원했는데 클라 선언 스키마만 빠짐).

### 검증

- probe_semantic_cursor ×2 ALL PASS (59체크), semc4_adhoc ×2 ALL PASS (33체크)
- capture_window 실물: 격자가 툴바 아래 마진과 함께 배치, 클라 영역 내 수납 확인

### 레슨 추가

9. **공용 레이아웃 베이스의 "관례 재귀"는 오버라이드 지점부터 의심** — 베이스
   PerformLayout이 꼬리에서 무조건 자식 재배치하는 구조는, 배치를 다른 훅(OnRectChanged
   도크 패스)에 위임하는 파생 클래스와 조합하면 이중 적용이 된다. 자식 재귀를 가상
   메서드로 분리해 파생이 끌 수 있게 하는 게 정답.
10. **픽셀 포렌식 3연속 오판 후 계측 전환** — 한 텍스처에 여러 시점의 지오메트리가
    누적되면 픽셀 분석은 반증 불능. 렌더 산식 소유자(클라)에 임시 로거를 달아 춤춤을
    관측하는 편이 한 방.

