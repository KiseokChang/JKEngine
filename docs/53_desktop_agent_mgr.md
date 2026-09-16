# docs/53 — 데스크탑 에이전트 관리자 (agentmgr) as-built

- 날짜: 2026-09-17
- 스펙: `docs/superpowers/specs/2026-09-16-agent-manager-design.md`
- 플랜: `docs/superpowers/plans/2026-09-16-agent-manager.md`
- 상위: docs/32 §7 (후보 앱 — 에이전트 관리자), docs/51 (P4 SDK 계약)

## 1. 요약

에이전트 권한/트리거/신뢰/설치 앱을 하나의 4탭 ImGui 앱으로 봉합:

- **서버 도구 5종 신규**: `agent_permissions`, `permission_set`, `trust_revoke`,
  `installed_list`, `read_receipts` (커밋 de7d2b9/72c5355/7cd4393)
- **브로커 갱신**: jkagentd kToolsListJson + IsKnownTool + LoadPermissions에 5종 추가,
  write 2종(permission_set/trust_revoke)은 브로커 기본 deny — 권한 파일 편집 자체가
  승인 행위이므로 (커밋 d542415)
- **jkchat 승인 프롬프트**: `[권한 변경] tool → decision`, `[신뢰 해지] name (fp…)`
  2종 추가 + 팔레트 `/agentmgr` (커밋 bcee1e6)
- **jkapp_agentmgr**: 4탭 GUI + agentmgr.jkx + 런처 아이콘 (커밋 9528113)

## 2. 게이트 뱃지 모델 — 이 문서의 핵심 "왜"

스펙 §2.1 조사 결론: 서버가 실제로 게이트하는 에이전트 도구는 3종뿐이다 —
`close_window`(server/deny), `trust_request`(server/ask), `run_console_app`(server/ask).
브로커 도구의 "게이트"는 jkagentd의 bool 맵(perms)일 뿐 서버 권한 매트릭스가 아니다.
기존 `capture_window` 주석("서버 게이트")은 부정확해서 정정했다 — 브로커 bool 맵이 필터.

이 조사가 권한 탭 설계를 결정한다:

- **게이트 3분류**: `server`(kPermMatrix 행), `server(fixed)`(permission_set —
  AgentToolAllowed 첫 줄에서 무조건 Ask), `broker`(브로커 deny 맵).
- **effective = fixed → server 파일값-or-default → broker-none이면 allow**.
- 매트릭스는 파일 스코프 `kPermMatrix[]` 26행 — 신규 도구 추가 시 한 곳만 고치면 된다.

## 3. permission_set 안전 설계 (스펙 §2.2)

- **하드코딩 Ask**: `AgentToolAllowed` 첫 줄에서 `permission_set`은 무조건 Ask.
  이유: permission_set이 permission_set을 allow로 바꾸면 2단 우회가 된다. 파일에
  `"permission_set":"allow"`를 미리 심어도 Ask를 강제한다 (fixed 게이트).
- **알려진 키 전재기록**: WritePermissionsEntry가 RMW로 모든 알려진 도구 키를
  재기록한다. 부분 파일(`{"close_window":"allow"}`만 있음)을 써도 다음 RMW에서
  전체 매트릭스로 복원되는 자기 치유 효과. 서버(fixed) 행은 파일에 쓰지 않는다.
- 검증 순서: missing_tool → unknown_tool → bad_decision, 전부 즉답 에러.
  파킹은 검증 통과 후 — 승인 이벤트 payload에 `target_tool`+`decision`.
- 승인 파이프라인: approve 루프가 kind 기반 분기(permission_set →
  WritePermissionsEntry), 성공 플래그는 `result.find("\"ok\":true")`.

## 4. trust_revoke (스펙 §2.3)

- **DoS 통로 논의**: revoke는 신뢰 스크립트를 즉시 못 쓰게 하므로 악용 시 피해가
  크다 → askCapable에 포함(기본 Ask, trust.json 파일 행으로 allow/deny 가능).
- **raw text surgery 이유**: AgentJson 재직렬화는 ts(int64)를 잃는다(파서 계약 —
  int64 접근자 부재, docs/38). 그래서 TrustRecordText(brace 스캔)로 레코드 원문을
  찾고 RevokeTrustRecord가 콤마 흡수하며 제거한다.
- **.bak 신규 적용**: 삭제 전 trust.json → trust.json.bak (1세대만 보존).
  승인 전 park → 승인 후 즉시 revoke → `{"ok":true,"written":true,"restart_needed":true}`.
- `trust_list` 행에 `,"fp":"<전체 지문>"` 추가 — UI 해지 버튼이 잘린 표시용이
  아니라 전체 지문으로 동작하게.

## 5. 파서 계약 델타 (docs/38 2레벨 리더)

구현이 스펙 대비 강제한 계약 — 이후 도구 추가 시 재활용:

- **read_receipts**: `ok`는 bool 접근자가 없어 `"0"/"1"` 문자열로 보낸다.
  `ts`는 epoch **초** (원본 ms를 1000으로 나눔). limit 1..200(기본 50), 역순,
  256KB 테일.
- **agent_permissions**: `AgentJson`은 복사/대입 불가 → pbuf에 읽고 1회 생성
  (`jk::agent::AgentJson perm(fileExists ? pbuf : "{}")`).
- **트리거 topics 열 포기**: 2레벨 리더가 topics 배열을 못 읽어 트리거 탭은
  name/enabled만 표시한다.
- **설치 목록 kind**: launcherIcons_ 기준 `console | jkx | builtin` 3종.

## 6. as-built 파일 + 커밋

| 커밋 | 내용 |
|---|---|
| de7d2b9 | 읽기 3종 agent_permissions/installed_list/read_receipts |
| 72c5355 | permission_set fixed-ask + 승인 파이프라인 write |
| 7cd4393 | trust_revoke 승인 게이트 + trust.json RMW(.bak) + trust_list 전체 fp |
| d542415 | jkagentd 브로커 5종 (기본 deny, args rebuild, selftest) |
| bcee1e6 | jkchat 승인 프롬프트 2종 + 팔레트 /agentmgr |
| 9528113 | jkapp_agentmgr 4탭 GUI + jkx + 아이콘 |
| a120400 | (Part 1) jkctl zip-slip 백슬래시 픽스 — 패키지 경로 |

파일: `src/server/JKWindowServer.cpp` (kPermMatrix/5 브랜치/ approve 루프 kind 분기),
`include/server/JKWindowServer.h` (PendingApproval +permTool/permDecision),
`tools/jkagentd/main.cpp`, `tools/jkchat/main.cpp`, `src/apps/ClientPaletteApp.cpp`,
`include/apps/ClientAgentMgrApp.h`, `src/apps/ClientAgentMgrApp.cpp`,
`src/apps/JKAppModule_agentmgr.cpp`, `CMakeLists.txt` (타깃 + JKX_ICON_APPS +
jkx_packages DEPENDS), `assets/icons/launcher_agentmgr@{1x,2x}.png`.

## 7. 프로브 + 회귀

- **probe_agentmgr.ps1** (신규, 공식 회귀): 16체크 — spawn / shape /
  permission_set E2E(3a응답·3b파일·3c매트릭스) / trust_revoke E2E(4a응답·4b삭제·4c
  .bak) / not_found·bad_fingerprint / installed_list / read_receipts(빈·역순·ok
  플래그·cap) / trigger 토글 회귀. **ALL PASS**. 종료 시 permissions.json 삭제 +
  trust/receipts 복원.
  - 환경 전제: `build/apps/triggers/*.jkx` 설치 + trust.json에 레코드 1건 이상
    존재(빈 스토어면 시딩 splice가 `[,{...}]` 불법 JSON을 만들고 jktriggers
    fail-closed 재기록으로 fake가 유실돼 4a FAIL). 스펙 §6의 mtime 게이트는
    미적용 — 로컬 개발 프로브 전제.
  - 리뷰 픽스 반영: 시딩 result를 객체형으로(`"ok":true` 서브스트링 스캔이
    문자열 이스케이프를 못 찾는 문제) + first/second ok:"1"/"0" 단언 + 실행 전
    stale trust.json.bak 삭제(4c 스테일 패스 방지).
  - 플랜 대비 델타: (1) `Set-Content -Encoding UTF8` → `[IO.File]::WriteAllText`
    (PS5.1 BOM — Part 1 레슨), (2) trigger_list 행은 `{"name","topics","enabled"}`
    순서라 정규식이 topics를 건너뜀(플랜 노트 (c) 예중). Receive-Job은 Object[]라
    `-join "\`n"` 후 판정.
- **spawn 스모크**: mgr_t6_spawn.ps1 — launch → list_windows 'Agent Manager' PASS.
- **회귀**: probe_agent_mcp PASS(4체크), probe_agent_trust PASS(4체크, pack-records 4),
  probe_agent_triggerctl PASS(3체크), probe_agent_chat PASS, jkagentd --selftest
  0 failures (5종 selftest id:4 포함), jkdesktop test **0 failures**.

## 8. 레슨 (이번 파트에서 새로 발견)

- **Receive-Job은 Object[]**: 파이프라인 결과를 `-match`에 바로 넘기면
  bool이 아닌 배열이 바인딩돼 Check가 미판정된다(조용한 false pass). `-join`으로
  문자열화 후 판정. 다른 프로브는 `$x = (& exe ...) -join` 패턴이라 걸리지 않았다.
- **jkx_packages DEPENDS 누락**: add_custom_command(OUTPUT …jkx)만 추가하고
  jkx_packages 커스텀 타깃 DEPENDS에 넣지 않으면 ALL 빌드가 리팩을 건너뛴다 —
  신규 .jkx는 두 곳 다 추가.
- **EscapeJson 배치**: cpp 무명 네임스페이스 자유함수를 헤더에서 static 멤버로
  선언하면 링크 undefined. 멤버로 노출할 필요가 없으면 헤더 선언을 빼고 자유함수로.
- **jkdesktop 테스트 인자는 `test`** (dash 없음): `--test`를 주면 dispatch
  (`strcmp(argv[1],"test")`)에 걸리지 않아 그냥 데스크탑 셸이 떠서 무한 렌더
  스핀 — hang처럼 보인다(gdb 스택으로 판명: RunMain→JKApplication::Run).
- (재확인) **아이콘 GDI+ 스크립트**: PS5.1에서 backtick 연속행 + inline
  -ArgumentList는 바인딩 실패 — 변수 사전계산 + 직접 생성자 호출(레슨 21).
## 9. 최종리뷰 (opus, 2026-09-17)

**VERDICT: APPROVE** — MAJOR 없음. 핵심 검증: trust 수술 콤마 엣지 4케이스,
RMW .bak 선기록 순서, fixed-ask 2단 우회 봉쇄, 브로커 triple-sync, 파서 계약 준수.
MINOR 픽스 반영 커밋: permission_set→permission_set bad_target 즉답(거짓
written:true 제거), 프로브 시딩 객체형+ok 단언+stale .bak 제거, 스펙 §7
self-approve 전제 명시 + §8 델타 2차(MergeRunning substring/effective 열 생략).
NON-BLOCKING 잔여는 후속 세션에서 전부 소각 (2026-09-17, leftovers 플랜
docs/superpowers/plans/2026-09-17-agentmgr-sdk-leftovers.md):
- ImGui Begin false → End 스킵: `const bool open = ImGui::Begin(...); ImGui::End();
  if (!open) return;` (f91a677). NoDecoration/NoMove로 close 불가라 도달 불가지만
  계약 준수로 봉합.
- WritePermissionsEntry 4KB 상한: fseek/ftell 전체 읽기 + 256KiB 캡, AgentJson
  파싱. 캡 초과는 파싱 실패 → bad_target 자기치유. trust_revoke의 기록/분기
  존재 2개 사이트도 8MiB 캡 전체 읽기(초과는 not_found 정직 반환) (c930532).
  프로브로 고정: probe_agentmgr 3d(4KB+ 시드 JSON, close_window:allow 판별키)
  ·5c-5e(999 레코드 + 64KB 초과 tail, approve E2E) → 21/21.
- approve self-approve 룰링 (spec §7): 파킹을 requesterId 자기 연결에서
  approve하면 승인 없는 허가 — permission_set/trust_revoke만 봉쇄
  (`{"ok":false,"error":"self_approve"}`). close_window는 예외: ask 모드에서
  채팅 자신의 /close를 자기 승인 스트립으로 해소하는 건 docs/31 §3의 설계 UX.
  probe_approve_self.ps1 5체크(1-parked/2-자기거부/3-cross-approve/4-파킹
  답신/5-파일) (a75f05e). jkctl은 승인 연결이 없어 2회 잔여 없음 — 룰링은
  서버 게이트로 해소됨.
- jkchat 단일 승인 스트립(선존 M2): ApprovalUi 큐(f31e424) — 요청 도착 시
  front 표시, 나머지 큐잉(`[대기] 승인 요청 #N` 로그), resolved 시 front
  회전 또는 큐에서 제거. docs/31 self-close 흐름 보존.

픽스 후 회귀 전부 PASS: jkdesktop test 0, jkagentd --selftest 0,
probe_approve_self 5/5, probe_agentmgr 21/21, probe_agent_chat PASS,
probe_agent_trust PASS, probe_jkctl_init 23/23 ALL PASS.
