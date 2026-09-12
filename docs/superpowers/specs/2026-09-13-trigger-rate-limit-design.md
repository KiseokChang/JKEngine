# 트리거 rate limiter — 발화 상한 + 타이머 상한 + publish_event 연결 상한

- 날짜: 2026-09-13
- 상태: 설계 확정 (사용자 위임 진행 — "다음 것도 쭉쭉 진행, 추천대로". docs/32 §8
  "무한 루프 트리거는 호스트가 막지 않는다" 잔여 항목 + 스크립트 신뢰 모델(docs/37)
  후속 경질화. 설계 결정은 컨트롤러 재량 — 사용자 슬립 중, ledger에 ruling 기록)
- 선행: docs/32 (트리거 호스트), docs/37 (신뢰 모델)
- 후속 문서: 구현 완료 시 `docs/38_trigger_rate_limit.md`

## 1. 목표와 위협 모델

신뢰 모델(docs/37)은 **비신뢰** 스크립트의 eval을 막지만, **신뢰된** 스크립트의
폭주(publish→on 셀프 루프, 버그로 인한 발화 폭주)는 그대로 통과시킨다 — 채팅창/
알림 센터/서버가 스팸으로 익사한다. 이 설계는 같은 머신의 정상 스크립트가
버그·셀프루프로 이벤트 버스를 침수하는 것을 막는 **편의 보호막**이다 (보안
경계 아님 — 악의적 스크립트는 cap도 편집 가능).

적용 지점 3곳:
| 지점 | 보호 대상 | 메커니즘 |
|---|---|---|
| jktriggers 핸들러 발화 | 이벤트 버스 하류 (채팅/알림/서버) | source(컨테이너)별 슬라이딩 윈도우 cap |
| jktriggers 타이머 | 스크립트가 setInterval을 계속 찍어 대량 생성 | 라이브 타이머 전역 상한 |
| 서버 publish_event | 모든 에이전트 클라이언트 얼굴 (프로브·MCP 포함) | 연결별 고정 윈도우 cap |

## 2. jktriggers — 핸들러 발화 cap

- **단위**: `TriggerReg.source` (컨테이너명 / dev 스크립트의 state/triggers 경로명) —
  enable/disable 키와 동일 단위. dev 스크립트는 컨테이너와 예산을 공유 안 함
  (source가 다르므로 자동 분리).
- **기본값**: 소스당 **60회 / 60초** (고정, 설정 없음 — YAGNI; 필요 시 후속).
  슬라이딩이 아닌 **고정 윈도우**: `windowStartMs`로부터 60초 경과 시 카운터·
  윈도우 리셋. 구현 단순 우선.
- **동작**: DispatchEvent에서 filter 통과 후 JS_Call 직전에 예산 확인·소비.
  초과 시 핸들러 **스킵**(이벤트 하나만 — 다른 트리거는 정상 발화).
- **통지는 윈도우당 1회**: 초과 첫 이벤트에서 `desktop.publish("agent.notify",
  {"title":"트리거 발화 제한","body":"<source> — 이벤트 발화 상한 도달"})` 1회 +
  HostLog 1회. 윈도우 리셋 시 재통지 허용 (폭주 자체는 계속 조용히 스킵).
- **cap 상호작용 (preflight ruling)**: 서버 연결 cap(§4)은 **60/10s**로 맞춘다 —
  로컬 cap(60/60s)과 동일 수치로, 셀프루프 패턴에서 항상 **로컬 cap이 먼저
  바인딩**되어 notify-once·정지 지점이 결정론적. 서버 cap이 더 낮으면 루프가
  로컬 통지 전에 서버에서 굶는다. (초안 30/10s → 정정.)
- **예산 소비는 성공 발화만**: JS_Call이 예외로 끝나도 1회로 센다(발화 자체 소비).
  필터 미스·disabled는 소비 아님.
- dead-letter: 현재 디스패치는 동기 호출 + 큐 없음 — 개념 미적용 (문서로만 명시).

## 3. jktriggers — 라이브 타이머 상한

- **전역 64개 상한** (setTimeout/setInterval 합계). 초과 신규 생성 → HostLog
  1회 + 콜백 미등록 (JS 예외 던지지 않음 — 기존 실패 경로 관례). 기존 타이머는
  불변. 런어웨이 타이머 채워먹기(발화마다 새 setInterval)를 구조적으로 차단.

## 4. 서버 — publish_event 연결 cap

- HandleAgentQuery publish_event 브랜치: **연결별 고정 윈도우 60 이벤트 / 10초**
  (§2 상호작용 — 로컬 cap과 동일 수치).
  초과 이벤트는 **드롭** + `{"ok":true,"dropped":1}` 회신(발신자가 재시도 폭탄을
  안 돌리도록 ok로 통과, `dropped` 필드로 정직 표시) + 서버 로그 1회/윈도우.
- 상태: `map<uint64_t connId, {windowStart, count}>` — 브로드캐스트 전 확인.
  연결 종료 시 엔트리 제거 불필요 (connId 재사용 시 리셋만).

## 5. 신뢰 경질화 (docs/37 후속, 미룬 minor 3건)

1. **`TrustRecord.ts` int → long long** (Y2038 음수 표시 방지). 로더의 AgentJson
   접근자가 int64를 지원하지 않으면 GetArrStr + strtoll 폴백 허용.
   SaveTrustRecords는 수동 직렬화 — 서식만 `%lld`로.
2. **서버 trust_request `name` 길이 상한 96바이트**(topic 상한과 동일 근거) —
   초과 시 `bad_name` 거부 (bad_fingerprint/bad_origin과 같은 모양, 파킹 전).
   buf[1024] 브로드캐스트 절단 엣지 해소.
3. **trust_list 파손 구분**: trust.json 읽기 실패/파손/용량 초과 시
   `{"ok":false,"error":"trust_store_unreadable"}` — "레코드 0개"와 구분.
   얼굴 3종(agentctl/palette/jkchat /trust)은 비ok 회신을 이미 원문 표시하므로
   소비자 변경 불필요 (구현자가 실측 확인).

## 6. 테스트

- **selftest (jktriggers)**: 윈도우 cap 경계(60→61회), 리셋 후 재발화, 타이머
  상한(64+1), ts int64 라운드트립(19억+ 초 값).
- **`probe_agent_ratelimit.ps1`** (신규, 페이즈 구성 — cap 상호작용으로 결정론 확보):
  1. **버스트 페이즈** (신규 윈도우): 핸들러가 `ratelimit.server` 토픽으로 70회
     publish → events_list에서 fired == 60 + 서버 `rate-capped` 로그 1회
  2. 10초 대기 (서버 윈도우 리셋) → **셀프루프 페이즈**: `on X → publish X` →
     발화 60±1 + `rate limit:` 로그 1회 + `트리거 발화 제한` 알림 채팅 트랜스크립트 1회
  3. 격리 — 다른 토픽 트리거는 cap 도달 이후에도 발화
  4. 타이머 상한 — 70개 setTimeout → `timer dropped` 7회(70-64)
- 기존 회귀 전부 유지 (triggers 7/7, trust 7/7, events 5/5, chat 7/7, mcp 5/5,
  e2e 7/7, palette 4/4).

## 7. 제한

- 고정 기본값 — 튜닝 UI/설정 파일 없음 (필요 시 state 파일로 확장).
- 연결 cap은 트리거의 desktop.publish도 포함 — cap 도달 시 스크립트의 정상
  방송도 드롭될 수 있음 (동일 소스 예산과 별개; 30/10s는 실사용 대비 넉넉).
- rate cap은 편의 보호막 — 같은 머신의 스크립트는 자기 예산 상태를 관찰·회피
  가능 (보안 경계 아님, §1).