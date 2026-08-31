# SDL2 JKWindow / GWES Platform Gap Analysis

> 지뢰찾기(`MineSweeperApp`) 구현을 통해 발견한 JKENGINE SDL2 프로토타입의
> GWES(Graphics Windowing Event Subsystem) 및 앱 플랫폼 부족점과 개선 우선순위.
>
> 문서 날짜: 2026-08-30

---

## 1. 개요

현재 `prototype/sdl2_jkwindow`는 업무용 폼 UI(버튼, 에디트, 리스트, 파일/메시지
다이얼로그, 한글 입력)에 적합한 기반을 갖췄습니다. 그러나 게임, 그림판, 메모장 등
새로운 앱을 만들면서 플랫폼이 제공하지 않는 기능을 직접 우회하거나 반복 구현해야
하는 지점이 드러났습니다.

이 문서는 발견된 부족점을 **우선순위**와 **개선 방향**으로 정리하며, 향후 하나씩
구현할 수 있도록 가이드합니다.

---

## 2. 부족점 및 개선 방향

### ✅ P1 — 강제 다시 그리기 (Invalidate / Partial Redraw) — 2026-08-31 구현 완료

**문제**
- `JKControl`은 매 프레임 전체 `OnPaintClient`를 다시 그림.
- 지뢰찾기 셀 하나가 바뀌어도 전체 9×9 그리드를 다시 그려야 함.
- 큰 보드나 복잡한 커스텀 컨트롤에서 성능/반응 문제가 생길 수 있음.

**구현 내용**
1. `JKControl::Invalidate()` 및 `JKControl::InvalidateRect(const JKRect&)` 추가 (`src/JKControl.cpp`).
   - 로컬 좌표를 스크린 좌표로 변환 후 가장 가까운 `JKWindow`에 dirty region 등록.
2. `JKWindow::AddDirtyRect()` / `HasDirtyRects()` / `ClearDirtyRects()` 추가 (`src/JKWindow.cpp`).
3. `JKApplication::Render()`가 dirty region이 있으면 `RenderDirtyRegions()`로 부분 갱신, 없으면 기존 전체 그리기 fallback 유지.
4. `RenderDirtyRegions()`는 각 dirty rect를 클립 후 배경 클리어 → 윈도우 계층 다시 그리기 → backbuffer에서 해당 영역만 blit.

**적용 사례**
- `MineGrid` 우클릭 시 변경된 셀 ±2px 무효화 (3D 테두리 겹침 고려).
- `MineGrid` 좌클릭 Flood Fill이나 코드(Chord) 시 전체 보드 무효화.

**교훈 / 발견된 다음 문제**
- 부분 그리기를 할 때 **겹치는 시각적 요소**가 있으면 무효 영역을 약간 확장해야 한다.
  지뢰찾기 셀의 `Box3D` 2px 테두리는 이웃 셀 위로 삐져나오므로, 한 셀만 깨끗이 그리면
  이웃 테두리가 잘린 것처럼 보일 수 있다.
- `JKWindow::OnPaintClient()`가 **전체 클라이언트 배경을 항상 `FillRect`로 지우므로**, 
  부분 클립 상태에서도 안전하게 동작하지만, 배경을 먼저 지우는 비용이 있다.
- 모달/자식 윈도우가 겹칠 때 dirty region은 **부모 기준**으로 누적되지만, 실제 그리기는
  윈도우 전체 계층을 다시 그리므로 정확함. 다만 dirty rect를 좀 더 정교하게 합치거나
  (union / minimize) 최적화하면 성능이 더 좋아진다.

**핵심 파일**
- `include/JKControl.h`, `src/JKControl.cpp`
- `include/JKWindow.h`, `src/JKWindow.cpp`
- `include/JKApplication.h`, `src/JKApplication.cpp` (`Render()`, `RenderDirtyRegions()`)
- `src/apps/MineSweeperApp.cpp`

---

### P1 — 레이아웃 도크/매니저 (Dock / Fill Layout)

**현재 상황**
- `Anchor`/`Margin`/`Padding`은 있지만, "위쪽 툴바를 뺀 나머지를 그리드가 채운다"
  같은 자동 배치가 없음.
- 지뢰찾기 툴바/그리드, 메모장 메뉴/에디트, 그림판 툴바/캔버스를 모두 절대
  좌표로 배치해야 함.

**개선 방향**
1. `JKControl`에 `SetDock(uint32_t flags)` 추가:
   - `DOCK_TOP`, `DOCK_BOTTOM`, `DOCK_LEFT`, `DOCK_RIGHT`, `DOCK_FILL`.
2. `JKWindow::PerformLayout()`에서 도크 순서를 계산:
   - 엣지(Edge) 컨트롤을 먼저 배치, 남은 공간을 `DOCK_FILL` 컨트롤이 차지.
3. 앵커 기반 레이아웃과 조합: 도크가 설정되면 앵커 무시.

**수혜 앱**
- 모든 새 앱(지뢰찾기, 메모장, 그림판)의 창 크기 조절 대응.

**핵심 파일**
- `include/JKControl.h`
- `src/JKControl.cpp` (`PerformLayout`, `SetRect`)

---

### P2 — 전역 단축키 / 가속기 테이블 (Accelerator Table)

**현재 상황**
- `F2 = New Game`, `Ctrl+N`, `Ctrl+S` 같은 단축키를 컨트롤/윈도우마다 직접
  `KeyDown`으로 처리해야 함.
- 포커스가 어디 있느냐에 따라 단축키가 먹히지 않을 수 있음.

**개선 방향**
1. `JKApplication`에 가속기 등록 API 추가:
   ```cpp
   void RegisterAccelerator(SDL_Keycode key, uint16_t controlId,
                            std::function<void()> onActivate);
   void RegisterAccelerator(SDL_Keycode key, uint16_t controlId,
                            uint16_t targetControlId); // 메시지 전송
   ```
2. `JKApplication::Run()`의 `KeyDown` 처리 전에 가속기를 먼저 체크.
3. `SDLK_F2`, `Ctrl+S` 같은 복합 키 지원 (`SDL_GetModState()` 활용).

**수혜 앱**
- 지뢰찾기(F2 재시작, F1 도움말)
- 메모장(Ctrl+N, Ctrl+O, Ctrl+S, Ctrl+Z)
- 그림판(Ctrl+Z/Y, 도구 단축키)

**핵심 파일**
- `include/JKApplication.h`, `src/JKApplication.cpp`
- `include/JKEvent.h` (가속기 전용 이벤트 타입 추가 고려)

---

### P2 — 모달 다이얼로그 생명주기 및 소유권 정리

**현재 상황**
- `JKMessageBox`를 `mainWindow_->AddControl()`하면 메인 윈도우와 `modalWindow_`
  양쪽에서 두 번 그림.
- 콜백 안에서 메시지 박스 자체를 삭제하면 UB.
- `ShowModalMessage()`는 이미 복원 콜백을 제공하지만, 메시지 박스의 삭제 시점이
  불명확.

**개선 방향**
1. 모달 윈도우는 반드시 최상위 레이어로만 그리도록 `Render()` 수정.
2. `JKMessageBox` 등 모달 다이얼로그에 소유자(`owner`) 명시.
3. 닫힐 때 자동 삭제 또는 재사용 가능한 두 가지 모드를 지원:
   - `JKMessageBox::Show()` → 자동 삭제 모드
   - `apputil::ShowModalMessage()` → 재사용 모드(슬롯)
4. `JKApplication`이 열린 모달 목록을 추적하여 중첩 모달 안정성 향상.

**수혜 앱**
- 지뢰찾기 승리/패배 메시지
- 메모장 "저장하시겠습니까?" 확인
- 모든 파일/확인 대화상자

**핵심 파일**
- `include/JKMessageBox.h`, `src/JKMessageBox.cpp`
- `include/apps/AppUtil.h`, `src/JKApplication.cpp` (`Render`, `SetModalWindow`)

---

### ✅ P2 — 비트맵 Blit / Sprite API — 2026-08-31 구현 완료

**현재 상황**
- `JKDC`는 도형(사각형, 원, 텍스트)만 지원. 비트맵 이미지를 화면에 붙이려면
  `JKOffscreenSurface`를 활용해야 하지만 공개 API가 불명확.
- 지뢰찾기 bomb/flag 아이콘은 `"*"`, `"F"` 문자로 대체.

**구현 내용**
1. `JKDC::DrawSprite(JKPoint, texture, texW, texH)` 및
   `JKDC::DrawSpriteX(const JKRect&, texture, texW, texH, adjflag)` 추가 (`include/JKDC.h`, `src/JKDC.cpp`).
   - 내부적으로 `JKRenderBackend::BlitTexture`를 사용.
   - `DrawSpriteX`는 `ADJ_XYCENTER` 등 조정 플래그로 목적지 사각형 안에서 정렬 및 클리핑.
2. `JKResourceCache`에 런타임 RGBA 버퍼에서 텍스처를 생성하는
   `CreateImageFromRGBA(key, w, h, rgba)` 및 텍스처 크기를 반환하는 `GetImageSize(key)` 추가.
3. `MineSweeperApp`에서 16×16 RGBA 아이콘을 런타임 생성하여 캐시에 등록:
   - `CreateMineIcon()` (검은 원형 폭탄)
   - `CreateFlagIcon()` (빨간 깃발)
   - `CreateQuestionIcon()` (회색 물음표)
4. `MineGrid::DrawCell`에서 캐시된 텍스처가 있으면 스프라이트로 그리고, 없으면 기존 ASCII 폴백 유지.

**수혜 앱**
- 지뢰찾기(폭탄/깃발 아이콘)
- 그림판(도구 아이콘, 스탬프)
- 메모장(줄번호/상태 아이콘)

**핵심 파일**
- `include/JKDC.h`, `src/JKDC.cpp`
- `include/JKResourceCache.h`, `src/JKResourceCache.cpp`
- `src/apps/MineSweeperApp.cpp`

---

### P3 — 랜덤 / 유틸리티 API

**현재 상황**
- 지뢰 배치에 `<random>`을 직접 사용. 시드 고정, 게임 리플레이, 테스트 재현이
  어려움.

**개선 방향**
1. `include/JKUtil.h` 또는 `include/JKRandom.h` 추가:
   ```cpp
   void RandomSeed(uint32_t seed);
   int RandomInt(int min, int max);
   double RandomDouble();
   ```
2. 내부적으로는 `std::mt19937` 유지하되 전역 상태 노출 금지.
3. `MineSweeperGame::NewGame(seed)` 형태로 시드 지정 가능.

**수혜 앱**
- 지뢰찾기(랜덤 시드, 난이도, 리플레이)
- 모든 확률/랜덤 요소가 있는 게임

**핵심 파일**
- `include/JKRandom.h`, `src/JKRandom.cpp`
- `include/apps/MineSweeperApp.h` (`NewGame(seed)`)

---

### P3 — 파일 I/O 추상 (JKTextFile / Encoding Helper)

**현재 상황**
- `JKFileDialog`는 있지만, 텍스트 파일의 UTF-8 ↔ KSSM 변환을 직접 구현해야 함.
- `std::fstream` + `Utf8ToKssm()` 조합으로 우회 가능.

**개선 방향**
1. `JKTextFile` 클래스 추가:
   - `LoadUtf8(path)` / `LoadKssm(path)` / `SaveUtf8(path)` / `SaveKssm(path)`.
2. 내부적으로는 UTF-8 ↔ KSSM 변환을 `JKHangulUtil`에 위임.
3. 줄 단위 읽기, CR/LF 통합 지원.

**수혜 앱**
- 메모장(파일 열기/저장)
- 모든 텍스트 기반 데이터 편집

**핵심 파일**
- `include/JKTextFile.h`, `src/JKTextFile.cpp`
- `include/JKHangulUtil.h`, `src/JKHangulUtil.cpp`

---

### P4 — Undo / Redo 기반 상태 관리

**현재 상황**
- 에디트는 내부적으로 버퍼를 갖고 있지만, 일반 앱 상태(그림판 필기, 지뢰찾기
  깃발)에 대한 Undo/Redo는 없음.

**개선 방향**
1. `JKCommand` 인터페이스 설계:
   ```cpp
   class JKCommand {
   public:
       virtual ~JKCommand() = default;
       virtual void Execute() = 0;
       virtual void Undo() = 0;
   };
   ```
2. `JKCommandStack` (Undo/Redo 스택) 추가.
3. 앱이나 컨트롤에서 `ExecuteCommand(std::unique_ptr<JKCommand>)` 호출.

**수혜 앱**
- 그림판(브러시, 도형, 지우개)
- 메모장(텍스트 편집 — JKEdit에 통합)
- 지뢰찾기(깃발 실수 취소)

**핵심 파일**
- `include/JKCommand.h`, `src/JKCommand.cpp`

---

### P4 — 사운드 (JKAudio)

**현재 상황**
- SDL2만 링크되어 있음. 사운드 효과를 재생할 수 없음.

**개선 방향**
1. SDL_Mixer를 선택적 의존성으로 추가.
2. `JKAudio` 또는 `JKSound` 클래스:
   - `LoadWav(path)`, `Play()`, `Stop()`, `SetVolume()`.
3. 없으면 무반응(no-op)으로 동작하도록 설계.

**수혜 앱**
- 지뢰찾기(클릭/폭발), 메모장(경고음), 그림판(효과음)

**핵심 파일**
- `include/JKAudio.h`, `src/JKAudio.cpp`
- `CMakeLists.txt` (SDL2_mixer 의존성 추가)

---

### P5 — 창 크기/좌표계 개선

**현재 상황**
- `JKApplication::Init(width, height)`가 메인 윈도우의 논리 좌표계를 결정.
- `OnInit()`에서 `SetWindowRect()`를 해도 `UpdateScale()`이 덮어씀.
- 작은 게임창(320×380)을 만들려면 Init 인자로 정확히 맞춰야 함.

**개선 방향**
1. `Init()`은 최소 창 크기만 제안, 실제 논리 크기는 `OnInit()` 이후
   `mainWindow_->SetWindowRect()`로 확정.
2. 또는 `JKApplication`에 `SetLogicalSize(int w, int h)`를 별도 제공.
3. HiDPI/레터박스 스케일링은 이 논리 크기를 기준으로 동작.

**수혜 앱**
- 지뢰찾기(작은 고정 창)
- 모든 크기가 중요한 게임/유틸리티

**핵심 파일**
- `include/JKApplication.h`, `src/JKApplication.cpp`

---

## 3. 새로 발견된 문제점 / 다음 개선 후보

이번 Invalidate 구현을 통해 원래 목록에 없던 구체적인 문제들이 드러났습니다.

### R1 — 겹치는 시각적 요소의 부분 무효화
- `Box3D`, 테두리, 그림자 등이 이웃 픽셀에 그려지면 단순 셀 rect 무효화로는 잔상이 남는다.
- 원본 JKWINDOW는 `RepaintRegion.MinimizeCount()`와 `NopaintRegion`으로 더 정교하게 처리.
- SDL2 프로토타입에서는 우선 무효 영역을 수동 확장(지뢰찾기 셀 ±2px)으로 해결하지만,
  일반적인 해결챠은 컨트롤이 자신의 "영향 반경"(visual padding)을 알려주는 것.

### R2 — 배경 클리어 비용
- `JKWindow::OnPaintClient()`가 항상 전체 클라이언트를 `FillRect`로 지운다.
- 부분 클립 시에도 이 비용은 무시할 만하지만, `PaintClient`가 clip stack을 존중하도록
  보장해야 한다. 현재 `PushClipRect`가 상위 클립과 교차하므로 안전.

### R3 — Dirty Region 합치기(Union) 최적화
- 현재는 `std::vector<JKRect>`에 그대로 쌓는다. 같은 셀을 여러 번 무효화하면 중복 blit 발생.
- 원본의 `RectQue` / `MinimizeCount()`처럼 겹치는 rect를 합치는 최적화가 필요.

### R4 — 모달/자식 윈도우의 독립적 dirty region
- `MineWindow`(플로팅) 같은 자식 윈도우가 `AddDirtyRect`하면 부모 `JKWindow`로 올라간다.
- 부모가 전체를 다시 그리므로 정확하지만, 최상위 `JKApplication`이 각 윈도우별 dirty를
  따로 관리하면 더 효율적이다.

---

## 3. 우선순위 요약

| 우선순위 | 항목 | 상태 | 다음 앱에서 자연스럽게 구현 | 영향 범위 |
|----------|------|------|------------------------------|-----------|
| **P1** | 강제 다시 그리기 (Invalidate) | ✅ 구현 완료 (2026-08-31) | 지뢰찾기 성능 개선, 그림판 | JKControl, JKWindow, JKApplication |
| **P1** | 레이아웃 도크 (Dock/Fill) | 미구현 | 메모장, 지뢰찾기, 그림판 | JKControl, PerformLayout |
| **P2** | 전역 단축키 (Accelerator) | 미구현 | 메모장, 지뢰찾기 | JKApplication |
| **P2** | 모달 생명주기 정리 | 미구현 | 모든 메시지/확인 박스 | JKMessageBox, JKApplication |
| **P2** | 비트맵 Blit/Sprite | ✅ 구현 완료 (2026-08-31) | 지뢰찾기, 그림판 | JKDC, JKResourceCache |
| **P2** | Dirty Region Union 최적화 | 미구현 | 지뢰찾기 성능 심화 | JKWindow |
| **P3** | 랜덤/유틸리티 API | 미구현 | 지뢰찾기(시드, 난이도) | JKRandom |
| **P3** | 파일 I/O 추상 | 미구현 | 메모장 | JKTextFile |
| **P4** | Undo/Redo 명령 스택 | 미구현 | 그림판, 메모장 | JKCommand |
| **P4** | 사운드 | 미구현 | 게임 효과음 | JKAudio + CMake |
| **P5** | 창 크기/좌표계 개선 | ✅ 일부 해결 (2026-08-31) | 모든 앱 | JKApplication |

---

## 4. 권장 진행 순서 (2026-08-31 갱신)

사용자가 "하나씩 개선"하기로 했으므로, 다음과 같은 순서를 권장합니다.

### 완료된 항목

1. ✅ **Invalidate / Partial Redraw** — 2026-08-31 완료.
2. ✅ **Bitmap Blit / Sprite** — 2026-08-31 완료.

### 남은 항목

3. **Dirty Region Union 최적화 (R3)**
   - Invalidate가 많이 발생하는 지뢰찾기 E 모드(16×30)나 그림판에서 중복 blit 제거.
   - 작업량이 적고 성능 향상이 큰 항목.

4. **Dock/Fill Layout**
   - 메모장이나 지뢰찾기를 리팩토링하면서 툴바/콘텐츠 자동 배치 구현.
   - 레이아웃 반복 코드를 대폭 줄임.

5. **Accelerator Table**
   - 메모장 단축키를 구현하면서 JKApplication 레벨 단축키 추가.
   - 지뢰찾기 F2 재시작도 함께 해결.

6. **Modal Dialog Lifecycle**
   - 메모장의 "저장하시겠습니까?" 확인 상자 구현하면서 메시지 박스 생명주기 정리.

7. **Random / TextFile / Audio / Command**
   - 각 앱의 고급 기능을 추가하면서 순차적으로 도입.

---

## 5. 관련 파일

- `prototype/sdl2_jkwindow/include/JKControl.h`
- `prototype/sdl2_jkwindow/include/JKWindow.h`
- `prototype/sdl2_jkwindow/include/JKApplication.h`
- `prototype/sdl2_jkwindow/include/JKDC.h`
- `prototype/sdl2_jkwindow/include/JKMessageBox.h`
- `prototype/sdl2_jkwindow/include/apps/AppUtil.h`
- `prototype/sdl2_jkwindow/src/apps/MineSweeperApp.cpp` (참조 구현)
- `ARCHITECTURE_DOCS/12_sdl2_prototype_roadmap.md`
