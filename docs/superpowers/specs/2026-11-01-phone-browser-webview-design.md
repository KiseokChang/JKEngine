# 폰 브라우저 WebView 실증 라인 — 설계 (2026-11-01)

## 1. 결정 원존 (이 설계의 근거)

- **폰은 두 겹 구조** (탐사 원문 2026-11-01): DeX 데스크톱 = Termux 위 네이티브 jkdesktop(SDL),
  JK 앱(jkapk APK) = 별도 얇은 클라이언트(MainActivity → POST localhost:8090/talk).
  WebView는 Android View라 **Termux 네이티브 창에 심을 수 없다** → 브라우저는 **jkapk 안의
  새 Activity(BrowserActivity)**로 성립, DeX에서는 별도 Android 윈도우로 뜬다.
- Windows 쪽 브라우저(jkapp_browser)는 Windows 전용 CEF 배포판 링크 — 폰에 재용 불가
  (engine/CMakeLists.txt:890 `WIN32` 게이트, docs/70 §4 원존).
- 후보 실측 조사(WebSearch 2026-11-01): Android WebView — androidx.webkit 1.12+ 패스키
  opt-in·1.14+ Payment Request opt-in(WebView 137+ Google Pay 네이티브 시트);
  GeckoView — Credential Manager 패스키 네이티브(Android 14+), 4주 릴리스 활성 유지.
- **엔진 결정은 감이 아니라 실측으로** — 같은 테스트를 양쪽에 돌려 측정표 원문으로 판정한다
  (사용자 지시 "실증적 비교가 필요하겠네요" → 쭉쭉 진행).

## 2. 라인 구조 (T 사다리)

| T | 내용 | 성립 기준 |
|---|---|---|
| **T1** | BrowserActivity WebView 뼈대(주소창+뒤/앞/새로고침+Go) | gradle assembleDebug PASS + 폰에서 페이지 렌더 |
| **T2** | 측정 하니스 — [측정] 버튼·asset 페이지 세트·타이밍 수집·JSON dump | 측정 JSON 산출(md5 관문 수확) |
| **T3** | androidx.webkit opt-in 봉입(패스키 WebAuthn FOR_APP+원격 디버깅) | 설정 적용 실측 표기 |
| **T4** | 배포 probe `phone_browser.sh`(배포+측정 JSON 수확, 설치는 사용자 수수) | probe 게이트 전원 green |
| **T5** | GeckoView 교체판 — [GeckoView]/[WebView] 토글·동일 측정 하니스 | 양쪽 측정 JSON 쌍 산출 |
| **T6** | 측정표 원문 수확 + docs/100 as-built + 판정 게이트 | 측정표 표 사용자 결정 대기 |

- 인증 실축(웹상 패키 낭인)·체감 = 사용자 EYES 게이트(수수 판정).
- **결제 축은 이 라인에서 실축하지 않는다** — 결제는 원칙상 Custom Tabs 홉아웃 구조 원장행
  (별도 라인)이고, 에이전트 자율 결제는 금지(사람 승인 하드게이트 원칙 원존).

## 3. 측정 축 (판정 게이트 5개)

1. 조달·빌드 공수 — gradle resolve 시간·APK 크기(양쪽 실측)
2. 체감 성능 — asset 오프라인 페이지 3종 + example.com: 로드ms·first-contentms
3. 에이전트 표면 — 페이지 개봉→최종 URL·제목·링크 10개 JSON 회독(WebView=evaluateJavascript,
   GeckoView=HistoryDelegate/ProgressDelegate — 링크 회독 갭도 판정 자료로 정직 기록)
4. 인증 실측 준비 — webauthn.io 개봉 가능(실 낭인 패스키는 EYES)
5. 기타 — 원격 디버깅(CDP) 개방 표기(WebView 전용 — 에이전트 연결 계약 후보)

## 4. 파운 문화 변경 원존 (명시 기록)

- jkapk v1 "gradle 의존 신설 0(android framework+kotlin stdlib only)" 문화 원존
  (build.gradle.kts:103-106) — 본 라인이 **androidx.webkit(+Task T5 GeckoView)** 추가로
  문화 변경. 정당화: 비교 자체가 라인의 목적이고, webkit 라이브러리는 패스키 opt-in의
  유일 조달기. 얇아짐 원칙(D4 최소 산출)은 유지 — 의존은 2개 상한.
- APK/모델/SDK 산출물 무추적(D4) — 커밋 금지 유지.
- probe는 설치하지 않는다(D5) — 설치는 사용자 수수.

## 5. 라인 밖 (원장행 명시)

- DeX 그리드(jkdesktop 런처)에서 브라우저 아이콘 → console-app `am start` 래퍼 후보(별도 라인).
- /talk 연동(에이전트가 브라우저 URL 개봉 지시) 후보(별도 라인).
- 결제·Custom Tabs 홉아웃+사람 승인 하드게이트(별도 라인, 사용자 착수 지시 대기).