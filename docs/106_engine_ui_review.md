# 106 — JKEngine 구조·UI 검토 보고 (오케스트레이션 파이프라인, 2026-11-10)

**방법**: ruflo MCP 미등록(프로젝트 스코프 — §7 정직) → 내장 Workflow 4축 병렬
검토(엔진 아키텍처 / UI 디자인 체계 / Android APK 경계 / 빌드·배포·검증) →
**발견 32건**(high 15 · medium 17) 수집 → HIGH 5건 **적대적 반증 검증** →
**4건 확정 · 1건 반증**.

**검증 등급 표기**: ✅ = 반증 검증 통과(원문 재확인) · ◻ = 에이전트 원문
근거만(반증 검증 대기 — 인용 file:line은 실측) · ✕ = 반증됨.

---

## 1. 즉시 조치 (P0 — 각 30분~수시간, 고통/위험 직결)

### 1-1. ✅ 태스크바 창 버튼이 우측 시계 트레이를 덮는다
`engine/src/apps/ClientTaskbarApp.cpp:347` — `avail` 산식이 **정리 셀(44px)·실제
시작 x를 빼지 않아** 창 6개(1280폭)에서 트레이 시계 앞부분 ~36px가 가려진다
(일반식: 겹침 = 52 − r px; 폭 800·창 5개에서도 ~51px). 정확 수형 =
`trayLeft = client.w - kBarMargin - kTrayW; avail = trayLeft - kButtonGap - x;`
+ 배치 루프 클램프. **검증 원문**: 산수 전개·z-order(시계 셀이 먼저
AddControl되어 버튼이 위에 그려짐)까지 실측 확정.

### 1-2. ✅ main.cpp 9,703줄 셀프테스트가 출하 TU에 컴파일
`engine/src/main.cpp:1120-10823` — 게이트(NDEBUG/타깃) 0, 실행 분기만
(`:10939`). 오브젝트가 jkdesktop .obj 총합의 **55.8%**(1.44MB/2.59MB),
Release exe·안드로이드 libjkc.so 양쪽에 `AppSelfTest` 문자열 존재. 수형 =
`src/tests/JKSelfTest.cpp` + `jk_selftest` 정적 라이브러리, host 타깃만 링크.

### 1-3. ✅ JKWindowServer.cpp 9,649줄 — 컴포지터·에이전트 허브·설정/노트/파일/
트러스트 동거. `HandleAgentQuery` 3,373줄·distinct 도구 44종(원 보고 "20여
종"은 과소). 플랫폼 분기 26곳+ 산재. 수형 = `JKAgentHub`/`JKServerOverlays`
분리 + `IWindowServerServices` 인터페이스 뒤로.

### 1-4. ✅ 안드로이드 빌드의 소스 목록 수동 복제 — 단일 진실원 부재
`android/.../jni/CMakeLists.txt:75` 자백 주석("그대로 흡수"). 참조 수 실측
engine 221 · apk 217 · spike-b 139 — **spike-b에 `JKLlmTurnHttp.cpp` 결손 =
드리프트 실증**. 수형 = `cmake/jk_sources.cmake` 공유 + configure 시
정본 비교 `FATAL_ERROR`.

### 1-5. ◻ Android `StateDir()`가 쓰기 불가 경로(nativeLibraryDir)를 가리킨다
`JKWindowServer.cpp:8434` — Android 분기 없이 exe 경로 기반 → `save_layout`·
`capture_window`가 폰에서 **조용히 실패**할 수 있는 구조. 오늘 트레이의
"정리" 셀이 save_layout과 연결돼 있어 실사용 경로다. 수형 = ExeDirNoSlash
공유 + Start() 1회 생성/쓰기가능 stderr 확인.

### 1-6. ◻ 엔진 앱(chang.jkdesktop)에 RECORD_AUDIO 런타임 요청이 없다 —
`JkVoice.begin`이 항상 true. jkcontrol 승인은 **패키지 단위라 엔진에 무효**.
오늘 "mic error 9" 사가의 구조적 재발 경로(신규 설치 직후). 수형 =
MainActivity/JkVoice에서 요청 + /voice 응답에 권한 상태 반영.

### 1-7. ◻ 검증 래퍼 3종이 **항상 exit 0** — 실패가 파이프라인 밖으로 샌다
`engine/run_selftest.sh:7`(echo 후 exit 0), `smoke_test.sh:25`,
`wsl_regress.sh:10-15`($?가 echo의 rc). 수형 = rc 캡처·`exit $rc`·pipefail.

### 1-8. ◻ 빌드 산출물에 지문 없음(versionCode=1·versionName 고정·APK 내부
타임스탬프 1981 고정) + `--version` 부재 → **스테일 APK 판별 불가**(오늘
밤 "설치 Success=낡은 APK" 사건의 구조 원인). 수형 = buildConfigField
GIT_SHA/BUILD_TIME + `jkdesktop --version` + probe 하드 게이트.

### 1-9. ◻ probe가 통일 이전 산출물을 검사 — `phone_apk.sh:55`가 **jkapk
APK(654MB·applicationId=chang.jkapk)** 경로를 가리켜 다른 앱을 통과시킨다.
46개 probe 전부 git 미추적(`git ls-files` 0) — 새 클론엔 배포·검증
절차가 존재하지 않는다.

---

## 2. 구조 개선 (P1 — 반나절~2일, 비용의 곱셈 지점)

| # | 발견 | 수형 |
|---|---|---|
| 2-1 | ✕ **반증됨**: "jkcore 통째 복제" — 정적 링커는 참조 오브젝트만 끌어옴(모듈별 심벌 0건 실측). **좁은 명제만 참**: QuickJS가 모듈마다 중복(원인 = jkclient·JKTextAtlas의 AgentJson 의존) | AgentJson 의존 절단 또는 jkcore 공유 라이브러리화 |
| 2-2 | ◻ 모듈 29개 정적 링크 — APK 라이브러리 88MB 중 **76.3MB가 모듈**(2.6MB×29) | jkcore/jkclient SHARED 전환(Android PIC 이미 ON) → **~70MB 절감** + 새 앱 추가를 매니페스트 1행으로 |
| 2-3 | ◻ jkserver가 셸을 PUBLIC 링크 — 컴포지터가 데스크톱 정책에 역의존, 헤드리스에도 셸 비용 | `IInProcessShell`(널 구현) 인터페이스 |
| 2-4 | ◻ 앱 구현 16 TU가 단일프로세스와 모듈 양쪽 컴파일(호스팅 2벌) | in-process dlopen 통일 또는 레거시 컴파일 게이트 |
| 2-5 | ◻ 중복 유틸: JSON 이스케이프 **21벌**(출력도 갈림 — `\n` vs `\u a`), QuickJS 리더 5벌(행마다 런타임 생성 2곳), 파일리더/Trim 각 5~6벌 | `jk::json::Escape/ParseFile`·`jk::fs::ReadAll` 헤더 신설+CI 카운트 게이트 |
| 2-6 | ☑ 크롬 기하 상수(2026-11-12 — JKChromeMetrics.h+등호 단정)가 클라 페인트·서버 페인트·히트테스트 3곳 중복(주석 동기) | `JKChromeMetrics.h` 단일 + 셀프테스트 등호 |

## 3. UI 디자인 체계 (P1~P2)

- ☑ **테마 토큰 누락**(2026-11-12 봉합 — §4c): 라벨·툴팁·시계 텍스트 색이 리터럴(`235,235,240` 등)
  — kLight 프리셋에서 런처 라벨이 안 보인다. 수형 = JKTheme에 launcherLabel
  Text/Shadow·tooltip*·desktopClockText 토큰 + 캐시 테마 세대 무효화.
- ☑ **텍스트 스케일 3갈래**(2026-11-12 봉합 — docs/109 §5): 라벨 = outputScale 미적용(HiDPI에서 절반),
  툴팁 = ×s, 시계 = 고정 4배. 수형 = (font_scale×outputScale) 통일 래스터.
- ☑ 런처 그리드(2026-11-12 458e43c 봉합)(비Android 리사이즈 미반영·cols=0 UB),
  콘솔 앱 desc가 라벨(96px 셀 넘침·중앙 정렬/클립 부재).
- ☑ 데스크톱 시계 사건 판정(2026-11-12 봉합 — 16a2541 후속)(clockLastRect_ 영원히 0 —
  더티 스킵 무력화) → 분/자리 변경 시만 발행.
- ◻ 텍스트 렌더 파이프라인 셸·태스크바 이중 복제(패드 5 vs 3 드리프트) →
  `jk::text::TextureFactory` 추출.
- ☑ 태스크바 네비 셀 역할 enum(2026-11-12 봉합 — NavGlyph)("뒤"…) — 문구 변경 시 조용히
  퇴화, 5바이트+콜론이면 무엇이든 7세그. Role enum + cellH 비례 글리프.
- ◻ 자산 추출 무버전 스탬프(APK 갱신해도 스테일 자산 유지)·41MB UI 스레드
  복사. 수형 = assets.stamp(versionCode+해시)·워커 스레드·파일당 try.
- ◻ `allowBackup` 미설정 → **cloud API 키(chat.json)가 Auto Backup 대상**.
  수형 = allowBackup=false/dataExtractionRules 제외 + Keystore 이관(후속).

## 4. Android 경계 (P2)

- ◻ JNI 훅 3종(브라우저)이 클래스로더 해석 30줄씩 복제·BrowserActivity
  미캐시 → JNI_OnLoad 캐시(검증된 JkVoice/JkHttp 패턴) + `jkhooks_android.h`
  단일 ABI 헤더.
- ◻ ForegroundService가 광고하는 수명을 소유하지 않음(START_STICKY 재기동 =
  빈 서비스) — 엔진은 Activity SDL 스레드 소유. 장기 = 서비스 소유, 단기 =
  알림 탭 재개·BrowserActivity 오버레이 테마.
- ◻ jkcontrol 실행 경로가 메인 스레드 **3.4초 블록**(sleep 900+2500)·/voice
  `doOutput` 누락·force-stop 권한 밖. 수형 = Executor 이전·setDoOutput.

## 4a. P1 진행 정산 (2026-11-11)

- ✅ **2-2 모듈 SHARED 전환 폐곡**: Android 트리 jkcore/jkclient/imgui/implot
  SHARED + c++_shared — 실측 APK 라이브러리 **88MB → 19.4MB(약 68MB 절감)**,
  모듈당 2.6MB → 0.06~0.35MB. 폰 실전 검증: 지뢰찾기 창 개봉 원문(SHARED
  dlopen 성립)·BUILD_SHA=c69fbb2.
- ✅ **1-2 셀프테스트 분리 폐곡**: main.cpp 11,408 → 1,697줄·main 오브젝트
  1.38MB → 0.19MB(86%↓)·selftest 993 PASS 0 failure(무손실). JKSelfTest.cpp
  (9,883줄) + 조력자 사본 3정(ReadWholeFile·RunScriptTestFile·selftest_llm)
  ·selftest_http는 이동(중복 정의 소각).
- ◻ **1-3 서버 TU 분리는 이번 라운드 판정: 기계적 절단 불가** —
  HandleAgentQuery(4062-7430, 3,369줄)가 같은 TU의 파일 스코프 조력자
  (JsonEsc·SettingsKvPath·NotesPath·ValidFingerprint·저널 writer)를 광범위
  사용 — 인터페이스(IWindowServerServices) 설계 후의 2-TU 추출로 다음 세션
  정식 착수. 절단 시도 금지(반쪽 추출 = 회귀 위험).

## 4b. 자율 연속 라운드 정산 (2026-11-11 오전 — 잔여 P0/§4)

- ✅ **allowBackup OFF**(API 키 Auto Backup 차단 — manifest+dataExtractionRules
  state/ 제외 이중 방어).
- ✅ **자산 버전 스탬프**(versionCode+트리 사이즈 — 샘 이름 스테일 자산 소각)
  + 파일당 try 격리.
- ✅ **JNI BrowserActivity 클래스 캐시**(JNI_OnLoad — 3훅 로더 댄스 절감·
  실패 시 기존 로더 폴백).
- ✅ **jkcontrol** — UI 블록 3.4초 → 워커 스레드(ANR 근접 소각)·postVoice
  doOutput 누락 수형.
- ◻ **한글 라벨 v2 이월(정직)**: 계측 완벽 분리 — ①dc 텍스트 경로 정상
  (ASCII "AB" 렌더 원문) ②**Utf8ToKssm = Android bionic iconv CP949 부재로
  전면 빈 문자열**(kssmlen=0 로그 원문 — 진원) ③직접 fast path(UTF-8→
  wCodeTable)로 '뒤'(KSSM 2350 음절 내) 성공·'홈/최/정'(밖) 실패 ④아틀라스
  유니코드 직행(DrawUnicodeGlyph 공개·EnsureMiniAtlas) 폰 무증상 미출력.
  기반 인프라는 커밋 완료 — 다음 세션 = 자식 아틀라스 블릿 경계(SW 렌더러)
  재판정.
- ✅ 서버 TU 설계 스펙 = docs/108(4단계·게이트·역치).

## 5. 권고 실행 순서

1. **오늘 밤 사건 직결 5건**: 1-1(트레이 가림)·1-5(StateDir)·1-6(권한)·
   1-7(exit 0)·1-8+1-9(지문·probe 경로) — 배포 신뢰성 회복.
2. **P1 구조**: 2-2(공유 라이브러리 70MB 절감 — 빌드 시간·설치에도 이득) →
   1-2(셀프테스트 분리) → 1-3(서버 TU 분리) → 1-4(cmakes 단일 진실원).
3. **UI 체계**: 3장 토큰·스케일 통일 → 런처 리사이즈/라벨 정리.
4. Android 경계(4장)는 다음 APK 세션.

## 6. 방법론 메모 (재사용 자산)

- 4축 병렬(find) → HIGH 반증(verify) → 종합 = 실측에서 **반증 1건**이 나와
  "정적 링크 = 통째 복제" 오진을 걸러냈다 — 반증 단계가 없으면 잘못된
  리팩터(불필요한 공유 라이브러리화 근거)로 이어질 수 있었다.
- 비용: 에이전트 9 · 서브토큰 ~57.6만 · 19분. 결과물 = 32건 중 검증 5건.

## 7. 정직 표기

- ruflo MCP는 JKENGINE 프로젝트에 미등록(다른 프로젝트 스코프) — 내장
  Workflow로 대체 실행(사용자 승인 하에). ruflo 조사 = docs/107.
- ◻ 등급 27건은 에이전트 원문 인용 실측이나 반증 검증 미실시 — 착수 전
  1회 재확인 원칙(원장 관행).

## 4c. 테마 토큰 봉합 정산 (2026-11-12)

- JKTheme 신설 토큰 5: `launcherLabelText`·`tooltipText`·`tooltipBg`·
  `tooltipBorder`·`desktopClockText` — 프리셋 3종 전부 명시(kDefault는 구
  리터럴 값 그대로 = 다크 픽셀 불변, kLight는 Win11 라이트 대응색).
- 소비처 봉합: 셸 `TextTextureEx`(색 명시 본체 — 툴팁/라벨/시계 공용) ·
  툴팁 박스 면·테두리 · 시계(LabemTextureColored) · 태스크바 미니 라벨
  (`taskbarTextNormal`).
- 캐시 무효화 = `theme::Generation()` 세대 카운터(같은 프리셋 재적용 무변).
  셸은 Draw 진입에서 세대 비교 → 라벨·툴팁·시계 텍스처 파괴, 태스크바는
  미니 텍스처 맵 파괴. 아틀라스 글리프 이미지는 색과 무관해 유지.
- 회귀망: 셀프테스트 106-3a~3d(프리셋 3종 대비 3:1 · 세대 카운터).
- 남은 눈확인: PC `theme.json` {"preset":"light"} 로 런처 라벨 가독 확인.
