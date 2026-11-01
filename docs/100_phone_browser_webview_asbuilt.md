# #123 폰 브라우저 WebView 실증 라인 as-built (docs/100)

**상태: T1-T6 폐곡 — 측정표 원문 수확 완료(2026-11-01 13:55 폰 실측)·엔진 최종 판정 = 사용자 EYES 대기**

## 1. 결정 원존 (스펙 승계)

- 스펙: `docs/superpowers/specs/2026-11-01-phone-browser-webview-design.md`
- 계획: `docs/superpowers/plans/2026-11-01-phone-browser-webview.md`
- 표면 모델: 폰은 두 겹 — DeX 그리드 = Termux 네이티브 jkdesktop, JK 앱(jkapk) =
  얇은 APK 클라이언트. WebView는 Android View라 Termux 네이티브 창에 심을 수
  없다 → 브라우저 = jkapk 안 **BrowserActivity**, DeX에서 별도 Android 윈도우.
- Windows jkapp_browser(CEF)는 WIN32 게이트 유지 — 폰 재용 불가 원존 유지.

## 2. 실측 원존 (빌드 계보 — 2026-11-01)

### 2.1 핀 계보 (조달 사슬 — fail-closed 원문 전원 수취)

1. GV **159.0.20261022150525**(최신 안정판, 2026-10-22) → `checkDebugAarMetadata`
   **11건 원문**: "requires compile against 37 … :app compiled android-34" +
   androidx.core **1.19.1** 끌당(AGP **9.1.0** 이상 요구 원문 — AGP 표 실측
   검색: compileSdk 37 = AGP 9.1.1+, 36.1 = 8.13, 36 = 8.9.1, 35 = 8.6.0).
2. 우회 플래그 시도 원존: `android.suppressUnsupportedCompileSdk=37` +
   `android.experimental.disableCompileSdkChecks=true`(AOSP CheckAarMetadataTask
   계약 근거) → compileSdk 37 문제는 소각·**androidx.core min AGP 검사는
   플래그 밖** — 결국 GV 핀 다운 퇴로. **두 플래그는 시도 후 소각**(원문 보존).
3. GV **141.0.20250806102122** 핀 — AAR metadata 직측(`minCompileSdk=1` 원문 —
   GV141/140 AAR 온전 다운로드 후 unzip -p 검증, 159와 비교 원존).
   GV141 POM: kotlin-stdlib **2.1.21**(컴파일러 1.9.24와 불호 — "metadata 2.1.0,
   expected 1.9.0" 원문) → stdlib **1.9.24 극선**(resolutionStrategy force).
   androidx.core **1.16.0** 끌당 — minCompileSdk **35**·min AGP **8.6** 직측 원문.
4. 최종 조합: **AGP 8.5.2→8.6.0 반계단 업 + compileSdk 35 + GV 141 +
   stdlib 1.9.24 극선** — Gradle 8.9 dist 호환, 플래그 0.

### 2.2 platform 조달 원존 (sdkmanager 편도 2회 실패 → 직배치)

- `sdkmanager "platforms;android-37"` **"Failed to find package" 2회 원문**(index
  listed 상태와 불일치). → `platform-37.2_r02.zip` 직배치
  (sha1 d52b3062af96f7c4dfdca5a479cd1db33ae83762 등어) + platform-35
  (`platform-35_r02.zip`).
- **함정 원존(직배치 2건)**: ①zip에는 `package.xml`이 없음 — AGP "Failed to find
  Platform SDK with path: platforms;android-37" → android-34 설치판을 템플릿으로
  package.xml 복제+경로 정정. ②copy한 package.xml의 **`<api-level>34</api-level>`
  잔존** → AAR 검사가 "compiled against android-35"라고 인쇄하면서도 계속 FAIL
  (좌표는 경로 문자열이고 실 API 등급은 package.xml 내부에서 판독하는 함정
  — api-level 정정 후 소각). ③zip의 source.properties는 `${PLATFORM_VERSION}`
  템플릿 — 치환+ApiLevel 정리 원존. ④android-37.2 dir명 → android-37 rename.

### 2.3 빌드 환경 원존 (T1 재확보)

- gradle 빌드는 PATH에 **msys64/ucrt64/bin** 필요 — vulkan-shaders-gen host DLL
  (RC=127 원문, PATH 부여 후 RC=0 — 조달기 §9 gradle 주석 원존 재확보).
- gradle에 `-j3` 없음(ninja 습관 혼입 원문 — `--max-workers=3`).
- gradle wrapper = `build/android-sdk/wrapper/gradlew.bat -p engine/android/jkapk`.
- **함정 원존(런2 실측)**: 실행 중 probe 편집 금지 — bash는 스크립트를 증분 파싱
  하므로 편집으로 줄이 밀리며 **팬텀 문법 오류**("syntax error near unexpected
  token `(' " — 실제로는 전부 정상 문장)가 뜹니다. 런2 상실: scp rc=0 후 수확
  leg 미수행·md5 게이트 미수행 — 원격 md5는 ssh 직측으로 별도 봉인(243cc2f8
  등어). 이후 probe는 "실행 중 편집 금지" 룰 원존.

### 2.4 산출 크기 실측 (판정 축 ①)

| 형태 | bytes | 비고 |
|---|---|---|
| WebView판(HEAD, e9a790c 포함) | 480,032,757 | md5 29158786d8e538dbb758627985edfeab |
| 이원판(T5 — GV 141 공존) | **654,712,394** | **+174.7MB가 GeckoView AAR 비용** |

- 네이티브 증분 원존: whisper_jni.c.o 재컴파일 2026-11-01 11:59:19(e9a790c 소스
  11:34:44 → 이후 — 음성 라인(jkengine-89) 판 요청과 동일 판 계보).

## 3. 측정표 (판정 게이트) — **폰 실측 원문 확정(2026-11-01 13:55)**

수확 원문: `engine/tmp/browser_measure/`(무추적 — md5 봉인 원문)
webview 2런(md5 3e5bfdb2…/68806129…) + geckoview 1런(md5 b2a25804…) — 전페이지
`ok:true`(gecko asset 대응 개봉도 성립 — ok=false 결측 0).

### 로드 시간(폰 실측 — 단위 ms, 단순 오프라인/네트워크 표층 기준선)

| 페이지 | WebView 런1 / 런2 | GeckoView 141 |
|---|---|---|
| p1(정적) | 176 / 116 | **53** |
| p2(500행 DOM) | **180 / 193** | 267 |
| p3(3단 타이머) | 99 / 109 | **74** |
| example.com(네트) | **44 / 88** | 42 |
| firstContentMs | 2/2·2/1·6/14 | **-1 (등가 계약 부재 — 정직 결측)** |

**판독**: 전 페이지 소음 띠가 양축 교차(gecko가 p1·p3·example 우위, p2 DOM
렌더는 webview 우위) — **표층 성능 = 동급, 결정 축 아님**. firstContent는
gecko 등가 계약 부재로 비교 불가(정직 결측 원존).

### 판정 근거 요약 (사용자 결정 재료)

| 축 | WebView | GeckoView 141 |
|---|---|---|
| ① 조달/크기 | **0MB(시스템 소유·기기 업데이트)** | +174.7MB(AAR) — §2.4 실측 |
| ② 표층 성능 | 동급(위 표) | 동급 |
| ③ 에이전트 표면 | **evaluateJavascript 즉발+CDP 원격 개방(T3)** — 에이전트 계약 최단 | delegate 계약 — link/DOM 회독은 WebExtension 별도 = **갭** |
| ④ 인증 | FOR_APP opt-in(임의사이트=FOR_BROWSER는 특권 등급 갭) | **패시키 네이티브(임의사이트)** |
| ⑤ 결제 | 라인 밖 — Custom Tabs 홉아웃+사람 하드게이트 원장행 | 동일 |

**작성부 권고(측정표 판독 기준)**: WebView 우세 — 0MB·에이전트 CDP 계약·성능
동급. GeckoView는 "임의 사이트 패시키가 실제 필요가 될 때" 재판정(그때도 GV
최신화에 AGP 9.1 트둥 업 수반 — §2.1 계보 원존).

**★판정 확정(2026-11-01 사용자 선언 "결론에 동의해요")**: **브라우저 엔진 =
WebView 기본 확정** — GeckoView 토글은 퇴로/비교판으로 유지(삭제 없음 — 같은
APK 공존·측정 하니스 그대로). 판정 근거 = §3 측정표 원문(표층 동급·GV +174.7MB
·에이전트 CDP 축 우위). **잔여 EYES(별행)**: webauthn.io 실 낭인 패시키(인증 축
실측)·브라우저 일상 체감 — 인증 축에 실제 필요 시 재판정 축.

## 4. EYES 대기 (사용자 선언만 결제)

1. 폰 BrowserActivity 렌더·탐색 체감(webview판)
2. [GeckoView] 토글 전환 체감
3. webauthn.io 실 낭인 패스키(인증 축 실측 — 사용자 수수)

## 5. 라인 밖 원장행

- DeX 그리드 아이콘 = console-app `am start` 래퍼(별도 라인).
- /talk → 브라우저 개봉 leg(별도 라인 — 에이전트 연결 계약).
- 결제·Custom Tabs 홉아웃 + **에이전트 자율 결제 금지·사람 승인 하드게이트**(별도 라인,
  사용자 착수 지시 대기).
- 조율 계약(jkengine-89 세션): 사용자 설치 가능 판 덮어쓰기 전 통지·시험용 별도
  파일명(PHONE_BROWSER_APK_REMOTE env — probe 원문).

## 6. 커밋 사슬

e4a02ad T1 뼈대 → 5f41c88 T2 측정 → 7db1cc3 T3 webkit 봉입 → 6b2b25f T4 probe
→ a367679 T5 GeckoView 교체판 (핀 계보 §2.1) → 7f1e890 T4 r1(원격 파일명 env+
조율 계약 — **phone_browser.sh 실행 중 편집 금지 원존 §2.3**)