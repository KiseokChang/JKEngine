# 스펙: 채팅 자연어 승격 — 폰 cloud 모델 턴 (2026-10-08)

발단(사용자 실사용 2026-10-08): 폰 jkweb/jktalk 채팅은 T7 stub 라우터 — 정해진
트리거만 접수하고 자연어는 안 먹는다. 사용자 지향: "PC에서처럼 ollama 깔아서
ollama launch claude로 연결" + "빨리 도는거 보고 싶네요". T8의 "stub 유지"
판정 근거는 폰 로컬 qwen2.5:0.5b(0.29 tok/s)뿐이었다 — **cloud 측정 누락**이
판정의 구멍. PC cloud 턴(glm-5.3-flash:cloud)은 3.2s 실측(duration_api_ms
3221·출력 206 tok) — 속도 장벽은 해소. 폰 Wi-Fi 전환 불요(핫스팟 인터넷으로
cloud 추론 가능).

## 목표

폰 채팅(jkweb 브라우저 + jktalk 파이프)에서 자연어 한 줄이 cloud 모델 턴
(~3-10s)을 거쳐 **실제 동작**(launch/close/focus/list)으로 이어진다.
PC PC-브리지(jkbridge 8899)는 이미 보유 — 본 라인은 폰 지향.

## 설계 결정 (컨트롤러 룰링)

1. **백엔드 슬롯 계약 승계** — jktalk/jkweb 둘 다 이미 슬롯 주석을 달아뒀다
   (engine/tools/jkweb/main.cpp:342-344: `text → ChatAction + 안내문`).
   LLM은 라우터의 **뇌 자리**를 대체한다(기존 tool dispatch·ServerMeta·페이지
   계약 무변) — claude CLI가 MCP로 도구를 직접 실행하는 폰 전재구축 아님.
2. **동기 턴 브리지** — JKLmEngine::StartTurn은 비동기 콜백이다. jkweb은
   연결당 1스레드(jkweb/main.cpp:581)이므로 **동기 래퍼**(StartTurn+CV 대기
   → LlmTurnResult)를 덧댄다. jktalk의 동기 ProcessTurn에도 같은 래퍼.
3. **cfg 분기** — `state/chat.json`의 `engine`이 라우터를 이끈다:
   - 기존: "ollama"(=launch claude) | "claude" | "stub" (JKLlmEngine.h 계약)
   - 신설 **"ollama-direct"**: `ollama run <model>`(또는 /api/chat HTTP) 텍스트
     턴 — 폰에 claude CLI(node 스택 ~50MiB+)가 없어도 되는 경로. 폰 1차 채택,
     PC 유지치도 허용.
   - **fallback 원칙**: cfg 미구성/엔진 스폰 실패 → 기존 기존 라우터(stub)로
     조용히 폴백 — 승격은 가법이고, 없으면 지금 모양을 보존한다.
4. **승격 후에도 트리거 라우터는 정직한 1차** — 즉발 확정 트리거(정확 매치)는
   LLM 왕복 없이 바로 동작(지연 최소), 비매치만 LLM 턴. 정보성 응답에는 모델
   텍스트를 안내문으로 회신.
5. **순서 제약** — ① 폰 조달·실측(T1)이 먼저: 사용자가 "빨리 보고 싶다" →
   조달 직후 curl/run 실측 영수증부터. ② 구현 배선은 chat-close-fix 라인
   완전 착지(리뷰 CLEAN + 폰 재배포) **이후** — 공유 워킹 트리 배포 원천
   오염 사고(T4 fix r1 함정 #11) 재발 방지.

## 조달 전제 (실측 근거)

- 폰 Termux ollama는 구판 — `launch` 서브커맨드 부재(phone_ollama_try.sh
  원장). 신판 조달 경로: `pkg install ollama`(Termux 패키지) 또는 공식
  설치 스크립트/바이너리 — 가능하면 핀된 release URL 선호(조달기 계약:
  sha256 핀 부재는 docs/81 §4 deferred).
- cloud 모델 사용에는 ollama **서명(signin) 게이트**가 있다 — PC는 완료,
  폰은 미실측. 서명은 폰 브라우저 인증 흐름 → **사용자 게이트** 후보.
- claude CLI(node)는 폰 미설치 — `ollama launch claude` 경로의 진실 게이트
  (honest-fail 원장 합법, 실패 영수증이 곧 결과물 — phone_ollama_try 선례).
- 폰 bionic 주의: glibc 바이너리 exec 거부는 "No such file or directory"로
  기만 표시(docs/81 §3 #8).

## 검증 계약

- 폰 E2E 자연어: "지뢰찾기 켜줘"→launch ok+창 생성 단정, "닫아줘"→실제
  해소(chat-close-fix 0e25762 협력), 전무 트리거→정직 회신 — probe로
  receipt, 최종은 사용자 육안 결제(가짜 결제 기록 금지).
- 셀프테스트: 동기 브리지·cfg 분기·fallback 케이스 1n 계열 신설 — 3축
  캐논 갱신 계보 기록.
- 원장: docs/80 §5 보완(T8 판정 cloud 미측정 정정) + 신설 docs 착지.

## 범위 밖

- jkbridge PC 경로 변화 없음. HTTP 어댑터는 CLI 직접 경로로 moot.
- 폰 git 원격·공용망 노 없음(기존 보안 계약 불변 — 사내 IP 비노출).