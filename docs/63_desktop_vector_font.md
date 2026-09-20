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
   - 페이지 키 = `(fg 색, cp)` — 1단계는 크기 고정, 데스크탑은 bold 미사용이라 키에서 뺌
   - 페이지 = **fg 색당 1개, tightly-packed**, 가득 차면 다음 페이지 신설
     (터미널의 chunk 방식보다 임의 코드포인트가 흩어지는 데스크탑 텍스트에 적합)
   - stb_truetype 래스터라이즈 → 셀 크기(한글 16×16/영문 8×16) 스케일 → fg색 채움 +
     AA 알파 (BlitTexture의 텍스처 알파 블렌딩은 터미널이 이미 증명)
4. `GlyphSrc(cp)` rect를 `BlitTexture`로 dst 셀 위치에 블릿 — 글자당 블릿 1회
   (현행 픽셀 루프 대비 빠름, 래스터라이즈는 최초 1회)
5. **글리프 단위 폴백**: ①KSSM→cp 변환 실패 ②폰트에 cp 없음 → 그 글자만 기존 비트맵 경로

메모리: fg 색 가짓수가 터미널보다 많음 → 색당 페이지. 페이지 512×512×4B ≈ 1MB 내외로
실용 범위. 페이지는 (색, 페이지 번호) 단위로 캐시 키 부여.

## 4. 설정과 오류 처리

- `text.font_path` 신규 설정 키 (기본 `C:\Windows\Fonts\malgun.ttf`) — docs/54 설정 허브:
  settings_set 화이트리스트 추가 + settings GUI 표시
- 서버/클라 각 프로세스가 기동 시 settings.json에서 폰트 경로를 읽어 아틀라스 장착
- **fail-safe**: TTF 미존재·파손 → 아틀라스 Init 실패 → 미장착 = 기존 비트맵 경로 그대로,
  로그 1줄 (조용한 침묵 폴백이 아니라 진단 가능하게)
- 개별 글리프 실패는 그 글자만 폴백 (§3.5)

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
- 비례폭 텍스트 API — 별도 설계 (전진 규칙이 달라져 좌표 계산 재작성 수반)

## 7. 미결/열린 질문

- fg 색 폭증 시 페이지 메모리 상한 — 필요 시 LRU 폐기(백로그)
- 클라 모드 앱의 settings.json 읽기 경로(공유 state dir) 확인 — 구현 계획 단계에서 확정