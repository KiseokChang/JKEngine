# 앱 정복 사다리 (App-Conquest Ladder) 설계

- 날짜: 2026-09-21
- 원안: docs/superpowers/specs/2026-09-09-desktop-agent-platform-design.md §6.6 (앱 정복 사다리, scenarios Part 8)
- 토대: docs/58_app_tool_hub.md (앱 도구 허브, 2026-09-20 완결) — MCP 도구 등록·릴레이·게이트
- 상태: M1 구현 완료(2026-09-21), minesweeper 코드 정복 — 프로브 공식런+회귀 스윕 배치 대기, LLM 실전 체크리스트는 docs/62 §6 (as-built: docs/62)

## 1. 왜 이것인가

docs/29 스펙이 정의한 앱 정복 사다리(minesweeper→…→taskbar)는 그동안 개별 조각만
완성됐다: 앱 도구 허브(docs/58)로 vplayer 6도구·filedlg 3도구가 MCP로 노출됐고,
launch/observe/recover에 해당하는 도구와 이벤트는 이미 있다. 남은 것은 이 조각을
**앱 단위 완성 계약**으로 묶는 것이다.

2026-09-21 사용자 합의 3건:

1. 정복은 **MCP 적용과 묶어서** 간다 — drive는 MCP 앱 도구가 1순위 수단.
2. MCP 정의가 없는 앱은 **입력 주입(send_input)으로도 검증**해야 하고, 이 트랙이
   나중에 자동화의 기반이 된다.
3. **코드로 짜둔 검증 도구(프로브)는 자동화의 초기 코드가 된다** — 프로브의
   drive 단계를 OS 레벨이 아니라 에이전트 도구 표면으로 작성하면, 그 시퀀스가
   트리거 스크립트(jktriggers)로 그대로 이식된다.

## 2. 정복 계약 (앱 1개 정복의 완성 조건)

| 단계 | 의미 | 수단 |
|---|---|---|
| launch | 앱 실행 + 등장 확인 | launch_app → window.created 이벤트 |
| observe | 상태 읽기 | MCP 앱 도구(등록 시) / capture_window + 에이전트 이벤트 |
| drive | 기능 조작 | **트랙 A: MCP 앱 도구(app_tool 릴레이) 우선** / **트랙 B: send_input 폴백** |
| verify | 결과 검증 | 도구 응답 단언이 최선 / 없으면 픽셀 판정(capture_window)·이벤트 |
| recover | 장애 복구 | app.crashed 이벤트 → 재스폰 → 사이클 재실행 성공 |

판정 주체 = **둘 다**(사용자 확정):

- **(a) probe_conquest_<app>.ps1** — 5단계 전 사이클 자동 검증, **2연통 원칙**.
  회귀 게이트 역할을 겸한다.
- **(b) LLM 실전 세션** — 폰/채팅에서 LLM이 그 앱 기능을 말로(MCP 도구) 조작하는
  표준 절차 1회 통과. 사용자 눈확인이 최종 판정.

앱별 남는 산출물: ① 정복 프로브 ② (MCP 승격 가능한 앱이면) AgentToolRegister
등록 코드 ③ 이식 가능한 drive 시퀀스(자동화 씨앗).

## 3. 신규 공용 조각 (앱별 코드 0줄 목표)

### 3.1 send_input 서버 도구 (신규)

- 역할: 트랙 B의 유일한 조작 수단. 에이전트 도구로서 결정론적 호출형.
- 인자 스케치: `{target: <window id>, op: "key"|"click"|"type"|"wheel", ...}`
  - key: {vk, scan?, mods?}, click: {x, y, button?, count?}, type: {text},
    wheel: {dy, x?, y?}
- 좌표: **논리 데스크톱 좌표** — 서버가 대상 레이어의 표면 좌표로 변환해 기존
  InputEventPayload 경로로 주입(와이어 신규 0, 서버 도구 1종). docs/27 단계2 UI
  자동화(injectKey/injectMouse, RespondMessage 라우팅)에서 검증된 수단의 도구화.
- 게이트: 기존 승인 파이프라인에 `send_input` 키 추가. **기본 ask**(사용자 검토 시
  확정 — 자동화 편의를 위해 런타임 permissions.json에서 allow로 조정 가능).
- 캡처 오버레이·서버 자기 창은 대상에서 제외(크롬 존 좌표 주입 금지 — 크롬은
  close_window 도구가 담당).

### 3.2 정복 프로브 템플릿

- 공통 헬퍼(launch 대기/window.created 수신/drive 호출/단언/recover 재실행) +
  앱별 시나리오만 추가하는 골격. engine/tools/probes/ 관례(PS5.1, `> log 2>&1`
  파일 리다이렉트, BOM)를 따른다.
- **원칙 — 프로브의 drive/verify는 도구 표면으로 작성한다. OS SendInput 직접
  호출 금지.** 기존 프로브가 SendInput을 쓴 이유(좌표 주입 수단 부재)가
  send_input 도구로 해소되므로, 신규 정복 프로브는 도구 호출만으로 5단계를
  돌린다. 이것이 자동화 이식성의 전제다.

### 3.3 LLM 실전 체크리스트

- 폰/채팅에서 앱 기능을 말로 조작하는 표준 절차: 앱 실행 → 기능 1~2개 발화 →
  결과 확인. 앱마다 § 스크립트 한 줄로 축적한다.

## 4. 원칙: 프로브 = 자동화의 초기 코드 (사용자 합의 3)

- 정복 프로브의 drive 시퀀스 = 도구 호출 시퀀스 → **jktriggers JS 스크립트로
  1:1 이식 가능**(같은 파이프 프로토콜을 걷는 소비처가 이미 있다).
- 후속 자동화 형태: 시나리오 재실행("매일 밤 이 시나리오 돌려 줘"), 트리거 연동.
- 스펙 범위 밖: 스케줄러, 시나리오 저장소 — §7 백로그.

## 5. 사다리 순서

minesweeper → tetris → scriptdemo → taskmgr → terminal → vplayer → browser →
taskbar.

- **첫 목표 = minesweeper**: 템플릿 확립(트랙 B 전용 — MCP 승격 없이 send_input
  만으로 5단계 완주).
- **vplayer**: MCP 도구 6개가 이미 있어 트랙 A 시제 케이스(사실상 최선석) —
  정복 프로브만 신설해 MCP 트랙 공식을 재확인.
- **taskbar**: 최종 관문(셸 자체 정복).
- 순서는 재조정 가능하나, minesweeper로 템플릿을 먼저 굳히는 것은 고정.

## 6. 기록과 검증

- as-built 문서: `docs/62_conquest_ladder.md` — 각 정복 완료마다 § 추가.
- 검증: 프로브 2연통 + LLM 실전 세션(사용자 눈확인) + 기존 회귀 무손상
  (probe_app_tools 등 도구 허브 회귀).

## 7. 범위 밖 (백로그)

- 앱별 MCP 승격 도구 설계는 각 정복 시점에 결정(계약에 "선택"으로만 명시).
- 자동화 스케줄러/시나리오 저장소 — §4의 후속.
- send_input의 스크립트 실행(매크로/시퀀스 일괄) — 필요 시 단일 호출 반복으로
  충분한지 먼저 확인.
- 폰 미러 등 워크숍 백로그(docs/60 §5)와 무관하게 병행.

## 8. 결정 기록

| 질문 | 결정 |
|---|---|
| 판정 주체 | 프로브 + LLM 실전 **둘 다** (사용자 확정) |
| drive 수단 | MCP 우선, send_input 폴백 이중 트랙 (사용자 확정) |
| 프로브 작성 원칙 | 도구 표면만 — OS SendInput 금지 (사용자 합의 3에서 유도) |
| send_input 기본 권한 | ask (2026-09-21 사용자 승인) |
| 첫 목표 | minesweeper (템플릿 확립) |