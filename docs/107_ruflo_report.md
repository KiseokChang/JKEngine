# 107 — Ruflo 조사 보고서 (상사 보고용, 2026-11-10)

**작성 근거**: 로컬 설치본 `ruflo v3.74.1` CLI 실측(명령·doctor·MCP 도구 카탈로그·
패키지 메타데이터·설치 구조) + 이번 세션의 동등 오케스트레이션 실전(내장 Workflow
파이프라인) 참조. 공개 웹 문서 교차 확인은 미실시(§7 정직 표기).

---

## 1. 요약 — 결론 먼저

**Ruflo란?** 오픈소스(**MIT**) AI 에이전트 오케스트레이션 플랫폼. 실측상
**구 `claude-flow`(github.com/ruvnet/claude-flow) v3의 npm 재배포판**(패키지명
`ruflo`, 바이너리 `ruflo`; 도움말 예제가 여전히 `claude-flow ...`로 병기됨).
CLI + MCP 서버 두 표면으로, Claude Code 같은 호스트에서 "60+ 특화 에이전트
스웜"을 스폰·조정·학습시키는 것을 목표로 한다.

**도입 판단 (과제 관점)**: **"개념·패턴 참조 강함, 제품 기반 채택은 신중"**.
- 강점: 오케스트레이션 패턴의 백과사전(스웜·합의·훅 학습·벡터 메모리·정책
  엔진), MIT라 포크 가능, MCP 표준 표면.
- 약점: 설치 1.2GB(의존성 트리 거대 — 공급망 표면), 극도로 빠른 릴리스
  페이스(3.74.x — 버전 고정·포크 관리 없이는 재현성 리스크), 단일 메인테이너
  생태계, 사내 보안 정책 검토 필요(MCP 도구가 파일·셸·외부 레지스트리에 넓게
  접촉).
- 권고: **폐쇄망 샌드박스 파일럿 + 버전 핀**, 제품 코어로는 **핵심 원칙만
  자체 최소 구현**(§6 MVP 범위 — ruflo 전체의 약 20%로 우리 과제 요구의 80%).

---

## 2. 정체·출처 (실측 사실)

| 항목 | 실측 값 |
|---|---|
| 패키지 | `ruflo` — home page: `https://github.com/ruvnet/claude-flow#readme` |
| 버전/라이선스 | **v3.74.1 / MIT** |
| 설명 원문 | "Enterprise AI agent orchestration platform. Deploy 60+ specialized agents in coordinated swarms with self-learning, fault-tolerant consensus, vector memory, and MCP integration" |
| 설치 실태 | npm 전역 설치, **1.2GB**(`node_modules`에 @anthropic-ai·@google·@huggingface·@grpc·electron 등 대형 의존성 군) |
| MCP 표면 | stdio 서버: `ruflo mcp start` — `ruflo mcp tools` 카탈로그 실측 **100여 종**(agent/swarm/memory/config/hooks/policy/neural/…) |
| 프로젝트 스코프 | `.claude.json`에 **프로젝트별 등록**(실측: `I:/progwork/bachata_page`에만 등록 — 그래서 본 JKENGINE 세션에서는 미연결) |
| 진단 | `ruflo doctor` 원문: v3.74.1 최신·Node 24·Claude Code v2.1.312 인식, 데몬/메모리DB 미기동(초기화 전) |

---

## 3. 원칙 (설계 철학 — 명령 체계 역추적)

1. **에이전트 = 1급 시민**: spawn/list/status/stop/metrics/health/logs/pool
   — 에이전트를 프로세스처럼 수명주기 관리.
2. **조정은 계층·군집**: `swarm`(V3 15-agent hierarchical mesh·pheromone 적응
   스케줄링), `hive-mind`(여왕 주도 합의·투표), `autopilot`(전 태스크 완료까지
   지속 실행).
3. **자기학습 루프**: pre/post-edit·command·task 훅 → 학습 → `route`(Q-learning
   라우팅)·`explain`(결정 투명성) — 같은 실수를 구조적으로 줄이는 설계.
4. **기억 우선**: sql.js 벡터 메모리(의미 검색·시간 감쇠·HNSW), Claude Code
   메모리 브리지(memory_import_claude·unified search).
5. **작업 분해·라우팅**: `route`·`analyze`(변경 위험)·`progress` — 작업을
   에이전트로 보내는 경로가 도구의 중심축.
6. **가드레일 내장**: `policy` 엔진(observe/enforce 모드·**결정 원장**
   decision ledger, ADR-324), `security`(CVE·위협 모델링), `claims`(권한),
   auto-approve 설정 검사(ADR-509).
7. **상태 지속성**: 세션 저장/복원·이벤트 소싱·데몬(백그라운드 워커).
8. **개방 표면**: MCP 표준·provider 추상화·플러그인(IPFS 기반 레지스트리).
9. **스펙 주도 문화**: 도움말 곳곳에 ADR 번호 참조(ADR-016/025/122/324/330/
   380/509) — "결정을 문서로 남긴다"는 운영 철학이 도구 자체에 반영.

## 4. 동작 형태 (운영 흐름)

```
init(프로젝트 초기화·훅 설치) → doctor(환경 진단) → swarm init/start
  → agent spawn(역할별) → task 제출/라우팅 → 조정(메시/합의/페로몬)
  → 훅 자기학습(pre/post) → 메모리 저장·검색 → session 저장/복원
        ↑ MCP 모드: 호스트(Claude Code)가 도구로 각 단계 호출
```

- 데몬이 백그라운드 워커로 상주 가능 — "지속 실행(autopilot)"이 기본 사상.
- 실패 처리: 합의(consensus)·fault-tolerant 표기 — 다수결 투표 기반 판정.

---

## 5. 이번 작업 참조 — 동등 오케스트레이션 실전 비교

본 세션에서 ruflo가 미등록이라 **호스트 내장 Workflow**로 동일 취지의
파이프라인을 실행했다(4축 병렬 검토 → HIGH 반증 검증 → 종합). 이 경험으로
"오케스트레이터에 실제로 필요한 것"을 실측 관점에서 정리하면:

| 축 | 내장 Workflow (이번 실측) | Ruflo (실측 계약 기준) |
|---|---|---|
| 파이프라인 정의 | **코드(JS 스크립트) — 결정적** | YAML/템플릿 + 스웜 설정 |
| 실행 단위 | agent(prompt, schema) — 경량 | 에이전트 수명주기 전체(스폰·풀·헬스) |
| 검증 패턴 | 반증 투표·렌즈 다양화 (스크립트로 조합) | consensus(voting) 내장 |
| 출력 강제 | **JSON Schema 강제(StructuredOutput)** | 도구 스키마(MCP) |
| 재개 | **runId 기반 프리픽스 캐시 재개** | 세션 저장/복원 |
| 메모리 | 없음(턴 단위) | **벡터 메모리·학습 영속(핵심 차별점)** |
| 가드레일 | 권한 프롬프트 | **정책 엔진+결정 원장(감사)** |
| 비용 통제 | budget(토큰 목표·하드 캡) | 성능/메트릭 계열 |

**해석**: 우리 실전에서 오케스트레이션의 가치는 (a) 축 분해의 결정성
(b) 반증 검증의 강제 (c) 스키마 강제에서 나왔다. Ruflo의 고유 가치는
**장기 학습·메모리·대규모 스웜·감사 원장** — 즉 "여러 세션에 걸친 누적
지능"이 필요한 과제인지가 도입의 실질 판단 기준이 된다.

---

## 6. 도입 vs 자체 제작 — 판단 프레임

| 기준 | 질문 | 판정 포인트 |
|---|---|---|
| 요구 적합 | 과제가 "누적 학습·대규모 스웜·감사 원장"을 요구하나? | 요구가 단발 파이프라인이면 자체 최소 구현이 우세 |
| 보안/통제 | MCP 도구가 파일·셸·외부(IPFS 레지스트리·클라우드 에이전트)에 접촉 — 사내 반입 정책과 충돌 없나? | 폐쇄망 샌드박스·도구 화이트리스트 검토 필수 |
| 공급망/재현성 | 1.2GB 의존성·주 단위 릴리스 — 버전 핀·SBOM·포크 전략이 있나? | 핀 고정·포크 유지보수 비용 산정 |
| 벤더 리스크 | 단일 메인테이너 생태계(MIT라 포크 허용) | 장기 유지보수 주체를 우리가 질 수 있는가 |

**권고안**:
1. **파일럿**: 폐쇄망 샌드박스, 버전 핀(3.74.1), 감사 로그 확인 — 2~3주.
2. **자체 제작 최소 코어(MVP)**: ①에이전트 스폰·수명주기 ②결정적 파이프라인
   정의(파이프라인/병렬/검증) ③JSON 스키마 강제 출력 ④runId 재개 ⑤결정 원장
   (감사) — **이 5개가 실전 가치의 대부분**(이번 세션 실측과 일치).
3. 원칙은 ruflo §3을 참조하되, 학습·메모리는 후속(§MVP 밖) — 첫 릴리스에서
   제외해 복잡도를 통제.

---

## 7. 리스크·미확인 (정직 표기)

- **미확인 1**: 공개 문서 교차 확인 미실시(웹 조회 안 함) — GitHub README·
  릴리스 노트 확인이 필요. 특히 성능 주장 "HNSW 150x"·"FlashAttention
  2.49~7.47x"는 **도구 자체 표기**이며 제3자 검증 전이다.
- **미확인 2**: 내부 구현 검토는 미실시(도구 계약·구조 관측까지 —
  src/ 내부는 chat-ui·mcp-bridge·ruvocal 등 배치만 확인).
- **운영 맥락**: `bachata_page` 프로젝트 등록의 주체·시점은 사용자 확인 필요
  (등록 자체는 실측).
- **사내 규약**: 외부 패키지·IPFS 레지스트리·클라우드 관리 에이전트 접촉
  여부는 보안 부서 판정 사항 — 본 보고서는 기술 판단만 제시.

## 8. 부록 — 실측 원문 요지

- `ruflo --version` → `ruflo v3.74.1`
- `ruflo doctor` → "MCP Servers: 1 servers (ruflo configured:
  ...projects[I:/progwork/bachata_page])", "Daemon Status: Not running",
  "Memory Database Presence: Not initialized"
- `ruflo mcp tools` → agent 19종·swarm 5종·memory 20여 종(agenticow 계열
  포함)·config 6종·hooks 30여 종 등 — 전부 Enabled
- 패키지 구조: `bin/`·`src/`(chat-ui·mcp-bridge·ruvocal·nginx·config·scripts)·
  README — 총 1.2GB
