# 설정 허브 (settings hub) 설계 — 2026-09-18

## 0. 원천과 결정

- docs/superpowers/specs/2026-09-09-desktop-agent-platform-design.md §7 후보 앱
  "설정 허브 — 권한 매트릭스·장치 토글·receipt 보존기간 통합(단계 4)".
- **앱 선택 (사용자 확정 2026-09-18)**: 노트보다 설정 허브 먼저. 전제 — 설정
  허브의 핵심이던 권한 매트릭스 GUI는 agentmgr(docs/53)이 이미 구현했으므로
  본 앱은 **권한 본체를 중복 구현하지 않고 요약+라우팅**으로 분담한다.
- **범위 (사용자 선택)**: 테마/트리거/데이터(보존기간·레이아웃) + 권한
  요약/라우팅 + 장치 토글(전역 오디오, 캡처 스위치). 마이크/카메라 물리
  활성(N7)은 장치 접근 경로 자체가 없어 4단계로 미룬다 — UI에는 비활성
  "예정" 행으로 자리만 남긴다.
- **접근 방식 (사용자 확정 "A로 가지만 B를 바라봄")**: agentmgr의 검증된
  도구-소비 패턴 + 신규 도구 최소. 신규 도구의 봉투는 B(서버 소유 설정
  스키마 레이어)로 진화 가능한 모양으로 짠다 — B 확장 시 key/kind/scope
  필드를 넓히고 `settings_read` 수집기가 스키마를 소유하면 된다.

## 1. 문제 정의

설정이 흩어져 있다: 테마는 `theme_set`(팔레트에서만), 트리거는
`trigger_toggle`(터미널), idle 임계는 `state/idle_minutes` 파일 수기 편집,
receipt는 보존기간 자체가 없어 무한 증가, 레이아웃은
`save_layout`/`restore_layout`(에이전트 전용), 오디오 음소거는 앱별 수동.
"설정을 한 화면에서 보고 바꾸는" 사용자 얼굴이 없다.

## 2. 아키텍처

### 2.1 앱·표면

- `jkapp_settings` — ImGui 클라 1창(단일 스크롤 5섹션), jkapp_agentmgr의
  패턴(서버 도구 agentctl 소비, 런처 셀) 그대로. 진입: 런처 셀 + 팔레트
  `/settings` + jkchat 슬래시. 900x620.
- 섹션: **테마 | 트리거 | 데이터 | 권한 | 장치** — 탭 대신 단일 스크롤(설정은
  훑어보는 화면; agentmgr 4탭은 기능별 심층이므로 탭, 설정은 넓고 얕음).

### 2.2 신규 서버 도구 2종 (B의 씨앗)

- **`settings_read {}`** →
  `{"ok":true,"settings":[{"key","kind","value",...}]}` — 서버가 현재 상태를
  수집: `theme.current`(프리셋), `triggers` 배열(이름+on), `idle_minutes`,
  `receipt_retention_days`, `audio_master_mute/volume`(서버 상태),
  `layouts` 배열(파일명+창수). kind는 "string"|"int"|"bool"|"list".
  **이 봉투가 B 진화의 계약** — B 단계에서 scope/choices/range 필드가
  추가되고 GUI가 범용 렌더러가 된다.
- **`settings_set {key, value}`** → 화이트리스트만 수용:
  | key | value | 적용 |
  |---|---|---|
  | `idle_minutes` | int ≥ 0 | `state/idle_minutes` 파일 (trig_idle이 읽는 것) |
  | `receipt_retention_days` | int ≥ 1 (0=무제한 유지는 금지 — 무제한이 현재값) | 적용 시 receipts.jsonl 즉시 정리(아래 2.4) + 서버 KV 저장 |
  | `audio_master_mute` | bool | 서버 KV + `audio.master` 이벤트 방송 |
  | `audio_master_volume` | int 0..100 | 서버 KV + `audio.master` 이벤트 방송 |
  | `capture_allow` | bool | `capture_window`/`capture_region`의 permissions.json 오버라이드(allow/ask) — `permission_set` 로직 재사용 |
  미지원 키 = `bad_key`, 타입 위반 = `bad_value`. reply에 적용 후 값 실어
  반환(`{"ok":true,"applied":{key,value}}` — 클라 단일 신뢰원, fullscreen
  응답 규약과 동일).
- kPermMatrix: 둘 다 `("settings_read","none","allow")`,
  `("settings_set","none","allow")` — theme_set/trigger_toggle과 동일 분류
  (외관·환경 설정, 승인 행위 아님). 단 `capture_allow` 내부에서 쓰는
  permission 로직은 permission_set의 ask-고정 계약을 그대로 받는다? — 아니다,
  permission_set 도구 자체가 Ask 고정이므로 settings_set의 capture_allow는
  **permission_set을 도구로 재호출하지 않고** 내부에서 동일 쓰기 함수를
  재사용하되 **결과는 Ask 없이 반영한다**(사용자가 GUI에서 직접 누른
  스위치 = 자기 설정 변경; 에이전트가 설정을 바꿔 권한을 우회하는 경로는
  settings_set도 Ask로 봉쇄 — **capture_allow 키만 Ask, 나머지 allow**).
  - 이중 권한 우회 봉쇄 레슨(docs/53 permission_set 하드코딩 Ask)의 대칭
    적용: GUI가 아니라 에이전트 경로로 capture_allow를 쓰는 순간도 Ask —
    도구별이 아니라 **키별** 게이트.
- 서버 KV: `state/settings.json` 신규 `{"audio_master_mute":bool,
  "audio_master_volume":int,"receipt_retention_days":int}` — 부팅 시 로드,
  재시작 복원. 256KiB 캡 레슨 준수(작은 KV, 캡은 방어선으로만).

### 2.3 audio.master — 코어 레벨 이벤트 수용 (모든 앱 일괄)

- 서버 `settings_set(audio_master_*)` → `PushAgentEventJson`으로
  `{"topic":"audio.master","data":{"mute":bool,"volume":0..100}}` 방송.
- **`JKClientApplication` 코어가 구독+드레인+적용**:
  `surface_->SendAgentEventSubscribe(true)` 1회 + Run 루프에서
  `DrainAgentEvents` → `"topic":"audio.master"` 니들 매치 →
  `JKSoundManager::GetInstance().SetMasterVolume(mute ? 0.f : volume/100.f)`.
  앱별 구현 없이 모든 ImGui 클라가 수용.
- **단일 펌프 원칙(설계 정정)**: 코어가 이벤트 큐의 유일 소비자가 되고 가상
  `OnAgentEvent(const std::string&)` 훅으로 앱에 전달한다. vplayer의 자체
  `PumpAgentEvents`/`PumpAgentReplies`는 이 펌프로 이관(풀스크린 미러 로직은
  그대로, 위치만 이동) — 이중 드레인 경쟁 방지. **vpt12가 이 리팩의 회귀
  게이트**. 구독은 기존 자체 구독 앱(notify, palette 등)과 멱등 공존.
- `settings_read`가 부팅 시 서버 KV를 읽어 초기값 제공 — 이벤트 미수신
  앱(구버전)은 다음 이벤트까지 이전 볼륨 유지(수용).

### 2.4 GUI 섹션별 데이터 경로

| 섹션 | 표시 | 조작 |
|---|---|---|
| 테마 | 현재 프리셋 + 3버튼(dark/light/classic) | `theme_set` — 응답 preset을 즉시 표시 |
| 트리거 | `trigger_list` 행(이름+토픽+on) + 체크박스, idle 임계 인트 | `trigger_toggle`, `settings_set(idle_minutes)` |
| 데이터 | receipt 건수+최근 ts, 보존기간 콤보(7/30/90/365), 레이아웃 목록+저장/복원/삭제? | `settings_read`(receipts 통계는 read_receipts 재사용), `settings_set(receipt_retention_days)`, `save_layout`/`restore_layout` |
| 권한 | permissions.json 오버라이드 행 요약(tool/kind/allow) | "agentmgr 열기" 버튼 — `launch_app{app:"agentmgr"}` |
| 장치 | 음소거 체크 + 볼륨 슬라이더(커밋-on-release), 캡처 스위치, N7 예정 행 | `settings_set(audio_master_*)`, `settings_set(capture_allow)` |

- 레이아웃 삭제는 1차 제외(YAGNI — 파일 수동 삭제 가능).
- GUI 트리거는 settings_read의 `triggers` 배열을 원천(트리거 토글 후
  settings_read 재수집 — 폴링 없음, 설정 화면은 정적 데이터).

### 2.5 receipt 보존기간

- 현재값(무기한)과 임계의 차이: `settings_set(receipt_retention_days:N)`은
  (a) 서버 KV 저장 (b) **즉시 정리** — receipts.jsonl 전체 리라이트(ts epoch
  초 기준, now - N*86400 이전 행 삭제). 파일은 작음(256KiB 캡) — 전체
  리라이트 허용. .bak 1세대 보존(bookmarks/trust 관례).
- 정리 대상은 receipts뿐 — trust/permissions는 불변(수술 위험 레슨).

## 3. 비목표 (YAGNI)

- 권한 매트릭스 본체 GUI — agentmgr 탭 1의 영역(요약+라우팅만).
- 장치 물리 활성(N7 표시기, audio/camera agent) — 4단계.
- settings_write 범용 스키마 계층(B) — 1차는 화이트리스트 KV, 봉투만 B형.
- 트리거 생성/수정 UI — docs/34의 토글만.
- 레이아웃 삭제, 데스크탑 배경화면 설정.
- 창 모드별 창 크기 기억 등 개별 앱 설정 — 각 앱이 이미 소유.

## 4. 리스크와 방어

- **이중 드레인 경쟁** (§2.3 정정): 코어 단일 펌프 + vplayer 이관 — vpt12
  회귀로 검증. 기존 자체 구독 앱의 이벤트는 코어 펌프가 **무시하고 전달만**
  하므로(훅 미구현 앱은 버림) notify/palette 영향 0.
- **settings_set의 에이전트 우회**: capture_allow 키만 Ask(§2.2) — 에이전트가
  설정 도구로 권한을 넓히는 경로 봉쇄. mute/volume/idle/retention은 화면·데이터
  위험도가 낮아 allow(theme_set 분류).
- **receipts 정리 중 손상**: 리라이트 실패 시 .bak 복원 후 ok:false —
  trust_write의 실패-복원 계약 준수.
- **audio.master 미수신 앱**: 스크립트 앱 등 구버전 — 다음 이벤트까지 무음
  미적용. 부팅 KV 복원으로 서버 재시작 시 일관.
- **프로브**: audio.master 이벤트는 agent-events CLI로 캡처(vpt12 이벤트
  캡처 패턴). 볼륨 적용 실측은 vplayer 사운드가 테스트 미디어에 없으므로
  이벤트+서버 KV 실측으로 한정(앱 적용은 코드 리뷰 항목).

## 5. 검증 계획

- **probe_settings.ps1 (신규)**: 스폰 → settings_read 셰이프(키 존재) →
  theme_set 스왑 → settings_read 반영 → trigger_toggle → settings_read
  반영 → settings_set(idle_minutes=0) → 파일 실측 →
  settings_set(receipt_retention_days=7) → receipts.jsonl 정리 실측(오래된
  행 감소, .bak 생성) → settings_set(audio_master_mute) → audio.master
  이벤트 캡처 + 서버 KV → settings_set(capture_allow) by agentctl →
  ask 승인 요청 발생 실측 → restore.
- **회귀**: vpt12(이벤트 펌프 리팩 게이트), selftest, jkagentd selftest(도구
  2종 등록), probe_agentmgr(도구 경로 공유), vpt11, probe_agent_palette
  (/settings 추가).
- **게이트**: mtime(레슨 37) + 클린바이너리 공식런(레슨 61).

## 6. 최종리뷰

- opus 최강 모델(상임).