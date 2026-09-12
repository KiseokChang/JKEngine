# 37. Desktop Agent 스크립트 신뢰 모델 — 지문 + trust.json + 승인 일반화

- 날짜: 2026-09-13
- 상태: 구현 완료. 스펙 `docs/superpowers/specs/2026-09-13-script-trust-design.md`

이전: docs/36 (LLM 토큰 스트리밍). docs/32 §8 "스크립트 서명/신뢰 모델은 후속" 항목의
구현. 트리거 번들의 .js와 .jkx가 무단 변경되면 재승인을 요구하는 로컬 신뢰 기준선.

## 1. 모델 (요약)

- 지문 = SHA-256 (BCrypt): dev .js는 파일 전체, .jkx는 MANI+SCRI 페이로드 TOC 순 연결.
- 저장소 state\trust.json — source:"pack"(패커 자기-증명)/"user"(승인 기록). 없음/파손 = 전부 비신뢰 (fail-closed).
- 승인 = close_window 파이프라인 일반화: trust_request 도구 → agent.approval_request(kind/name/origin/fingerprint) → jkchat 프롬프트 → approve. trust.json 쓰기 권위는 로더.

## 2. 서버 변경점 (`engine/src/server/JKWindowServer.cpp`)

- PendingApproval kind 일반화, approve의 Close는 close_window만.
- AgentToolAllowed: trust_request 기본 ask (close_window는 기존대로 deny, 나머지 allow).
- trust_list 도구 (지문 15자 표시 — `"sha256:"+8hex` 절단).
- 승인 만료는 기존 close_window와 동일 60초 — 초과 시 `approval_timeout` 응답 +
  `agent.approval_resolved` decision:"timeout" 브로드캐스트.

## 3. jktriggers 변경점 (`engine/tools/jktriggers/main.cpp`)

- connect-first 시작(승인 쿼리를 위해), TrustGate가 두 로딩 경로를 게이트.
- PackMode가 source:"pack" upsert (user 레코드 보존, 지문 불일치 재승인).
- jktriggers --selftest: SHA-256 벡터(NIST FIPS 180-4) + trust store +
  컨테이너 지문.

## 4. 얼굴

- jkchat [신뢰 요청] 프롬프트 + /trust, 팔레트 /trust, jkagentd trust_list.

## 5. 테스트

- probe_agent_trust.ps1 (7 체크): approval-request → approve → script-ran →
  user-record → restart-trusted(무재승인) → deny-skipped → pack-records.

## 6. 제한

- 내용 변경(재빌드 포함) → 지문 변경 → 재승인. 로컬 기준선은 편의 모델(보안 경계 아님).
- reload 중 승인 대기는 블로킹(최대 60초). 실서명 승격 경로는 스펙 §7.