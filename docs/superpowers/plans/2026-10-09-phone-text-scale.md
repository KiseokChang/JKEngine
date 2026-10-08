# POSIX 텍스트 스케일 수리 구현 플랜

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to execute this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 폰(posix)에서 텍스트가 `text.font_scale`을 글리프 스스로 따라한다 — 클라 글리프 벡터 결선 + 비트맵 폴백 셀 확대 + posix 기본 배율 1.5 + 크롬 타이틀 밴드 개선.

**Architecture:** 결함은 "셀 기하만 스케일, 글리프는 비트맵 고정" — 원인 후보가 2갈래(설정 미도달 vs DrawGlyph 실패)이므로 T1로 근원을 코드 원문+실측으로 단정한 뒤 T2에서 결선한다. T2 = 벡터 아틀라스 1순위(docs/63 기존 경로, Windows와 동일) + 실패 폴백을 비트맵 글리프 nearest 셀 확대로 값싸게 봉합 + posix 미설정 기본 1.5. T3 = 서버 크롬 타이틀 밴드(설계상 비트맵 유지 — "신설 글리프 엔진 없음" 존중)를 셀 메트릭 연동으로 확대. 와이어 접촉 0.

**Tech Stack:** C++(jkcore JKDC/JKTextAtlas, jkclient, jkserver), stb_truetype(헤더온리 — posix 링크 의존 없음, 실측 확인됨), posix selftest 케이스, bash probes(ffmpeg x11grab 캡처 선례).

**Spec:** docs/superpowers/specs/2026-10-09-phone-text-scale-design.md

## Global Constraints

- **계약 무변**: 와이어 프로토콜·클라 CommitSurface 계약 접촉 0. 컴포지터/더티프레젠트(docs/85 배선) 접촉 0.
- **Windows 무회귀**: Windows 기본 배율 1.0 유지(셀 {8,16,16})·selftest 캐논 518 유지 계보. 글리프 결선·폴백 수정은 Windows 경로에 **동작 변화 없어야**(아틀라스 정상이면 기존 블릿 그대로).
- **posix 기본 font_scale = 1.5** (settings 미설정 기본 — 스펙 결정. Windows는 1.0).
- **크롬 타이틀 벡터화 불포함**(새 글리프 엔진 신설 금지 — 기존 비트맵 경로 확대만).
- 캐논 계보 기록 의무(현 Win 518/WSL 495/posix 226/폰 495 — docs/85 §2.1).
- 커밋 전 rev-parse 실측, amend 금지, origin main 직행 (확립 관행).
- **가짜 결제 기록 금지** — 폰 결제는 사용자 육안 선언만.
- 폰 ssh 수속 계약 (docs/81 §6 원문): PHONE_HOST 환경변수(기본값·기록 금지 — IPv4 리터럴 커밋 grep 게이트), `$TMPDIR`, pkill 브래킷 `'[j]kdesktop'`, 원격 스크립트는 **파일로**(`$TMPDIR` 두고 `sh` 실행, 인라인 argv에 킬 대상 문자열 원문 금지 — 자기 소각 사건 원장) + `rm -f -- "$0"` 자기 소각 + REMNANT 검사, tar 오염 게이트(배포 대상 소스 더러우면 `git show "HEAD:$f"` HEAD blob 스테이징), `< /dev/null` 금지, `$TMPDIR` 임시.
- 폰 캡처 = `ffmpeg -f x11grab` (ImageMagick `import`는 폰 libheif x265 심볼 파단 — 사용 금지).
- vplayer 테스트 미디어: `i:\@keep` 영상(19금) 테스트 사용 금지 — 이 라인은 무관하나 관행 승계.
- posix 빌드 `$?` 파이프 함정: `sh build.sh | tail` 금지 — 풀 로그 리다이렉트 + grep error. WSL selftest 출력은 **WSL 내부 리다이렉트**.
- STT/STB: JKTextAtlas는 stb_truetype(헤더온리) — posix 빌드에 폰트 라이브러리 링크 의존 없음(스펙 이하 원문 확인: `STB_TRUETYPE_IMPLEMENTATION lives in JKTextAtlas.cpp`).

---

### Task 1: 근원 규명 — posix 클라 글리프가 비트맵으로 떨어지는 지점 단정

**Files:**
- Create(진단 필요 시): `engine/tools/probes/wsl_text_scale_diag.sh` (WSL 서버+클라 부팅 후 캡처 — 선례: `wsl_dirty_present.sh`·capture 계열)
- Modify: 없음(소스 무변경 — 진단 설비+원장만). 진단용 임시 코드가 필요하면 커밋하지 말고 로컬 재현 후 되돌린다.

**Interfaces:**
- Consumes: 스펙 실측 정리 4행; `JKDC::DrawGlyph` 요구 3조건 (`engine/src/JKDC.cpp:198` — `textAtlas_ && textCache_ && backend_`); 클라 아틀라스 설치 원문 (`engine/src/client/JKClientApplication.cpp:195-218` — fontPath = `jk::text::ResolveDesktopFontPath()`, 빈 경로/Init 실패 시 stderr 경고 1행 후 비트맵 유지); 설정 파일 위치 (`engine/src/JKTextAtlas.cpp:39-44` — exe dir + `state/settings.json`, posix `/proc/self/exe` 기반 — CWD 무관, `engine/src/fs/JKFs_posix.cpp:23-37`).
- Produces: **원장 표 1건** (리포트 §결론): 결선 지점 = [①설정 미도달(클라 exe dir settings 부재/내용 다름) ②font_path 빈→경고+비트맵 ③Init 실패(freetype/stb·폰트 파일 불량) ④DrawGlyph 내부 실패(EnsureGlyph/GetImage/BlitTexture) ⑤클라가 다른 dc_ 인스턴스(설치 안 된 인스턴스)로 그림] 중 정확 1개 이상 — 각 후보에 대한 증거(코드 원문+행번호+실측 로그)와 **posix 공통 여부**(WSL 동형 재현 판정) + WSL/폰 캡처 영수증.

**진단 설계 (후보별 증거 수집):**

| 후보 | 단정 방법 |
| --- | --- |
| ① 클라 설정 미도달 | 클라 프로세스 안에서 실제 경로 인쇄(진단 재현): `GetExecutablePath()`+settings 존재/내용 대조 — 서버가 읽은 것과 **같은 파일인지**. 폰: 클라 exe 위치(`which jkapp_*`류)/서버 위치 실측. |
| ② font_path 빈 | settings.json에 `text.font_path` 있음/없음 ×클라 해석 — 폰 현 settings는 font_path 포함(1.9 시험 부기) — **시험 시점 부속 정보로 재확인**. |
| ③ Init 실패 | 폰/WSL에서 클라 stderr을 **파일로 수집**(클라 스폰 경우 stderr 리다이렉트 — 서버 jksrv.log와 별개 파일). 경고 3종("font files missing"/"no vector font configured"/"vector font init failed") 어느 것이 나오는지. |
| ④ DrawGlyph 내부 실패 | ①②③ 배제 후: 임시 진단 인쇄 1행(로컬, 커밋 안 함) 또는 EnsureGlyph 반환 단정 — 폰트 파일에 cp 래스터 성공 여부. |
| ⑤ 다른 dc_ 인스턴스 | 모듈이 자체 JKDC를 만들어 dc_ 교체하는지 소스 추적(`SetTextAtlas` 호출처 전수 — JKApplication.cpp:118-157·JKClientApplication.cpp:200 외에 있나). |

- [ ] **Step 1:** WSL 클라 캡처 진단 — WSLg 서버+지뢰찾기 클라 부팅, 클라 stderr 수집 + 캡처로 WSL 클라 글리프 비트맵/벡터 판정(**posix 공통 여부**). `wsl_text_scale_diag.sh` 파일로 (REMNANT 검사는 WSL쪽도 선례 승계).
- [ ] **Step 2:** 폰 A/B 재현 — settings `text.font_path` 유/무 ×`font_scale=1.9` 조합 2레그 캡처(기존 영수증 `engine/tmp/desk_s10.png`(1.0 정상)·`desk_s19.png`(1.9 파손) 대조). 파일 스크립트+`rm -f -- "$0"`+REMNANT. 레그 조건·결과를 리포트 원문 기록. **시험 후 settings를 1.0·font_path 유지 상태로 복원**(부팅 확인 BOOT-OK).
- [ ] **Step 3:** 원장 표 확정 — 5후보 판정표(증거+행번호+posix 공통 여부)+T2에 넘길 결선 지점 1건 명시. 리포트 `task-1-report.md`.
- [ ] **Step 4:** 커밋 `probe(server): 텍스트 스케일 진단 스크립트 (T1)` (진단 probe 신설 시; 소스 무변경이면 probe만 커밋)

---

### Task 2: 클라 결선 수리 — 벡터 아틀라스 실사용 + 비트맵 폴백 셀 확대 + posix 기본 1.5

> T1 원장 표를 읽고 시작 — 추측 금지, 단정된 지점을 수리.

**Files:**
- Modify: `engine/src/JKTextAtlas.cpp:142-163` (`GetCellMetrics` — 미설정 기본 배율: `#if defined(_WIN32)` 1.0, 그 밖 1.5 — 컴파일타임 기본 헬퍼(예: `text::DefaultFontScale()`)로 분리해 selftest에서 직접 단정)
- Modify: `engine/src/JKDC.cpp:138-190(폴백 3함수)·226-262` (비트맵 폴백 nearest 셀 확대)
- Modify: `engine/src/client/JKClientApplication.cpp` + `engine/src/JKApplication.cpp` (T1 단정 지점 결선 — 예: 설정 리졸버 공용화/경고 가시화 1회 보강; T1 결과에 비례, 최소화)
- Test: `engine/tools/posix_selftest/main.cpp` + `engine/src/main.cpp` (쌍둥이, 신설 케이스 계열 `2t`)

**Interfaces:**
- Consumes: T1 원장 표 (결선 지점); 기존 `JKTextAtlas::Init/InitFallback/EnsureGlyph`·`JKResourceCache`; `CellMetrics{engW, hanW, cellH}` (hanW=2×engW 불변식 — docs/65 O4 원문 `engine/src/JKTextAtlas.cpp:129-133`).
- Produces: ①`jk::text::DefaultFontScale()` (샘플 시그니처 — `float DefaultFontScale();` Win 1.0 / posix 1.5, `GetCellMetrics` 미설정 분기가 소비) ②비트맵 폴백이 셀 크기(`GetCellMetrics()`)에 맞춰 nearest 확대로 그린다 — `PutEngGlyph8x8/8x16`→engW×cellH, `PutHanGlyph16x16`→hanW×cellH. 고정 8/16 픽셀 드로잉은 **좌표 매핑으로 자연 스케일**(예: 목표 셀 w,h에 대해 `col*srcW/w` 샘플) — 텍스처 신설 불요(DC 픽셀 드로잉 경로 유지). KSSM 쌍은 반올림 좌표에서 4/9px 오차(8→15px 등 홀수 폭)를 허용한다(비트맵 폴백 한계 — 스펙 fail-safe 명시). ③폴백 진입 빈도 경고(진단 설비 — 1회성 stderr 라인, 스팸 없음: 프로세스당 1회, static 플래그).

**비계측 수리 원문(스펙 결정 4):** 아틀라스 미장착으로 폴백 비트맵을 그리기 시작하는 첫 시점에 1회: `Warning: vector atlas inactive; drawing stretched bitmap glyphs (font_scale=…)` — 원인(설정 미도달/Init 실패/미커버)을 짧게 병기. DrawGlyph 실패→폴백 반복 중 중복 인쇄 금지.

- [ ] **Step 1:** 실패 케이스 먼저 — selftest `2t` 계열: ①`ComputeCellMetrics(1.5f)` 산술 단정(engW=12·hanW=24·cellH=24) ②미설정 기본 배율 플랫폼 단정(**posix/WSL/폰 축 기대 1.5, Win 축 기대 1.0** — 쌍둥이의 셀 기대값은 플랫폼별 상수로 컴파일타임 분기) ③비트맵 확대 순수 헬퍼 케이스(신설 시 — 확대 샘플 인덱스 3가지: s=1.0 동일·1.5·2.0). 4/9px 폴백 오차 수용 원문 코멘트.
- [ ] **Step 2:** 구현 — DefaultFontScale + GetCellMetrics 기본 분기 + 폴백 3함수 셀 확대 + T1 단정 지점 결선 + 경고 1회.
- [ ] **Step 3:** 3축 selftest 실측 — Win 518(N)/WSL 495(N)/posix 226(N) 계보 기록(WSL은 **WSL 내부 리다이렉트**). Windows 클라 벡터 경로 원문 재확인(기존 블릿 무변).
- [ ] **Step 4:** 커밋 `fix(client): posix 클라 글리프 벡터 결선+비트맵 폴백 셀 확대+기본 1.5 (T2)`

---

### Task 3: 크롬 타이틀 밴드 개선 — 서버 측 비트맵 경로 셀 메트릭 연동

> 스펙 결정 1: 크롬 벡터화 불포함 — **기존 비트맵 경로에서 밴드 높이·글자 크기 확대만**. "새 글리프 엔진 없음" 설계 원문 유지.

**Files:**
- Modify: `engine/src/server/JKWindowServer.cpp` (타이틀 밴드 그리기+텍스처 캐시 경로 — 원문 코멘트 앵커: 크롬 타이틀 글리프 경로 = Utf8ToKssm + JKDC 비트맵 폰트, 한글 16px/영문 8px 고정 — `docs/` app-tool-hub §5 스펙 착수 실측 행. 구현자가 8100대 실측 후 행 고정)
- Test: `engine/tools/posix_selftest/main.cpp` (순수 부분만 — 밴드 높이 산식이 순수 함수면 케이스 1건; 아니면 수기 실측 명시)

**Interfaces:**
- Consumes: `text::GetCellMetrics()` (T2 기본 분기 — posix 서버도 1.5); 기존 타이틀 텍스처 캐시 경로(문자열→비트맵 텍스처).
- Produces: 타이틀 밴드 높이 + 타이틀 글자 크기가 배율을 따른다: **밴드 높이 = 최소 현행값 보장 + `cellH` 배수 연동**(구현자 실측 기반 — 현행 밴드 높이 상수를 찾아 `max(현행, cellH + 여백패드)`), 타이틀 글자는 기존 비트맵 글리프를 타이틀 셀 크기로 nearest 확대 블릿(T2 폴백과 동일 기법 — 같은 인쇄기 재용). 폰 1.5 기본 → 타이틀 ~1.5x, Windows 1.0 → **현행 그대로(픽셀동일)** — Windows 타이틀 무변이 배선의 단정 대상.

- [ ] **Step 1:** 원문 실측 — 밴드 높이 상수·타이틀 글자 인쇄 원문 행 확정(리포트 기록), Windows 현행 불변 설계 단정 방식 명시(1.0 배율 분기 동일성 — 최선: 산식 자체가 s=1.0에서 현행값과 등호).
- [ ] **Step 2:** 구현 — 밴드 높이+타이틀 글자 셀 연동. 텍스처 캐시 키에 배율 반영(문자열×크기 — 스타일 캐시 계조 원장 존중).
- [ ] **Step 3:** 3축 selftest 실측(캐논 유지 확인) + Windows 창 타이틀 수기 확인(픽셀동일).
- [ ] **Step 4:** 커밋 `feat(server): 크롬 타이틀 밴드 셀 메트릭 연동 (T3)`

---

### Task 4: 폰 실측 결제 관문 — 기본 1.5 출하+A/B 캡처

**Files:**
- Create: `engine/tools/probes/phone_text_scale.sh` (선례: `phone_dirty_present.sh` 승계 — tar 게이트+REMNANT+폰 selftest)

**Interfaces:**
- Consumes: T2/T3 배포 완료 상태; 폰 캡처=ffmpeg x11grab 선례(PHONE_HOST 환경변수만); 기존 영수증 `engine/tmp/desk_s10.png`(1.0)·`desk_s19.png`(1.9 구판) — 신구 비교 축.
- Produces: ①폰 재배포(9-10분 리빌드)+폰 selftest 캐논(2t 포함 신계보) ②settings **font_scale 키 삭제(=1.5 기본 발동)+font_path 유지** 상태 캡처 desk_def15.png ③1.9 재시험 캡처 desk_s19b.png(클립·밀려남 해소 단정 — 구판 영수증 대조) ④지뢰찾기 launch→제목/버튼/숫자 실측 ⑤크롬 타이틀 밴드 개선 캡처 ⑥육안 게이트 산출(결제 기록 금지) — **사용자 육안 선언만 최종 관문**.

- [ ] **Step 1:** probe 스크립트 — 폰 재배포(tar 오염 게이트+HEAD blob 스테이징)→리빌드→폰 selftest 4축 계보→1.5 기본 부팅→x11grab 캡처(1920px)→launch 지뢰찾기→세부 캡처→1.9 부팅 재시험→1.5 기본 복원 부팅 BOOT-OK. 원격 스크립트 파일 규약+자기 소각+REMNANT 전부.
- [ ] **Step 2:** 실측 — 캡처 3종을 `engine/tmp/`로 회수(파이프 단열 — ls등 혼입 금지, 스펙 A/B 축 원문). **사용자에게 육안 요청을 알린 후 대기** — 결제는 사용자 선언.
- [ ] **Step 3:** 커밋 `test(server): 폰 텍스트 스케일 실측 probe (T4)`

---

### Task 5: as-built docs/86 + 원장 착지

**Files:**
- Create: `docs/86_text_scale_asbuilt.md` (docs/85 문체 계승)
- Modify: `docs/85_dirty_present_asbuilt.md` §4 D-계열 후속 포인터(신관측→docs/86) 1-2행

**Interfaces:**
- Consumes: T1-T4 전부 + 사용자 결제 선언(별도 커밋 시점 — 컨트롤러 운용).
- Produces: §0 문서 체계·§1 배선 원장(rev-parse 전체 SHA)·§2 캐논 표+폰/WSL 캡처 영수증(좌표/확대 산술)·§3 함정 원장(T1 5후보 판정표·비계측 stderr 이야기·4/9px 폴백 오차·KSSM 쌍 정합)·§4 deferred(DPI·X11 폰 상관·DeX fit-scale)·§5 사용자 결제 게이트·§6 커밋 원장(IP grep 게이트 병기).

- [ ] **Step 1:** docs/86 작성 — 커밋마다 rev-parse 실측 후 기록(전체 SHA).
- [ ] **Step 2:** 커밋 `docs(font): posix 텍스트 스케일 수리 as-built (T5)`

---

## 사용자 게이트 요약 (플랜 밖 정리 — 컨트롤러 운용)

1. T4 후 사용자 폰 육안 결제 — 텍스트 확대·클립 해소·깜빡임/찌꺼기 없음 선언만 결제.
2. Windows 타이틀 밴드 무변(픽셀동일) — T3 리뷰어 검증 축.