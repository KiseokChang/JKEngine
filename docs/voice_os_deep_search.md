# 구두 OS 조작 딥서치 (deep search) — 사례·원리·선례 전수 수집 (2026-10-26)

> 사용자 지시 2026-10-26: "구두로 OS 조작하는 사례를 다양하고 깊게 수집 — 토큰 아끼지 말고"
> 배경: docs/voice_os_manipulation_direction.md(S1-S8+v1.5/v2/v3)가 "단순하다"는
> 사용자 판정 → 본 문서는 상용·접근성·오픈소스·LLM 에이전트·폰·HCI·한국어 8축을
> 실측 검색(WebSearch 2026-10)으로 깊게 수집하고, 기존 방향(v1.5/v2/v3)을 재판정한다.
> 룰: 가짜 링크 금지 — **링크는 검색/패치로 실측된 것만** (§11). 추정 제품명은 "명칭만" 표기.

---

## ① 실행 요약 (10줄)

1. **접근성(보조공학)이 가장 깊은 사례원** — Windows Voice Access와 macOS Voice Control은 "사람의 몸(마우스·키보드) 전체"를 구두로 대체하는 완성 계통이 이미 존재(오프라인 동작 포함). 명령 문법·숫자 오버레이·3×3 그리드가 공식 문법으로 정립돼 있다(§2).
2. **"번호/그리드 오버레이"는 양사 공통 표준 UX** — 클릭 대상이 어휘로 안 잡히면 화면에 번호를 붙여 "클릭 3(3번 클릭)"으로 해소. JKEngine(capture+send_input)이 이미 갖춘 원존으로 도입 가능(§2·§9c).
3. **상용 어시스턴트는 "앱 인텐트 선언" 모델로 수렴** — Apple App Intents(iOS 18.4+ 사실상 유일 경로·WWDC25 Apple Intelligence 연결), Google은 Assistant → Gemini 완전 교체(2026-09, App Actions/shortcuts.xml만 잔존), Bixby는 Quick commands(2-3 동작 묶음)로 사용자 정의 어휘를 제공(§1).
4. **오픈소스는 "파이프라인+인텐트 파서 이층"이 표준** — OVOS(Adapt=키워드/Padatious=예제 신경망), Home Assistant Assist(HassIL 문법파서 + HassLLM 폴백·Wyoming 프로토콜), Rhasspy(FST=fsticuffs), Willow(ESP32 온디바이스 400명령 MultiNet — 발화완료→동작 500ms) — JKEngine의 "어휘 라우터 즉발+LLM 폴백"과 동형(§3).
5. **LLM 컴퓨터 사용 에이전트는 "보기→행동→피드백 루프"로 정착** — Anthropic computer use(스크린샷+좌표, Claude Sonnet 4.5 OSWorld 61.4%), Gemini 2.5 Computer Use(좌표 스케일 0-999+안전 결정 safety_decision), OpenAI Operator→ChatGPT agent(2025-07 통합)→ChatGPT Work(2026 이관)로 빠른 흥망 교체(§4).
6. **이층 라우팅의 정량 선례 확보** — Hyundai LLM ContextBridge(COLING 2025): Rule-NLU→소형 PLM분류기→LLM 정제 폴백에서 인텐트 정확도 0.896→0.917·멀티턴 +45%(0.152→0.600) — **LLM 단독 라우팅(0.636)보다 하이브리드가 우세** = JKEngine 구조의 학술적 정당화(§8d).
7. **지연 문턱 근거 확정** — Nielsen 0.1s/1s/10s + VUI 적응판(Cohen-Giangola-Balogh 2004): <1s 즉답, 1-5s는 인지("한순간만") 필수, >5s 이탈. JKEngine v1(0.2s 어휘·0.9-4s LLM)은 이에 부합 — TTS 응답 설계 문턱도 확보(§6).
8. **한국어 STT는 파인튠 전형이 명확** — KsponSpeech(60만 발화/1000시간) 파인튠로 Whisper large-v2 CER 19.6%→7.3%(2026-09 pyannote 발표·Echo 평균 4.7%); 상용 API(CLOVA Speech 스트리밍·ETRI 무료 1000건/일) 병행 가능. 국내 TV 사례(NUGU "아리아")는 한국어 어휘 표의 직접 원문(§7).
9. **보안 리스크 카탈로그 확보** — 스킬 스쿼팅(USENIX'18 — 92.6% 성공), 가짜 각성어 FakeWake(CCS'21), 초음파/레이저 주입(DolphinAttack·LightCommands 110m) — JKEngine 상시 청취(v3) 설계의 필수 가드(§9f).
10. **재판정(§10)**: v1.5 어휘 확장 = **승인 강화**(산업·학술 선례 다수 — 단, "문장 템플릿+와일드카드" 형식 확장 제안). v2 TTS/PC 릴레이 = 근거 확정(1s 문턱). v3 상시 청취 = 각성어 선례+보안 가드 조건부 승인. **신규 제안 v1.6 "번호 오버레이+확정/취소 어휘"** — 조달 소액·사용자 체감 최대 후보.

---

## ② §1 상용 구두 OS 조작 (제품 사례)

### 1.0 총론 — 상용은 3가지 모델로 분류된다

| 모델 | 구조 | 대표 | JKEngine 대응 |
|---|---|---|---|
| **개발자 인텐트 선언** | 앱이 기능을 인텐트로 노출→시스템이 발화를 매칭 | Apple App Intents·Google App Actions(shortcuts.xml) | 도구허브=인텐트 선언에 대응(docs/77 배지토큰 계약 동형) |
| **스크린 이해+GUI 원시동작** | LLM이 화면을 보고 클릭/타이핑 | Operator/ChatGPT agent·computer use·Mariner·Gemini CU | capture_window+send_input이 갖춘 원존 |
| **OS 접근성 명령 문법** | 사람 몸 전체를 구두 대체·화면 오버레이 | Voice Access·macOS Voice Control·Dragon | §2 — JKEngine에서 가장 배앓는 곧은 원문 |

### 1.1 Apple — Siri + App Intents + Apple Intelligence

- **App Intents**([developer.apple.com/documentation/appintents](https://developer.apple.com/documentation/appintents)) — 앱 기능(엔티티·파라미터·바로가기)을 선언해 Siri/Spotlight/단축어/Apple Intelligence/액션 버튼에 노출. iOS 16+ 현행 Swift API.
- **iOS 18.4+에서 App Intents가 사실상 유일 경로**로 정착(구 NSUserActivity/Intents 프레임워크는 iOS 13 이후 표기 폐지 경로) — Siri가 앱 결과를 기기 내 시각 인터페이스로 즉시 보여줘 응답 개선(9to5Mac 2025-04 실측 보고).
- **WWDC 2025(2026-10 기준 최신 축)**: 앱 엔티티를 Apple Intelligence에 넘기는 generative functions·서드파티 "rich actions"를 시스템 지능이 문맥으로 소개 — 데모: "Uber로 평소 대로 주문해" → Apple Intelligence가 Threads에 여정 공유를 제안하는 다앱 흐름. "Intelligent Actions"는 기기 내 요약 LLM이 자연語 요청의 날짜·가격을 해석.
- **현안**: 완전 AI 재설계 Siri는 iOS 26.4(2016-03 예상)로 지연 보고(Bloomberg Gurman) — **구두 OS 조작은 프레임워크 선언이 먼저, 모델 나중**이라는 순서 원문.
- 원리: **"자연어 요청 → 앱 선언 인텐트 매칭 → 앱 내 동작"** — JKEngine의 도구 허브(launch/close/capture...)가 곧 App Intents에 해당.

### 1.2 Android/Google — Assistant 소멸 → Gemini

- **Conversational Actions는 2023-06-13 서비스 종료(쏜다운데이)**([CA sunset](https://developers.google.com/assistant/ca-sunset)) — 개발자 커스텀 대화앱은 소멸, App Actions·Smart Home·Media Actions만 생존([App Actions 정책](https://developers.google.com/assistant/console/policies/app-actions-policies)).
- **2026-09-04부터 Google Assistant 삭제 개시** → [말기 완전 교체](https://9to5google.com/2026/09/28/google-assistant-gemini-android/)([개시 보고](https://9to5google.com/2026/08/04/google-assistant-september-2026-shutdown/)) — 폰·태블릿·Wear OS·이어폰·Android Auto(폰 투사)까지. 되돌림 설정 소멸. Nest/자동차/Google TV는 유예 중.
- 개발자 방향: [App Actions/shortcuts.xml로 Android 음성 상호작용](https://developers.googleblog.com/en/helping-developers-create-meaningful-voice-interactions-with-android/) — **"built-in intents(시스템 제공 어휘) + 사용자 단축어"**가 Android의 잔존 구두 표준.
- 원리: 어시스턴트 엔진은 계속 소멸/교체되지만 **인텐트 선언 계약은 생존** — JKEngine도 "엔진(ollama 모델)은 갈아끼우고 계약(도구 JSON)은 고정" 원칙과 동형.
- Android Auto 실측 어휘(검색 요약): "가장 가까운 주유소로 안내해"·"내 메시지 읽어줘"·"Tune to 98.7 FM"·"에어컨 켜줘"(Google built-in 차량) — Gemini 전환 후 **후속 정제("미국 그건 Prewitt에 있는 걸로")**의 멀티턴이 강조된 것.

### 1.3 Samsung — Bixby Quick commands·Routines·DeX 조합

- **Quick commands** — 사용자가 발화 트리거("눈 아픈데")를 만들고 2-3 동작 묶음("밝기 최대+블루라이트 필터")에 묶는 기능(Bixby 설정→Quick commands. Routines 양쪽에서 마이그레이션 제공). **"사용자 정의 어휘→복수 동작"의 상용 가장 단순 원문.**
- **Routines(Modes and Routines)** — if(시간·위치·Wi-Fi) then(앱 실행·설정) 계약 — **음성 기동 조건도 시작 트리거로 등록 가능**(자율 v3의 트리거 계약 선례).
- **DeX+Bixby**: [Samsung DeX 문서](https://developer.samsung.com/technologies/dex) — DeX 내 Bixby 패널에서 "DeX로 전환해줘"·"새 창에서 열어줘"·"창 닫아줘" 등 **데스크톱 셸 창 조작 어휘** 실측. JKEngine 폰 DeX 축에서 동일 도메인이 성립한 것(docs/91 S1)과 정렬.

### 1.4 Windows — Copilot 음성 + Voice Access

- **Voice Access**(Win11 22H2+, 오프라인, 6개국어 — 한국어 미지원) — §2.1에서 상세. **"무인 컴퓨터 조작"의 공식 답변**인 것은 Copilot 음성이 아닌 접근성 쪽.
- 로그인(23H2+) 지원 — **재시작 직후부터 구두 완결**이 중요 설계 요소(로그인 화면 접근).

### 1.5 macOS — Siri + Voice Control

- §2.2 상세. Voice Control는 오프라인·커스텀 어휘·오버레이 3종(번호·이름·그리드).

### 1.6 차량 — CarPlay/Android Auto/Automotive

- Android Auto: 휠 버튼 롱프레스·"Hey Google"·마이크 아이콘 3입구 — **손대지 못하는 도메인에서는 접입구 복수가 표준**. 내비·전화·메시지 읽기·미디어·(Google built-in) 공조 제어.
- Gemini for Android Auto/Automotive(2025년말~): 멀티턴 정제("아까 그거 대신") — 조작 연속성이 단발 명령에서 문맥 연속으로 이동하는 사례.

### 1.7 TV / 워치

- **TV**: Alexa built-in TV — "Alexa, Netflix 열어줘"·"30초 뒤로"·"HDMI 1로 바꿔줘"·(Echo에서) "거실 TV 꺼줘"·"보헤미안 랩소디 Fire TV에서 틀어줘" — **기기 간 지정("on [TV]")**이 어휘 문법의 인자형. Alexa+(2025)는 대화 기억("난 SF 좋아해")+자연 발견("톰 행크스 나온 것 봐줘").
- **국내 TV**: §7.3 NUGU/B tv 원문.
- **워치**(Apple): [Siri로 시계·홈 제어](https://support.apple.com/guide/watch/control-apple-watch-smart-home-devices-apd30bcf523b/watchos) — "'읽기 씬' 설정해줘"·"사무실 불 꺼줘"·"현관 잠겼어?"·"위층에 '장 간다' 알려줘" — **씬(scene)이라는 중간 인자형**(복수 기기 묶음·발화 짧음). [씬 자동화](https://support.apple.com/en-us/102313)는 시간·위치·센서 트리거 계약 — **v3 트리거 계약의 원존**. 문 잠금 같은 보안 동작은 기기 잠금 해제가 선행 요구([원격 요구사항](https://support.apple.com/guide/watch/remotely-access-your-smart-home-accessories-apd1af68cb2a/watchos)) — **고위험 동작 게이트 선례**.

---

## ③ §2 접근성/보조공학 — 가장 깊은 사례원 (문법 실측)

### 2.1 Windows Voice Access (문법 원문)

공식: [명령 목록](https://support.microsoft.com/en-us/accessibility/windows/voice-access/voice-access-command-list)·[마우스](https://support.microsoft.com/en-us/accessibility/windows/voice-access/use-the-mouse-with-voice)·[화면 항목](https://support.microsoft.com/en-us/accessibility/windows/voice-access/use-voice-to-interact-with-items-on-the-screen)·[FAQ](https://support.microsoft.com/en-us/accessibility/windows/voice-access/voice-access-frequently-asked-questions-faqs)·[다중 모니터](https://support.microsoft.com/en-US/accessibility/windows/voice-access/use-voice-access-on-a-multi-display-setup)

**문법 5카테고리(공식 분류)** — JKEngine 어휘 표의 분류틀로 그대로 쓸 수 있다:

| 카테고리 | 실측 예시 | 기능 |
|---|---|---|
| 기동/제어 | "Voice access wake up"·"What can I say" | 각성·가능 명령 문의 |
| 앱·창 | "Open [app]"·"Switch to [app]"·**"Snap window to [direction]"**·"Show all windows" | launch/focus/list 동형 |
| 컨트롤 | "Click [name]"·"Focus on [name]"·"Scroll [direction]"·"Move brightness right five times" | 컨트롤 지정+반복 인자 |
| 마우스/키보드 | "Press A"·"Double-click"·"Right-click"·"Move mouse up" | 몸 대체 원시동작 |
| 텍스트 | "Select previous three words"·"Bold that" | dictation/편집/포맷 |

**모드 3종**: Commands / Dictation / Default — **조작 모드와 입력 모드의 분리**가 원리.

**숫자 오버레이(실측 문법)**:
- "Show numbers"(전체)·"Show numbers here"·"Show numbers on [app]"·"Show numbers on taskbar"
- "Click three" 또는 그냥 "Three" — **성공 상호작용 후 자동 소거**·"Hide numbers"/"Cancel"
- **부분 매칭**: "Click privacy"가 "Privacy & security"에 통과. 특수문자·하이픈 처리 실측("Click dial up").

**그리드 오버레이(실측 문법)**:
- "Show grid" → 3×3 → "Show grid here"·"Show window grid"(활성 창 한정)
- 번호만 발화 = 해당 타일로 **재분할(드릴다운)** — "Mouse grid [n][n]..."으로 다층 연속 지정. 다중 모니터는 디스플레이 문자 접두("Mouse grid A 114")
- "Undo"(한층 복귀)·"Hide grid"
- **드래그 계약**: "Mark [n]"(원점)→드릴→"Drag"(도착) — 2단계 마킹 프로토콜. 
- 공식 한계 원문: 주 모니터 외 포커스에서 오류 가능 — **오버레이도 도메인 특화 취약점이 있다**.

**기동어 설계**: "Voice access wake up"은 Dragon·기존 Windows Speech Recognition과 **의도적으로 전혀 안 겹치는 3단어** — 충돌 회피 기계어 선례.

### 2.2 macOS Voice Control

공식: [명령](https://support.apple.com/guide/accessibility-mac/control-your-mac-and-apps-using-voice-control-mh40719/12.0/mac/12.0)·[시작](https://support.apple.com/en-ie/102225)·[명령 상호작용 가이드](https://support.apple.com/guide/accessibility-mac/use-voice-control-commands-mh40719/mac) + [thirdparty 튜토리얼](https://eshop.macsales.com/blog/92455-how-to-enable-and-use-voice-control-in-macos-sonoma/)

- **그리드**: "Show grid"(화면)/"Show window grid"(창) — **클릭 가능 컨트롤이 없는 임의 픽셀**을 목표로 설계. 번호만 발화=드릴다운(재분할), "Click [n]"=클릭+그리드 자동 소거, "Zoom [n]"·"Drag [n] to [m]".
- **더 이상 분할 불가한 최소 타일**에선 번호만 말해도 클릭 — **드릴 상태가 자발적으로 클릭으로 전환**되는 종료 규칙이 설계 포인트.
- **오버레이 3종**: 숫자("Show numbers")·이름("Show names", Sonoma+)·그리드 — 개인 설계 정책을 커버리지에 맞춰 고르게 함.
- **명령 모드 분리**: Dictation/Spelling/Command·"Show commands"로 가능 명령 표시 — **문법 자체를 화면에 표시**하는 정직성 원존(WYSIWYS — What You Say Is What You see).
- "Go to sleep"/"Wake up"·"Stop listening" — 세션 수명 계약.
- 탭 간 ~0.5초 이격 권고(공식) — 연속 명령의 운율 문턱 실측값.

### 2.3 Dragon (NaturalSpeaking) — 커스텀 명령 산업 표준

공식: [MyCommands 개요](https://www.nuance.com/products/help/dragon15/dragon-for-pc/enx/professionalindividual/content/MyCommands/mycmds_1/mycommands_overview.htm)·[KB 6895](https://nuance.custhelp.com/app/answers/detail/a_id/6895/~/how-to-create-a-custom-command-in-dragon-naturallyspeaking)·[열린 명령](https://www.nuance.com/products/help/dragon/dragon-for-pc/scriptref/Content/mycommands/mycmds_2/open_ended_macros.htm)·[고급 스크립트 PDF](https://www.mhsinformatics.org/AHLTA-Files/Dragon-Files-not-Macros_As-U-Type/l-3177-001%20Advanced%20Scripting.pdf)·[워크북](https://dragon.nuance.com/shared/user-docs/gd-dragon-professional-individual-and-dragon-legal-individual-workbook-en-us.pdf)

- **MyCommands 4종** — JKEngine 도구 분류와 놀라운 동형:
  1. Auto-text(텍스트 삽입)  2. **Macro Recorder**(마우스·키스트로크 반복)  3. **Step-by-Step**(메뉴·키스트로크 사슬+앱 실행+마이크 토글)  4. **Advanced Scripting**(VBA로 임의 기능 — SendKeys·Wait·HeardWord(다른 음성 명령 호출))
- **열린 명령(open-ended)**: 명령 이름 끝에 임의 딕테이션 인자("search company for <dictation>") — **발화 인자의 LLM 무요한 슬롯 채움**의 20년 선례.
- **Command Browser + nsadmin 배포**(.dat 내보내기·기업 전개) — **어휘 표의 배포/버전 관리**가 산업에서 완성된 영역.
- **Natural Language Commands**(워크북) — "창 전환·메뉴 클릭" 같은 자유 서술이 문서형 어휘가 아니라 **서술형 문법**으로 허용되는 상용 가장 늘어난 폼.

### 2.4 기타

- **Voice Finger**(Windows, 마우스·타이핑 구두 대체, 번호 기반)·**Tazti**(100+ 기본 명령)·**e-Speaking**(SAPI 기반 커맨드 콘솔) — 명칭만(2026-10 검색에서 웹 실측·링크 미확보). Voice Access의 무료 공식화 이후 후보 계열이 대체로 소멸한 것 자체가 **공식 무료화가 생태계를 흡수**했다는 시장 원문.
- **Windows Speech Recognition**(구세대, SAPI 4/5 문법 .grxml) — Voice Access의 직전 세대. "어쩔 수 없이 Windows Speech Recognition를 썼다"는 2026 검색에서도 언급되는 현역 사용 실측 — **한국어 지원이 이 세대에만 있었던 것**이 한국어 파이프라인의 유의점.

### 2.5 접근성 섹션의 핵심 원리 요약 (JKEngine 이식용)

1. **어휘가 안 잡히면 번호로** — 양사 공통 폴백. 클릭 가능 컨트롤 인식률 100% 불가를 정직하게 인정한 설계.
2. **드릴다운 재분할** — 한 번의 발화로 정밀위를 못 잡으면 상호작용을 분할해도 괜찮다(공식 문법).
3. **오버레이는 자발 소거** — 성공 후 자동 제거 = **조작의 성공 신호가 피드백**인 설계.
4. **모드 분리** — "명령만"/"받아쓰기만"이 혼잡 오류를 줄인다.
5. **기동어는 유니크 문구** — 충돌(다른 어시스턴트·다른 명령)을 설계 단계에서 제거.
6. **전 화면 대치가 되어야 "OS"다** — 특정 앱 명령이 아니라 **몸 전체 인터페이스**를 말하는 것(JKEngine 목표와 직접 정렬).

---

## ④ §3 오픈소스 음성 에이전트 시스템 (인텐트 파서 비교)

### 3.1 OVOS(Mycroft 계승)

공식: [스킬 구조](https://openvoiceos.github.io/ovos-technical-manual/401-skill_structure/)·[인텐트](https://openvoiceos.github.io/ovos-technical-manual/399-intents/)·[인텐트 설계](https://openvoiceos.github.io/ovos-technical-manual/403-intents/)·[Mycroft 문서](https://mycroft-ai.gitbook.io/docs/skill-development/skill-structure)·[마이그레이션](https://openvoiceos.github.io/beta-technical-manual/migrating-from-mycroft/)

- **스킬 구조**(원문): `locale/<lang>/.dialog`(응답 문장무작위)·`.intent`(Padatious 예제)·`.voc`(Adapt 키워드 동의어)·`.entity`·`.rx`(정규식 엔티티) — **어휘=파일**·코드=__init__.py 분리.
- **인텐트 파서 2종 비교(원문 표 요약)**:

| | Adapt | Padatious |
|---|---|---|
| 방식 | 키워드/룰(IntentBuilder) | 소형 신경망·예제문(.intent) |
| 강점 | 유연·문맥(converse) 연계 | 정확도·엔티티 추출 간단·나쁜 인텐트 만들기 어려움 |
| 약점 | 잘못 만들면 오매치 쉬움 | 예제를 풍부하게 필요·숫자를 `#`로만 취급 |

- OVOS는 **파이프라인 프레임워크로 정체** — 여러 파서를 병렬/직렬 실행 가능하게 재설계(MycroftSkill→OVOSSkill·단일 @intent_handler 통합·backendless). **"어휘 파서를 하나 선택"이 아니라 "사다리"로 간다**는 산업 흐름 원문.

### 3.2 Home Assistant Assist

공식: [Assist 개요](https://www.home-assistant.io/dashboards/assist/)·[개발자 문서](https://developers-voice.home-assistant.io/docs/voice/overview/)·[Wyoming](https://github.com/rhasspy/wyoming)·[ADR-0017](https://github.com/home-assistant/architecture/blob/master/adr/0017-assist-pipeline.md)·[Year of the Voice Ch.2 블로그](https://www.home-assistant.io/blog/2023/04/27/year-of-the-voice-chapter-2/)

- **파이프라인**: wake(openWakeWord·microWakeWord — ESP32-S3 온디바이스 각성 ~200ms)→STT(wyoming-faster-whisper)→인텐트(HassIL 문법파서+HA 문장 템플릿)→동작→TTS(Piper).
- **Wyoming** = 소켓 JSON+오디오 청크·버전 헤더+info 메시지 공고 — **"스테이지 교체 가능한 소켓 계약"** 설계. JKEngine의 서버↦클라 계약(docs/90 등)과 직접 비교 가능.
- **LLM 폴백 공식화**: HassLLM — 로컬(Ollama)·클라우드로 노출된 기기/영역 목록을 템플릿에 넣어 LLM 턴. 사용자 커스텀 문장(YAML intents: 패턴·와일드카드·리스트·대안·정규식)은 **기본 문장 템플릿과 합쳐서** 파싱 — **"문법 파서 + LLM"이 공식 2층**인 것.
- 파이프라인 스테이지는 이벤트 큐+asyncio 상태머신·websocket run API — 소켓 경합/세션 정리 계약이 정식화돼 있어(ADR-0017) JKEngine의 세션 관리 원리 참조.

### 3.3 Rhasspy / Willow (로컬 폰·임베디드)

공식: [Rhasspy 백서](https://rhasspy.readthedocs.io/en/latest/whitepaper/)·[Willow 동작 원리](https://heywillow.org/how-willow-works/)·[Willow 로컬 명령](https://heywillow.org/features/Local-Command-Support/)·[Willow 이슈 #446](https://github.com/HeyWillow/willow/issues/446)

- **Rhasspy**: 완전 오프라인→**JSON 인텐트 이벤트** 출력. 파서 **fsticuffs** = 템플릿을 **유한상태전이(FST)**로 컴파일 → 문장을 인텐트+슬롯으로 정확매치. 정지어 건너뛰기 퍼지 매치·언어모델 혼합으로 반자유 인식. Rhasspy 3는 컨테이너 파이프라인 리셋.
- **Willow**(ESP32-S3-BOX, ~$50): **발화완료→동작 500ms 이하·실패율 <1% 실측** — 빠른 로컬 파이프라인 수치 원문. 로컬 모드는 **Espressif MultiNet 온디바이스 최대 400명령**(TURN ON/OFF 문법 한정 — **정확매치 어휘 사전 방식**)·서버 모드는 WIS(Whiper+TTS+LLM·WebRTC). 각성은 openWakeWord의 ESP32 측 수행(서버 미담당 — 이슈 #446 원문)·컨테이너만으로는 Rhasspy 권고.

### 3.4 Leon AI·기타

공식: [getleon.ai](https://getleon.ai)·[leon-ai/leon](https://github.com/leon-ai/leon)·[docs](https://docs.getleon.ai)

- 브레인 모듈이 텍스트/오디오→인텐트 분류→**액션 큐**로 스킬 실행. 스킬은 Node/Python SDK·`config/<lang>.json`에 문장/엔티티 정의 + `action.json`. 내장 예제 스킬에 **Spotify play/pause/skip/volume(재생계 어휘 원문)** — §8b 어휘 표의 직접 참조.
- 2025 로드맵: openWakeWord 도커·LLM 스킬 — 소형 프로젝트도 어휘+LLM 이층으로 수렴.

### 3.5 인텐트 파서 구조 비교 (어휘→의도 4유형 종합표)

| 유형 | 방식 | 대표 | 지연 | 오류 성격 |
|---|---|---|---|---|
| **정확매치(어휘/문형)** | 문장 템플릿·FST·regex 튜플 | HA HassIL·Rhasspy fsticuffs·Willow MultiNet·JKEngine 어휘 라우터 | ~0.2s 이하 | 문법 밖 전부 미스(정직) |
| **예제 신경망(작다)** | 소형 분류기·Padatious | Padatious(HA·OVOS) | 수십 ms | 예제 밖 흐름·숫자 취약 |
| **문맥 규칙(키워드 슬롯)** | 키워드+요구 슬롯 조합 | Adapt· Dragon open-ended | 수십 ms | 오매치 위험(오버 럭키) |
| **LLM(오픈/리모트)** | 자연어→도구 JSON | HassLLM·AutoVoice Natural(로컬 LLM beta)·ChatRouteTurn | 0.9-4s+(JKEngine 실측) | 유연·지연·환각 — **확정 계약(JSON 스키마) 필수** |

- **산업 수렴**: 앞 3층(빠른 확정매치)으로 상황 90% 이상 소화·LLM은 폴백/정제(HA 공식·AutoVoice·C.O.D.A — [mattordev/coda](https://github.com/mattordev/coda) — **위험도 기반 로컬/클라우드 라우팅**까지 실장).

### 3.6 스트리밍 STT 전형 (whisper.cpp/faster-whisper/VAD)

실측 조사 원문(검색 수취):

- **whisper.cpp** PR #3653([whisper-stream-pcm](https://github.com/ggml-org/whisper.cpp/pull/3653)) — stdin PCM 스트리밍·에너지+Silero VAD 2종·f32le/s16le 16k mono. JKEngine이 whisper.cpp라면 **파이프 연결만으로 스트리밍 전환 가능**.
- **faster-whisper 백엔드**([v3DJG6GL](https://github.com/v3DJG6GL/faster-whisper-backend-archive/blob/main/README.md)) — WS 스트리밍 + **LocalAgreement-2 안정화**(부분결과 뒤바뀜 억제) + 2층 VAD(Silero/에너지) 엔드포인팅.
- 실설계 원문([zackees/transcribe-anything #124](https://github.com/zackees/transcribe-anything/issues/124)) — Silero VAD 30ms 홉·200ms 안정 침묵 청킹·28s 강제 분할·부분 디코드 200ms·**p50 목표 ≤1000ms** ·Silero가 webrtcvad 대신 선호(경계 정밀) — whisperX 등도 같은 선택.
- **실전 딕테이션 템플릿**([faster-whisper-dictation](https://github.com/bhargavchippada/faster-whisper-dictation)) — 마이크→Silero→WhisperLiveKit→포커스 앱 타이핑. 실측 항목 원문: VAD threshold 0.6-0.8로 무응답 환각 억제·침묵 중 "Thank you" 반복 루프 방지용 Voice Activity Controller — **VAD 파라미터 튜닝의 실측 정량치**.
- Silero VAD 내장 whisper.cpp 랩퍼([qvac 문서](https://docs.qvac.tether.io/ecosystem/addons/transcription-whispercpp/)) — threshold·min_speech/silence_duration_ms·max_speech_duration_s 튜닝 인자.

---

## ⑤ §4 LLM 컴퓨터 사용 에이전트(2024-26 동향)

### 4.1 Anthropic computer use

공식: [기사](https://www.anthropic.com/news/developing-computer-use)(2024-10-22 실측 패치) — API 루프를 docs.anthropic.com computer-use(현 platform.claude.com 계열)로 참조.

- **루프 구조**: `computer` 도구(display_width_px 등) 선언 → Claude가 tool call(좌표 클릭·스크린샷요청)→실행기(PyAutoGUI/VM)→**스크린샷을 tool result로 되먹임**→반복. **Claude는 스크린샷을 자발찍지 않는다 — 실행기가 찍어 되먹이는 계약** — JKEngine capture_window+send_input의 소유자 분리와 동형.
- 좌표계: 구버전 0-1000 정규화 → 2025-01-24부터 픽셀. **표현 차이가 계약을 뒤집는** 실측 요소.
- OSWorld: Sonnet 4.5 **61.4%**(이전 22%) — 그래도 "인간 대비 ~50% 낮은 신뢰성" 공식 표기 + 스크린샷 왕복 지연이 느림의 주원인 + **프롬프트 주입 위험**(본 화면에 명령 문백이 몰아넣는 것) — 샌드 박스+민감데이터 배제 권고.
- 파생: Claude Desktop은 위 레벨이 아니라 도구 호출(mcp 등)로 PC를 다룬다 — **"화면 이해"와 "도구 호출"은 다른 계약 층**이라는 실측.

### 4.2 OpenAI — Operator → ChatGPT agent → ChatGPT Work

공식: [Operator 소개](https://openai.com/index/introducing-operator/)(2025-01-23)·[ChatGPT agent](https://openai.com/index/introducing-chatgpt-agent/)(2025-07-17)·[시스템 카드](https://openai.com/index/chatgpt-agent-system-card/)·[도움말](https://help.openai.com/en/articles/11752874-agent)·[릴리스노트](https://help.openai.com/en/articles/11794368-chatgpt-agent-release-notes)

- Operator: CUA(Computer-Using Agent — GPT-4o 시각+RL 훈련 GUI 조작) 연구 프리뷰. 브라우저에서 폼/구매/반복 태스크.
- **가드 런웨이(원문 총정리)** — JKEngine 안전 게이트 설계의 가장 완전한 선례:
  1. **Takeover mode** — 로그인·결제 등 인증 절차는 사용자가 직접(모두 손뗀 상태 아님)
  2. 주요 동작 전 **사용자 확인(confirmations)**·은행 거래 거부
  3. 민감 사이트에서 **watch 모드**·프롬프트 주입 방어
  4. 파트너(DoorDash·Instacart·OpenTable·Priceline·StubHub·Thumbtack·Uber·Stockton)와의 화이트리스트 협력 — **"전 사이트 허용"이 아니라 협력 도메인 우선**.
- ChatGPT agent(2025-07): Operator+Deep Research+텍스트 브라우저+터미널+API 커넥터 통합·**사용자 중단/사용자 인계/정지**를 상시 가능·HLE 41.6% 등 벤치. 2026 확인: [도움말 현재](https://help.openai.com/en/articles/11752874-agent) "agent mode 없음→ChatGPT Work+클라우드 브라우저로 이관" — **1년 반 만에 제품명 교체**. 교훈: **계약(도구·권한·확인)을 제품에 묶지 말고 계층에 고정**.

### 4.3 Google — Project Mariner → Gemini Agent

공식: [Mariner 배포(TechCrunch)](https://techcrunch.com/2025/05/20/google-rolls-out-project-mariner-its-web-browsing-ai-agent/)·[중단(The Verge)](https://www.theverge.com/tech/925559/google-project-mariner-shut-down)·[Agent Mode 발표(The Verge)](https://www.theverge.com/news/670848/google-agent-mode-gemini-app-project-mariner-i-o-2025)·[Gemini 2.5 CU(9to5google)](https://9to5google.com/2025/10/07/gemini-2-5-computer-use-model/)

- Mariner(2024-12 출시): 픽셀/텍스트/코드/이미지/폼 이해+타이핑/스크롤/클릭. I/O 2025: 클라우드 VM 병행 10태스크·AI Ultra($249.99/월)·**Teach and Repeat(한번 보여주면 학습해 반복)** — **사용자 데모가 계약이 되는 프로그램적 인자형**. 
- **2026-05-04 중단** → 기술은 Gemini Agent·AI Mode(Serach)·Chrome auto-browse로 흡수 — 개인 실험 제품 역시 폐곡이 빠르다.
- **Gemini 2.5 Computer Use**(2025-10-07): WebVoyager **88.9%**(self-reported, Browserbase 하네스로는 79.9% — 평가 차 분석 필요)·Online-Mind2Web 최고 품질+**최저 지연**. AndroidWorld **69.7%**(접근성 트리 없이 스크린샷 만)·데스크톱 OS 제어는 비목표(브라우저 최적화) — **"GUI 에이전트 = 화면 픽셀만"** 분기가 명확한 반원. 도구 액션: click/double_click/triple_click/right_click/drag_and_drop/scroll/move/long_press + type/press_key/hotkey + wait/take_screenshot/open_app/list_apps — 좌표는 0-999 정규화. **safety_decision** 필드로 단계별 사용자 게이트 필존.

### 4.4 Open Interpreter·Adept ACT-1

공식: [Open Interpreter](https://github.com/OpenInterpreter/open-interpreter/)·[The New Computer Update](https://changes.openinterpreter.com/log/the-new-computer-update)·[01 프로젝트](https://01.openinterpreter.com/getting-started/introduction)·[ACT-1](https://www.adept.ai/act)

- Open Interpreter: 코드 우선(파이/JS/셸 실행) + **Computer API**(`computer.display.view()·mouse.click("화면 텍스트")·keyboard`) + **OS Mode**(`interpreter --os` — 로컬 비전 모델로도 GUI 제어) + Open Procedures(RAG 코드 힌트) + 01 하드웨어(음성 언어모델 컴퓨터). **현재는 Rust 기반 코딩 에이전트로 재설계**(Codex 포크·Kimi/DeepSeek/GLM 등 절값 모델 타깃) — 소프트웨어도 끊임없이 재탄생한다는 실측.
- ACT-1(2022-09): "행동 트랜스포머" — Chrome 확장으로 화면을 보고 클릭/타이핑/스크롤. **"한 문장 = 10+ 클릭 대체"(Salesforce 케이스)·1회 인간 피드백 자기교정** — 사람 뜻의 원문(명령 문백)→화면 동작 원문의 최초 공개 시공. 2024 팀 유출→Amazon 흡수·제품 사라짐 — **선례가 증발해도 관건(행동 기초모델)은 표준이 된다**는 시장 원문.

### 4.5 §4 총론 — JKEngine에 이식 가능한 5개 설계 결론

1. **보기→행동→피드백 루프는 계약** — 스크린샷 소유자(실행기)와 이해 주역(모델)을 반드시 분리.
2. **좌표계·액션 스키마를 버전으로 고정** — normalized/pixel 혼용의 실측 사고 존재.
3. **확정 게이트 3종** — takeover(직접 손)/confirm(주요 동작)/safety_decision(단계별) — JKEngine 도구 게이트 계약의 표기 산업 원존.
4. **프롬프트 주입 = 화면을 보는 에이전트의 고유 리스크** — 접근성 명령 문법(§2, 컨트롤 이름 매칭)이 LLM 화면 이해보다 주입에 강한 구조.
5. **제품 폐곡 빈도가 높다** — Operator/Mariner/ACT-1이 1-2년 내 교체·소멸 = **어휘·도구·계약은 자체 소유, 모델은 갈아끼움** 전략이 정답.

---

## ⑥ §5 폰 특화 (안드로이드 접근성 에이전트·폰→PC 원격)

### 5.1 Tasker+AutoVoice — 가장 큰 커뮤니티 사례

공식/커뮤니티: [AutoVoice Play](https://play.google.com/store/apps/details?id=com.newstand.autovoice)·[joaoapps natural 페이지](https://www.joaoapps.com/autovoice/natural.html)·[Natural BETA 발표(reddit r/tasker)](https://www.reddit.com/r/tasker/comments/1j3687t/autovoice_natural_beta_taskers_aipowered/)·[Natural Play beta](https://play.google.com/store/apps/details?id=com.newstand.autovoice.natural)·[커스텀 가이드(Medium 2018)](https://medium.com/the-phoenix-voice/how-to-make-real-custom-voice-commands-for-android-b2ab6c6b8c30)

- **전형**: Google Now("OK Google, ask AutoVoice to …")→AutoVoice 프로필(정규식 Command Filter — 명명 캡처 `(?<year>\d+)`로 인자 추출)→Tasker 동작 사슬. **정규식=인자 슬롯 채움**이 10년 라이브 수백만 유저 실측 원존.
- **Natural(기능 정지 — "현재 재작 중, 사용 불가" 공식 표기)** → **Natural BETA(2025-03)**: **llama.cpp 기반 완전 로컬 LLM**으로 자연문 인식 — 온디바이스·클라우드 없음. 관제 계약: Tasker 6.3+·AutoVoice 4.0b·Android 8.0+·유료 구독/Patreon. **"정규식→LLM" 전환을 같은 개발자가 10년 만에 수행**한 것 = §3.5 산업 수렴의 최소 단위 원문.
- **Voice Assistant 임포트 패키지**([joaoapps](https://www.joaoapps.com/autovoice/voiceassistant/)) — "Go to kitchen"·"Start Netflix"·"Turn the volume up"·"Weather in Lisbon" 등 자연문을 **미리 정의한 어휘 세트** — 어휘 표 후보 사례원.

### 5.2 접근성 서비스 조합 에이전트

- **KillerInGit/Accessibility-Agent**([github](https://github.com/KillerInGit/Accessibility-Agent)) — AutoVoice(음성)+AutoInput(화면 접근성 서비스로 컨트롤 직접 터치/타이핑)+AutoTools 라이선스 조합·인텐트 루팅 — **"음성 인텐트 → 접근성 서비스 동작"의 오픈구조 원문**.
- **AutoInput/Bixby의 접근성 원리**: 안드로이드는 ACCESSIBILITY 서비스로 타 앱 컨트롤 트리를 읽고 터치 대체 — **iOS는 인텐트 선언만, Android는 화면 통과도 가능**이라는 플랫폼 차이의 진원.
- **Samsung Routines+음성 기동**·**Bixby Quick commands**(§1.3): 폰에서 2-3동작 복합이 가장 쉬운 상용 원문.

### 5.3 폰이 PC를 조작하는 원격 사례 — JKEngine이 희소 지대인 이유

실측 검색 원문(§5 통검): **"폰의 발화를 RDP로 PC에 밀어넣는 온셸프 제품은 부재"** — 옵션 구성만 실측:

| 구성 | 실체 | 비고 |
|---|---|---|
| Unified Remote·Phone Link | 폰 앱→PC 마우스/키보드/미디어 | **음성이 아닌 터치 리모컨** |
| Voice PC($0.99, 영어만)·tazti·e-Speaking 등 | PC 측 구두 명령 | **PC 마이크 필요 — 폰 발화 불가** |
| Tasker→AutoHotkey 네트워크 이벤트 | 폰이 트리거, PC가 실행 | 커뮤니티 조립 — 지연·계약 불균형 |
| Windows Speech Recognition+Dragon | PC 측 완성 문법 | 한국어 불가 — Dragon 높은 코스트 |
| **JKEngine(폰 발화→서버 도구→PC 조작)** | **동일 구조를 구두로 완결** | S1-S6(실측 폰 기기 성립) |

- **판정**: "폰 구두→PC OS 조작"은 상용 옵션이 실제로 없는 사이 — JKEngine의 구두 표면이 상용 대비 희소가치는 **이 공백**에 뿌리(문서의 사라진 S7 저널·EYES 원존과 병행).

---

## ⑦ §6 HCI/VUI 설계 근거 (지연·피드백·멀티모달)

공식/문헌 실측:

- **Nielsen 3한계**([nngroup](https://www.nngroup.com/articles/response-times-3-important-limits/), 1993, 패치 실측): **0.1s** 즉반응(무피드백 OK) / **1.0s** 사고 흐름 유지(지연 가시) / **10s** 사용자 주의 유지(진행 표시 필존).
- **VUI 적응판**(Cohen·Giangola·Balogh 2004 *Voice User Interface Design*, 검색 실측 요약): <200ms 반사적 응답 / 200-500ms 질문 응딥 / 500ms-1s 잠깐 생각 / **1-5s 인지 피드백("한순간만") 필존** / **>5s 이탈**. 음성은 배경이 안 보이므로 **시각 UI보다 체감 지연이 더 느림** — 인지 즉시성 원존.
- 최신 권고(검색 실측): 총 응답 1s 이하 도구·대화 5s 상한·TTS 합성이 지연 예산을 크게 먹음 — **"구두 인정→동작→구두 답 3단계의 각 문턱"**를 분리해야 한다는 것.
- **멀티모달(음성+시각) 근거 원리**: Voice Access/macOS의 오버레이가 공식 증명 — **소리는 명령, 화면은 상태** 역할 분담. HA Assist 파이프라인에서 텍스트 답+동작 병행·Apple 시각 Siri(기기 내 결과 카드) — 모두 "명령 채널(마이크)과 상태 채널(화면) 분리"라는 같은 원리.
- **오인 판정 UX 선례**: Voice Access 부분 매칭("privacy"→"Privacy & security")은 **오인 시도-성공 표기 없이 넉넉한 매칭**, 반면 macOS는 명령 목록을 화면에 표시("Show commands") — **오류 정직 표기 vs 문법 안내**가 양상 두 축. JKEngine의 stub 폴백(매치 안 됨 정직)은 후자 계열 — VUI 연구상 유효(침묵 실패가 최악).
- **연속 명령 운율**: macOS 공식 ~0.5s 이격 권고 — JKEngine 발화 분리 조정(침묵 200ms 기준 §3.6)과 상호 참조.

---

## ⑧ §7 한국어 특화

### 7.1 한국어 STT 성능·전형

실측 검색 원문(2026-10):

- **Whisper 한국어 파인튠 효과(KsponSpeech·AIHub CER %)**:

| 모델 | KsponSpeech(제로) | KsponSpeech(파인튠) | AIHub 제로→파인튠 |
|---|---|---|---|
| large-v2 | 19.57 | **7.28** | 12.17→5.14 |
| medium | 20.31 | **8.45** | 11.70→6.06 |
| small | 43.19 | **13.91** | 42.21→11.31 |

- **pyannote 한국어 파인튠(2025-09 발표)** — [블로그](https://www.pyannote.ai/blog/whisper-ko): 감정 대화(AI Hub 15만)로 파인튠 → **KsponSpeech Echo 평균 CER 4.7%**(large-v3 24.3→2.9%(파인튠)·medium 29.9→5.9%·small 46.1→9.3%). Zeroth 대화체 기준 평균 12.6%.
- **참조 자료**: [폴가이 STT 파인튠 가이드(wikibook/polyglot-ko)](https://github.com/wikibook/polyglot-ko)·[soynlpdev small 파인튠 CER 3.35% 사례](https://soynlpdev.ai.kr/8)·[Vito 블로그](https://www.vitoengine.com/whisper-finetuning-1)·[AI Hub](https://aihub.or.kr)
- **핵심 실측 함정(공통)**: KsponSpeech 특유의 전사 규칙(잡음 태그/화자전환 마크)을 그대로 학습하면 **잡음 태그를 예측** — 전사 정화 후 학습 필존. CTranslate2로 FP16 전환 시 CPU 2-3배 — **CTranslate2가 JKEngine 서버 CPU 추론에 직접 조달**.
- **판정**: JKEngine whisper.cpp 현행 — 폰 실기기 성립(S1-S4). 폰 성능상 small/turbo급이면 한국어 파인튠 효과(small 43→14)가 **가장 큰 수익 구간** — 폰 내장 v2의 한국어 전략 근거.

### 7.2 한국어 상용 API

실측 검색 원문:

- **CLOVA Speech(네이버 클라우드)** — [ncloud](https://www.ncloud.com/api-cms/service-product/static/clovaSpeech)·[CSR 개발자 페이지](https://developers.naver.com/products/clova/vrecog/): 단문 REST(60s)/장문(2-6h)/**gRPC 실시간 스트리밍(한·영·일)**·키워드 부스팅(고유명사 향상)·이벤트 탐지·월 20분 무료. **키워드 부스팅은 명령 어휘(launch_app 같은)를 우선 인식하게 하는 직접 선례**.
- **ETRI 음성인식 API** — [e-PreTX](https://epretx.etri.re.kr/apiDetail?id=88)·[공공데이터포털](https://www.data.go.kr/data/15124653/openapi.do): 무료·건당 ≤20s·1,000건/일·REST JSON+base64·안드로이드 SDK([EtriOpenASR](https://github.com/EtrISLP/EtriOpenASR)) — **짧은 명령형 발화에는 무료 축이 직접**.

### 7.3 한국어 음성 명령 사례 (NUGU/B tv 원문 — 어휘 표의 직접 재료)

실측 원문: [B tv-NUGU 매뉴얼](https://www.nugu.co.kr/static/service/service01-02-04.html)·[NUGU](https://www.nugu.co.kr/)·[B tv AI2 가이드](https://www.tworld.co.kr/poc/html/download/Btv_AI2_User_Guide.pdf)·[SK 블로그](https://blog.skbroadband.com/2691)

- 기동어 "아리아" — **한국어 기동어 설계 선례(3음절·국문 고유단어·"알리사"와 유사한 충돌 회피)**.
- 명령어 원문(표) — JKEngine 한국어 어휘 표의 형식적 원존:
  - "TV 켜줘"·"볼륨 10"·"TV 음소거 해줘"·"13번 채널 틀어줘"·"JTBC 틀어줘" — **숫자·프로명 인자 슬롯**
  - "정우성 나온 무한도전 찾아줘"·"이 중에서 무료 영화만 찾아줘" — **문맥 정제(필터 인자)** — LLM 폴백 도메인
  - "리모컨 찾아줘" — 하드웨어 부속 동작 연결(리모컨에서 블루투스 소리)
- AI2 셋톱박스 원문: "대기모드에서도 NUGU 사용 가능"(TV 꺼져도 음성)·**나이트 모드(자정~7시 음성인식 해제)** — **상시 청취의 시간대 가드**를 상용에서 공식 제공 — v3 설계 직접 참조.
- B tv Smart 3: NUGU+Google Assistant **이중 음성 서비스** — 어시스턴트 이중화가 상용 가전에 소비자 단위로 제공된 실측.

### 7.4 한국어 커스텀 명령 실측 상황 요약

- Windows Voice Access 한국어 미지원(공식 FAQ 원문) → **한국어 접근성 구두 조작은 상용 공백**.
- Dragon 한국어 계열 미지원 상태가 일반 지견(검색에서 한국어 라인 미보고) → 명칭만.
- 국내 STT API(§7.2)+폰 음성 인식(AutoVoice 등)은 실측 — **한국어 문법 파서 문양(Ha-형 템플릿)은 오픈소스에 희소** — JKEngine이 형식을 스스로 정의해야 하는 진원(§9b).

---

## ⑨ §8 사례 카탈로그 표 — 조작 도메인 × 인자 유형 × 피드백 형태

8축 실측 사례를 3열로 분류(각 셀 = §참조):

| 조작 도메인 | 인자 유형 | 피드백 형태 |
|---|---|---|
| **앱/창 lifecycle**(launch·close·focus·list·snap) | 고유명사(앱명·창명) — 퍼지 매칭·번호 폴백 | 창 실제 개방/포커스+목록 표기 |
| **화면 프리시전**(클릭·드래그·드릴) | **번호(오버레이)·좌표(그리드 드릴)** | **오버레이 자발 소거(성공 신호)** |
| **입력(키보드·받아쓰기)** | 자유 텍스트 | 포커스 앱 입력 반영 |
| **미디어(재생·볼륨·채널)** | 정수(볼륨·채널)·이름(곡·프로) | 재생 상태+피드백 음성 |
| **기기/시스템 설정**(밝기·음소거·공조) | 정수·방향("right five times") | 설정값 변화 |
| **복합/씬**(Quick commands·씬·Routines) | 사용자 정의 발화 | 동작 사슬 결과 |
| **브라우저/작업계**(agent 모드) | **문장(의도)·멀티턴 정제** | 진행 카드·인계(takeover)·확인 |
| **트리거/자율**(Routines·자동화) | 조건(시간·위치·센서)+발화 | 조건 충족 표기·사일런트 |

- **패턴**: 인자가 **고유명사/정수**면 어휘+번호로 **즉발**(VUI 지연 문턱 1s 이내 — NUGU·Voice Access 전형). 인자가 **문장/조건**이면 LLM/자율로 **대기+인지**("한순간만"·진행 카드). **JKEngine 도구 허브의 인자 원존이 이 분류와 1:1** — 라우터 설계의 분류틀.

---

## ⑩ §9 JKEngine 매핑 (도구 허브 20종+에 바로 적어 넣을 것)

### 9a. 사례 카탈로그의 JKEngine 이식 순서 (체감 대비 조달)

| 순 | 이식 | 원존 | 비용 | 판정 근거 |
|---|---|---|---|---|
| 1 | 재생계 어휘(재생·정지·다음곡·볼륨 N) — v1.5 | vplayer 도구+NUGU/Leon/Alexa 어휘 원문 | 어휘 표 확장만 | S8 해소 — 가장 자주 요청될 도메인 |
| 2 | 창 세밀 어휘("오른쪽으로 옮겨줘"·"반으로 줄여줘"·"Snap window to left") | window_move/resize/minimize+Voice Access 문법 | 어휘+인자 정수 파싱 | 도구 존재·지연 0 |
| 3 | 알림/파일 어휘(알림 뭐 왔어·어제 사진 보여줘) | notify·gallery 도구 | 어휘 | 기대 사례 소각 |
| 4 | **번호/그리드 오버레이** — v1.6 신규 | capture_window+send_input | 화면 캡처+번호 도형 오버레이+send_input 좌표 변환 | §2 양사 공통·접근성 표준 |
| 5 | 확정/취소 어휘("그래""취소") — 고위험 동작 게이트 | trigger_toggle+게이트 계약 | 어휘+게이트 | Operator confirm 선례 |
| 6 | PC 릴레이+TTS — v2 | jkbridge chat·whisper 폰 | 릴레이+Piper 등 TTS | 1s 문턱 §6 |
| 7 | 상시 청취(VAD+기동어) — v3 | whisper.cpp 스트림+Silero VAD 원문 | VAD 파이프+보안 가드 | §3.6+§9f |

### 9b. 어휘/인텐트 표 후보 (한국어 — 도구 허브 기준)

형식 제안: **문형(동사-명사-인자) → 도구(인자)** — HA 문장 템플릿+Dragon open-ended 절충.

| 발화(예시) | 인자 유형 | 도구 | 선례 |
|---|---|---|---|
| "{지뢰찾기} 켜줘/실행해줘" | 앱명(퍼지) | launch_app | 전축 실측(S1·S2) |
| "이 창 닫아줘/꺼줘" | 포커스 암시 | close_window | Bixby "창 닫아줘" |
| "참고로 {메모장}으로 바꿔줘/앞으로" | 앱명 | focus_window | "Switch to [app]" |
| "창 목록 보여줘/뭐 떠 있어" | 무인자 | list_windows | "Show all windows" |
| "창 왼쪽/오른쪽으로 옮겨줘" | 방향 | window_move | "Snap window to [direction]" |
| "창 반으로 줄여줘/더 크게" | 비율·정수 | window_resize | "Move ... five times" |
| "창 작게/아이콘으로" | 상태 | window_minimize | 접근성 표준 |
| "화면 찍어줘/스크린샷" | 무인자 | capture_window/region | NUGU·Alexa 실측 다수 |
| "음악 틀어줘"·"다음 곡"·"정지"·"볼륨 줄여줘/10" | 무인자·정수 | vplayer 재생계 | Leon Spotify·Alexa TV·NUGU "볼륨 10" |
| "이 창 가로채줘/번호 보여줘" — 번호 오버레이 | 번호 | send_input | Voice Access "Show numbers" |
| "그래(확정)/취소" | 게이트 | trigger_toggle+confirm | Operator confirm |
| "알림 뭐 왔어"·"{어제} 사진 보여줘" | 시간·무인자 | notify·gallery | §2 기대 사례 |

- **한국어 문형 특이점**: 조사("으로/를/를 줘") 처리 — 템플릿에서 조사를 와일드카드로("…로 옮겨줘")·어간-조사 변형을 문형 목록에 명기(HA YAML alternatives/regex가 형식 선례).
- **부모-자식 인자**: "지뢰찾기를 **더 잘 보이게** 해줘" 같은 서술형은 LLM 폴백(§9d 혼합형).

### 9c. 번호/그리드 오버레이 도입 판정 — **도입 권고(신규 제안 v1.6)**

- 근거: 양사 공식 문법(§2.1·§2.2)·컨트롤 인식 불가를 정직하게 인정하는 유일한 범용 폴백·JKEngine은 capture(화면 보기)+send_input(좌표 클릭)을 이미 소유 — **필요한 것은 (1)캡처 → UI 요소 감지(또는 3×3 그리드 무검지 (2)번호 도형 오버레이 창(무클릭 투과 — focus-clickthrough hint 기법 존재) (3)"클릭 3/그냥 3" 어휘**.
- **판정**: 최소 형태는 **그리드 모드(요소 검지 0 — 무검지 방식)**부터: 3×3 → 번호 발화 → 드릴(재분할) → 최소타일에서 자발 클릭(macOS 종료 규칙). UI 감지는 별도(접근성 API 트리 — Windows UI Automation이 원존)로 2단.
- 취약점 원문 반영: 주 모니터 외 오류(Voice Access)·드릴 상태 소거 규칙·오버레이 무클릭 투과가 필존(스크린샷에 오버레이가 찍히지 않는 것도 설계 요소 — 캡처가 오버레이와 독립).
- 드래그: "Mark [n] → Drag" 2단계 프로토콜(Voice Access) — 도입 시 동일 계약.

### 9d. LLM 턴과 어휘 라우터의 이층 구조 — **선례가 "상용 표준"을 실측으로 확정**

- **JKEngine 현재(v1)**: 어휘 즉발(0.2s) → 비매치 LLM 폴백(0.9-4s). = **HA HassIL→HassLLM·AutoVoice 정규식→Natural(로컬 LLM beta)·C.O.D.A와 동형**.
- **정량 선례(가장 중요)**: **Hyundai LLM ContextBridge(COLING 2025)** — [논문](https://aclanthology.org/2025.coling-industry.66v2.pdf) — 3층 Rule-NLU→소형 PLM 분류기→**LLM은 분류기 입력을 '정제'** — 인텐트 정확도 0.896→0.917·멀티턴 0.152→**0.600(+45%)**·LLM 직접 분류는 0.636(오히려 저하)·지연 +600ms(3s 상한 내). **교훈: LLM이 문백 정제, 소형 파서가 확정 매칭 — 반발(문백→LLM이 다음 턴)이 아니라 통과형**.
- **하이브리드 라우터 원리**([arXiv 2504.10519](https://arxiv.org/pdf/2504.10519v2.pdf)): 인텐트 라우터→전문 에이전트·모델 라우터(SLM/LLM 적재적 소환)·TinyAgent·RouteLLM 관련 — **확률·지연·비용 절반 소화**.
- **3층 구조 설계 스터디**([YazSes](https://mskazemi.com/yazses/design/research/studies/yazses-future-voice-hci/output/capability_cards/gap-001.html)): Tier1 문법 <10ms / Tier2 로컬 양자화 SLM 100-300ms(신뢰 <0.75면 폴백) / Tier3 클라우드 LLM(기본 끔) — **신뢰 임계 폴백 계약**이 정식 명시.
- **저지연 벡터 검색 선례**([LowLa, Devpost](https://devpost.com/software/lowla)) — faster-whisper→MiniLM 임베딩→ChromaDB 벡터 검색→신뢰 매치 즉발→LLM 폴백 — **어휘 표가 아니라 '문장 임베딩 유사도'가 정확매치를 대체**하는 3번째 방식 — 어휘 표의 "동의어 확장"을 자발화할 후보.
- **판정**: JKEngine "정확매치 즉발+LLM 폴백"은 **상용·오픈소스·학술 3축 표준**. 개선 방향 — (a)LLM 단독 폴백이 아니라 **ContextBridge형 "정제→재매칭"** 혼합형(v2 설계 근거) (b)어휘 표의 동의어를 벡터 검색으로 양산(LowLa·Padatious 예제 확대 방법) (c)신뢰/위험도 기반 로컬-클라 스위치(C.O.D.A·YazSes 선례) — **JKEngine 클라우드 LLM opt-out 밸브(docs/92)와 정합**.

### 9e. 한국어 특화 전략

1. **STT**: JKEngine 폰 whisper.cpp 현행 — small/turbo급에서 KsponSpeech 파인튠 효과 최대(43→14 CER) — **한국어 파인튠 모델을 폰 내장 어휘 라인의 전제**로. 서버 CPU는 CTranslate2 FP16 2-3배 실측.
2. **어휘**: NUGU/B tv 원문(~"볼륨 10"·"13번 채널")으로 **정수 인자의 한국어 서술("볼륨 10"="볼륨 십"="볼륨 열") 숫자 표기 다형** 필존 — LLM 폴백에서도 숫자 파인튠(§7.1 전사 규칙 정화) 필존.
3. **기동어**: "아리아" 선례 — 한국어 기동어는 **2-3음절 고유단어**·유니크 충돌 회피(Voice Access 원칙)·시간대 가드(B tv 나이트 모드).
4. **상용 API**: CLOVA 스트리밍(키워드 부스팅으로 명령 어휘 부여)·ETRI(무료 1000건/일) — **로컬 파인튠이 아직 아니라면 백업 축**으로 명시. (정직 표기: JKEngine 계약은 로컬 원존 원칙 — 클라우드는 opt-in 부존.)

### 9f. 리스크/보안 (음성 스푸핑·오동작 방지)

실측 문헌:

- **스킬 스쿼팅**([USENIX Security'18](https://www.usenix.org/system/files/conference/usenixsecurity18/sec18-kumar.pdf)) — 11,460 샘플·오류의 12.7%가 체계적·**92.6% 타깃 스킬 성공 스쿼팅**+사용자 지역/성별 표적(72.7-83.3% 성공) — **정확매치 어휘 시스템의 고유 문제** — JKEEngine은 어휘 표가 내부 고정(스킬 마켓 없음)이라 직접 리스크 낮음, 단 **도구 배포(기능 팩) 도입 시 응시**.
- **Dangerous Skills(IEEE S&P 2019)** — [논문](https://doi.org/10.1109/sp.2019.00016) — 음성 스쿼팅(VSA)+가장 무례(VMA)·시장 4,718 스킬 위험 스캔 원문.
- **FakeWake(CCS 2021)** — [논문](https://doi.org/10.48550/arxiv.2109.09958) — 8 기기에서 965개 퍼지 기동어로 잘못 깨우기·재훈련으로 거의 0%까지 감소 — **v3 상시 청취(오동작 launch)의 직접 위협 모델**. **방은: 기동어 3단어+국문 고유단어·VAD+기동어 2축 검증(B tv 200ms 이하 실측)·잡음 태그 억제(§7.1)**.
- **음성 어시스턴트 보안 총론**([ACM Computing Surveys 2022](https://dl.acm.org/doi/full/10.1145/3527153)) — 초음파(DolphinAttack)·레이저(LightCommands **110m**)·전자파·신호 주입·가짜 명령 히든 커맨드 — **"마이크가 항상 켜져 있으면" 물리 주입도 위협 모델** — v3에서 **항상 듣는 것과 동작하는 것의 분리(VAD 이전 단계에서도 도구 게이트 유지)**가 방어 원칙. [Computers & Security 2023 총론](https://doi.org/10.1016/j.cose.2023.103448) — 라이브리티 검출·음문 인증·EarArray 등 방어 카탈로그.
- **프롬프트 주입(화면 보는 에이전트)**(§4 Anthropic/OpenAI 공식) — JKEngine의 send_input은 도구 게이트(docs/57·80)가 원존 — **화면 캡처를 모델에 되먹이는 경우 화면 내 문자열을 명령으로 취급하지 않는 계약** 명시 필요(v2 이후 캡처→LLM 정제 도입 시).
- **고위험 동작 게이트 선례**: Apple 시계 문 잠금(기기 잠금 선행 요구)·Operator takeover/confirm·Gemini safety_decision — **JKEngine 확인/취소 어휘(§9b)+rate cap 원칙과 연결**.

---

## ⑪ §10 방향 재판정 — 기존 v1.5/v2/v3에 대한 판정

| 기존 방향 | 재판정 | 근거(§) | 정정/추가 |
|---|---|---|---|
| **v1 어휘 라우터+stub 폴백(성립·결제)** | **승인 — 산업 표준의 정확한 위치** | §3.5·§3.2(HA 공식 이층)·§8d(ContextBridge 3축 실측) | 형식은 HA 문형(와일드카드·대안·regex)+조사 취급 원칙(§9b) |
| **v1.5 어휘 확장(추천 다음)** | **승인·강화 — 재판단 무변경·우선순위 상위** | §1.7·§3.4 재생계 어휘 원문·§9a 1-3 | 순서: 재생→창 세밀→알림/파일 — 각 체감 판정(즉발 0.2s 내)은 유지 |
| **(신규) v1.6 번호/그리드 오버레이+확정/취소 어휘** | **신설 권고 — 조달 소액·사용자 체감 최대** | §2.1·§2.2 양사 공식·§9c | "번호 보여줘→클릭 3"·드릴·자발 소거·Mark/Drag — 도구 없이 창 어휘이 안 잡히는 모든 상황을 커버 |
| **v2 대화 품질(PC 릴레이+TTS)** | **근거 확정 — 지연 문턱이 TTS 설계의 본질** | §6 Cohen/Nielsen(1s 인지 필존·>5s 이탈) | TTS는 답의 시작(첫 단어)을 1s 내로 — 3s 상한은 문백 정제만. LLM 폴백을 "정제→재매칭" 혼합형(ContextBridge)으로 격상 · 캡처→LLM 도입 시 프롬프트 주입 계약 명시 |
| **v2 APK 내장 whisper** | **근거 강화 — 한국어 파인튠 효과가 폰 모델(small급)에서 최대** | §7.1(small 43→14) | 폰 NDK 전사+Silero VAD(§3.6 파라미터 실측 원문 직수) |
| **v3 상시 청취+자율** | **조건부 승인 — 보안 게이트가 본문으로** | §9f FakeWake·물리 주입·§1.7 씬 트리거/B tv 나이트 모드 | ①기동어 2-3음절 유니크(아리아/Voice Access 선례) ②VAD+기동어 2축 ③나이트 모드 동급의 시간대 가드 ④도구 게이트 유지 — "듣는 것"과 "동작하는 것" 분리 |
| **운영 계약(원장·fail-closed·EYES)** | **재판정 무변경 — 산업 전체가 같은 원칙** | §4.3(Takeover/confirm/safety_decision)·§2(오버레이 자발 소거 = 성공 신호 정직 표기) | — |

- **추가 단계 제안(위 표 외)**:
  1. **어휘 표의 배포·버전 계약**(Dragon Command Browser/nsadmin 선례) — 어휘 파일을 앱 자산으로·버전 명기·수정은 원자적 쓰기.
  2. **"What can I say" 동일 기능** — 현재 가능 어휘를 화면에 표시(macOS "Show commands" 선례) — 정직 표기 계약의 사용자 대면 축.
  3. **Teach and Repeat형 "이건 그렇게 해줘"** — 사용자 데모를 기록해 반복(Mariner I/O 2025 선례) — v3 이후 후보. (Macro Recorder 기법 — Dragon 12년선 원문 — send_input 사슬 녹음이 원존.)
  4. **STT 신뢰 폴백 계약** — 전사 신뢰/매치 신뢰 <임계면 dictation 모드로 전환(YazSes 0.75 선례) — 지금의 stub 폴백에 "신뢰" 축만 추가.

---

## ⑫ §11 출처 링크 전량 (§순 — 모두 실측 검색/패치 2026-10)

**Apple**
- https://developer.apple.com/documentation/appintents
- https://support.apple.com/guide/accessibility-mac/control-your-mac-and-apps-using-voice-control-mh40719/12.0/mac/12.0
- https://support.apple.com/guide/accessibility-mac/use-voice-control-commands-mh40719/mac
- https://support.apple.com/en-ie/102225
- https://support.apple.com/guide/watch/control-apple-watch-smart-home-devices-apd30bcf523b/watchos
- https://support.apple.com/guide/watch/control-your-home-apddad023a05/watchos
- https://support.apple.com/en-us/105027
- https://support.apple.com/en-us/102313
- https://support.apple.com/guide/watch/remotely-access-your-smart-home-accessories-apd1af68cb2a/watchos
- https://eshop.macsales.com/blog/92455-how-to-enable-and-use-voice-control-in-macos-sonoma/
- https://mcmw.abilitynet.org.uk/how-control-your-computer-your-voice-macos-11-big-sur

**Google/Android**
- https://developers.google.com/assistant/ca-sunset
- https://developers.google.com/assistant/console/policies/app-actions-policies
- https://developers.googleblog.com/en/helping-developers-create-meaningful-voice-interactions-with-android/
- https://9to5google.com/2026/09/28/google-assistant-gemini-android/
- https://9to5google.com/2026/08/04/google-assistant-september-2026-shutdown/
- https://techcrunch.com/2025/05/20/google-rolls-out-project-mariner-its-web-browsing-ai-agent/
- https://www.theverge.com/tech/925559/google-project-mariner-shut-down
- https://www.theverge.com/news/670848/google-agent-mode-gemini-app-project-mariner-i-o-2025
- https://9to5google.com/2025/10/07/gemini-2-5-computer-use-model/
- https://blog.google/innovation-and-ai/models-and-research/google-deepmind/gemini-computer-use-model/
- https://ai.google.dev/gemini-api/docs/computer-use
- https://ai.google.dev/gemini-api/docs/models/gemini-2.5-computer-use-preview-10-2025

**Samsung**
- https://developer.samsung.com/technologies/dex
- https://www.nugu.co.kr/
- https://www.nugu.co.kr/static/service/service01-02-04.html
- https://www.tworld.co.kr/poc/html/download/Btv_AI2_User_Guide.pdf
- https://www.tworld.co.kr/poc/html/download/Btv_Smart3_User_Guide.pdf
- https://blog.skbroadband.com/2691

**Microsoft/Windows**
- https://support.microsoft.com/en-us/accessibility/windows/voice-access/voice-access-command-list
- https://support.microsoft.com/en-us/accessibility/windows/voice-access/use-the-mouse-with-voice
- https://support.microsoft.com/en-us/accessibility/windows/voice-access/use-voice-to-interact-with-items-on-the-screen
- https://support.microsoft.com/en-us/accessibility/windows/voice-access/voice-access-frequently-asked-questions-faqs
- https://support.microsoft.com/en-US/accessibility/windows/voice-access/use-voice-access-on-a-multi-display-setup

**Dragon/Nuance**
- https://www.nuance.com/products/help/dragon15/dragon-for-pc/enx/professionalindividual/content/MyCommands/mycmds_1/mycommands_overview.htm
- https://nuance.custhelp.com/app/answers/detail/a_id/6895/~/how-to-create-a-custom-command-in-dragon-naturallyspeaking
- https://www.nuance.com/products/help/dragon/dragon-for-pc/scriptref/Content/mycommands/mycmds_2/open_ended_macros.htm
- https://www.mhsinformatics.org/AHLTA-Files/Dragon-Files-not-Macros_As-U-Type/l-3177-001%20Advanced%20Scripting.pdf
- https://dragon.nuance.com/shared/user-docs/gd-dragon-professional-individual-and-dragon-legal-individual-workbook-en-us.pdf

**오픈소스 음성 에이전트**
- https://openvoiceos.github.io/ovos-technical-manual/401-skill_structure/
- https://openvoiceos.github.io/ovos-technical-manual/399-intents/
- https://openvoiceos.github.io/ovos-technical-manual/403-intents/
- https://openvoiceos.github.io/beta-technical-manual/migrating-from-mycroft/
- https://mycroft-ai.gitbook.io/docs/skill-development/skill-structure
- https://www.home-assistant.io/dashboards/assist/
- https://developers-voice.home-assistant.io/docs/voice/overview/
- https://github.com/rhasspy/wyoming
- https://github.com/home-assistant/architecture/blob/master/adr/0017-assist-pipeline.md
- https://www.home-assistant.io/blog/2023/04/27/year-of-the-voice-chapter-2/
- https://rhasspy.readthedocs.io/en/latest/whitepaper/
- https://heywillow.org/how-willow-works/
- https://heywillow.org/features/Local-Command-Support/
- https://github.com/HeyWillow/willow/issues/446
- https://getleon.ai
- https://github.com/leon-ai/leon
- https://docs.getleon.ai

**STT스트리밍/VAD**
- https://github.com/ggml-org/whisper.cpp/pull/3653
- https://github.com/v3DJG6GL/faster-whisper-backend-archive/blob/main/README.md
- https://github.com/zackees/transcribe-anything/issues/124
- https://github.com/bhargavchippada/faster-whisper-dictation
- https://docs.qvac.tether.io/ecosystem/addons/transcription-whispercpp/

**LLM 컴퓨터 사용 에이전트**
- https://www.anthropic.com/news/developing-computer-use (2026-10-26 실측 패치)
- docs.anthropic.com/en/docs/build-with-claude/computer-use (현 platform.claude.com 계열 — 검색 실측)
- https://openai.com/index/introducing-operator/
- https://openai.com/index/introducing-chatgpt-agent/
- https://openai.com/index/chatgpt-agent-system-card/
- https://help.openai.com/en/articles/11752874-agent
- https://help.openai.com/en/articles/11794368-chatgpt-agent-release-notes
- https://github.com/OpenInterpreter/open-interpreter/
- https://changes.openinterpreter.com/log/the-new-computer-update
- https://01.openinterpreter.com/getting-started/introduction
- https://www.adept.ai/act

**폰/커뮤니티**
- https://play.google.com/store/apps/details?id=com.newstand.autovoice
- https://www.joaoapps.com/autovoice/natural.html
- https://www.reddit.com/r/tasker/comments/1j3687t/autovoice_natural_beta_taskers_aipowered/
- https://play.google.com/store/apps/details?id=com.newstand.autovoice.natural
- https://medium.com/the-phoenix-voice/how-to-make-real-custom-voice-commands-for-android-b2ab6c6b8c30
- https://www.joaoapps.com/autovoice/voiceassistant/
- https://github.com/KillerInGit/Accessibility-Agent
- https://github.com/mattordev/coda
- https://devpost.com/software/lowla

**HCI/라우팅**
- https://www.nngroup.com/articles/response-times-3-important-limits/ (2026-10-26 실측 패치)
- https://aclanthology.org/2025.coling-industry.66v2.pdf (Hyundai LLM ContextBridge)
- https://arxiv.org/pdf/2504.10519v2.pdf (Hybrid AI Routers)
- https://mskazemi.com/yazses/design/research/studies/yazses-future-voice-hci/output/capability_cards/gap-001.html

**보안**
- https://dl.acm.org/doi/full/10.1145/3527153 (ACM CSUR 2022 총론)
- https://doi.org/10.1109/sp.2019.00016 (Dangerous Skills, IEEE S&P 2019)
- https://www.usenix.org/system/files/conference/usenixsecurity18/sec18-kumar.pdf (Skill Squatting)
- https://doi.org/10.48550/arxiv.2109.09958 (FakeWake, ACM CCS 2021)
- https://doi.org/10.1016/j.cose.2023.103448 (Computers & Security 2023 총론)

**한국어 STT/상용 API**
- https://www.pyannote.ai/blog/whisper-ko
- https://github.com/wikibook/polyglot-ko
- https://soynlpdev.ai.kr/8
- https://www.vitoengine.com/whisper-finetuning-1
- https://aihub.or.kr
- https://www.ncloud.com/api-cms/service-product/static/clovaSpeech
- https://www.fin-ncloud.com/product/aiService/clovaSpeech
- https://developers.naver.com/products/clova/vrecog/
- https://epretx.etri.re.kr/apiDetail?id=88
- https://www.data.go.kr/data/15124653/openapi.do

**문헌(링크 없는 실측 서지)** — Cohen·Giangola·Balogh 2004 *Voice User Interface Design*(Addison-Wesley, 검색 실측 요약 — VUI 지연 문턱 적응판)·Voice Finger/Tazti/e-Speaking(Windows 구두 명령 소프트웨어 — 웹 실측 명칭만)·9to5Mac 2025-04 App Intents Siri 시각 인터페이스 보고(인용 요약 실측·단일 링크 미확보)·Bloomberg/Gurman iOS 26.4 Siri 보고(인용 요약 실측·링크 미확보)·Amazon Alexa built-in TV 명령/Alexa+(검색 요약 실측·개별 링크 미확보)·Android Auto 명령 실측 요약(개별 링크 미확보) — 위 4건은 본문에서 "명칭만"으로 분리 표기.