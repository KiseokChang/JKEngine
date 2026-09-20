# docs/63 — 데스크탑 벡터 폰트 전환 (JKDC::TextOut 관문 교체) (2026-09-21)

사용자 요구: "언제까지 비트맵 폰트만 쓸수는 없는데....."
브레인스톰(architectural 경로, 전 섹션 승인)으로 확정된 목표:

- **비주얼 품질** — 계단진 비트맵 글리프를 TTF(맑은 고딕 기본)로 교체
- **크기 자유도** — 1단계는 메트릭 동일(8×16)로 착지, 2단계에서 `text.font_scale`로 셀 확대
- **글자 커버리지** — 한자·특수문자 등 비트맵 폰트에 없는 글자도 폰트에 있으면 표시
- 폰트 소스: **설정 가능** (`text.font_path`, 기본 `C:\Windows\Fonts\malgun.ttf`)

부수 이유: `.spr` 비트맵 폰트 외부 파일 의존 제거는 docs/62 리눅스 포팅에도 직접 기여
(stb_truetype은 플랫폼 무관 — 폰트 레이어가 플랫폼 어댑터 목록에서 빠진다).

## 1. 현재 상태 (실측)

| 계층 | 폰트 | 비고 |
|---|---|---|
| 터미널 | **이미 벡터** — `JKGlyphAtlas`(stb_truetype) | monospace 셀 전용, (fg,bold) 페이지, 맑은 고딕 폴백 (docs/26) |
| 데스크탑 전반 | **비트맵** — `HangulManager` KSSM | 외부 .spr/fnt 폰트 파일, 영문 8×16/한글 16×16 고정 셀 |
| vfont 앱 | 레거시 .VFT 아웃라인 전용 (`JKVectorFont`) | 별개 계통, 이번 과제와 무관 |

- `JKDC::TextOut` 59개 호출 지점 × 20파일 — 전부 JKDC 한 관문으로 수렴
- 현재 글리프 출력은 **픽셀 단위 DrawPixel 루프** (`Put*Glyph`) — 텍스처 캐시·AA 없음
- `JKDC` 생성 지점 3곳: `JKClientApplication`(클라 앱), `JKApplication`(싱글 프로세스),
  `JKWindowServer`(배너/크롬 타이틀)
- KSSM↔UTF-8 변환은 이미 공용 유틸 (`JKHangulUtil.h`: `Utf8ToKssm`/`KssmToUtf8`)

## 2. 설계 — 접근법 A 확정 (관문 교체)

`JKDC::TextOut`의 진행 로직(바이트 순회, 영문 8px/한글 16px 전진)은 **그대로 두고**,
글리프 출력부만 아틀라스 블릿으로 교체. 59개 호출 지점 무수정.

### 신설 컴포넌트 — `JKTextAtlas` (jkcore)

- `Init(fontPath, engCellW=8, cellH=16)` — 폰트 로드 + 셀 메트릭/스케일 산출
- `EnsureGlyph(cache, fgColor, cp)` — 글리프를 RGBA로 래스터라이즈해 **색 구움 페이지**
  텍스처에 배치, JKResourceCache에 등록 (터미널과 동일한 "색 구움" 패턴 — 렌더
  백엔드에 per-blit tint가 없어서 fg색을 픽셀에 굽는 것은 이미 검증된 설계)
- `GlyphSrc(cp)` — 페이지 내 src rect (터미널 아틀라스와 같은 인터페이스 모양)
- 터미널 `JKGlyphAtlas`는 **건드리지 않음** — 회귀 리스크 완전 분리

### JKDC 수정

- `SetTextAtlas(JKTextAtlas*, JKResourceCache*)` 세터 신설
- 두 포인터 모두 있으면 글자를 아틀라스 경로로, 아니면 기존 HangulManager 비트맵 경로
- 기존 비트맵 코드는 **폴백으로 전부 보존** (삭제하지 않음)
- 3개 생성 지점에서 아틀라스 장착 (각 프로세스당 1회 Init)

### 레이아웃

고정 셀 격자 유지 — 영문 8×16, 한글 16×16 셀 안에 TTF 글리프 렌더. 전진 규칙이
비트맵과 동일하므로 `MeasureText`/`TextOutX`/모든 앱 좌표 계산 **무수정**.
비례폭(proportional)은 별도 과제 (2단계 이후).

## 3. 데이터 흐름 (글자 1개 단위)

1. 바이트 순회 (기존 로직 그대로): 영문 1바이트=8px, KSSM 2바이트=16px 전진
2. 코드포인트 변환: KSSM 2바이트 → `KssmToUtf8` 경유 Unicode cp (한글 완성형·한자·
   특수 포함), 영문은 ASCII 그대로
3. 아틀라스 조회 `EnsureGlyph(fg, cp)`:
   - **글리프당 소형 텍스처**(stride×16px) — 캐시 키 `desktext_%08x_%06x` (fg, cp),
     수요 시 1회 래스터라이즈 후 JKResourceCache 등록 (색 구움, 터미널과 동일)
   - [구현 편차 2026-09-21] 원설계 "fg 색당 tightly-packed 페이지" → 글리프당
     텍스처로 단순화. 근거: 페이지는 새 글리프마다 재업로드가 필요한데
     JKResourceCache의 동일 키 재등록 재업로드 의미론이 보장되지 않고, 데스크탑
     텍스트는 프레임당 수십 글자 규모라 터미널(수백 셀/프레임)과 달리 페이지
     압축 이득이 작다. 메모리는 글리프당 ~1KB (256×256×4B 페이지 대비).
   - stb_truetype 래스터라이즈 → 셀 크기(한글 16×16/영문 8×16) 스케일 → fg색 채움 +
     AA 알파 (BlitTexture의 텍스처 알파 블렌딩은 터미널이 이미 증명)
4. `GlyphSrc(cp)` rect를 `BlitTexture`로 dst 셀 위치에 블릿 — 글자당 블릿 1회
   (현행 픽셀 루프 대비 빠름, 래스터라이즈는 최초 1회). [as-built 2026-09-21]
   구현은 `PageKey(fg,cp)`로 텍스처를 직접 조회해 블릿 — `GlyphSrc`는 현재
   프로브 전용 단정 인터페이스
5. **글리프 단위 폴백**: ①KSSM→cp 변환 실패 ②폰트에 cp 없음 → 그 글자만 기존 비트맵 경로

메모리: fg 색 가짓수가 터미널보다 많음 → 색당 페이지. 페이지 512×512×4B ≈ 1MB 내외로
실용 범위. 페이지는 (색, 페이지 번호) 단위로 캐시 키 부여.

## 4. 설정·폰트 소싱·오류 처리

- `text.font_path` 신규 설정 키 (기본 `C:\Windows\Fonts\malgun.ttf`) — docs/54 설정 허브:
  settings_set 화이트리스트 추가 + settings GUI 표시
- 서버/클라 각 프로세스가 기동 시 settings.json에서 폰트 경로를 읽어 아틀라스 장착
- **fail-safe**: TTF 미존재·파손 → 아틀라스 Init 실패 → 미장착 = 기존 비트맵 경로 그대로,
  로그 1줄 (조용한 침묵 폴백이 아니라 진단 가능하게)
- 개별 글리프 실패는 그 글자만 폴백 (§3.5)

### 4.1 폰트 소싱 순서 (기본값 해석)

설정이 없을 때의 기본값은 **플랫폼별 1차 후보 → 번들 폰트** 순으로 해석:

| 우선 | 소스 | Windows | 리눅스 (docs/62 대비) |
|---|---|---|---|
| 1 | 설정 `text.font_path` | (사용자 지정) | (사용자 지정) |
| 2 | 플랫폼 기본 | `C:\Windows\Fonts\malgun.ttf` | 시스템 Noto CJK (`/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc` 계열 탐지) |
| 3 | 번들 폰트 | `assets/fonts/` — **OFL 폰트만** (예: Noto Sans CJK KR, 나눔고딕) | 동일 |

- **라이선스**: 맑은 고딕은 MS 라이선스상 재배포 불가 → 번들 폰트는 OFL(Noto·나눔)만.
  "폰트 복사(번들)"는 Windows에서의 기본값이 아니라 **리눅스 등 폰트 부재 환경의 안전망**.
- 리눅스는 시스템 폰트가 배포판마다 달라 2차 탐지 실패가 잦을 수 있음 → 이때 번들 폰트가
  실질 기본값. (폰트 파일 의존이 없어지는 것 자체가 docs/62에 기여하는 지점)
- 한자 커버리지 실측(2026-09-21, Win11, GDI cmap 쿼리): 맑은 고딕·바탕·굴림 모두
  CJK U+4E00-9FFF 샘플 568개 **100%**, 맑은 고딕·바탕은 확장 B U+20000-2A6DF 샘플
  167개 **100%** — Win10+ 맑은 고딕은 "맑은 고딕 한/한자"가 통합된 로컬리제이션 버전.
  즉 **이 머신에서는 1차 폰트만으로 한자 커버리지 충분** → 보조 폰트 체인은 필수가 아니라
  2단계 후보 (§6).

## 5. 테스팅

- **신규 로직 프로브** `jktext_probe` — KSSM→cp 변환 단정, EnsureGlyph 배치(중복 rect
  없음, 페이지 만석 시 신설), GlyphSrc 단정, 헤드리스 래스터라이즈 검증 훅
  (터미널 `RasterizeFallbackPageForTest` 선례)
- **뷰 레벨 배선 프로브** — 한글/영문/한자 혼합 문자열을 testwin류 캔버스에 렌더,
  `capture_window` 실물 캡쳐 확인 (docs/61 레슨 30: 로직 클래스가 생기면 호출부 배선을
  뷰 레벨 프로브로 잠근다)
- **폴백 프로브** — 나쁜 폰트 경로 → 비트맵 경로 렌더 단정
- **회귀 ×2** — terminal_hangul, jkedit, vpt13/14 등 기존 프로브 전부 GREEN 유지.
  터미널은 코드 미접촉이므로 GREEN 유지 자체가 격리 증명

## 6. 2단계 로드맵 (별도 착수)

- `text.font_scale` — 셀 확대(8×16 → 12×24 등). UI 전체가 셀 기준이라 화면 전체 스케일.
  1단계 착지 검증 후 착수
- **보조 폰트 체인** (`text.font_fallback`, 미설정=끔) — 1차 폰트에 없는 글자를 보조 폰트로,
  그래도 없으면 비트맵. 터미널 `InitFallback`이 선례. 리눅스(맑은 고딕 부재)·사용자가
  좁은 폰트를 골랐을 때의 안전망. 현재 머신 실측(§4.1)상 필수 아님
  — (해소 2026-09-21, 2단계 Task 2 §9.6)
- 비례폭 텍스트 API — 별도 설계 (전진 규칙이 달라져 좌표 계산 재작성 수반)

## 7. 미결/열린 질문

- (해소 2026-09-21) 클라 모드 settings.json 읽기 — 앱은 jkdesktop 단일 프로세스
  (jkapp_*.dll 로드), exe-dir + `state\settings.json` 공용 경로로 서버/클라 동일
- fg 색 폭증 시 글리프 텍스처 수 상한 — (해소 2026-09-21, 2단계 Task 1 §9.5)
  `kMaxGlyphTextures = 1024` + `registered_` LRU 폐기. 배너 `bannerCache_`도
  같은 JKTextAtlas 인스턴스라 동일 상한이 적용된다(호스트당 ~1MB)
- 번들 폰트 선정(Noto Sans CJK KR vs 나눔고딕, 용량/포맷 .ttc vs .otf) — 리눅스 착수
  시점에 확정, Windows 1단계에는 불요

## 8. 관련 발견 — imgui 폰트 범위 교훈 (기록용)

- imgui `GetGlyphRangesKorean`은 한자 범위(CJK Ideographs) 미포함 — 폰트에 한자 글리프가
  있어도 범위에 없으면 안 그려짐. `ClientBrowserApp.cpp:479`의 ★☆(U+2605/2606) 로컬
  범위 추가가 동일 결의 선례.
- imgui 1.92부터는 동적 폰트 시스템(수요 시 글리프 로드)이 권장 — 범위 배열이 구식화.
  기존 ImGui 앱들은 범위 배열 관행 유지 중(동작 확인) → 본 과제와 무관하게 현행 유지.
- 벡터 폰트 전환(JKTextAtlas)은 아예 범위 배열이 아닌 **코드포인트 수요 시 조회** 모델 —
  이 클래스의 문제가 애초에 발생하지 않는 구조.

## 9. as-built (2026-09-21, head 324a55b — 1단계 완결)

### 9.1 배선 지점 (실측)

`JKDC::SetTextAtlas(atlas, cache)` 관문 세터(src/JKDC.cpp:186)를 거치는 장착 지점 —
설계 §2의 3개 생성 지점 그대로:

| 호스트 | 지점 | 비고 |
|---|---|---|
| `JKClientApplication` | src/client/JKClientApplication.cpp:200 | 클라 앱(데스크탑 셸 포함) |
| `JKApplication` | src/JKApplication.cpp:126 | 싱글 프로세스 앱(minesweeper 등) |
| `JKWindowServer` (승인 배너) | src/server/JKWindowServer.cpp:5855-5906 | 배너 전용 `bannerAtlas_`+`bannerCache_`+영구 `bannerBackend_` |

- 두 앱 호스트 모두 `jk::text::ResolveDesktopFontPath()`가 `state\settings.json`의
  `text.font_path`를 직독(기본 `C:\Windows\Fonts\malgun.ttf`) — 서버 KV 파이프 불요.
- Init 실패(`IsLoaded()==false`)는 미장착 + stderr 경고 1줄 → 글리프 단위 비트맵 폴백(§3.5).
- 서버 배너는 Task 6 리뷰 픽스(324a55b)로 **영구 `bannerBackend_`**를 캐시 소유자로 부여 —
  nullptr 캐시로 등록되는 경로 봉쇄. 동기 그리기 경로라 `DrawGlyph`의 즉시
  `FlushUploads` 폴백이 이 백엔드로 올린다.

### 9.2 링크 보류 백로그 소각 + 전체 회귀 실측

Task 4 시점(ninja [80/80]) 이후 Task 5/6이 착지해 라이브 데스크탑이 구 exe/dll을 쥐고
있었다(링크 보류). 라이브 데스크탑(jkdesktop+jkwinserver) 정지 → 풀빌드 → 회귀 →
재기동 복원 순으로 소각:

- **ninja 29/29 GREEN** — 링크 3건(jkdesktop.exe, jkwinserver.exe, jkapp_taskbar.dll)
  전부 성공. 링크 보류 0. (컴파일 결함 아닌 잠금 보류였음이 확인됨)

회귀 9종 **×2 연속 전부 GREEN** (`tools/probes/reg7_*.log`):

| 프로브 | 결과(×2 동일) |
|---|---|
| jktext_probe | PASS 26 FAIL 0 |
| jktext_view_probe | exit 0 — ink=189 aa=172 span=[9..61] |
| jkedit_probe | RESULT: ALL PASS |
| terminal_hangul_probe | 33/33 checks passed |
| terminal_hangul_view_probe | 18/18 checks passed |
| terminal_jamo_atlas_probe (재컴파일) | 9/9 checks passed |
| probe_textfont.ps1 | 7 PASS / 0 FAIL (server-up PASS, settings 복원 byte-identical) |
| probe_settings.ps1 | 17 ok — RESULT: ALL PASS |
| probe_approval_overflow.ps1 | parked-1..8 + reply + events(8) — PASS (배너 파이프라인 무수정 회귀) |

- `probe_settings.ps1`/`probe_approval_overflow.ps1`는 `jkdesktop --server`를 자가 스폰하므로
  **싱글 인스턴스 가드(docs/59 §11)에 걸리지 않도록 `jkwinserver.exe`가 내려간 상태에서** 실행한다.

- `terminal_jamo_atlas_probe` 재컴파일: JKGlyphAtlas.cpp의
  `STB_TRUETYPE_IMPLEMENTATION`이 JKTextAtlas.cpp로 이동해 단독 링크에 보충 필요.
  **실측 소스 목록**(계획서의 3파일 목록에 2개 보강 — 레슨 28 링크 오류는 입력 파일
  목록부터): `terminal_jamo_atlas_probe.cpp src/terminal/JKGlyphAtlas.cpp
  src/JKTextAtlas.cpp src/agent/JKAgentJson.cpp src/terminal/JKTerminalGrid.cpp
  src/terminal/JKVtParser.cpp build/libquickjs.a` — include에 `-Ithird_party/quickjs-ng`
  (JKTextAtlas의 settings 직독이 JKAgentJson를 끈다).

### 9.3 원설계 대비 편차 (전부 §3/커밋 메시지에 기록된 것 — 요약)

1. **글리프당 소형 텍스처** — 원설계 "fg 색당 tightly-packed 페이지" → 글리프당
   텍스처(캐시 키 `desktext_%08x_%06x`). 근거: JKResourceCache 동일 키 재등록
   재업로드 의미론 미보장 + 데스크탑 텍스트는 프레임당 수십 글자라 페이지 압축 이득
   작음. 메모리 글리프당 ~1KB.
2. **KssmCodepointToUnicode 왕복 가드** — KssmToUtf8이 매핑 없는 쌍을 '?'로 치환하므로,
   Utf8ToKssm 왕복이 원래 쌍으로 돌아올 때만 실제 매핑으로 인정(아니면 0=폴백 신호).
   신규 매핑 테이블 금지(docs/60 §7) 원칙으로 기존 역인덱스 재사용. [최종리뷰 픽스
   2026-09-21] 이 위에 쌍 단위 메모이즈(0도 캐시, 계약 불변)를 얹었다 — 리페인트마다
   반복되던 문자열 할당 2건+왕복 변환 제거. 비(非)Windows는 Utf8ToKssm이 항등이라
   왕복 가드가 무효 → 0(비트맵 폴백) 봉쇄(포트 시점 함정 주석).
3. **배너 영구 백엔드** — 배너 캐시의 등록/플러시 소유자로 `bannerBackend_`를 서버
   수명 동안 유지. 배너 텍스처 풀(`approvalBannerTexs_`)은 Stop에서 폐기.
4. **`DrawGlyph` 즉시 FlushUploads 폴백** — 텍스처 조회 실패 시 1회 플러시 후 재시도
   (동기 그리기 배너 경로 보호).

### 9.4 알려진 한계 (수용 — 1단계 범위 밖)

- **기존 ImGui 앱 폰트 경로 미변경** — imgui 범위 배열 관행 유지(§8). 본 과제와 무관.
- **설정 재시작 적용** — `text.font_path` 변경은 기동 시 읽힘(핫스왑 없음).
- **글리프 단위 비트맵 폴백은 계단 유지** — 변환 실패/폰트 부재 글자만 기존 KSSM
  비트맵 경로(계단진 그대로).
- **배너 글리프 텍스처 누적** — bannerCache_ 글리프 텍스처는 Stop까지 누적(LRU 폐기
  백로그, §7).
- **malformed 후행 하이바이트 엣지(최종리뷰 IMP-1 픽스)** — TextOut 루프에서 문자열
  끝에 홀로 남은 하이바이트(`i+1 >= n`인 `c & 0x80`)는 `EngPutCh`로 갔는데, 벡터
  경로는 `StrideOf`가 16을 돌려 16px 글리프 텍스처를 8px dst로 눌러 그리는 왜곡이
  생겼다(원래 비트맵 경로는 `FONT_8X8[ch&0x7f]` ASCII 폴백으로 8×8이었음 — 즉 이
  왜곡은 본 과제가 새로 만든 결함). 픽스: `EngPutCh`는 `ch < 0x80`일 때만 아틀라스를
  시도하고 하이바이트는 비트맵 폴백(ASCII 8×8 폴백 글리프)으로 라우팅 — 잘린 KSSM
  조각은 잘못된 글자 1개로 표시(정상 문자열에서는 발생하지 않는 에지).
- **눈확인 완료(2026-09-21)**: 데스크탑 재기동 후 전체 UI 텍스트 벡터 렌더 —
  사용자 "좋네요" 확인. 1단계 종결. (재기동 절차 기록: 구 서버 정지 → ninja 링크
  해소 29/29 → `jkdesktop.exe --server`; 구 서버 살아 있으면 단일 인스턴스 가드가
  신규 기동을 거부하는 것은 정상 동작)

### 9.5 글리프 텍스처 LRU/상한 (2단계 Task 1, 2026-09-21)

- **상한** `JKTextAtlas::kMaxGlyphTextures = 1024` — 글리프당 ~1KB → 호스트당
  ~1MB. 초과 등록 시 맨 앞(최장 미사용) 키를 폐기:
  `EvictOldest(cache)`가 `cache->UnloadImage(PageKey(fg, cp))` 후 `registered_`
  제거. 재요청은 자동 재등록(EnsureGlyph 계약 무수정).
- **LRU 터치** — EnsureGlyph 기존 히트 시 해당 키를 벡터 끝(MRU)으로 이동 → 폐기
  순서가 실사용을 따른다. `registered_`는 여전히 선형 스캔(상한 1024가 O(n) 봉쇄 —
  1단계 백로그의 registered_ 해시는 제외, 계획서 ruling).
- **UnloadImage 의미론 실측(픽스 불요)** — `JKResourceCache::UnloadImage`는
  `images_`(업로드된 텍스처 handle을 `pendingDestroys_` 큐 → 다음 `FlushUploads`에서
  백엔드 `DestroyTexture`)와 `pending_`(미업로드 — RGBA 버퍼/Surface 해제, 텍스처
  존재하지 않음)를 둘 다 올바르게 제거한다. 결함 없음.
- **링크 보충(레슨 28 후속)** — `EnsureGlyph`가 `UnloadImage`를 참조하게 되어
  JKResourceCache.o를 링크하지 않는 독립 프로브(jktext_probe,
  terminal_jamo_atlas_probe)의 캐시 스텁에 `UnloadImage` 정의 보충.
  docs/63 §9.2의 jamo 링크 목록은 소스 목록 변동 없음(스텁은 probe TU 내부).
- **프로브** — jktext_probe 26→33체크: (a) fg 12색 × ASCII 94cp = 1128 등록 후
  살아있는 글리프 수 == 1024, (b) 최연장 (fg,cp) 폐기(GlyphSrc 빈 rect +
  UnloadImage 기록) → 재 EnsureGlyph true + GlyphSrc 복원 + 상한 유지.
  ×2 GREEN. jktext_view_probe(실 렌더러 경로), terminal_jamo_atlas_probe 9/9도
  ×2 GREEN.

### 9.6 보조 폰트 체인 text.font_fallback (2단계 Task 2, 2026-09-21)

- **Face 리팩터링** — `JKTextAtlas` 내부 `struct Face { data, info,
  engScale/hanScale, engBaseline/hanBaseline }` 도입, `primary_`/`fallback_` 두
  면. Init의 메트릭 산출 블록(로드+'M'/0xAC00 스케일+em 캡+베이스라인)을
  `LoadFace(path, Face*, engCellW, cellH, hanCellW)` static 헬퍼로 추출 —
  primary 로드 계약 불변. 셀 격자(engCellW_/hanCellW_/cellH_)는 두 면 공유.
  한글 글리프 없는 폰트(consola)는 0xAC00 전진 0 → 영문 스케일 2배 폴백(기존
  레시피 그대로, em 캡이 뒤따름).
- **InitFallback** — Init 성공 후 호스트가 1회 호출. 실패 = false(체인 없음,
  경고 1줄, 치명 아님). **재 Init은 체인을 해제한다**(호스트가 재시도).
- **체인** — `EnsureGlyph(cache, fg, cp, useFallbackPage=false)` / `PageKey(fg,
  cp, useFallbackPage=false)` / `GlyphSrc(fg, cp, useFallbackPage=false)` /
  `RasterizeGlyphForTest(..., useFallbackPage=false)` — 기본 인자 false로 기존
  호출부 무수정. `JKDC::DrawGlyph`: 1차 EnsureGlyph 실패 → `EnsureGlyph(...,
  true)` → 성공이면 fallback 키로 블릿, 둘 다 실패 = 비트맵 폴백(호출부 계약
  무수정). RasterizeGlyph의 커버리지 검사는 선택된 면 기준(보조 면도 미커버면
  false).
- **캐시 키 분리** — fallback 페이지 접두어 `desktextf_%08x_%06x`(1차
  `desktext_`와 충돌 봉쇄). `registered_` 키 확장: `(fg << 34) | (cp << 1) | fb`
  (fg는 24비트 RGB라 58비트에 수렴) — 같은 (fg,cp)가 두 면에 공존. EvictOldest가
  이 인코딩을 복원하는 유일한 지점(cp는 `(key >> 1) & 0x7FFFFFFF` 마스크 필수 —
  fg 고위 비트 유입 차단).
- **설정** — `text_font_fallback` settings_set(빈 값 = 해제 **허용**,
  font_path와 반대 — 체인의 기본 상태가 "없음"이라 빈 값이 유효 목표 상태,
  >300 bad_value, applies_on_restart 에코) + settings_read `text.font_fallback`
  + Load/WriteSettingsKv `"text"."font_fallback"`. `ResolveDesktopFallbackPath()`
  는 settings 직독, **기본값 없음**(빈 = 체인 미설정). GUI: 설정 앱 텍스트 섹션
  2행(현재값 표시 + InputText, 현재값과 다를 때만 전송).
- **호스트 배선 3곳** — JKApplication/JKClientApplication(Init 성공 후) + 배너
  (JKWindowServer bannerAtlas_ Init 성공 후): `ResolveDesktopFallbackPath()`가
  비어있지 않으면 `InitFallback` 시도, 실패는 경고 1줄+체인 없이 계속.
- **프로브** — jktext_probe 33→54체크: T9(c) LRU≠FIFO(채움 1128 후 폐기 104건 =
  fg0 전체+fg1 0x21..0x2A → 최연장 생존 (fg1,0x2B)를 터치 → 신규 3건 → 터치 키
  생존 + (fg1,0x2C) 폐기 — 1단계 리뷰 MINOR 보충), T10 체인(consola 1차 — '가'
  미커버를 Rasterize 선체크로 확인 후 사용, fallback=맑은 고딕: 1차 실패 →
  fallback 등록 true + `desktextf_` 접두어 + 1차 키 불변 + 체인 미설정 false +
  InitFallback 파일 부재 false + ASCII 1차 성공) ×2 GREEN. probe_textfont.ps1
  7→9체크: text_font_fallback set→read 왕복 + 빈 값 해제 허용 ×2 GREEN(기존
  settings.json 백업/바이트동일 검증 재사용). 회귀: jkedit_probe ALL PASS,
  terminal_hangul_probe 33/33, terminal_jamo_atlas_probe 9/9, jktext_view_probe
  GREEN.
- **운영 실측** — 폰트 체인 없는 설정(기본)에서 기동: 기존 동작과 동일(체인
  미개입). 사용자 눈확인 대기: settings 앱에서 text.font_fallback = consola.ttf
  저장 → 재기동 → 한글 텍스트가 consola 1차+맑은 고딕 승계로 렌더되는지.