# T7 — LLM HTTP 어댑터 통일 (jkdesktop-apk · 컴팩트 플랜 2026-11-04)

> **as-built 2026-11-04**: T1-T4 실행+폰 실측 폐곡 — 셀프테스트 **984 PASS 0
> failure**(965 + 1n-g 신설 19). 폰 실측: ①local = 폰 /talk → **폰→PC 데몬
> HTTP 릴레이(192.168.11.130:11434) 실 턴** — "지금 창 목록 좀 봄줘"(STT식
> 오타 포함) → 모델 액션(JSON) 파스 → ListWindows → 서버 도구 원문
> `{"ok":true,"windows":[]}` — #119의 "폰→PC AP 격리 No route" 유계 해소
> 확인(폰 = 홈 Wi-Fi 서브넷 접속판) ②cloud = `[jkdesk] https transport
> wired=1`(logcat 원문 — JNI_OnLoad 주입) — 턴 실측 = API 키 사용자 준비
> (EYES·BLOCKED 유계 허용 — 플랜 원존). PC 실측: jktalk 실 턴 1.47s(local
> leg — engine="ollama"+llm_mode="local" 시딩 chat.json).
>
> 함정 원문: chat.json 시딩에 directory 백슬래시 단일 이스케이프 → JSON 파스
> 실패 → fileKnown=false → LLM leg 미발사(0.05s 즉립 — "engine이 구성됐다"
> 게이트의 fileKnown 절이 전부 먹는다 — "JSON 규약 그대로 백슬래시는 `\`
> 이중화 필수" 원문의 재발). APK chat.json seam = **CWD 상대
> "state/chat.json"**(LoadChatConfig의 __ANDROID__ leg — exe-dir =
> nativeLibraryDir = 읽기전용 원존).

스펙(docs/superpowers/specs/2026-11-01-jkdesktop-apk-unified-design.md §5
"LLM 계약 통일")의 Task 7 이행. 원존 계약: llm.mode=off|local|cloud
(settings) — local=ollama_host·cloud=ollama.com+Bearer API키; 폰 내장 모델
조달 0; 근사 매칭(ChatRouter 0.02s) 우선 유지.

**전제 검산(플랜 자체 조사 원문)**: jk::net = ListenTcp/Accept/RecvAll/Send
만 존재 — **ConnectTcp 부재·HTTP 1.1 클라 부재·TLS 0** → 설계 이원화:
(a) 로컬 ollama = 평문 HTTP(localhost:11434) 순수 native
(b) cloud(https://ollama.com) = **폰 플랫폼 TLS JNI 브리지**(JkVoiceService
선례 — HttpsURLConnection; 조달 0) — Win/WSL의 cloud = fail-loud 정직
거부(플랜 캐논 회귀 0).

## Tasks

### T1 설정+요청 조립(순수) + 셀프테스트
- `engine/src/agent/JKLlmTurnHttp.{h,cpp}` 신설:
  - `BuildOllamaChatJson(model, system, prompt)` — /api/chat 요청 본문
    (stream:false — v1 버퍼드 단발; delta 콜백 미제공, streamed=false 계약)
  - `ParseOllamaChatReply(buf)` → content 추출(json.message.content·
    에러 정직)
  - `NormalizeOllamaMode("off|local|cloud" — chat.json "llm_mode")`
- LoadChatConfig 확장: `"llm_mode"`, `"api_key"`(민감값 — app-private
  files/chat.json = 내부 저장), `"engine": "ollama-http"` 신설.
- selftest(main.cpp): 조립·파스·모드 판정 등호(네트워크 의존 0 — 표 원용).

### T2 jk::net ConnectTcp + HTTP POST (로컬 leg)
- `ConnectTcp(bindIp 없음 — host 문자열 getaddrinfo, port)` win32+posix 쌍.
- JKLlmTurnHttp: `HttpPostPlain(host, port, path, bodyJson, timeoutMs)`
  — HTTP/1.1 수동 조립(Content-Length·접두 없음 — 응답 본문 마지막 구간
  파스). 실패 = 정직 ""(소비자의 stub 폴백 계약과 동일).
- Win 실측: PC ollama 데몬 localhost:11434 live 1턴(주기: 데몬 유무 = 환경
  차 — 실패 원문도 영수증).

### T3 cloud leg(폰 JNI TLS 브리지)
- Java `JkHttp.java`: JNI `JkHttpPost(url, body)` —
  HttpsURLConnection + Bearer(api_key)·`Content-Type: application/json`
  ·타임아웃 60s·응답 본문 UTF-8 반환.
- jni 브리지: `JkLlmHttpPostCloud(url, body)` — JNIEnv
  (JkVoice 선례 env 취득 계약 재용), Android 이외 = 정직 빈 값.
- JKLlmTurnHttp의 transport = 함수 포인터 주입(순수 부품 계약 —
  selftest는 스턬 transport로 단정).

### T4 턴 배선
- LlmTurnThread: `engine=="ollama-http"` 분기 — 스폰 0·HTTP 턴 1회 →
  기존 Finish/turn job 계약 재용(정확히 한번 DoneFn).
- ChatRouter/ChatRouteTurn 무접촉(라우터 근사 매칭 우선 유지 — 원존).

### T5 폰 실측 (EYES — 사용자)
- local: PC 데몬 릴레이(ollama_host) /talk 턴 원문
- cloud: **API 키 사용자 준비**(미준비 = BLOCKED 유계 허용 — 플랜 원존)
- 로그 채널 = files/srv.err(원존).

### T6 docs+캐논 정산
- as-built 본 문서 갱신·Win/WSL/posix selftest 캐논 등호(캐논 숫자 이동 =
  계획된 신설).