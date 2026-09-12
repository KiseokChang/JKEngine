# 38. 트리거 rate limiter — 소스별 발화 예산 + 타이머 상한 + publish_event 연결 상한

- 날짜: 2026-09-13
- 상태: 구현 완료. 스펙 `docs/superpowers/specs/2026-09-13-trigger-rate-limit-design.md`

이전: docs/37 (스크립트 신뢰 모델). docs/32 §8 "rate limit/dead-letter 없음 — 이후
rate limiter" 항목의 구현. 신뢰 모델은 **비신뢰** 스크립트의 eval을 막지만,
**신뢰된** 스크립트의 폭주(publish→on 셀프 루프)는 그대로 통과시킨다 — 이 문서는
같은 머신의 정상 스크립트가 이벤트 버스를 침수하는 것을 막는 **편의 보호막**이다
(보안 경계 아님 — 악의적 스크립트는 자기 예산을 관찰·회피 가능).

## 1. 적용 지점 3곳

| 지점 | 위치 | 상한 |
|---|---|---|
| 핸들러 발화 | jktriggers DispatchEvent (filter 통과 후 JS_Call 직전) | 소스당 60회/60초 |
| 라이브 타이머 | jktriggers setTimeout/setInterval | 전역 64개 |
| publish_event | 서버 HandleAgentQuery | 연결당 60 이벤트/10초 |

## 2. 핸들러 발화 예산 (`engine/tools/jktriggers/main.cpp`)

- **단위 = `TriggerReg.source`** (컨테이너명 — enable/disable 키와 동일). 컨테이너는
  **예산 공유 단위**: 번들의 모든 핸들러가 같은 예산을 쓴다. dev 스크립트는
  source가 달라 자동 분리.
- **고정 윈도우 60회/60초** (`SourceBudget{windowStartMs,count,notified}` —
  설정 없음, YAGNI). `RateAllowAt`이 예산 확인·소비를 한 곳에서: 초과 시
  **스킵**(이벤트 하나만 — 다른 트리거는 정상 발화), 로그+notify는
  **윈도우당 1회** (`agent.notify` "트리거 발화 제한" + HostLog 1회).
- **소비 규칙**: consume-on-allow — 거부된 호출은 예산을 소비하지 않는다
  (서버 publish cap과 동일 시맨틱). 필터 미스·disabled·RateAllow 거부는
  JS_Call에 도달하지 않으므로 소비 아님.
- **cap 상호작용 (스펙 §2 ruling)**: 서버 연결 cap을 **60/10s**로 맞춰 셀프루프에서
  항상 로컬 cap이 먼저 바인딩 — notify-once·정지 지점이 결정론적. (초안
  30/10s → 서버가 더 낮으면 루프가 로컬 통지 전에 서버에서 굶으므로 정정.)
- **dead-letter 미적용**: 디스패치는 동기 JS_Call + 큐 없음 — 큐가 없으니
  적체(letter) 개념 자체가 없다. 문서로만 명시, 코드 없음.
- selftest: 60 허용 → 61번째 거부+notified 래치 → 같은 윈도우 내 재거부 →
  윈도우 롤오버 재허용 + 예산 리셋(notified=false, count=1) (인젝션 클럭).

## 3. 타이머 상한

- **전역 64개** (setTimeout+setInterval 합계, trig_idle 같은 기존 번들이 슬롯을
  점유 — 실측: trig_idle이 1개 홀드 → 신규 70개 중 7개 드롭). 초과 신규 생성 →
  HostLog 1회 + 콜백 미등록 (JS 예외 던지지 않음 — 기존 실패 경로 관례).
  기존 타이머 불변. 런어웨이 setInterval 채워먹기를 구조적으로 차단.

## 4. 서버 publish_event 연결 cap (`engine/src/server/JKWindowServer.cpp`)

- HandleAgentQuery publish_event 브랜치: **연결별 고정 윈도우 60/10s**
  (`publishBudgets_[connId]` — 브로드캐스트 전 확인). 초과 이벤트는 **드롭** +
  `{"ok":true,"dropped":1}` 회신 (발신자가 재시도 폭탄을 안 돌리도록 ok 통과,
  `dropped` 필드로 정직 표시) + `[server] publish_event rate-capped` 로그
  **윈도우당 1회**.
- 드롭은 `PushAgentEventJson`에 도달하지 않으므로 **events_list 통계에
  반영되지 않는다** (fired는 허용분만 오름).
- 예산은 **허용 이벤트만 소비** (드롭 경로 소비 없음 — 로컬 cap과 동일).
  브랜치가 clientsMutex_ 보유 중이라 락프리 유지. 연결 종료 시 엔트리 제거
  불필요 (connId 재사용 시 윈도우 리셋).

## 5. 신뢰 경질화 (docs/37 후속 minor 3건)

1. **`TrustRecord.ts` int → int64**: 로더가 throwaway-QuickJS 리더로
   `JS_ToInt64` 직독 (AgentJson에는 int64 배열 접근자 없음 — 신설 폴백 아닌
   전용 리더). SaveTrustRecords는 `%lld`. trust_list **표시는 여전히 int**
   (2038년 전 값만 실사 — 표시 경로 변경 미룸, §7). selftest에
   2200000000 라운드트립 핀.
2. **trust_request `name` 상한 96바이트** — 초과 `bad_name` 거부
   (bad_fingerprint/bad_origin과 같은 모양, 파킹 전).
3. **trust_list 파손 구분**: trust.json 읽기 실패/파손/용량 초과 →
   `{"ok":false,"error":"trust_store_unreadable"}` — "레코드 0개"와 구분.
   **fresh install 동작 변경**: 파일 없음도 unreadable로 회신 (기존은 ok,
   records:0).

## 6. 테스트

- **selftest (jktriggers)**: rate 윈도우 경계/롤오버 + ts int64 라운드트립.
- **`engine/tools/probes/probe_agent_ratelimit.ps1` (7 체크, 페이즈 구성 —
  번들 rate_probe는 패키지 경로로 신뢰 게이트 우회)**
  1. **버스트**: kick_burst → 핸들러 70회 publish → events_list
     `ratelimit.server` fired == 60 + 서버 `rate-capped` 로그 1회
  2. 10초 대기 (서버 윈도우 리셋) → **셀프루프**: on→publish 루프가 로컬 cap에서
     정지 — ping 58회 (60 예산 − 2 kick) + `rate limit:` 로그 1회
  3. **notify-once**: 채팅 트랜스크립트 "트리거 발화 제한" 1회
  4. 60초 대기 (로컬 윈도우 리셋) → **격리**: 다른 킥 정상 발화
  5. **타이머 상한**: 70개 setTimeout → `timer dropped` 7회
  6. 회귀: triggers 7/7 + trust 7/7
  (체크 7개 = burst-drop/self-loop-cap/rate-limit-log-once/notify-once/
  isolation/timer-cap/regression)

## 7. 제한

- 고정 기본값 — 튜닝 UI/설정 파일 없음 (필요 시 state 파일로 확장).
- 연결 cap은 트리거의 desktop.publish도 포함 — 도달 시 스크립트의 정상 방송도
  드롭될 수 있음 (로컬 소스 예산과 별개 예산).
- trust_list의 ts **표시**는 int 경로 유지 (2038년 전 값만 실사) — 표시 경로의
  int64 승격은 후속.
- `publishBudgets_`는 연결 종료 시 엔트리를 비우지 않음 (장기 실행 서버에서
  connId 수만큼 누적 — 재사용 시 리셋이라 동작상 무해).
- 프로브는 60초 로컬 윈도우 리셋 대기로 인해 실행 시간 ~70초 — 격리/타이머
  체크가 로컬 예산 소진 상태에서 시작하는 것을 피하기 위한 구조 (하드닝 후속:
  리셋 대신 신규 소스 사용 검토).