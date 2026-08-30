# 창/DPI 화면 검증 플레이북 (Verification Playbook)

> **요약**: 2026-08 창 잘림(DPI 스케일링) 버그 수정에서 실제로 사용한 **검증 방법론과 프로세스**를 정리한 문서.
> `14_sdl2_window_dpi.md`가 "무엇을" 구현했는지라면, 이 문서는 "어떻게 맞는지를 확인했는지"를 다룬다.
> 대상: `prototype/sdl2_jkwindow`의 화면 좌표·레이아웃·스케일 변경 작업 전후 검증.

---

## 1. 왜 '눈으로 보고 확인'이 작동하지 않았나

DPI/좌표계 버그는 일반적인 디버깅 직관이 잘 작동하지 않았다. 이 프로젝트에서 실제로 부딪힌 이유:

| 이유 | 실제 상황 |
|------|-----------|
| **측정 도구가 스스로 왜곡된다** | PowerShell 화면 캡처(`CopyFromScreen`)는 DPI-unaware 프로세스에서 논리 pt 크기로 축소된 비트맵을 내놓는다. "화면이 잘렸다"고 보이던 증거가 사실은 **캡처 도구의 왜곡**이었던 적이 있다 (§6) |
| **좌표 계층이 3개 섞여 있다** | 물리 px / 윈도우 논리 pt / 앱 논리 좌표(1920×1080 고정). 겉보기 증상만으로 어느 계층이 틀렸는지 구분 불가 |
| **float 오차가 픽셀 단위로 누적된다** | fit 스케일은 float이고, 반올림 위치에 따라 ±1px가 레터박스 대칭성·콘텐츠 경계에 누적된다 |
| **스케일은 창 크기와 무관해야 한다** | 등비 fit 스케일이 창 크기에 따라 달라지면 버그인데, 한 번 실행만으로는 "원래 그런 것"인지 구분 불가 |

→ 결론. **① 검증 도구의 신뢰성을 먼저 확보 → ② 앱과 독립된 경로로 측정 → ③ 기대값을 앱 로직과 별개로 재계산해 비교**. 이 세 가지가 아래 프로세스의 뼈대다.

---

## 2. 검증 프로세스 (5단계)

```
[1 빌드·셀프테스트]   배치 통일 빌드 + AppSelfTest 33체크 (논리 레벨 선배제)
        |
[2 지오메트리 측정]   Win32 API로 OS에게 직접 물어봄 (모두 물리 px)
        |
[3 기대값 독립 계산]   검증 스크립트가 레이아웃 규칙을 자체 재계산
        |
[4 픽셀 분석]         SetProcessDPIAware 후 물리 1:1 캡처 → 색 밴드/범위 스캔
        |
[5 판정·기록]         PASS/FAIL 리포트 + PNG 증거 + 기준값 대조
```

### ① 재현 가능한 빌드·실행

- `prototype\sdl2_jkwindow\build_sdl2_jkwindow.bat`으로 빌드한다 (I: 드라이브 직접 빌드 금지 — 절차는 `.claude\PROJECT.md`).
- `jkproto_sdl2_jkwindow.exe test` → `AppSelfTest: 0 failure(s)` (33 체크) — **좌표 문제와 무관한 논리 회귀를 먼저 배제**한다.
- 빌드·실행 절차가 통일되어야 측정값의 회귀 비교가 유효하다.

### ② 창 지오메트리 독립 측정

- 측정 주체를 **앱과 분리**한다. 앱 내부 로그가 아니라 OS(Win32 API)에게 직접 물어본다:
  - `GetWindowRect()` — 프레임(외곽) 사각형
  - `GetClientRect()` + `ClientToScreen()` — 클라이언트 원점/크기
  - `Screen.PrimaryScreen.Bounds` / `.WorkingArea` — 화면·작업영역
- 측정값은 **모두 물리 px로 통일**한다.
- 핵심 검사: 클라이언트 원점 y ≥ 25(인앱 타이틀바 확보), 프레임 상단 ≥ 0(상단 잘림), 프레임이 작업영역 내부(작업표시줄 침범).

### ③ 기대값 독립 계산

- 검증 스크립트가 앱의 레이아웃 규칙을 **스스로 재계산**한다. 앱이 계산한 값을 그대로 가져와 비교하면 순환논증이 된다 — 같은 버그가 앱과 검증 양쪽에 있으면 통과해 버린다.
- 계산식 (앱 논리 좌표 1920×1080 등비 스케일):
  - `fit = min(clientW/1920, clientH/1080)`
  - `contentW = Floor(1920*fit + 0.5)`, `contentH = Floor(1080*fit + 0.5)`
  - `lbx = Floor((clientW - 1920*fit)/2 + 0.5)` — 좌우 레터박스 폭
- 반올림은 반드시 `Floor(x + 0.5)`. PowerShell `[int]` 캐스팅은 **banker's rounding**(0.5 → 짝수)이라 기대값이 1px 어긋난다.

### ④ DPI-aware 스크린샷 + 픽셀 스캔

- `System.Windows.Forms` 어셈블리 로드 **전에** `SetProcessDPIAware()`을 호출해야 `CopyFromScreen`이 물리 픽셀 1:1로 캡처한다. 미호출 시 화면의 논리 해상도(125%에서 1536×864) 영역만 캡처해 모든 판정이 뒤틀린다.
- **색으로 구조를 판정**한다. 색 값은 소스에서 먼저 확인한다 (`Render()`의 클리어 색 = 레터박스 색):
  - 레터박스 밴드 = (192,192,192) → 좌우 폭·대칭성 측정
  - 인앱 타이틀바 = 파란 (B > 80, B−R > 30) → 존재·높이 측정
  - 콘텐츠 = "회색이 아닌" 행/열의 첫/끝 → 콘텐츠 물리 크기 역산 → scaleW/scaleH
- 판정 허용치: 색 ±10~12, 위치 ±1~2px. `JKRect`는 inclusive 좌표(Win16 GDI 관습, h=24면 실제 25행 그림)이므로 1px 어긋남은 정상 범위다.
- 캡처 PNG를 저장해 증거를 남긴다.

### ⑤ 판정·기록

- 모든 체크는 `Check "항목명" (조건) "측정 상세"` 형태로 PASS/FAIL 카운트된다. 실패 시 exit code 1 → 배치 자동화 가능.
- 측정값은 §5 기준값 표와 대조한다. 의도치 않은 변화면 회귀, 의도된 변경이면 기준값을 갱신하고 문서에 기록한다.

---

## 3. 검증 템플릿: `tools\verify_fixwin3.ps1`

위 2~5단계를 전부 구현한 자동 검증 스크립트. 앱 실행 → 측정 → 판정 → 정리까지 한 번에 수행한다.

```
powershell -NoProfile -ExecutionPolicy Bypass -File tools\verify_fixwin3.ps1 -mode occ
```

| 항목 | 내용 |
|------|------|
| 파라미터 | `-mode occ\|jango` (검증할 앱 모드), `-exe <경로>` (기본값: `prototype\sdl2_jkwindow\build\jkproto_sdl2_jkwindow.exe`) |
| 사전 조건 | 빌드 완료 상태. 스크린샷이 `C:\temp_jkwin_verify\`에 저장되므로 이 디렉터리가 존재해야 함 |
| 동작 | 앱 Start-Process → 5초 대기 → `SetForegroundWindow` → 측정·체크 → `CloseMainWindow` (무응답 시 강제 종료) |
| 출력 | 체크별 `PASS/FAIL` + `[info]` 측정값 + `RESULT[mode]: N pass, M fail`. 실패 시 exit 1 |

## 4. 자동 체크 항목 (모드당 14개)

| # | 분류 | 체크 | 통과 조건 |
|---|------|------|-----------|
| 1 | 지오메트리 | 타이틀바 온스크린 | 클라이언트 원점 y ≥ 25px |
| 2 | 지오메트리 | 프레임 상단 온스크린 | outerTop ≥ 0 |
| 3 | 지오메트리 | 네이티브 타이틀바 존재 | nativeBar ≥ 20px |
| 4 | 지오메트리 | 프레임이 작업영역 안 | outerBottom ≤ workBottom |
| 5 | 지오메트리 | 프레임이 화면 안 | 0 ≤ outerLeft, outerRight ≤ screenW |
| 6 | 픽셀 | 좌측 레터박스 밴드 색 | (192,192,192) ±12 |
| 7 | 픽셀 | 우측 레터박스 밴드 색 | (192,192,192) ±12 |
| 8 | 픽셀 | 인앱 타이틀바(파란) 표시 | B > 80, B−R > 30 |
| 9 | 픽셀 | 콘텐츠가 하단까지 표시 | 콘텐츠 하단 6px 지점이 밴드색 아님 |
| 10 | 픽셀 | 인앱 타이틀바 완전 표시 | 파란 밴드가 클라이언트 내부에 포함 |
| 11 | 스케일 | 세로 스케일 = fit | abs(scaleH − fit) < 0.005 |
| 12 | 스케일 | 가로 스케일 = fit | abs(scaleW − fit) < 0.005 |
| 13 | 위치 | 콘텐츠 좌측 경계 정합 | 예상 레터박스 시작 ±2px |
| 14 | 위치 | 콘텐츠 우측 경계 정합 | 예상 레터박스 끝 ±2px |

- 스케일 측정 방법: 클라이언트 내 "회색 아닌" 행/열 범위를 스캔하고 JKWindow 1px 보더 2px를 보정해 콘텐츠 물리 크기를 구한 뒤 1920/1080으로 나눈다.
- 좌우 밴드가 2px 이하(창 비율 = 앱 비율)면 6/7 대신 "레터박스 존재" 체크가 FAIL로 대체된다.

---

## 5. 회귀 기준값 (125% DPI 환경)

측정값을 기록해 두면 다음 변경 때 "어디가 깨졌는지"가 즉시 보인다. 원본 표는 `14_sdl2_window_dpi.md` §9.

| 항목 | 기준값 |
|------|--------|
| 화면 | 물리 1920×1080, Windows 스케일 125% (논리 1536×864), 작업영역 논리 1536×816 pt |
| 생성 클램프 크기 | 1528×781 pt |
| 클라이언트 크기 | 1910×976 px |
| fit (계산) | 0.9037 |
| 콘텐츠 스케일 (픽셀 역산) | 0.9036 ~ 0.9046 |
| 좌우 레터박스 밴드 | 87px / 88px (상·하 0px — 세로 제한) |
| 콘텐츠 물리 크기 | 약 1735×976 px (계산; 픽셀 스캔 실측 1735×977, inclusive ±1px) |
| 인앱 타이틀바 | kTitle=24 (앱 논리, inclusive → 25행) / 물리 실측: 파란 밴드 23px + 상단 1px 회색 테두리 |

기준값 갱신 절차: 의도된 변경이면 verify_fixwin3 측정값으로 이 표와 14번 문서 §9를 갱신하고, 커밋 메시지에 기준값 변화를 명시한다.

## 6. 검증 도구 자체의 함정 — 스크립트 진화사

검증 스크립트 자체가 DPI 버그를 갖고 있어 잘못된 결론을 낸 적이 있다 (1~3세대는 `C:\temp_jkwin_verify\`에만 남음):

| 세대 | 파일 | 문제/한계 |
|------|------|-----------|
| 1 | `probe_fixwin.ps1` | 단순 캡처+픽셀 샘플. **DPI-unaware**라 논리 크기 비트맵을 내놓아 물리 좌표와 어긋남 → "콘텐츠가 작업표시줄 아래로 넘친다"는 오판 유발 |
| 2 | `geom_fixwin.ps1` | Win32 지오메트리 중심. 지오메트리만으로는 렌더링(스케일·레터박스) 확인 불가 |
| 3 | `verify_fixwin2.ps1` | 체크 항목 체계화. 여전히 DPI-unaware 캡처 |
| 4 | `verify_fixwin3.ps1` | `SetProcessDPIAware()` 도입 + 지오메트리·기대값·픽셀 스캔 통합 → **최종 판정 스크립트** (레포 `tools\`에 편입) |

교훈: **검증 도구가 틀리면 앱이 맞아도 틀렸다고 보고한다.** 증상이 기대와 어긋나면 앱과 검증 도구를 둘 다 의심해야 하며, 도구 신뢰성 확인(캡처 크기 = 물리 해상도 등)을 프로세스의 선행 단계로 넣어야 한다.

## 7. 문서화 작업의 검증 — 코드·문서·실측 3-way 대조

아키텍처 문서를 작성할 때도 같은 원칙이 적용된다. 모든 주장을 세 소스와 대조한다:

- **사례 1 — 보더 주장 충돌**: 규칙 문서에는 "1px 회색 보더", 코드에는 `kBorder = 2`pt. 실측(픽셀 스캔)과 `PaintWindow()` 코드 확인 결과 둘 다 사실 — 2pt는 콘텐츠 오프셋, 1px는 `DrawRect`가 그리는 선. → 14번 문서 §8.2에 두 값의 역할을 구분해 기록.
- **사례 2 — 경로 대조**: 문서의 경로를 실제 파일시스템과 대조 → `prototype\jksdl2\` 오탈자 4곳 발견·수정.
- 원칙: **코드가 우선, 실측이 중재, 문서는 마지막에 갱신.** 문서끼리만 서로 인용하면 오류가 영구히 순환한다.

## 8. 보조 유틸과 에이전트 도구 주의

- `tools\fix_bom.ps1 -Path <파일>`: 한글 포함 .ps1의 UTF-8 BOM 확인/부착. editor 도구로 .ps1을 저장하면 BOM이 빠질 수 있고, BOM이 없으면 PowerShell 5.1이 cp949로 읽어 파싱이 깨진다.
- 캡처/스캔 임시 파일과 스크린샷은 `C:\temp_jkwin_verify\` 작업장에 둔다.

## 9. 마우스 좌표 스케일 검증 — `tools\probe_mouse_scaling.ps1` (2026-08-29)

"클릭이 ×배율만큼 밀린다" 계열 증상은 화면 픽셀 스캔만으로는 잡히지 않는다 — **SDL이 주는
이벤트 좌표의 단위**를 직접 실측해야 한다. 방법:

1. 앱에 임시 계측([MOUSEPROBE])을 넣어 250ms 스로틀로 한 줄에 기록한다: SDL 이벤트 원본 좌표
   (sdlRaw), 변환 후 앱 좌표, 창 pt/렌더 px 크기, ptToPhys/fit/letterbox, 그리고 Win32
   `GetCursorPos − 클라이언트 원점`(물리 px, w32c).
2. PMv2 드라이버가 커서를 창 위 알려진 물리 px 격자로 스윕한다(스텝당 450ms > 스로틀 = 스텝마다 최소 1행).
3. 독립 판정식: `sdlRaw == w32c × ptToPhys⁻¹`, `app == (w32c − letterbox) / fit`(±1px).
   - 125% 주 모니터 실측: 611×1.25≈w32c(764), (764−87)/0.9037=749 ✓ — 사슬 정합.
   - 모니터 전이 후 SDL이 창 pt를 뭉개면(1222×625) 이 식이 깨져 배율 오차가 즉시 드러난다.
4. 재사용 도구: `tools\probe_mouse_scaling.ps1 -Phase both|move2nd|stay` — 앱 스폰(ShowWindow 복원
   포함) → 그리드 스윕 → 보조 모니터 이동(SetWindowPos) → MOVED/GRID 출력 + `$TEMP\jk_probe_mouse_app.log`
   의 [DPISYNC](`Resync pt`/`UpdateScale` 수렴) 관찰. 14번 문서 §12 참조.
5. **버튼 클릭(캡처 경로) 회귀**: `tools\click_jango_probe.ps1` — 런처 버튼 클릭→모달 오픈(타이틀 밴드
   검출), 모달 Close 클릭→닫힘 판정(14 문서 §12.7). RESULT: PASS/FAIL로 종료.

## 10. 키보드 포커스/모달 검증 (2026-08-30)

마우스 좌표와는 다른 회귀 차원이다. 합성 키 입력(`keybd_event`)을 SDL 창에 보내고
stderr의 `[FOCUS]` 로그 + stdout의 콜백 메시지로 판정한다.

| 스크립트 | 검증 내용 |
|----------|-----------|
| `tools\verify_tab_navigation.ps1` | Tab/Shift+Tab이 활성 윈도우의 focusable 컨트롤을 순환. 4개 이상 ID 확인 |
| `tools\verify_dialog_keyboard.ps1` | `M` → `JKMessageBox` 열기 → `Enter`로 기본 버튼(OK) 실행/닫기 → `F` → `JKFileDialog` 열기 → `Escape`로 취소/닫기 → focus가 원래 컨트롤로 복원 |

주의: 드라이버도 `SetProcessDpiAwarenessContext(-4)`/`SetProcessDPIAware()`로 PMv2를 선언해야
Win32 좌표 비교가 성립한다. SDL 창에 키 입력이 가려면 창을 foreground로 만들고 클라이언트 중앙을
클릭해 키보드 포커스를 줘야 한다.

교훈: **이벤트 단위 의심은 픽셀 검증과 별개의 "인공 커서 격자 + 좌표 이중 로깅"으로 증명한다.**
체감(→클릭이 밀렸다)을 근거로 삼아 배율을 곱하는 보정은 절대 금지(14 문서 §11.2의 재해 반복).

## 11. IME 입력 검증 (2026-08-30)

`JKEdit`의 한글 입력은 OS IME를 기본으로 사용하고, 내부 2벌식 오토마타는 `F2` 폴백으로 동작한다.
SDL `TEXTEDITING`/`TEXTINPUT` 이벤트를 받아 KSSM 2바이트로 저장하며, 아래 두 단계로 검증한다.

### 11.1 자동 단위 테스트

`AppSelfTest`에 포함된 합성 이벤트 기반 테스트(5체크):

| 체크 | 내용 |
|------|------|
| ime pre-edit does not commit to buffer | `TEXTEDITING`만으로는 버퍼가 변하지 않음 |
| ime pre-edit update still not committed | 조합 중간 갱신도 버퍼에 반영되지 않음 |
| ime committed text stored as KSSM | `TEXTINPUT`이 들어오면 `Utf8ToKssm`로 변환해 2바이트 삽입 |
| f2 toggles internal hangul automata | `F2`로 내부 오토마타 모드 진입 |
| internal automata produces multi-byte KSSM | 내부 오토마타로 생성된 글자가 멀티바이트 KSSM임 |

이 테스트는 SDL 초기화 없이도 실행 가능하며, IME 이벤트 라우팅과 KSSM 변환 파이프라인의
기본적인 회귀를 막는다.

### 11.2 수동 통합 테스트

OS 한국어 IME를 직접 켜고 타이핑해야 하는 부분은 자동화할 수 없다. 절차:

1. `jkproto_sdl2_jkwindow.exe`를 실행해 메인 데모(장교/장비 관리 등)를 연다.
2. 에디트 컨트롤을 클릭해 포커스를 준다.
3. Windows 한국어 IME(기본 MS IME)를 켜고 "한글"을 입력한다.
4. 기대 결과:
   - 조합 중인 글자가 파란 하이라이트(반투명) 박스 안에 표시됨.
   - 빨간 보조 캐럿이 조합 문자열 안에서 IME가 알려주는 위치에 표시됨.
   - 스페이스/엔터로 확정하면 하이라이트가 사라지고 2바이트 KSSM 문자가 버퍼에 삽입됨.
   - 동일한 글자가 두 번 삽입되지 않음.
5. `F2`를 누르고 `gksrmf`를 입력하면 내부 오토마타가 "한글"을 생성해야 함.

### 11.3 구현상 안정화 포인트

외부 검토 의견을 반영해 다음과 같이 보강했다:

- **Win32 API 격리**: `JKPlatform` PAL을 두고 `JKEdit.cpp`에서 `<windows.h>`/
  `<imm.h>`를 완전히 제거. 헤더 오염과 호출 규약 리스크 차단.
- **조합 중 키 양보**: `imeComposing_`일 때 백스페이스/딜리트/방향키/엔터를 프레임워크가
  처리하지 않고 OS IME에 넘긴다. 이중 삭제/커서 꼬임 방지.
- **F2 동기화**: `F2`로 내부 오토마타로 전환하면 `JKPlatform::SetConversionMode`
  로 OS IME를 ASCII 모드로 강제 전환. OS IME와 내부 오토마타의 중복 조합 차단.
- **포커스 아웃 강제 확정**: `OnKillFocus`에서 `JKPlatform::CompleteComposition`을
  먼저 호출해 OS IME가 조합 문자를 `TEXTINPUT`으로내도록 유도한 뒤, 이벤트가 누락될
  경우를 대비해 로컬 `CommitComposition` 폴백도 유지.

> 개념·의도·구현·고려사항의 상세 설명은 `ARCHITECTURE_DOCS/16_sdl2_jkwindow_ime.md`를 참고.

## 12. 관련 문서


- `ARCHITECTURE_DOCS\14_sdl2_window_dpi.md` — 구현 내용 (좌표계 계층·재배치 알고리즘·렌더링 파이프라인·기준값 원본 표 §9, 마우스 배율 버그 §12)
- `ARCHITECTURE_DOCS\16_sdl2_jkwindow_ime.md` — `JKEdit` IME 입력 아키텍처 (데이터 파이프라인·입력 모드·PAL·안정화 설계)
- `.claude\PROJECT.md` — 변경 후 필수 검증 절차 (셀프테스트 → 모드 실행 → verify_fixwin3 → tab/dialog probes)
- `ARCHITECTURE_DOCS\12_sdl2_prototype_roadmap.md` — Phase 1 Input/Focus System 완료 상태
- `phase1_input_focus.md` — Phase 1 상세 작업 명세