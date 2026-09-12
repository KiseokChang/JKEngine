# 스크립트 신뢰 모델 — 해시 지문 + 신뢰 저장소 + 승인 파이프라인 일반화

- 날짜: 2026-09-13
- 상태: 설계 승인 (brainstorming 완료 — 사용자 결정: 로컬 신뢰 기준선, 해시 지문 + 신뢰 저장소,
  채팅창 승인 프롬프트, 트리거 스크립트만 적용, 서버 파이프라인 일반화(A안))
- 선행: docs/29 (M1 권한/receipt), docs/31 (승인 파이프라인), docs/32 (M2b 트리거 호스트)
- 후속 문서: 구현 완료 시 `docs/37_desktop_agent_trust.md`

## 1. 목표와 위협 모델

**목표**: 데스크톱 에이전트가 eval하는 트리거 스크립트에 신뢰 게이트를 둔다 —
내가 패킹한 것은 자동 신뢰, 외부에서 들어온 것은 기본 비신뢰로 첫 실행 시
채팅창 인라인 승인 프롬프트로 통과시킨다.

**위협 모델(로컬 신뢰 기준선)**: 같은 머신의 사용자가 패킹/작성한 스크립트는
신뢰한다. 공격 시나리오는 "외부에서 드롭된 스크립트가 사용자 몰래 eval되는 것" —
지문 기록 없는 스크립트는 프롬프트 없이 절대 실행되지 않는다. **이 모델은 보안
경계가 아니라 편의 모델이다** — 같은 머신의 공격자는 trust.json도 편집할 수
있다. 실서명(비대칭 키)은 승격 경로로만 남긴다(§7).

**적용 범위**: 트리거 스크립트만 — jktriggers가 eval하는 두 경로.
- dev: `<exeDir>\state\triggers\*.js`
- 패키지: `<exeDir>\apps\triggers\*.jkx` (MANI+SCRI 페이로드)

런처 `.jkx`(네이티브 DLL)는 미적용 — 저장소·파이프라인은 확장 가능 형태로 설계.

## 2. 지문(fingerprint) 정의

| 대상 | 지문 = SHA-256(...) | 비고 |
|---|---|---|
| dev .js | 파일 바이트 전체 | |
| .jkx 패키지 | MANI + SCRI 페이로드를 TOC 순서대로 연결한 바이트 | 컨테이너 전체가 아님 — 아이콘 등 무관 변경에 재승인 없음 |

- 표기: `sha256:<64hex>` (파일명 격 충돌 없음 — 지문 자체가 신원).
- 구현: Windows CNG(BCrypt) — MinGW `-lbcrypt` 링크 추가. 래퍼 `Sha256Hex`
  (+ 셀프테스트 알려진 벡터 "abc" → `ba7816bf8f01cfea414140de5dae2223...`).
  패커(`jktriggers --pack`)와 로더가 같은 exe에 있으므로 jktriggers 소스에
  둔다. JKJkxFile·서버·채팅창은 해시를 사용하지 않는다.
- **JKJkxFile 무변경**: SIGN TOC 엔트리는 이번 스코프에서 불필요 — 저장소가
  지문의 권위다 (§7 승격 경로 참조).

## 3. 신뢰 저장소 — `state\trust.json`

```json
{"records":[
  {"fingerprint":"sha256:abcd…","source":"pack","name":"trig_build","ts":1789…},
  {"fingerprint":"sha256:ef12…","source":"user","name":"my_trigger.js","ts":1789…}
]}
```

- **`source:"pack"`** — 패커가 자기 출력에 기록(내가 만든 것 = 자동 신뢰).
  지문 이미 존재하면 no-op인 **upsert** — 재빌드해도 `source:"user"` 레코드는
  절대 건드리지 않는다.
- **`source:"user"`** — 채팅창 승인 프롬프트로 허용될 때 **로더(jktriggers)가** 기록.
- 파일 없음 = 전부 비신뢰 (첫 부팅부터 게이트 유효).
- 읽기 실패/파손 = 전부 비신뢰 + 로그 (fail-closed).
- revoke는 trust.json 편집 + **jktriggers 재시작** — 도구 제공 안 함(스코프 밖,
  필요성 낮음: 내용이 바뀌면 지문 자체가 달라져 자동 비신뢰). 스크립트/신뢰는
  부팅 시 1회 로드 — `triggers.reload`는 활성 플래그만 재적재(docs/34 동작),
  전체 신뢰 재평가는 승격 후속. `(2026-09-13 최종 리뷰 수정)`

## 4. 승인 파이프라인 — 서버 일반화 (A안)

"하나의 API, 여러 얼굴" — close_window 승인 파이프라인(docs/31 §3)을 payload
일반화로 확장한다.

### 4.1 로더 게이트 (jktriggers)

- `LoadJsDir` / `LoadTriggerContainers`가 각 스크립트 eval 전에:
  지문 계산 → `IsTrusted(fingerprint)` 대조 → trusted면 eval.
- 비신뢰 → 서버에 `{"tool":"trust_request","args":{"name":…,"origin":"dev"|"package","fingerprint":…}}`
  쿼리 (기존 블로킹 `Query` — 시작 시점이므로 saveLayout 블로킹 예례와 동일 등급).
  `origin`은 스크립트 출처 — trust.json의 `source`(pack|user, 신뢰 기록 출처)와
  이름을 분리해 충돌을 피한다.
- 응답 분기:
  - `allow` → trust.json에 `source:"user"` upsert 후 eval
  - `deny` / `approval_timeout` / `approval_unavailable` / `permission_denied`
    → 스킵 + `[triggers]` 로그, 스크립트 없는 것과 동일하게 계속
- 신뢰 판정은 부팅 로드 시점에 한다 — trust.json 편집은 jktriggers 재시작으로
  반영된다. `(2026-09-13 최종 리뷰 수정)`

### 4.2 서버 (JKWindowServer)

- `PendingApproval` 일반화: `kind`("close_window"|"trust_request") + 표시용 페이로드.
- `trust_request` 도구 브랜치 (`HandleAgentQuery`): permissions.json의
  `"trust_request"` 게이트 — 기본값 **"ask"**(프롬프트). `"allow"`면 자동 승인
  (사용자의 명시적 선택), `"deny"`면 `permission_denied` 회신.
- `AgentToolAllowed`의 ask-degrade 규칙: ask-가능 집합에 trust_request 추가
  (비대상 도구의 ask→Allow degrade는 기존대로 유지).
- 보류 등록 + `agent.approval_request` 방송 — 페이로드에 `kind`, `name`,
  `fingerprint`, `origin` 추가. 만료(60s)/구독자 없음/거부 경로는 기존 공용 코드.
- `approve` 도구는 kind 무관하게 파킹 쿼리에 회신. **trust.json 쓰기는 서버가
  아니라 로더가** — 서버는 결정만 전달한다. receipts는 기존처럼 approve 호출이
  자동 기록.

### 4.3 채팅창 (jkchat)

- `agent.approval_request` 렌더링 kind 분기: `trust_request`면
  "스크립트 신뢰 요청 — <name> (<origin>), 해시 <앞 8자>…" 프롬프트.
  [허용][거부] 버튼과 approve 도구 호출은 기존 경로 공유.

## 5. 도구 얼굴

- **`trust_list`** (신규): trust.json 레코드 목록 — fingerprint 앞 8자/이름/source/ts.
  얼굴: agentctl, 팔레트 `/trust`, jkchat `/trust` (팔레트 패리티 관례),
  jkagentd 카탈로그(kToolsListJson) 등록.

## 6. 테스트

- 셀프테스트: SHA-256 알려진 벡터, trust.json 파싱/upsert 멱등성, 미기록 지문 판정.
- **`probe_agent_trust.ps1`** (신규):
  1. 서버+채팅 기동 → 비신뢰 .js를 `state\triggers\`에 드롭
  2. `agent.approval_request`(kind=trust_request) 캡처 → **approve allow** →
     스크립트 발화(마커 notify 실측) + trust.json 기록 확인
  3. 재적재(`triggers.reload`) → 프롬프트 없이 로드
  4. 두 번째 비신뢰 스크립트 → **deny** → 스킵 + 미발화
  5. 패커 기록(`source:"pack"`) 3종(trig_build/trig_idle/trig_crash)은
     부팅부터 무프롬프트
- 기존 회귀 전부 유지 (triggers 7/7, chat 7/7, mcp 5/5, e2e 7/7, palette 4/4,
  chat_llm 2/2).

## 7. 승격 경로 (이번 스코프 밖, 문서로만 확보)

- 실서명 도입 시: 패커가 개인키로 서명 → .jkx에 **SIGN TOC 엔트리(4cc "SIGN",
  버전 2 서명 블록)** 추가 → 로더가 공개키 검증. BCrypt는 Ed25519 미지원 →
  RSA/ECDSA. trust.json의 `source:"pack"`/`"user"`는 지문 기반으로 유지 가능.
- 런처 .jkx(네이티브 DLL) 확장: 같은 저장소 + 승인 파이프라인을
  `ScanJkxApps`/`SpawnClient`에 연결하면 됨 (DLL 해시는 빌드마다 바뀌어
  repack마다 재승인 — 도입 시 정책 결정 필요).

## 8. 제한

- 스크립트 내용 변경(재빌드 포함) → 지문 변경 → 재승인. 정직한 동작.
- 로컬 신뢰 기준선은 편의 모델 — 보안 경계 아님 (§1).
- 로드 중 승인 대기는 블로킹 — `triggers.reload`는 스크립트를 재로드하지 않으므로
  승인 대기 블로킹은 부팅 로드에서만 발생 (saveLayout 블로킹 예례와 동일 등급).
  `(2026-09-13 최종 리뷰 수정)`
- rate limit 없음 — 승인 요청은 로드당 1회라 발화 폭주 여지가 작음.