# docs/54 — 설정 허브 (settings hub) as-built

- 날짜: 2026-09-18
- 스펙: `docs/superpowers/specs/2026-09-18-settings-hub-design.md`
- 플랜: `docs/superpowers/plans/2026-09-18-settings-hub.md`
- 상위: docs/32 (에이전트 트리거/이벤트, 후보 앱), docs/53 (agentmgr — 권한 탭
  중복 해소: 설정 허브는 권한 표를 **표시+열기**만 하고 편집은 agentmgr에 위임)

## 1. 요약

노트(마진 코멘트/백로그)와 설정 허브 중 사용자가 설정 허브를 1순위로 확정
(docs/32 후보 앱 논의). 접근은 "A로 가지만 B를 바라봄" — 기존 도구 소비 +
최소 신규 도구, 봉투는 서버 소유 스키마 진화(B안) 여지를 남김:

- **서버 도구 2종 신규**: `settings_read`, `settings_set` (커밋 d0c5800)
- **state/settings.json KV**: `{"audio":{"mute":0/1,"volume":N},
  "retention":{"days":N}}` — 부팅 로드(LoadSettingsKv) + settings_set 쓰기,
  런타임 미러 멤버(audioMasterMute_/audioMasterVolume_/receiptRetentionDays_)
- **코어 에이전트 이벤트 펌프**: JKClientApplication이 유일 소비자 —
  Subscribe(true)는 Init에서, 드레인은 Run()의 DrainInputChannel 직후,
  `virtual void OnAgentEvent(const std::string&)` 훅으로 앱에 분배 (커밋 39ab853).
  audio.master는 코어가 직접 JKSoundManager::SetMasterVolume에 적용.
- **vplayer 펌프 이관**: 자체 Subscribe+PumpAgentEvents를 OnAgentEvent override로
  교체 — 단일 드레인 원칙, vpt12 = 회귀 게이트 (커밋 197f2f4)
- **jkapp_settings**: 5-섹션 단일 스크롤 GUI + settings.jkx + 팔레트 `/settings`
  (커밋 6e34d1a)
- **probe_settings 15체크** 2연속 ALL PASS (커밋 8e83e76)

## 2. settings_read / settings_set (스펙 §2.2)

- kPermMatrix +2행: `{"settings_read","none","allow"},{"settings_set","none","allow"}`
  — 읽기/설정은 서버 게이트 없이 열고, 위험한 **키 단위**만 Ask로 파킹(§3).
- settings_read 봉투: `{"ok":true,"settings":[{key,kind,value}...],
  "receipts":{"rows":N,"last_ts":T}}`. 소스: theme.current(기본 테마 경로의
  preset), trigger.*(state/triggers.json), idle_minutes(state/idle_minutes
  파일), audio_master_*/receipt_retention_days(KV 멤버), layout_*.json 열거
  (FindFirstFileA), receipts 통계(256KiB 꼬리 읽기 — read_receipts 계약 재사용).
- settings_set 화이트리스트 (그 외 bad_key):
  - `idle_minutes` 0..1440 → state/idle_minutes 파일 (기존 관행 유지)
  - `receipt_retention_days` ≥1 → KV + 즉시 PruneReceipts (실패 시 prune_failed
    경고를 응답에 붙임). **0 = 무기한 = 현재 관행이므로 set 불가(bad_value)** —
    "무기한"을 UI 콤보에서 보여주되 선택 불가로 봉합.
  - `audio_master_mute` 0/1, `audio_master_volume` 0..100 → KV + audio.master
    이벤트 방송. **볼륨 set은 뮤트를 해제한다**(음량을 올렸는데 소리가 안 나는
    함정 방지).
  - `capture_allow` → 파킹 승인(§3).
- 봉큔回: `{"ok":true,"applied":{<key>:<val>}}` — 단, "applied"의 키는 동적이라
  docs/38 2레벨 리더가 못 읽는다. 클라이언트는 **요청 인자를 진실원**으로 삼는다
  (ClientSettingsApp ApplyReply가 arg를 key로 사용하는 이유).

## 3. capture_allow 키별 Ask — 플랜 대비 드리프트 (이 문서의 핵심 "왜")

플랜은 settings_set을 도구 차원 Ask 게이트(AgentToolAllowed)로 잠그려 했지만,
구현 직전 계약 확인에서 **사망 판정**: AgentToolAllowed는 "none" 게이트 도구에
대해 파일값이 "ask"여도 Allow를 돌린다(askCapable은
close_window/trust_request/run_console_app/trust_revoke 4종뿐).
즉 플랜의 게이트는 영원히 발화하지 않는 죽은 코드였다.

대안 채택 — **키 차원 파킹 파이프라인**(docs/53 파킹 재사용):

- settings_set이 `capture_allow` 키를 받으면 값(0/1 → ask/allow 결정)을
  검증한 뒤 구독자 확인 → PendingApproval(kind "capture_allow",
  permDecision="allow"/"ask") 파킹 + `agent.approval_request` 이벤트
  (`"tool":"settings_set","kind":"capture_allow","target_tool":"capture_window"`).
- approve 루프에 kind 분기 추가: **capture_window와 capture_region을 함께**
  WritePermissionsEntry — 캡처 도구는 쌍이므로 반쪽 허용은 성가신 함정.
- 얼굴 2종: jkchat 전용 스트립(`[캡처 허용] → decision`), 설정 GUI 체크박스는
  승인 전까지 pending 스트립 상태. 에이전트든 GUI든 **전부 파킹** — 키 수준
  Ask 균일화 (docs/53 레슨 대칭).
- 승인 불가(구독자 없음) 시 `approval_unavailable` 즉답 — GUI는
  "jkchat(에이전트 구독) 필요" 상태로 안내.

## 4. 코어 이벤트 펌프 (스펙 §2.3)

- **단일 펌프**: 기존 앱들은 각자 SendAgentEventSubscribe(true)를 부르고
  자체 루프에서 DrainAgentEvents를 돌렸다 — audio.master처럼 "여러 앱+코어가
  모두 알아야" 하는 이벤트엔 소유자가 필요. JKClientApplication이 Init에서
  한 번 구독하고 Run에서 드레인 → audio.master는 코어 직접 적용,
  나머지는 `OnAgentEvent` 가상 훅으로 파생 앱에 분배.
- vplayer 이관: 자체 구독 제거 + PumpAgentEvents → OnAgentEvent override.
  window.fullscreen(_exit) 미러는 이벤트 본문의 `id`가 자기 SurfaceId일 때만
  반응(기존 셀프 필터 유지). PumpAgentReplies는 별개 큐라 그대로 앱 소유.
- **jkclient 정적 링크 주의**: libjkclient을 바꾸면 앱 DLL 전부 재빌드 필요
  (vpt12가 게이트인 이유 — vpt12 2연속 ALL PASS로 봉합).

## 5. jkapp_settings GUI (스펙 §2.4)

- 5-섹션 단일 스크롤 (`##settingsbody`, 항상 세로 스크롤바, y≥30은 서버 크롬):
  1. **테마** — preset 3버튼(현재 프리셋 disabled), theme_set 재사용
  2. **트리거** — 체크박스(trigger_toggle) + idle InputInt(커밋온체인지)
  3. **데이터** — receipts 통계(rows/마지막 수신 시각), 보존기간 콤보
     (무기한 라벨 = 선택 불가), 레이아웃 저장/복원/목록(save_layout/
     restore_layout 재사용)
  4. **권한** — 매트릭스 요약(기본값 아닌 행 표) + agentmgr 열기 버튼
     (launch_app) — **편집은 agentmgr 소유**(docs/53 중복 해소)
  5. **장치** — 뮤트 체크박스, 볼륨 슬라이더(커밋온릴리즈: IsItemActive로
    드래그 중엔 로컬만), 캡처 체크박스(pending 스트립 → settings_set
    capture_allow), N7 플레이스홀더 행(disabled)
- 초기 로드: settings_read + agent_permissions 2발. 캡처 스위치의 진실원은
  settings_read에 캡처 키가 없으므로 **agent_permissions의 capture_window
  override**.
- 볼륨/뮤트 응답 후 UI 값은 **요청 인자로 싱크**(§2 applied 동적 키 계약).

## 6. jkagentd 등록 (docs/53 4곳 룰)

- tools/list 2종(settings_read 무인자, settings_set key/value) +
  IsKnownTool + LoadPermissions + args-rebuild 분기(nested
  params.arguments.key/value 재조립). selftest 체크 5번 추가.

## 7. events_list 카탈로그 보완

- `audio.master` 행 추가 (`data.mute,data.volume`), agent.approval_request
  desc에 capture_allow 힌트 추가 — settings_set이 발화하는 토픽이 카탈로그에
  빠져 있었던 누락을 마감에 발견해 봉합.

## 8. 테스트

- **probe_settings.ps1 15체크 2연속 ALL PASS**: read 봉투 셰이프/theme_set
  왕복/트리거 왕복(seed+toggle)/idle 파일 5→30/receipt 정리+ .bak(30d 가짜 행
  시드→7d 프룬→old 소멸+fresh 생존)/retention 0 bad_value/audio.master
  이벤트 캡처+read 반영/capture_allow 파킹→approve→written/bad_key+bad_value.
  state 백업/복원에 **permissions.json 포함**(check 14b가 capture_* override를
  쓰는데 probe_agentmgr이 그 파일의 생명주기를 소유 — 첫 공식런에서 ABORT로
  발견해 수리).
- 회귀: run_selftest 0 failure, jkagentd --selftest 0, probe_agentmgr ALL PASS,
  vpt11 ALL PASS, vpt12 2연속 ALL PASS, probe_agent_palette PASS(/settings 커맨드).
- vpt11 1차 런에서 S2 페이싱 0줄 플레이크(재시도 GREEN) — 환경 요인으로 기각,
  코드 연관 없음.

## 9. 레슨

1. **리다이렉트된 stdout은 C 런타임 블록 버퍼링** — RunAgentEvents가 이벤트
   행을 내보낸 뒤 flush하지 않으면 프로브가 런 중간에 빈 파일을 읽는다(종료
   시 한꺼번에 쏟아짐). 행 단위 `fflush(stdout)` 추가 + 프로브는
   Wait-EventLine 재시도 폴링으로 읽기. 1차 공식런 3 FAIL의 근원.
2. **게이트 계약은 구현 전에 실동선을 확인** — 플랜의 capture_allow Ask는
   askCapable 목록 밖이라 발화 불능. "도구 차원 게이트"가 안 되면 **키 차원
   파킹**이 정석(스펙 §2.2 희망 vs 파서/매트릭스 실제).
3. **동적 JSON 키는 2레벨 리더 사각지대** — settings_set의 applied.<key>는
   못 읽으니 클라는 요청 인자를 진실원으로. (docs/38 파서 계약의 또 다른
   귀결)
4. **프로브 state 생명주기는 다른 프로브 소유 파일까지** — permissions.json은
   probe_agentmgr가 소유하므로 probe_settings도 백업/복원해야 한다(ABORT로
   즉시 포착).
5. **JKWindowServer.cpp TU는 windows.h 금지 유지** — Win32 신규 사용
   (FindFirstFileA 3종)도 손선언(구조체+extern)으로. constexpr
   reinterpret_cast는 불가 → `static inline void*` 함수로.

## 10. 알려진 한계 / 다음

- **사용자 눈확인 대기**: settings 창(스폰/5섹션/볼륨 슬라이더/캡처 스트립/
  agentmgr 연동) — 눈확인 항목에 추가.
- audio.master의 코어 적용은 전역 마스터 볼륨 — 앱별 볼륨(트리거 소스와 무관한
  개별 제어)은 미범위(스펙 §3 YAGNI).
- 레이아웃 목록 열거는 exe-dir layout_*.json FindFirstFileA — state로 이전
  시 SettingsKvPath와 같은 경로 정리 레저.
- receipt 프룬은 전체 읽기(파일이 커지면 스트리밍 프룬 레저).