# SDL2 JKWindow Prototype Roadmap

> SDL2 기반 JKWINDOW GUI 프로토타입의 단계별 개발 계획.  
> 웹 재구현 로드맵(`06_web_port_roadmap.md`)과는 **별개**이며, JKENGINE의 원본 GUI 프레임워크를 SDL2 위에서 동작하도록 재현하는 것이 목표입니다.

---

## 1. 목표와 범위

### 목표

- JKWINDOW의 핵심 클래스 구조(`JKControl` → `JKWindow` → `JKApplication`)를 SDL2로 재현.
- 메시지 루프, 이벤트 라우팅, 마우스/키보드 입력, 포커스/캡처, 모달 대화상자를 동작시킴.
- 한글 출력/입력, 리소스 캐시, JKDBASE 호환 데이터 파일 I/O를 실험적으로 구현.
- Windows(MSYS2/UCRT64) + VS Code 환경에서 빌드/실행 가능한 실행 파일을 검증.

### 범위

- **In scope**: SDL2 프로토타입 전용 코드(`prototype/sdl2_jkwindow/`).
- **Out of scope**: 원본 DOS/VESA 코드 수정, 웹 포팅, 완전한 JKDBASE/JKWINDOW 기능 재현.

---

## 2. 현재 상태 요약

| Phase | 이름 | 상태 | 위치 |
|-------|------|------|------|
| 0 | Foundation / Basic Controls | ✅ 완료 | `main` (commit `0da2d41`) |
| 1 | Input / Focus System | 🔄 진행 중 | `phase1-input-focus` (commit `cf61dc5`) / `phase1_input_focus.md` |
| 2~4 | Layout / Rendering / Resource | ✅ 브랜치 구현 완료 | `phase2-full-stack` (commit `d467ee2`) |
| 5 | Integration & Verification | ⏳ 미시작 | — |

> Phase 2, 3, 4는 각각 "Layout", "Rendering Backend", "Resource/Data Layer"로 나누어 계획했으나, 실제 구현은 `phase2-full-stack` 브랜치에서 **통합**되어 한 커밋(`d467ee2`)으로 들어갔습니다. 본 로드맵에서는 개념상 구분을 유지하되, 구현 커밋은 동일함을 명시합니다.

---

## 3. Phase 상세

### Phase 0: Foundation / Basic Controls ✅

- 기본 `JKApplication` / `JKWindow` / `JKControl` 계층
- 단일 SDL 창, 메시지 루프, 마우스/키보드/QUIT 이벤트 라우팅
- `JKButton`, `JKStatic`, `JKEdit`, `JKCheckBox` 등 기본 컨트롤
- 모달 윈도우 라우팅의 초기 골격
- 한글 매니저 통합 (`JKHangulManager`, `JKHangulAutomata`)

**검증 기준**

- [x] `prototype/sdl2_jkwindow`가 CMake로 빌드됨.
- [x] 데모 윈도우가 실행되고 버튼/에디트/체크박스에 입력이 반응함.

---

### Phase 1: Input / Focus System 🔄

> 상세 작업 명세: `../phase1_input_focus.md`

이 Phase의 목표는 마우스 캡처, 키보드 포커스, Tab 내비게이션, 모달 입력 격리를 **견고하게** 만드는 것입니다.

#### 1.1 Mouse capture

- [x] `JKApplication`에 `captureControl_`과 `SetCapture()` / `ReleaseCapture()` / `GetCapture()` API 복원.
- [x] 마우스가 컨트롤 영역을 벗어나도 `MouseMove`/`MouseUp`이 캡처한 컨트롤로 전달되도록 라우팅.
- [x] `JKButton`: 눌린 상태에서 커서가 벗어나면 해제, 다시 들어오면 복원. 클릭은 `MouseUp` 시점에 커서가 버튼 안에 있을 때만 발생.
- [x] `JKEdit`: 드래그 선택 시 커서가 에디트 박스를 벗어나도 선택 범위가 계속 갱신되도록 캡처 사용.
- [ ] 창 이동/크기 조정 중에도 캡처/좌표 변환이 안정적으로 동작 (regression test).

#### 1.2 Focusable controls & Tab navigation

- [x] `JKControl`에 `IsFocusable()` / `SetFocusable()` 추가.
- [x] `JKControl::SetFocus()`, `IsFocused()`, `OnSetFocus()`, `OnKillFocus()`, `PaintFocus()` 구현.
- [x] `JKWindow::FocusFirstChild()`, `FocusNextChild()`, `FocusPrevChild()` 구현.
- [x] `JKButton`, `JKCheckBox`, `JKEdit`, `JKListBox`, `JKComboBox`에 `SetFocusable(true)` 설정.
- [ ] `Tab` → `FocusNextChild()`, `Shift+Tab` → `FocusPrevChild()` 라우팅 (`JKWindow::RespondMessage` 또는 `JKApplication::PreProcessMessage`).
- [x] 현재 포커스 컨트롤에 파란 사각형 포커스 인디케이터 그리기 (`PaintFocus`).

#### 1.3 Keyboard activation

- [x] `JKButton`: `Space` / `Enter`로 눌림/떼기 애니메이션과 `OnClick()` 실행.
- [x] `JKCheckBox`: `Space`로 체크 토글.
- [ ] `JKMessageBox`: `Enter`를 기본 버튼으로, `Escape`를 취소 버튼으로 처리 (현재 Escape만 처리됨).
- [ ] `JKFileDialog`: `Escape`로 취소/닫기.

#### 1.4 Modal input isolation

- [x] `JKApplication`에 `modalWindow_`와 `SetModalWindow()` / `GetModalWindow()`.
- [x] 모달 윈도우가 열려 있을 때 마우스 hit-test와 키보드 라우팅이 모달 윈도우로 우선 전달.
- [x] `JKMessageBox::Show()`에서 `FocusFirstChild()` 호출 후 모달 등록.
- [ ] 모달이 닫힐 때 이전 포커스 윈도우/컨트롤로 포커스 복원 (`inputWindow_` 스택 또는 복귀 컨트롤 저장).
- [ ] `JKMessageBox` / `JKFileDialog`가 열려 있는 동안 메인 윈도우의 컨트롤은 클릭/키보드 이벤트를 받지 않음.

#### 1.5 Verification

- [ ] `prototype/sdl2_jkwindow` 빌드 성공.
- [ ] 데모 윈도우에서 `Tab`이 edit → button → checkbox → list → combo 순으로 순환.
- [ ] 멀티라인 에디트에서 마우스를 에디트 박스 밖으로 드래그해도 선택이 계속 확장/변경됨.
- [ ] 버튼을 누른 채 마우스가 버튼 밖으로 나갔다가 다시 들어오면 눌린 상태가 복원됨.
- [ ] `MessageBox`/`FileDialog`가 열려 있을 때 메인 윈도우 클릭이 무시되고 `Escape`로 닫힘.

---

### Phase 2: Layout & Rendering Backend ✅ (branch: `phase2-full-stack`)

> 본 Phase의 코드는 `phase2-full-stack` 브랜치 커밋 `d467ee2`에서 구현되었습니다.

- [x] `JKControl`에 Anchor / Margin / Padding / AutoSize 레이아웃 API 추가.
- [x] `PerformLayout()`과 `MeasureContent()`를 통해 부모 크기 변경 시 자동 재배치.
- [x] `JKRenderBackend` 인터페이스 분리: SDL2 렌더링 세부사항을 `JKSDLRenderBackend`로 캡슐화.
- [x] `JKDC`가 `JKRenderBackend`를 사용하도록 리팩토링.
- [x] `JKOffscreenSurface` 및 back-buffer 구현: HiDPI/물리 픽셀 스케일링 대응.

**merge 시 추가 작업**

- [ ] `phase2-full-stack` → `phase1-input-focus` 또는 `main`으로 머지.
- [ ] 머지 후 창 크기 조정, 앵커 기반 재배치, back-buffer 재생성이 충돌 없이 동작하는지 확인.

---

### Phase 3: Resource & Data Layer ✅ (branch: `phase2-full-stack`)

> 본 Phase의 코드도 `phase2-full-stack` 브랜치 커밋 `d467ee2`에서 구현되었습니다.

- [x] `JKResourceCache`: 폰트/텍스처 리소스 등록 및 조회.
- [x] `JKDataFile`: JKDBASE `.dat` 파일 형식 호환의 생성/읽기/쓰기/레코드 추가 API.
- [x] `JKOffscreenSurface`: off-screen 텍스처 기반 그리기.

**merge 시 추가 작업**

- [ ] `JKDataFile`로 생성한 파일이 향후 원본 JKDBASE와 바이너리 호환되는지 문서화.
- [ ] `JKResourceCache`를 통해 아이콘/비트맵 폰트 로딩 시 캐시 히트/미스 테스트.

---

### Phase 4: Dialog / Menu / Advanced Controls ✅ (branch: `phase2-full-stack`)

> 본 Phase의 코드도 `phase2-full-stack` 브랜치 커밋 `d467ee2`에서 구현되었습니다.

- [x] `JKMenu`: 메뉴바 및 드롭다운 메뉴 항목.
- [x] `JKMessageBox`: OK/OKCancel/YesNo/YesNoCancel 버튼 조합.
- [x] `JKFileDialog`: 파일 선택 대화상자 골격.
- [x] `JKScrollBar`, `JKListBox`, `JKComboBox`: 고급 컨트롤.

**merge 시 추가 작업**

- [ ] 메뉴 포커스/키보드 내비게이션 (방향키, Enter, Escape).
- [ ] `JKFileDialog` 실제 파일 시스템 탐색 및 선택 결과 반환.
- [ ] `JKListBox` / `JKComboBox`의 키보드 선택/스크롤 연동.

---

### Phase 5: Integration & Verification ⏳

Phase 1~4의 코드를 통합하고, SDL2 프로토타입이 안정적으로 빌드/실행되도록 검증합니다.

- [ ] `phase1-input-focus` + `phase2-full-stack` 통합 머지.
- [ ] Windows MSYS2 UCRT64 환경에서 CMake 빌드 검증.
- [ ] HiDPI 모니터에서 논리 좌표/물리 픽셀 스케일링 검증.
- [ ] 창 크기 조정, 최소화/복원, 전체화면 전환 시 레이아웃/렌더링 검증.
- [ ] 한글 조합 입력(완성형 → KSSM 변환) 및 출력 검증.
- [ ] 마우스/키보드 이벤트에 대한 간단한 단위/통합 테스트 추가 (선택).
- [ ] SDL3 마이그레이션 포인트 문서화 (`SDL_bool`, 창 생성 API 등).

> **2026-08 업데이트**: 창 배치(프레임 중앙 배치 + 클라이언트 원점 보정), DPI 배율 계산, 레터박스 스케일링, 마우스 좌표 변환이 `phase2-full-stack` 작업 트리에서 구현·검증되었습니다(125% DPI 환경). 상세 설계는 `14_sdl2_window_dpi.md` 참고.

---

## 4. 브랜치/커밋 맵

| Phase | 브랜치 | 핵심 커밋 | 제목 |
|-------|--------|-----------|------|
| 0 | `main` | `0da2d41` | Phase 0: reusable UI controls and modal routing for SDL2 prototype |
| 1 | `phase1-input-focus` | `cf61dc5` | Phase1-input-focus-system |
| 2~4 | `phase2-full-stack` | `d467ee2` | Phase2-4-layout-rendering-resource |
| 5 | 미정 | — | Integration & Verification |

---

## 5. 문서/코드 링크

| 문서/파일 | 설명 |
|-----------|------|
| `../phase1_input_focus.md` | Phase 1 상세 작업 명세 |
| `11_jkwindow_sdl_mapping.md` | JKWINDOW → SDL2 클래스 매핑 설계 |
| `10_sdl2_windows_setup.md` | Windows + MSYS2 + SDL2 개발 환경 세팅 |
| `../prototype/sdl2_jkwindow/` | SDL2 프로토타입 소스 디렉터리 |
| `../prototype/sdl2_jkwindow/src/main.cpp` | Phase 2/4 데모 및 통합 테스트 진입점 |

---

## 6. 결정사항/가정

1. **Phase 2~4는 통합 구현됨**: 원래 분리된 Phase 2(Layout), Phase 3(Resource/Data), Phase 4(Dialog/Controls)로 계획했으나, `phase2-full-stack` 브랜치에서 한 번에 구현되었습니다. 본 로드맵은 개념적 구분을 유지하면서도 실제 커밋 하나를 공유함을 명시합니다.
2. **웹 포팅과 독립**: 본 로드맵은 SDL2 프로토타입 전용입니다. 웹 포팅 계획은 `06_web_port_roadmap.md`를 참고합니다.
3. **Phase 5는 선택적**: MSYS2 환경 구축 및 빌드 검증이 가능해지면 Phase 5를 본격적으로 시작합니다.

