# 데스크탑 벡터 폰트 2단계 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** docs/63 §6 2단계 3종 — 글리프 텍스처 LRU/상한, 보조 폰트 체인(`text.font_fallback`), `text.font_scale`(셀 확대, 옵트인) — + 1단계 잔여 minors 소각.

**Architecture:** JKTextAtlas에 (1) LRU 폐기(cap 초과 시 오래된 글리프 텍스처를 캐시에서 UnloadImage), (2) 보조 폰트 face(2차 폰트 — 1차에 없는 글자만 승계, 캐시 키 접두어 분리), (3) 셀 메트릭 진실원(`jk::text::CellMetrics()` — scale 1.0 = 현행 8/16/16 동일)을 얹는다. JKDC와 코어 위젯은 리터럴 8/16을 메트릭 조회로 바꾼다. 터미널·이미지 쪽 8/16은 폰트 메트릭이 아니므로 접촉하지 않는다.

**Tech Stack:** C++(MinGW/ninja), stb_truetype(단일 IMPL — JKTextAtlas.cpp 유지), JKResourceCache, docs/54 설정 허브, ImGui(settings GUI), PS5.1 프로브.

**Spec:** `docs/63_desktop_vector_font.md` (§4.1 폰트 소싱, §6 2단계 로드맵) — 스펙이 계획에 우선.

## Global Constraints

- **메트릭 진실원**: `jk::text::CellMetrics()`(함수 로컬 static, 1회 산출) — `{engW, hanW, cellH}`. scale 1.0에서 정확히 `{8, 16, 16}` — 기본값은 **오늘과 픽셀 동일**, 모든 기존 프로브 무수정 GREEN 유지.
- **font_scale은 옵트인(기본 1.0)**: 창·태스크바·데스크탑 기하(창 크기/기본 위치)는 스케일하지 않는다 — 텍스트 셀·텍스트 인지 레이아웃만. >1.25에서 레거시 앱(OccUI/Jango/Insa 등 수제 레이아웃) 넘침은 **문서화된 한계**(docs/63 §10 기록).
- **터미널/아이콘 8·16 접촉 금지**: `JKTerminalGrid.cpp`(터미널 셀), `MineSweeperApp/ClientMineSweeperApp kIconSize`, `JKScrollBar.cpp`(화살표 아이콘), `apputil::PadRight(s, 16)`(글자 수) 등은 폰트 메트릭 아님 — 손대지 않는다. vfont/JKVectorFont 계통도 무관.
- **폴백 체인**: 1차 폰트에 cp 없음(FindGlyphIndex==0)만 2차로 승계. 2차도 없으면 비트맵. 미설정=끔(플랫폼 기본 없음). 캐시 키 충돌 금지 — 보조 페이지 키는 별도 접두어.
- **설정은 재시작 적용**(docs/54 허브 관행) — font_fallback/font_scale 모두 `applies_on_restart` 에코.
- **설정 값 상한**: font_path/font_fallback 300자, font_scale float 1.0–3.0 범위 밖 거부(bad_value).
- **외부 의존 0** — 새 라이브러리 금지(stb_truetype만, IMPL 단일 정의 JKTextAtlas.cpp 유지).
- **프로브**: PS5.1 ASCII-only, `> log 2>&1` 리다이렉트, ×2 연속. 서버 자가 스폰 프로브는 jkwinserver 내려간 상태에서 실행(싱글 인스턴스 가드, docs/63 §9.2).
- **커밋**: 한국어 컨벤션 + `Co-Authored-By: Claude Code <noreply@anthropic.com>`, `git add`는 파일 단위(절대 `-A`).
- **라이브 데스크탑이 exe/dll을 점유하면 링크 보류** — 컴파일 GREEN이면 진행 가능, 링크 실패는 세션 말미/재기동 후 소각(환경, 결함 아님).
- **KSSM 2바이트 조합형 인코딩 single source of truth** — `KssmCodepointToUnicode` 계약 불변.

---

### Task 1: 글리프 텍스처 LRU/상한

**Files:**
- Modify: `engine/include/JKTextAtlas.h` (상수+LRU 멤버)
- Modify: `engine/src/JKTextAtlas.cpp` (EnsureGlyph LRU, 폐기)
- Modify: `engine/tools/probes/jktext_probe.cpp` (체크 2건 추가 — 상한 동작)

**Interfaces:**
- Consumes: `JKResourceCache::UnloadImage(const std::string& key)` (기존), `CreateImageFromRGBA` (기존)
- Produces: `JKTextAtlas` 내부 동작 변경만 — 공개 API 시그니처 무수정 (EnsureGlyph/PageKey/GlyphSrc 계약 유지)

- [ ] **Step 1: UnloadImage 의미론 실측** — `engine/src/JKResourceCache.cpp`의 `UnloadImage`가 `images_`와 `pending_`을 **둘 다** 제거하는지, 업로드된 백엔드 텍스처(handle)를 파기하는지 확인. pending만 제거되고 텍스처가 남는 구조면 그 결함을 이 태스크에서 함께 고친다(UnloadImage가 pending에 있으면 텍스처 파기 없이 pending 제거, images에 있으면 백엔드 DestroyTexture 호출 — 현행 구현에 맞춰 판정).
- [ ] **Step 2: 상한+LRU 구현** — `JKTextAtlas.h`에 `static constexpr size_t kMaxGlyphTextures = 1024;`(주석: 글리프당 ~1KB → 호스트당 ~1MB 상한). `registered_`(vector<uint64_t>)를 LRU로 사용: EnsureGlyph **기존 히트 시** 해당 키를 벡터 끝(MRU)으로 이동, **신규 등록 후** `registered_.size() > kMaxGlyphTextures`면 맨 앞(LRU) 키를 폐기 — `cache->UnloadImage(PageKeyOf(evicted))` 후 벡터에서 제거. 폐기 헬퍼 `EvictOldest(JKResourceCache*)` private 추가. 키 복원은 `(key >> 32, key & 0xFFFFFFFF)`.
- [ ] **Step 3: 프로브** — `jktext_probe.cpp`에 2체크: (a) 동일 fg로 kMaxGlyphTextures+개 서로 다른 cp를 EnsureGlyph 후 `registered_.size() == kMaxGlyphTextures` 단정 — 단, cp 도메인: ASCII 0x21–0x7E(94개) × 여러 fg(예: 0x000000/0xFFFFFF/0xFF0000/0x00FF00/0x0000FF — 5색 = 470) + KSSM cp 몇 개로 1024 초과 채우기가 어려우면 상수를 테스트 주입 가능하게 (`kMaxGlyphTextures`를 그대로 쓰되 프로브에선 fg 12색 × 94cp = 1128 > 1024로 충분 — fg는 uint32이므로 임의 색 12개 사용 가능), (b) LRU 순서 단정: 캡 초과 직후 가장 오래된 (fg, cp)의 `GlyphSrc`가 empty(폐기)이고 재 EnsureGlyph가 true(재등록) — RecordingBackend 기존 선례 사용.
- [ ] **Step 4: 실행** — `g++` 빌드+실행 명령은 회귀 스크립트 선례(`engine/tools/probes/` 내 jktext 빌드 라인) 따름. PASS 확인.
- [ ] **Step 5: Commit** — `feat(text): 데스크탑 글리프 텍스처 LRU/상한 1024 (docs/63 §6)`

### Task 2: 보조 폰트 체인 (text.font_fallback)

**Files:**
- Modify: `engine/include/JKTextAtlas.h` / `engine/src/JKTextAtlas.cpp` (Face 리팩터링+InitFallback+키 접두어)
- Modify: `engine/src/JKDC.cpp` (DrawGlyph 체인)
- Modify: `engine/src/server/JKWindowServer.cpp` (settings_set/read/KV)
- Modify: `engine/src/apps/ClientSettingsApp.cpp` (텍스트 섹션 2행)
- Modify: `engine/tools/probes/jktext_probe.cpp` (체인 체크)
- Modify: `engine/tools/probes/probe_textfont.ps1` (설정 왕복 1체크)

**Interfaces:**
- Consumes: 1단계 `KssmCodepointToUnicode`(무수정), `ResolveDesktopFontPath` 패턴, docs/54 `text_font_path` 분기 선례(JKWindowServer.cpp:3786-3802, ClientSettingsApp.cpp:356-378)
- Produces: `jk::text::ResolveDesktopFallbackPath()`(미설정="" 반환), `JKTextAtlas::InitFallback(const std::string&)`, `EnsureGlyph(cache,fg,cp,useFallbackPage)` + `PageKey(fg,cp,useFallbackPage)` (기본 인자 false — 기존 호출부 무수정)

- [ ] **Step 1: Face 리팩터링** — JKTextAtlas 내부에 `struct Face { std::vector<uint8_t> data; std::unique_ptr<stbtt_fontinfo> info; float engScale, hanScale; int engBaseline, hanBaseline; };` 도입, `primary_`/`fallback_` 두 면. `Init`의 메트릭 산출 블록(로드+'M'/0xAC00 스케일+em 캡+베이스라인)을 `LoadFace(const std::string&, Face*, int engCellW, int cellH, int hanCellW)` 헬퍼로 추출 — primary 로드는 기존과 동일 계약(IsLoaded/실패 시 false). StrideOf/ScaleOf/BaselineOf/RasterizeGlyph는 face 선택자(`useFallbackPage` 또는 cp 커버리지)를 받아 확장.
- [ ] **Step 2: InitFallback + 체인** — `InitFallback(fontPath)`: LoadFace(fallback_, 동일 셀) 실패 시 false(체인 없음=폴백 비활성, 로그 1줄). `RasterizeGlyph`에 useFallbackPage 분기: primary `FindGlyphIndex==0`이고 fallback_.info 있으면 fallback face로 래스터라이즈(그래도 0이면 false=비트맵 폴백). **캐시 키**: `PageKey(fg, cp, useFallbackPage)` — fallback이면 `desktextf_%08x_%06x` 접두어 교체(충돌 봉쇄). `EnsureGlyph`/`GlyphSrc`에 useFallbackPage(기본 false) 전달 — 키가 다르므로 registered_에 별도 등록(동일 vector, 키 자체가 (fg,cp)라 (fg,cp,face) 3중 필요 → `registered_` 엔트리를 `(fg<<34)|(cp<<1)|fbBit`로 확장하거나 별도 vector — 구현자 선택, 문서화).
- [ ] **Step 3: JKDC 체인** — `DrawGlyph(p, cp, stride)`: 1차 EnsureGlyph 실패 시(현재 null→FlushUploads 재시도 후에도 실패) fallback 시도 — `EnsureGlyph(cache, fg, cp, true)` → 성공이면 `GetImage(PageKey(...,true))` 블릿. 둘 다 실패=false(비트맵 폴백). 호스트 배선: `JKClientApplication.cpp:200`/`JKApplication.cpp:126`/배너(JKWindowServer.cpp:5861-5868) — Init 성공 후 `ResolveDesktopFallbackPath()`가 비어있지 않으면 `InitFallback` 시도(실패는 경고 1줄, 체인 없이 계속).
- [ ] **Step 4: 설정 배선** — 서버: `text_font_fallback` settings_set 분기(빈 값=해제 허용, >300 bad_value, `applies_on_restart` 에코 — 3786 분기 옆 동일 패턴) + settings_read 항목 + LoadSettingsKv/WriteSettingsKv `"text":{"font_fallback":...}` (3591 인접). `ResolveDesktopFallbackPath()`: settings 직독("text","font_fallback"), **기본값 없음**(빈 문자열). GUI: ClientSettingsApp 텍스트 섹션에 2행(현재 경로 TextDisabled + InputText + Query::Set "text_font_fallback") — textFontBuf_ 선례에 fallbackFontBuf_ 추가.
- [ ] **Step 5: 프로브** — jktext_probe: primary=훅용 소형 폰트 경로 대신 **consola.ttf**(main.cpp:1741 선례 — 한글 글리프 없음을 `FindGlyphIndex` 선체크로 확인 후 사용, 하드코딩된 '한글 있음' 가정 금지) + fallback=malgun: (a) KSSM '가' EnsureGlyph(useFallbackPage=false)==false, (b) EnsureGlyph(...,true)==true, (c) PageKey 접두어 `desktextf_` 단정, (d) ASCII는 1차 성공 단정. probe_textfont.ps1: settings_set `text_font_fallback` → settings_read 왕복 1체크(서버 자가 스폰 하네스, 기존 백업/복원 재사용).
- [ ] **Step 6: Commit** — `feat(text): 보조 폰트 체인 text.font_fallback — 미커버 cp 승계+캐시 키 분리 (docs/63 §6)`

### Task 3: text.font_scale (셀 확대, 옵트인)

**Files:**
- Modify: `engine/include/JKTextAtlas.h` (CellMetrics+해석기)
- Modify: `engine/src/JKTextAtlas.cpp` (메트릭 단일 진실원)
- Modify: `engine/include/JKDC.h` / `engine/src/JKDC.cpp` (리터럴→메트릭)
- Modify: 위젯: `engine/src/JKMenu.cpp`, `engine/src/JKComboBox.cpp`, `engine/src/JKListBox.cpp`, `engine/src/JKEdit.cpp`, `engine/src/JKStatic.cpp`
- Modify: `engine/src/client/JKClientApplication.cpp`, `engine/src/JKApplication.cpp`, `engine/src/server/JKWindowServer.cpp` (Init 셀 인자 + settings 분기)
- Modify: `engine/src/apps/ClientSettingsApp.cpp` (GUI 3행)
- Modify: `engine/tools/probes/jktext_probe.cpp` (메트릭 체크)

**Interfaces:**
- Consumes: Task 2의 Face 구조(Init에 셀 인자 전달 경로)
- Produces: `jk::text::CellMetrics{int engW, hanW, cellH}` + `const CellMetrics& text::CellMetrics()`(함수 로컬 static — 프로세스당 1회, settings `text.font_scale` 직독). scale 기본 "1.0" → {8,16,16}.

- [ ] **Step 1: 메트릭 진실원** — `CellMetrics` + 산출식: `engW=max(4,round(8*s))`, `hanW=max(8,round(16*s))`, `cellH=max(8,round(16*s))`, s=설정 float(파싱 실패/범위 밖=1.0). `text::CellMetrics()` 함수 로컬 static(스레드 안전 C++11 초기화, 1회 산출 — MeasureText가 정적이라 이 경유). 주석: 재시작 적용, 프로세스당 1회.
- [ ] **Step 2: JKDC 리터럴 치환** — JKDC.cpp: `kTextCellH`(15)·MeasureText(50,57,59,65)·TextOut 진행(269,274)·TextOutInRect(282-283)·TextOutX(319,325)·EngPutCh cp<0x80 DrawGlyph stride 인자 → 전부 `text::CellMetrics()`. **MeasureText는 static 유지**(호출부 무수정), 내부에서 진실원 직접 조회. DrawGlyph/HanPutCh의 16 리터럴도 동일. 셀 메트릭이 아닌 8·16(`JKDC.cpp:180`의 PutEngGlyph 폴백 오프셋 등 비트맵 내부 픽셀 오프셋)은 손대지 않는다 — 폰트 메트릭과 비트맵 글리프 내부는 구분.
- [ ] **Step 3: 위젯 치환** — 텍스트 메트릭만: JKMenu.cpp:48(`label.size()*8`→`*m.engW`, 꼬리 16은 패딩→`m.engW*2` 판단 후 결정), 62-63(`maxLen*8`→engW, `items*16`→cellH), 103-104(8/16 패딩), 156/176(kItemH 16→cellH). JKComboBox.cpp:51(`items*16`→cellH), 97(`inner.h-16`→cellH 판단). JKListBox.cpp(행 높이 상수 — 구현부 확인 후 cellH). JKEdit.cpp xOf(바이트당 전진 8/16→engW/hanW) — 캐럿/선택 좌표 일관성 필수. JKStatic.cpp:33(`size.y=16`→cellH). 각 치환에 "폰트 메트릭 vs 레이아웃 여백" 판단 주석 — 여백 4/8px 등은 유지.
- [ ] **Step 4: 호스트 배선** — 3곳 Init: `textAtlas_->Init(fontPath, m.engW, m.h, m.hanW)`(m=text::CellMetrics()). 서버 settings: `text_font_scale` settings_set(float 파싱, 1.0–3.0 밖 bad_value, applied echo + applies_on_restart) + settings_read + LoadSettingsKv/WriteSettingsKv `"text":{"font_scale":...}`. GUI: 텍스트 섹션 3행(InputText 숫자, Enter→Query::Set "text_font_scale").
- [ ] **Step 5: 프로브** — jktext_probe에 2체크: scale 미설정 CellMetrics()=={8,16,16}, (b) 산출식 단정은 settings 직독이 개입하므로 **산출 함수를 순수 함수로 분리**(`ComputeCellMetrics(float s)` 노출) — ComputeCellMetrics(1.0)={8,16,16}, (1.25)={10,20,20}, (3.0)={24,48,48}, (0.5)={8,16,16}(하한 클램프는 s≥1.0만 허용이므로 0.5는 파싱 단계 거부 — 순수 함수엔 클램프만: 0.5→{8,16,16}). 기존 26체크+Task1·2 체크 무수정 통과(scale 1.0 = 동일 픽셀).
- [ ] **Step 6: 실행+회귀** — jktext_probe/jkedit_probe ×2 GREEN. (셀 확대 실물 확인은 눈확인으로 유예 — 기본 1.0이라 회귀 없음.)
- [ ] **Step 7: Commit** — `feat(text): text.font_scale 셀 확대 — 메트릭 진실원+코어 위젯 치환 (docs/63 §6)`

### Task 4: 잔여 minors + 프로브 보강 (배치)

**Files:**
- Modify: `engine/src/apps/ClientSettingsApp.cpp` (textFontBuf_ 300→301)
- Modify: `engine/tools/probes/jktext_view_probe.cpp` (음성 통제 자동화+span 단정)

**Interfaces:**
- Consumes: 1단계 view probe ReadPixels 포맷 일치 선례
- Produces: 없음(내부 보강)

- [ ] **Step 1: GUI 버퍼** — `char textFontBuf_[300]`→`[301]`(NUL 포함 — 300자 값 수용, 1단계 리뷰 유보).
- [ ] **Step 2: view probe 음성 통제 자동화** — jktext_view_probe 2페이즈: 페이즈 1(현행, 폰트 장착) ink/aa 단정 유지 + **span 단정 추가**(영문 글리프 잉크 폭 ≤ engW, 한글 ≤ hanW — 1단계 리뷰 유보), 페이즈 2(atlas 미장착 = 나쁜 폰트 경로 Init 실패) aa==0(비트맵 계단) 단정 — 기존 수동 음성 통제를 자동화.
- [ ] **Step 3: 실행** — ×2.
- [ ] **Step 4: Commit** — `test(text): view probe 음성 통제 자동화+span 단정+GUI 301 버퍼 (docs/63 §6)`

### Task 5: 전체 회귀 + docs as-built

**Files:**
- Modify: `docs/63_desktop_vector_font.md` (§10 as-built)

- [ ] **Step 1: 전체 회귀 ×2** — jktext_probe, jktext_view_probe, jkedit_probe, terminal_hangul_probe, terminal_hangul_view_probe, terminal_jamo_atlas_probe(링크 목록 docs/63 §9.2 준수), probe_textfont.ps1, probe_settings.ps1, probe_approval_overflow.ps1. 서버 자가 스폰 프로브는 jkwinserver 정지 상태.
- [ ] **Step 2: docs/63 §10** — as-built: 3 기능 구현 편차/한계(font_scale 옵트인·기하 무스케일 한계·비트맵 폴백 글리프는 1× 크기), 회귀 결과 표, 눈확인 항목(font_scale 실물).
- [ ] **Step 3: Commit** — `docs: docs/63 2단계 as-built — LRU/폴백 체인/font_scale (§10)`

## Self-Review

- **스펙 커버리지**: docs/63 §6 3항목(font_scale/보조 체인/LRU) + §7 백로그(GUI 301/span/음성 통제) 매핑 완료. registered_ 해시는 LRU 캡이 O(n)을 1024로 봉쇄해 **의도적으로 제외**(YAGNI — 계획서 판정). 비례폭 API는 스펙상 "별도 설계" — 2단계 이후 유지.
- **플레이스홀더**: 없음 — 각 Step에 파일·행·값 명시.
- **타입 일치**: `EnsureGlyph(cache,fg,cp,useFallbackPage=false)`, `PageKey(fg,cp,useFallbackPage=false)`, `CellMetrics{engW,hanW,cellH}`, `ResolveDesktopFallbackPath()` — 태스크 간 일관.