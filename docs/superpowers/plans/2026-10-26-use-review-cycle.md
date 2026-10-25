# 사용→리뷰→OS 업데이트 루프 (#113) 플랜

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:subagent-driven-development.

**Goal:** 서버 도구 호출 1건당 저널(JSONL)+수확 probe(원문 분류 요약)+EYES 결제 규약 — 사이클의 수확·분류 자동화.

**Architecture:** jkdesktop 서버 도구 dispatch 지점 저널 훅(절대 블로킹 없음)·probe jkharvest.sh(agentctl 접점만 — Win/WSL/폰 공통)·EYES 결제 행 형식 규약.

**Spec:** docs/superpowers/specs/2026-10-26-use-review-cycle-design.md (D1-D6)

## Global Constraints

- 저널 쓰기 실패 = 조용히 스킵(도구 응답·동작 무영향 — D1)·기존 도구/이벤트 원존 무변조(D5 — 추가만).
- 캐논 승계(Win 837/WSL 814/posix 304/폰 814) — 신설 계보 2w 증분만.
- 커밋 계약 전부(명시 경로·IP 0건(-F)·push·amend 금지·트레일러)·서브에이전트 스폰 금지·폰 런 중 편집 금지.

---

### Task 1: 서버 저널 leg

**Files:**
- Modify: `engine/src/server/JKWindowServer.cpp` — HandleAgentTool(도구 dispatch 진입점 — 원문 위치는 구현자 실측)에 저널 훅: 도구명+args 요약(≤128자 ts·ok/err) → `state/use_journal.jsonl` append(뮤텍스·flush·실패=무시·락 경합 최소 — 기존 events/receipt 락과 분리 검토 원존)·창 관리(launch/close/focus) 도구도 자연 흡수(도구 채널 위에서 오니)·**도구 20종+에 접점 1곳만**(DRY)
- Test: selftest 신설 **2w** — ①저널 append 형식(JSONL 파싱 등호) ②실패 무시(dir 읽기전용 → 도구 응답 무영향) ③args 절단 128자 ④ts 밀리초 정형 — 4건 목표
- [ ] **Step 1:** 2w 작성(fail 확인) 2. 구현 3. 빌드 Win/WSL 등호(+) 4. 커밋 `feat(apps): 서버 도구 호출 저널 leg (T1)`

### Task 2: 수확 probe jkharvest.sh

**Files:**
- Create: `engine/tools/probes/jkharvest.sh` — D3: agentctl로 journal tail(events_list 도구에 저널 소싱 불요 — 서버 파일 read 계약: **state/use_journal.jsonl 파일 직독 — 폰 ssh 세션 경로**)·bridge_report_*.txt 목록·분류 요약(에러 키워드 행 최근 10건·리포트 개수)·`HARVEST-VERDICT: HARVEST-OK` 1행+요약 스트림 — Win/WSL/폰 공통 실행(대상 = 실행 위치의 서버)·폰 동형 세그먼트(주소 미인쇄)·2런
- [ ] **Step 1:** probe+런 2회(WSL) 2. 커밋 `test(probes): 수확 probe jkharvest (T2)`

### Task 3: 폰 동형+EYES 규약 문서화+as-built

**Files:**
- Modify: probe 폰 런(접촉 가능 시 — 미접속 = BLOCKED 원장행 원존·강제 재시도 금지)·**EYES 결제 행 규약**(docs/93? — docs/90 §5 EYES 표 근처에 형식 1절)+사이클 지도 docs 갱신(빈 조각 3건 → 실장 표기)+docs/90 §4.1 부기
- [ ] **Step 1:** 폰 런(가능시)+docs 2. 커밋 `docs(apps): 사용→리뷰 루프 as-built+EYES 규약 (T3)`

## 사용자 게이트

1. 수확 리포트 모양 체감 = 사용자 선언(EYES).
2. D1-D6 = 재량 — 기각 시 즉시 수리.